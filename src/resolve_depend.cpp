#include "resolve_depend.hpp"

#include <filesystem>

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

    // f.file_path = xp_make_string(permanent_allocator(), path);

    return f;
}






void collect_all_imports_in_ast_file(AstFile ast_file, Array<Ast *> *imported_packages) {
    for(isize i = 0; i < ast_file.top_levels.count; i++) {
        Ast *top_level = ast_file.top_levels[i];

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



Array<Package> resolve_dependencies(xpString main_path) {
    defer(xp_arena_allocator_clear(stage_allocator()));

    // 所有package所在
    Array<Package> packages = make_array<Package>(permanent_allocator());
    Array<PackageState> package_states = make_array<PackageState>(stage_allocator());

    auto add_new_package = [&](Package pkg) {
        array_push_back(&packages, pkg);
            array_push_back(&package_states, PackageState::Unresolved);
    };
    auto update_package_state = [&](isize package_index, PackageState new_state) {
        package_states[package_index] = new_state;
    };

    if(!check_directory_legel(main_path) && !check_file_legal(main_path)) {
        // TODO ERROR: main package路径不合法
        error_msg(NULL, "input path is not invalid directory or file: %s", main_path.c_str);
        return packages;
    }

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

    // 解析主package
    add_new_package(main_package);


    // 检查每个package的import, 添加新的package
    for(isize i = 0; i < packages.count; i++) {


        Package pkg = packages[i];
        update_package_state(i, PackageState::Resolving);

        for(isize j = 0; j < pkg.ast_files.count; j++) {
            AstFile ast_file = pkg.ast_files[j];


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


            // 处理每个imported package paths
            for(isize k = 0; k < abs_paths.count; k++) {
                xpString abs_import_path = abs_paths[k];

                if(!check_directory_legel(abs_import_path)) {
                    continue;
                }

                // 检查该package是否已经解析过 或 正在解析中

                // TODO: 目前似乎永远不会出现正在解析中的情况
                bool already_resolved = false;
                for(isize m = 0; m < packages.count; m++) {
                    if(xp_string_cmp(packages[m].path, abs_import_path) == 0) {

                        if(package_states[m] == PackageState::Resolving) {
                            // TODO ERROR: 循环依赖

                            UNREACHABLE();
                            error_msg(NULL, "circular dependency detected for package: %s", abs_import_path.c_str);
                        }

                        already_resolved = true;
                        break;
                    }

                }
                
                if(!already_resolved) {

                    // 解析该package
                    Package imported_package = tokenize_and_parse_package(abs_import_path.c_str);

                    // 添加到packages列表
                    add_new_package(imported_package);
                }
            }

            
        }

        update_package_state(i, PackageState::Resolved);


    }

        

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