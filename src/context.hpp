#ifndef CREST_CONTEXT_HPP
#define CREST_CONTEXT_HPP

#include <filesystem>

#include "package.hpp"
#include "error_msg.hpp"

struct ThreadPool;

struct Context {
    std::filesystem::path compiler_path;
    std::filesystem::path current_working_directory;
    std::filesystem::path output_path;


    xpString main_src_dir_path;

    Package global_blank_package;


    // NOTE: const_decl:function_decl_value, block, for_stmt, 
    xpHashMap<Ast *, Scope *> ast_scope_map;

    ErrorReporter reporter;

    Array<Package> all_packages;


    ThreadPool *thread_pool;
};

Context *context();


#endif // CREST_CONTEXT_HPP