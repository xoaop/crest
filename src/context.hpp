#ifndef CREST_CONTEXT_HPP
#define CREST_CONTEXT_HPP

#include <filesystem>

#include "package.hpp"
#include "error_msg.hpp"

#include "cir_builder.hpp"
#include "scope.hpp"

struct ThreadPool;


struct Context {
    std::filesystem::path compiler_path;
    std::filesystem::path current_working_directory;
    std::filesystem::path output_path;
    bool cir_dump = false;
    bool scope_dump = false;
    const char *target_triple = nullptr;   // -target 显式指定；null → 用 LLVM 默认 triple


    xpString main_src_dir_path;

    // Package搜索路径 — 按优先级排列
    Array<xpString> package_search_paths;

    Ref<Package> global_blank_package = Ref<Package>::INVALID_REF;

    ErrorReporter reporter;

    Array<Package> all_packages;

    Array<Scope> all_scopes;


    ValueMemory static_mem;

    ThreadPool *thread_pool;
};

Ref<Package> add_package(Context *ctx, Package pkg);


CIRInstruction* inst(CIRInstructionRef ref);

Context *context();





#endif // CREST_CONTEXT_HPP