#include "resolve_depend.hpp"

#include <filesystem>
#include <print>

#include "ast.hpp"
#include "tokenizer.hpp"
#include "parser.hpp"

#include "path.hpp"

#include "error_msg.hpp"



#include "context.hpp"

enum class PackageState {
    Unresolved,
    Resolving,
    Resolved,
};



void collect_all_imports_in_ast_file(AstFile ast_file, Array<Ast *> *imported_packages);
AstFile tokenize_and_parse_file(const char *path, Scope *parent);
Array<Package> resolve_dependencies(xpString main_dir_path);
Package tokenize_and_parse_package(const char *path_of_package_dir);
bool check_directory_legel(xpString path);
bool check_file_legal(xpString path);







// 
// 输入文件路径, 把源文件tokenize并parse为AstFile
//
AstFile tokenize_and_parse_file(const char *path, Scope *parent) {
    xpString code_str = file_to_string(path, permanent_allocator());
    
    xpPair<SourceCode, Array<Token>> src_code_and_tokens = tokenize(xp_make_string(permanent_allocator(), path), code_str);
    SourceCode src_code = src_code_and_tokens.first;
    Array<Token> tokens = src_code_and_tokens.second;

    AstFile f = parse_file(tokens, src_code);

    return f;
}






void collect_all_imports_in_ast_file(AstFile ast_file, Array<Ast *> *imported_packages) {
    for(Ast *top_level: ast_file.top_levels) {
        if(top_level->type == AstType_ConstDecl && top_level->ConstDecl.value_ast->type == AstType_Import) {
            array_push_back(imported_packages, top_level->ConstDecl.value_ast);
        }
    }
}


xpString import_path_to_package_path(xpOption<xpString> search_prefix, xpString import_path, xpAllocator allocator) {
    xpString abs_import_path = xp_make_string_zero();
    
    if(search_prefix.has_value()) {
        xpString prefix = search_prefix.unwrap();

        if(xp_string_equal(prefix, xp_string_c("std"))) {
            std::filesystem::path std_lib_path = context()->compiler_path / "std";
            xpString std_lib_path_str = xp_make_string(stage_allocator(), std_lib_path.string().c_str());
            
            abs_import_path = concat_path(std_lib_path_str, import_path, stage_allocator());
            
        } else {
            // TODO ERROR: 不支持的import搜索前缀
            UNREACHABLE();
        }

    } else {
        xpString default_base_path = context()->main_src_dir_path;
        abs_import_path = concat_path(default_base_path, import_path, stage_allocator());
    }

    return abs_import_path;
}

xpOption<Package *> get_package_by_import(xpOption<xpString> search_prefix, xpString import_path, Array<Package> *all_packages) {
    xpString abs_import_path = import_path_to_package_path(search_prefix, import_path, stage_allocator());

    for(isize i = 0; i < all_packages->count; i++) {
        if(xp_string_equal((*all_packages)[i].path, abs_import_path)) {
            return xpOption<Package *>::some(&(*all_packages)[i]);
        }
    }

    return xpOption<Package *>::none();
}


void resolve_packages_from_imports(Package curr_pkg, xpHashMap<xpString, PackageState> &package_states, Array<Package> &all_packages) {

    // 更改当前package状态为正在解析中
    xp_hash_map_set(&package_states, curr_pkg.path, PackageState::Resolving);


    for(isize j = 0; j < curr_pkg.ast_files.count; j++) {
        AstFile ast_file = curr_pkg.ast_files[j];
        Array<Ast *> import_asts = make_array<Ast *>(stage_allocator());
        collect_all_imports_in_ast_file(ast_file, &import_asts);


        // 处理搜索路径
        Array<xpString> abs_paths = make_array<xpString>(stage_allocator());
        for(Ast *import: import_asts) {
            xpOption<xpString> search_prefix = import->Import.search_prefix;
            xpString import_path = import->Import.path;
            xpString abs_import_path = import_path_to_package_path(search_prefix, import_path, stage_allocator());

            array_push_back(&abs_paths, abs_import_path);
        }


        // 递归处理每个imported package paths
        for(isize k = 0; k < abs_paths.count; k++) {
            xpString abs_import_path = abs_paths[k];
            if(!check_directory_legel(abs_import_path)) {
                continue;
            }

            // 检查该package是否 已经解析过 或 正在解析中
            PackageState *state_of_imported_package = xp_hash_map_get(package_states, abs_import_path);
            if(state_of_imported_package == nullptr) {
                Package imported_package = tokenize_and_parse_package(abs_import_path.c_str);

                array_push_back(&all_packages, imported_package);
                xp_hash_map_insert(&package_states, imported_package.path, PackageState::Unresolved);

                resolve_packages_from_imports(imported_package, package_states, all_packages);
            } else {
                PackageState state = *state_of_imported_package;
                
                if(state == PackageState::Resolving) {
                    // TODO ERROR: 循环依赖
                    context()->reporter.report_error(
                        import_asts[k]->span, 
                        curr_pkg.ast_files[j].source_code,
                        "circular dependency detected for package: {}", 
                        abs_import_path
                    );

                }
            }
        }
        
    }

    xp_hash_map_set(&package_states, curr_pkg.path, PackageState::Resolved);
}





Array<Package> resolve_dependencies(xpString main_path) {
    defer(xp_arena_allocator_clear(stage_allocator()));

    // 所有package所在
    Array<Package> packages = make_array<Package>(permanent_allocator());
    xpHashMap<xpString, PackageState> package_state_map = xp_hash_map_make<xpString, PackageState>(stage_allocator());


    if(!check_directory_legel(main_path) && !check_file_legal(main_path)) {
        // TODO ERROR: main package路径不合法
        std::println("Error: main package path '{}' is not a valid directory or file", main_path.c_str);
        exit(1);

        return packages;
    }


    // 解析main package
    Package main_package;
    if(is_existing_directory(main_path)) {
        context()->main_src_dir_path = main_path;

        // main_dir_path是一个目录
        main_package = tokenize_and_parse_package(main_path.c_str);
    } else if(is_existing_file(main_path)) {
        // main_dir_path是一个文件, 它是main package的唯一文件
        std::filesystem::path p{std::string(main_path.c_str, (size_t)main_path.length)};
        xpString parent_dir_path = xp_make_string(permanent_allocator(), p.parent_path().string().c_str());
        context()->main_src_dir_path = parent_dir_path;
        
        main_package = make_package(parent_dir_path, permanent_allocator());
        AstFile main_file = tokenize_and_parse_file(main_path.c_str, &main_package.package_scope);
        array_push_back(&main_package.ast_files, main_file);
    }


    array_push_back(&packages, main_package);
    xp_hash_map_insert(&package_state_map, main_package.path, PackageState::Unresolved);


    resolve_packages_from_imports(main_package, package_state_map, packages);


    return packages;
}




// 根据package目录路径得到package, 包括：
// 1. 解析该目录底下所有.crest文件为AstFile，并存入Package结构体中
// 2. 创建Package的Scope
Package tokenize_and_parse_package(const char *path_of_package_dir) {

    xpString package_dir_path = normalize_path(xp_string_c(path_of_package_dir), permanent_allocator());

    // 扫描该目录下所有.crest文件
    Array<xpString> crest_files = scan_crest_files(path_of_package_dir, permanent_allocator());

    // 解析每个文件
    Package package = make_package(package_dir_path, permanent_allocator());
    for(isize i = 0; i < crest_files.count; i++) {
        xpString crest_file_path = crest_files[i];
        AstFile ast_file = tokenize_and_parse_file(crest_file_path.c_str, &package.package_scope);

        array_push_back(&package.ast_files, ast_file);
    }

    return package;
}





Array<xpString> from_relative_to_absolute_import_paths(xpString base_path, Array<xpString> relative_import_paths) {
    Array<xpString> absolute_import_paths = make_array<xpString>(permanent_allocator());

    for(isize i = 0; i < relative_import_paths.count; i++) {
        xpString relative_path = relative_import_paths[i];
        xpString absolute_path = concat_path(base_path, relative_path, permanent_allocator());
        array_push_back(&absolute_import_paths, absolute_path);
    }

    return absolute_import_paths;
}


bool check_directory_legel(xpString path) {
    return is_existing_directory(path);
}

bool check_file_legal(xpString path) {
    return is_existing_file(path);
}