#pragma once

// C ABI 降级（extern_C 边界）。
//
// 按目标分派：
//   Microsoft x64  —— 聚合体按大小一刀切：1/2/4/8 字节当同宽整数走寄存器，
//                     其它大小传指针；返回 >8 字节走 sret。
//   System V x86-64 —— 聚合体按 8 字节分块分类（INTEGER / SSE / MEMORY）：
//                     ≤16 字节走寄存器，>16 字节或含未对齐字段走内存；
//                     返回 >16 字节（或 MEMORY 类）走 sret。
//
// 只对 extern_C 边界生效：Crest 内部调用两边自洽，不受此约束。
// 尺寸/偏移来自语言布局（type_size_of / field_offset_in_struct），当前仅支持本机目标。

#include "type.hpp"
#include "target.hpp"
#include "llvm_global.hpp"



enum class ABIKind {
    MSX64,
    SysVX64,
};

// 平台 ABI 由目标平台决定；C 语言的类型映射见 make_c_abi_plan。
ABIKind current_abi(TargetOS target);


// System V x86-64 分类

struct SysVChunk {
    bool is_sse = false;      // 全 float/double 字段 → SSE；否则 INTEGER
    unsigned bytes = 0;       // 该块属于本类型的有效字节数（1..8）
    unsigned f32_count = 0;   // SSE 块内 f32 个数
    bool f64 = false;         // SSE 块内是 double
};

struct SysVShape {
    bool memory = false;      // >16 字节，或含未对齐字段
    int count = 0;            // 0..2
    SysVChunk chunks[2];
};

SysVShape classify_sysv(TypeRef type);


// 签名计划：phase1 声明和调用点共用同一份决定

struct ABIParamPlan {
    enum class Kind {
        Scalar,      // 一个 LLVM 参数（types[0]）
        Aggregate,   // 寄存器类聚合体，拆成 ntypes 个 LLVM 参数
        ByValPtr,    // 传指针（SysV 带 byval+align；MS 裸指针）
    } kind = Kind::Scalar;

    TypeRef source_type = nullptr;
    LLVMTypeRef types[2] = {nullptr, nullptr};
    int ntypes = 0;
    bool signext = false;
    bool zeroext = false;
    int align = 0;            // ByValPtr 的对齐
};

struct ABIFunctionPlan {
    ABIKind abi = ABIKind::MSX64;
    LLVMTypeRef fn_type = nullptr;
    TypeRef ret_type = nullptr;

    bool sret = false;
    bool ret_signext = false;
    bool ret_zeroext = false;

    // SysV 寄存器类聚合体返回：调用结果是这些块（1 或 2 个），需存回聚合体；
    // MS x64 的 ≤8 字节聚合体返回同样是 1 个整数块。
    LLVMTypeRef ret_chunk_types[2] = {nullptr, nullptr};
    int ret_chunks = 0;

    Array<ABIParamPlan> params = {};   // 固定形参，与源码形参一一对应
};

// C 语言在平台 ABI 上的类型映射：聚合体怎么分块、何时走 sret/byval。
ABIFunctionPlan make_c_abi_plan(TypeRef func_type, ABIKind abi);

// 把 plan 的属性写到函数声明或调用上。
// is_call=true 用 LLVMAddCallSiteAttribute，否则 LLVMAddAttributeAtIndex。
void apply_c_abi_attrs(LLVMValueRef fn_or_call, const ABIFunctionPlan& plan, bool is_call);
