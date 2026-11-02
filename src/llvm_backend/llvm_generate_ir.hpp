#pragma once

#include "parser.hpp"
#include "package.hpp"
#include "lcir.hpp"

#include "llvm-c/Core.h"
#include "llvm-c/Comdat.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Analysis.h"
#include "llvm-c/BitWriter.h"
#include "llvm-c/Transforms/PassBuilder.h"
#include "llvm-c/Linker.h"


enum class LLVMIROptimizationLevel {
    O0 = 0, // 不优化, 生成的IR更接近源代码, 适合调试
    O1, // 适度优化, 在不显著增加编译时间的前提下优化性能
    O2, // 大幅优化, 生成高性能的IR, 可能会显著增加编译时间
    O3, // 极限优化, 生成最高性能的IR, 可能会极大增加编译时间, 适合发布版本
    Os, // 优化代码大小, 生成更小的IR, 适合嵌入式或对二进制大小敏感的场景
    Oz, // 极限优化代码大小, 生成最小的IR, 适合极端对二进制大小敏感的场景
};


struct LLVMIRGenerateConfig {
    LLVMIROptimizationLevel optimization_level = LLVMIROptimizationLevel::O0;
    // 共享库：给 extern_C 定义打 dllexport（Windows 的 DLL 默认不导出任何符号）
    bool export_dll_symbols = false;
};



void init_llvm();

// 跨包 extern_C 符号一致性：同名最多一个定义，ABI 签名必须兼容。
// 必须在 init_llvm() 之后调用（要用 make_c_abi_plan 比布局）。
void check_extern_c_symbols(const xpHashMap<Ref<Package>, lcir::Module>& modules);

Array<xpString> gen_ir_all_packages(xpHashMap<Ref<Package>, lcir::Module>& modules, LLVMIRGenerateConfig config);
