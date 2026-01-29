#include "resolve_depend.hpp"

#include "xoaop.h"
#include "ast.hpp"
#include "tokenizer.hpp"
#include "parser.hpp"
#include "package.hpp"

#include "path.hpp"








void collect_all_imports_in_ast_file(AstFile ast_file, Array<Ast *> *imports);
AstFile tokenize_and_parse_file(const char *path);
Array<Package> resolve_dependencies(xpString main_dir_path);
Package resolve_package(const char *path_of_package_dir);
Array<xpString> resolve_import_paths(AstFile ast_file);
Array<xpString> from_relative_to_absolute_import_paths(xpString base_path, Array<xpString> relative_import_paths);










// 
// 输入文件路径, 把源文件tokenize并parse为AstFile
//
AstFile tokenize_and_parse_file(const char *path) {
    xpString code_str = file_to_string(path, permanent_allocator());
    
    Tokenizer tokenizer;
    tokenizer_init(&tokenizer, code_str);
    tokenize(&tokenizer);

    AstFile f = parse_file(tokenizer.token_array);

    f.file_path = xp_make_string(permanent_allocator(), path);

    return f;
}






void collect_all_imports_in_ast_file(AstFile ast_file, Array<Ast *> *imports) {
    for(isize i = 0; i < ast_file.top_levels.count; i++) {
        Ast *top_level = ast_file.top_levels[i];
        if(top_level->type == AstType_Import) {
            array_push_back(imports, top_level);
        }    
    }    
}



Array<Package> resolve_dependencies(xpString main_dir_path) {
    // 所有package所在
    Array<Package> packages = make_array<Package>(permanent_allocator());

    // 解析主package
    Package main_package = resolve_package(main_dir_path.c_str);
    array_push_back(&packages, main_package);


    // 检查每个package的import, 添加新的package
    for(isize i = 0; true; i++) {


        bool all_resolved = true;


        
        
        
        Package pkg = packages[i];

        for(isize j = 0; j < pkg.ast_files.count; j++) {
            AstFile ast_file = pkg.ast_files[j];

            // 获取该文件import的所有package路径
            Array<xpString> relative_import_paths = resolve_import_paths(ast_file);

            // 根目录路径 + 相对import路径 -> 绝对import路径
            Array<xpString> absolute_import_paths = from_relative_to_absolute_import_paths(main_dir_path, relative_import_paths);
            
            // 处理每个imported package paths
            for(isize k = 0; k < absolute_import_paths.count; k++) {
                xpString abs_import_path = absolute_import_paths[k];

                // 检查该package是否已经解析过
                bool already_resolved = false;
                for(isize m = 0; m < packages.count; m++) {
                    if(xp_string_cmp(packages[m].path, abs_import_path) == 0) {
                        already_resolved = true;
                        break;
                    }
                }

                if(!already_resolved) {
                    all_resolved = false;
                }


                if(!already_resolved) {
                    // 解析该package
                    Package imported_package = resolve_package(abs_import_path.c_str);

                    // 添加到packages列表
                    array_push_back(&packages, imported_package);
                }
            }
        
        }

        if(all_resolved) {
            break;
        }


    }

        

    return packages;
}




// 根据package目录路径得到package, 包括：
// 1. 解析该目录底下所有.crest文件为AstFile，并存入Package结构体中
// 2. 创建Package的Scope
Package resolve_package(const char *path_of_package_dir) {

    xpString package_dir_path = xp_make_string(permanent_allocator(), path_of_package_dir);

    // 扫描该目录下所有.crest文件
    Array<xpString> crest_files = scan_crest_files(path_of_package_dir, permanent_allocator());

    // 解析每个文件
    Package package = make_package(package_dir_path, permanent_allocator());
    for(isize i = 0; i < crest_files.count; i++) {
        xpString crest_file_path = crest_files[i];
        AstFile ast_file = tokenize_and_parse_file(crest_file_path.c_str);

        array_push_back(&package.ast_files, ast_file);
    }

    return package;
}




// 获取一个文件import的所有依赖的package路径(相对于项目根目录)
Array<xpString> resolve_import_paths(AstFile ast_file) {
    xpString file_path = ast_file.file_path;

    Array<Ast *> imports = make_array<Ast *>(permanent_allocator());
    collect_all_imports_in_ast_file(ast_file, &imports);

    Array<xpString> imported_paths = make_array<xpString>(permanent_allocator());
    for(isize i = 0; i < imports.count; i++) {
        Ast *import_ast = imports[i];
        xpString import_path = import_ast->Import.path;
    
        array_push_back(&imported_paths, import_path);
    }

    return imported_paths;
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