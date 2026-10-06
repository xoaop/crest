#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "package.hpp"
#include "error_msg.hpp"
#include "target.hpp"
#include "link.hpp"

#include "cir_builder.hpp"
#include "scope.hpp"

#include "value_array.hpp"

#include "lcir.hpp"

struct ThreadPool;


struct Context {
    std::filesystem::path compiler_path;
    std::filesystem::path current_working_directory;
    std::filesystem::path output_path;
    bool cir_dump = false;
    bool scope_dump = false;
    std::string target_triple;   // 选项解析阶段定好：-target 生成，未指定则用宿主 triple
    TargetInfo target;
    const char *target_cpu = nullptr;      // -cpu 显式指定；null → generic
    const char *target_features = nullptr; // -features 显式指定；null → 用 cpu 自带特性
    LinkSubsystem link_subsystem = LinkSubsystem::Console;
    LinkCrt link_crt = LinkCrt::Static;
    std::vector<std::string> linker_args;     // -linker，原样透传给链接器，可重复


    xpString main_src_dir_path;

    // Package搜索路径 — 按优先级排列
    Array<xpString> package_search_paths;

    Ref<Package> global_blank_package = Ref<Package>::INVALID_REF;

    ErrorReporter reporter;

    Array<Package> all_packages;

    Array<Scope> all_scopes;

    xpHashMap<Ref<Package>, lcir::Module> lcir_modules;

    // 闲置：ValueMemory 体系的静态内存区
    // ValueMemory static_mem;

    // 编译期产出的 Value 槽（字面量字节、变量槽）：permanent，不回收
    ValueArray static_values;

    ThreadPool *thread_pool;
};

Ref<Package> add_package(Context *ctx, Package pkg);


const CIRInstruction& inst(CIRInstructionRef ref);

Context *context();
