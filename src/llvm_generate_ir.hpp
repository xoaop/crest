#ifndef CREST_LLVM_GENERATE_IR_HPP
#define CREST_LLVM_GENERATE_IR_HPP

#include "parser.hpp"
#include "package.hpp"

#include "llvm-c/Core.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Analysis.h"
#include "llvm-c/BitWriter.h"


struct LLVMLoopBlocks {
    LLVMBasicBlockRef post_block;
    LLVMBasicBlockRef merge_block;
};



// LLVM IR 生成器, 保存生成一个Module所需的状态
// 目前一个Module就代表一个package
struct LLVMGenerator {
    LLVMContextRef ctx;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMTargetMachineRef target_machine;
    LLVMTargetDataRef target_data;

    xpHashMap<xpString, LLVMValueRef> locals;

    Array<LLVMLoopBlocks> loop_stack;

    xpHashMap<xpString, LLVMTypeRef> struct_types;

    xpHashSet<xpString> declared_extern_functions;

    Package *pkg;
    Scope *curr_file_scope;
};





void gen_ir_all_packages(Array<Package> all_packages);



struct LLVMState {
    LLVMValueRef curr_function;
    LLVMBasicBlockRef curr_block;
    LLVMBasicBlockRef entry;
};



#endif