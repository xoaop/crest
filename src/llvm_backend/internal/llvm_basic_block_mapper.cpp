#include "llvm_global.hpp"

#include "llvm_basic_block_mapper.hpp"

#include "print.hpp"


LLVMBasicBlockRef LLVMBasicBlockMapper::first_frag_blk() {
    ASSERT_MSG(fragments.count > 0, "Remember to add at least one frag for block before getting first frag blk");
    return fragments[0];
}

LLVMBasicBlockRef LLVMBasicBlockMapper::last_frag_blk() {
    ASSERT_MSG(fragments.count > 0, "Remember to add at least one frag for block before getting last frag blk");
    return fragments.back();
}

LLVMBasicBlockRef LLVMBasicBlockMapper::exit_blk() {
    ASSERT_MSG(exit_block != nullptr, "Exit block not created yet");
    return exit_block;
}

LLVMBasicBlockRef LLVMBasicBlockMapper::frag_at(isize i) {
    ASSERT_MSG(i >= 0 && i < fragments.count, "frag_at index out of range");
    return fragments[i];
}

isize LLVMBasicBlockMapper::frag_count() {
    return fragments.count;
}


LLVMBasicBlockMapper::LLVMBasicBlockMapper(xpAllocator allocator, LLVMValueRef curr_func, bool create_exit_block) {
    fragments = make_array<LLVMBasicBlockRef>(allocator);
    break_vals = make_array<LLVMValueRef>(allocator);
    break_srcs = make_array<LLVMBasicBlockRef>(allocator);
    owner_func = curr_func;
    if(create_exit_block) {
        create_exit();
    } else {
        exit_block = nullptr;
    }
}

LLVMBasicBlockRef LLVMBasicBlockMapper::add_frag_blk(const char *name) {
    auto bb = LLVMAppendBasicBlockInContext(g_llvm_session.ctx, owner_func, name);
    fragments.push_back(bb);
    return bb;
}

void LLVMBasicBlockMapper::create_exit() {
    ASSERT_MSG(exit_block == nullptr, "Exit block already exists");
    exit_block = LLVMAppendBasicBlockInContext(g_llvm_session.ctx, owner_func, "block.exit");
}
