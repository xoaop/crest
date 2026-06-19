#ifndef CREST_LLVM_GENERATE_IR_HPP
#define CREST_LLVM_GENERATE_IR_HPP

#include "parser.hpp"
#include "package.hpp"

#include "llvm-c/Core.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Analysis.h"
#include "llvm-c/BitWriter.h"
#include "llvm-c/Transforms/PassBuilder.h"
#include "llvm-c/Linker.h"


enum class LLVMIROptimizationLevel {
    O0, // 不优化, 生成的IR更接近源代码, 适合调试
    O1, // 适度优化, 在不显著增加编译时间的前提下优化性能
    O2, // 大幅优化, 生成高性能的IR, 可能会显著增加编译时间
    O3, // 极限优化, 生成最高性能的IR, 可能会极大增加编译时间, 适合发布版本
    Os, // 优化代码大小, 生成更小的IR, 适合嵌入式或对二进制大小敏感的场景
    Oz, // 极限优化代码大小, 生成最小的IR, 适合极端对二进制大小敏感的场景
};


struct LLVMIRGenerateConfig {
    LLVMIROptimizationLevel optimization_level = LLVMIROptimizationLevel::O0;
};



void init_llvm();
Array<xpString> gen_ir_all_packages(Array<Package>* all_packages, LLVMIRGenerateConfig config);





#endif