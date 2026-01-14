#ifndef CREST_LLVM_GENERATE_IR_HPP
#define CREST_LLVM_GENERATE_IR_HPP

#include "parser.hpp"

#include "llvm-c/Core.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Analysis.h"
#include "llvm-c/BitWriter.h"


struct LLVMLoopBlocks {
    LLVMBasicBlockRef post_block;
    LLVMBasicBlockRef merge_block;
};

struct LLVMGenerator {
    LLVMContextRef ctx;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMTargetMachineRef target_machine;
    LLVMTargetDataRef target_data;

    xpHashMap<xpString, LLVMValueRef> locals;

    Array<LLVMLoopBlocks> loop_stack;

    xpHashMap<xpString, LLVMTypeRef> struct_types;
};




void init_llvm_generator(LLVMGenerator *gen);
void free_llvm_generator(LLVMGenerator *gen);


void gen_ir_astfile(AstFile f);


struct LLVMState {
    LLVMValueRef curr_function;
    LLVMBasicBlockRef curr_block;
    LLVMBasicBlockRef entry;
};

void load_state(LLVMGenerator *gen, LLVMState state);
LLVMState save_state(LLVMValueRef curr_function, LLVMBasicBlockRef curr_block, LLVMBasicBlockRef entry);

#endif