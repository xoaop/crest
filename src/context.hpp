#ifndef CREST_CONTEXT_HPP
#define CREST_CONTEXT_HPP

#include <filesystem>

#include "package.hpp"
#include "error_msg.hpp"

#include "cir_builder.hpp"

struct ThreadPool;

struct Context {
    std::filesystem::path compiler_path;
    std::filesystem::path current_working_directory;
    std::filesystem::path output_path;


    xpString main_src_dir_path;

    // Package搜索路径 — 按优先级排列
    Array<xpString> package_search_paths;

    Package global_blank_package;


    // NOTE: const_decl:function_decl_value, block, for_stmt, 
    // xpHashMap<Ast *, Scope *> ast_scope_map;

    ErrorReporter reporter;

    Array<Package> all_packages;


    ValueMemory static_mem;

    ThreadPool *thread_pool;
};

Context *context();


#endif // CREST_CONTEXT_HPP