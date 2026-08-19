#include "xoaop.h"   // XP_ASSERT_DEFAULT

#include "llvm_global.hpp"

#include "context.hpp"
#include "print.hpp"
#include "error_msg.hpp"


LLVMSession g_llvm_session = {};


void init_llvm() {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeDisassembler();

    // 全局会话创建一次（跨 package 共享）：ctx + target_machine + target_data
    // target triple：-target 显式指定优先，否则用 LLVM 默认 triple
    const char *triple = context()->target_triple
                       ? context()->target_triple
                       : LLVMGetDefaultTargetTriple();
    LLVMTargetRef target;
    char *error = nullptr;
    if(LLVMGetTargetFromTriple(triple, &target, &error)) {
        err("Error getting target: {}", error);
        LLVMDisposeMessage(error);
        XP_ASSERT_DEFAULT(0);
    }
    g_llvm_session.ctx = LLVMContextCreate();
    char *host_cpu = LLVMGetHostCPUName();
    char *host_features = LLVMGetHostCPUFeatures();
    g_llvm_session.target_machine = LLVMCreateTargetMachine(
        target,
        triple,
        host_cpu,
        host_features,
        LLVMCodeGenLevelDefault,
        LLVMRelocPIC,
        LLVMCodeModelDefault
    );
    LLVMDisposeMessage(host_cpu);
    LLVMDisposeMessage(host_features);
    g_llvm_session.target_data = LLVMCreateTargetDataLayout(g_llvm_session.target_machine);
}
