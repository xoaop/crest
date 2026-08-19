#include "compile.hpp"

#include "common.hpp"
#include "cir_builder.hpp"
#include "cir_interpreter.hpp"
#include "context.hpp"
#include "package.hpp"
#include "path.hpp"

#include "ast_file.hpp"
#include "file.hpp"
#include "parser.hpp"
#include "source_code.hpp"
#include "tokenizer.hpp"

#include <filesystem>



bool compile_package(PackageRef pkg_ref);

// 根据package目录路径得到package
Package tokenize_and_parse_package(const char *path_of_package_dir);

// 输入文件路径, 把源文件tokenize并parse为AstFile
AstFile tokenize_and_parse_file(const char *path);

xpOption<xpString> resolve_package_path(xpString import_path, xpAllocator allocator);

// 构造循环 import 报错消息
xpString make_import_cycle_message(PackageRef target);


bool check_directory_legel(xpString path);
bool check_file_legal(xpString path);




xpString make_import_cycle_message(PackageRef target) {
    Package *p = package_by_ref(target);
    std::string msg = "import 循环依赖: '";
    msg.append(p->path.c_str, (size_t)p->path.length);
    msg += "'";
    return xp_make_string_capacity(permanent_allocator(), msg.data(), (isize)msg.size());
}


xpOption<PackageRef> compile_package_from_import(xpString import_path, xpString *cycle_err) {
    auto resolved = resolve_package_path(import_path, permanent_allocator());
    if(resolved.is_none()) {
        return xpOption<PackageRef>::none();
    }

    return compile_package_from_path(resolved.unwrap(), cycle_err);
}



xpOption<PackageRef> compile_package_from_path(xpString path, xpString *cycle_err) {
    if(!is_existing_directory(path) && !is_existing_file(path)) {
        return xpOption<PackageRef>::none();
    }


    // TODO: 优化
    // 如果已经存在于 context()->all_packages 中，则直接返回已有的 PackageRef；
    // 若该包仍在编译中（state == Solving，尚未编译完成的祖先包）→ import 循环依赖
    xpString abs_path = normalize_path(path, permanent_allocator());
    for(isize i = 0; i < context()->all_packages.count; i++) {
        if(xp_string_equal(context()->all_packages[i].path, abs_path)) {
            if(context()->all_packages[i].state == SymbolState::Solving) {
                if(cycle_err) {
                    *cycle_err = make_import_cycle_message(i);
                }
                return xpOption<PackageRef>::none();
            }
            return xpOption<PackageRef>::some(i);
        }
    }

    // 根包（首个 add，global_blank_package 尚未设置）＝ builtin：scope/global_blank/基础类型符号特殊处理
    bool is_root_pkg = (context()->global_blank_package == -1);

    Package pkg;
    if(is_existing_directory(path)) {
        pkg = tokenize_and_parse_package(path.c_str);
    } else {
        // 单文件包: path 直接用归一化的文件路径(与 dedup 的 abs_path 一致)
        pkg = make_package(abs_path, permanent_allocator());
        AstFile file = tokenize_and_parse_file(path.c_str);
        pkg.ast_files.push_back(file);
    }

    PackageRef ref = add_package(context(), pkg);

    // 根包注册为 global_blank_package（scope 创建与基础类型符号在 resolve 阶段处理）
    if(is_root_pkg) {
        context()->global_blank_package = ref;
    }

    package_by_ref(ref)->state = SymbolState::Solving;

    compile_package(ref);
    
    package_by_ref(ref)->state = SymbolState::Solved;

    return xpOption<PackageRef>::some(ref);
}



bool compile_package(PackageRef pkg_ref) {
    resolve_package(pkg_ref);
    xp_arena_allocator_clear(stage_allocator());

    // TODO: 分开不同包的错误计数, 目前别的包的错误也在这里统计了, 导致本包即使没有错误也返回false
    // 相当于直接不解析了
    if(context()->reporter.error_count > 0) {
        return false;
    }

    CIRBuilder builder{stage_allocator()};
    builder.build_cir_package(pkg_ref);
    xp_arena_allocator_clear(stage_allocator());


    analyze_package(pkg_ref);

    // TODO: 分开不同包的错误计数, 目前别的包的错误也在这里统计了, 导致本包即使没有错误也返回false
    // 相当于直接不解析了
    if(context()->reporter.error_count > 0) {
        return false;
    }

    return true;
}





Package tokenize_and_parse_package(const char *path_of_package_dir) {

    xpString package_dir_path = normalize_path(xp_string_c(path_of_package_dir), permanent_allocator());

    // 扫描该目录下所有.crest文件
    Array<xpString> crest_files = scan_crest_files(path_of_package_dir, permanent_allocator());

    // 解析每个文件
    Package package = make_package(package_dir_path, permanent_allocator());
    for(isize i = 0; i < crest_files.count; i++) {
        xpString crest_file_path = crest_files[i];
        AstFile ast_file = tokenize_and_parse_file(crest_file_path.c_str);

        package.ast_files.push_back(ast_file);
    }

    return package;
}


AstFile tokenize_and_parse_file(const char *path) {
    xpString code_str = file_to_string(path, permanent_allocator());
    SourceCode src_code = make_source_code(xp_make_string(permanent_allocator(), path), code_str, permanent_allocator());
    Array<Token> tokens = tokenize(&src_code);

    return make_ast_file(parse(tokens, &src_code), src_code);
}




xpOption<xpString> resolve_package_path(xpString import_path, xpAllocator allocator) {
    Array<xpString>& search_paths = context()->package_search_paths;

    for (isize i = 0; i < search_paths.count; i++) {
        xpString base = search_paths[i];
        xpString candidate = concat_path(base, import_path, allocator);

        if (check_directory_legel(candidate)) {
            return xpOption<xpString>::some(candidate);
        }
    }

    return xpOption<xpString>::none();
}




bool check_directory_legel(xpString path) {
    return is_existing_directory(path);
}

bool check_file_legal(xpString path) {
    return is_existing_file(path);
}