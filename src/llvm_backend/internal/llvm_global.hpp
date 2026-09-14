#pragma once

// LLVM-C 头文件统一入口：所有 llvm-c 类型在这里一次性 include。
// 其他内部文件只需 include 本头即可获得全部 LLVM 类型。

#include "llvm-c/Core.h"
#include "llvm-c/Comdat.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Analysis.h"
#include "llvm-c/BitWriter.h"
#include "llvm-c/Transforms/PassBuilder.h"
#include "llvm-c/Linker.h"

#include "type.hpp"   // TypeRef / TypeHashKey


// 全局 LLVM 会话：跨 package 共享，init_llvm() 创建一次
struct LLVMSession {
    LLVMContextRef ctx;
    LLVMTargetMachineRef target_machine;
    LLVMTargetDataRef target_data;

    // 具名 struct / union 的 LLVM 类型缓存，键为 TypeHashKey
    xpHashMap<TypeHashKey, LLVMTypeRef> struct_types;
    xpHashMap<TypeHashKey, LLVMTypeRef> union_types;
};


extern LLVMSession g_llvm_session;

void init_llvm();   // 初始化 LLVM + 创建全局会话（main 入口调用）

// 类型 → LLVM 类型；struct / union 走 g_llvm_session 里的缓存
LLVMTypeRef get_llvm_type_from_type(TypeRef type);

// 类型在目标 data layout 下占的字节数；void 无尺寸，为 0
int size_of_type(TypeRef type);






