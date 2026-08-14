#pragma once

#include "xoaop.h"                // xpAllocator / isize
#include "array.hpp"              // Array
#include "cir_instruction_ref.hpp" // CIRInstructionRef
#include "llvm_global.hpp"        // 全部 LLVM-C 类型

struct LLVMBasicBlockMapper {
    LLVMBasicBlockMapper() = default;
    LLVMBasicBlockMapper(xpAllocator allocator, LLVMValueRef curr_func, bool create_exit_block = true);

    LLVMBasicBlockRef add_frag_blk(const char *name);
    LLVMBasicBlockRef first_frag_blk();
    LLVMBasicBlockRef last_frag_blk();
    LLVMBasicBlockRef exit_blk();
    void create_exit();
    LLVMBasicBlockRef frag_at(isize i);
    isize frag_count();

    LLVMValueRef owner_func;  // 该 mapper 所属的 LLVM 函数

    // break φ：收集到达该 block 的带值 break（值 + 所在 BB + 结果 key）
    Array<LLVMValueRef> break_vals;
    Array<LLVMBasicBlockRef> break_srcs;  // 每个 break 所在的 LLVM BB（φ incoming block）
    CIRInstructionRef phi_key;            // block 结果 key（BlockRef 指令位置），φ 保存时用

private:
    Array<LLVMBasicBlockRef> fragments;
    LLVMBasicBlockRef exit_block;
};
