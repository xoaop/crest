#include "xoaop.h"

#include <cstring>

#include "llvm_global.hpp"

#include "context.hpp"
#include "common.hpp"
#include "print.hpp"
#include "error_msg.hpp"


LLVMSession g_llvm_session = {};


void init_llvm() {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeDisassembler();

    // 全局会话创建一次（跨 package 共享）：ctx + target_machine + target_data
    XP_ASSERT_DEFAULT(!context()->target_triple.empty());
    const char *triple = context()->target_triple.c_str();
    LLVMTargetRef target;
    char *error = nullptr;
    if(LLVMGetTargetFromTriple(triple, &target, &error)) {
        err("Error getting target: {}", error);
        LLVMDisposeMessage(error);
        XP_ASSERT_DEFAULT(0);
    }
    // 默认基线 CPU，产物不随编译机变；-features 可单独覆盖
    const char *cpu = "generic";
    const char *features = "";
    char *owned_cpu = nullptr;          // native 分支的临时串，建完 machine 后释放
    char *owned_features = nullptr;
    if(context()->target_cpu) {
        if(strcmp(context()->target_cpu, "native") == 0) {
            // LLVMCreateTargetMachine 不认 "native"，显式取宿主 CPU
            owned_cpu = LLVMGetHostCPUName();
            owned_features = LLVMGetHostCPUFeatures();
            cpu = owned_cpu;
            features = owned_features;
        } else {
            cpu = context()->target_cpu;
        }
    }
    // -features 覆盖 cpu 自带特性
    if(context()->target_features) {
        features = context()->target_features;
    }
    g_llvm_session.ctx = LLVMContextCreate();
    g_llvm_session.target_machine = LLVMCreateTargetMachine(
        target,
        triple,
        cpu,
        features,
        LLVMCodeGenLevelDefault,
        LLVMRelocPIC,
        LLVMCodeModelDefault
    );
    if(owned_cpu) LLVMDisposeMessage(owned_cpu);
    if(owned_features) LLVMDisposeMessage(owned_features);
    g_llvm_session.target_data = LLVMCreateTargetDataLayout(g_llvm_session.target_machine);

    g_llvm_session.struct_types = xp_hash_map_make<TypeHashKey, LLVMTypeRef>(permanent_allocator());
    g_llvm_session.union_types = xp_hash_map_make<TypeHashKey, LLVMTypeRef>(permanent_allocator());
}
