#ifndef CREST_LLVM_BACKEND_INTERNAL_LLVM_ABI_HPP
#define CREST_LLVM_BACKEND_INTERNAL_LLVM_ABI_HPP

// C ABI 降级（Microsoft x64 软件约定）。
//
// 规则：聚合类型（struct / union）按大小一刀切——
//   1 / 2 / 4 / 8 字节 → 当同宽整数走寄存器（i8 / i16 / i32 / i64）
//   其它大小           → 参数传指针；返回走隐藏 sret 指针
//
// 出处是 MS 的 x64 约定，不是 LLVM 定的：LLVM IR 只如实执行你给的函数类型，
//   `i32 @f(%T)` 和 `i32 @f(ptr)` 是两张不同的图纸，后端都接受。
//   clang 在 CodeGen 阶段做这个降级（classifyArgumentType 一类），
//   Crest 原先跳过这一步，所以对 C 边界会生成不兼容的调用。
//
// 只对 extern_C 边界生效：Crest 内部调用两边自洽（切片 []T 是 16 字节
// struct，按值拆寄存器传没任何问题），不需要也不应该受此约束。
//
// 本文件只放"纯规则"：不碰 LLVMGenerator，尺寸由调用方算好传进来
// （尺寸必须用目标 data layout 的真实值，即 LLVMGenerator::size_of_type）。

#include "type.hpp"
#include "llvm_global.hpp"   // LLVMTypeRef / LLVMValueRef

struct LLVMGenerator;   // 前向声明（生成侧接口用）

namespace llvm_abi {

// 聚合类型在 C 边界上是否需要降级（>8 字节 → 传指针）
bool needs_downgrade(TypeRef type, int size);

// 尺寸 → 整数位宽；返回 0 表示"走指针"
int int_width_for(int size);

// 返回类型是否走 sret（隐藏出参指针）
bool uses_sret(TypeRef ret_type, int size);

}   // namespace llvm_abi


// ── 生成侧（实现在 llvm_abi.cpp，只服务 extern_C 边界）──
// 尺寸一律由调用方用 gen.size_of_type()（目标 data layout 的真实值）算好传进来。

// 建 extern_C 函数的 LLVM 函数类型：参数按 ABI 降级，sret 走首参
LLVMTypeRef gen_abi_func_type(LLVMGenerator& gen, TypeRef func_type);

// 把实参装箱成 C ABI 形状：≤8 字节 → 同宽整数；>8 字节 → 指针
LLVMValueRef gen_abi_arg(LLVMGenerator& gen, LLVMValueRef val, int size);

#endif // CREST_LLVM_BACKEND_INTERNAL_LLVM_ABI_HPP
