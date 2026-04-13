#ifndef CREST_CONTEXT_HPP
#define CREST_CONTEXT_HPP

#include <filesystem>

#include "package.hpp"
#include "error_msg.hpp"


struct Context {
    std::filesystem::path compiler_path;
    std::filesystem::path current_working_directory;


    xpString main_src_dir_path;

    Package global_blank_package;


    // NOTE: const_decl:function_decl_value, block, for_stmt, 
    xpHashMap<Ast *, Scope *> ast_scope_map;

    ErrorReporter reporter;

    Array<Package> all_packages;
};

Context *context();


#endif // CREST_CONTEXT_HPP