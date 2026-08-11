#ifndef CREST_CONTEXT_HPP
#define CREST_CONTEXT_HPP

#include <filesystem>

#include "package.hpp"
#include "error_msg.hpp"

#include "cir_builder.hpp"

struct ThreadPool;


using PackageRef = isize;


struct Context {
    std::filesystem::path compiler_path;
    std::filesystem::path current_working_directory;
    std::filesystem::path output_path;
    bool cir_dump = false;
    bool scope_dump = false;


    xpString main_src_dir_path;

    // Package搜索路径 — 按优先级排列
    Array<xpString> package_search_paths;

    Package *global_blank_package = nullptr;

    ErrorReporter reporter;

    Array<Package> all_packages;


    ValueMemory static_mem;

    ThreadPool *thread_pool;
};

PackageRef add_package(Context *ctx, Package pkg);

Context *context();


#endif // CREST_CONTEXT_HPP