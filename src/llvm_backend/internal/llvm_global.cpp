#include "xoaop.h"   // XP_ASSERT_DEFAULT

#include "llvm_global.hpp"

#include "print.hpp"
#include "error_msg.hpp"


LLVMSession g_llvm_session = {};


void init_llvm() {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeDisassembler();

    // 全局会话创建一次（跨 package 共享）：ctx + target_machine + target_data
    LLVMTargetRef target;
    char *error = nullptr;
    if(LLVMGetTargetFromTriple(LLVMGetDefaultTargetTriple(), &target, &error)) {
        err("Error getting target: {}", error);
        LLVMDisposeMessage(error);
        XP_ASSERT_DEFAULT(0);
    }
    g_llvm_session.ctx = LLVMContextCreate();
    char *host_cpu = LLVMGetHostCPUName();
    char *host_features = LLVMGetHostCPUFeatures();
    g_llvm_session.target_machine = LLVMCreateTargetMachine(
        target,
        LLVMGetDefaultTargetTriple(),
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
