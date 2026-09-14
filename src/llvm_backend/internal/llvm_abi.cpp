#include "internal/llvm_abi.hpp"

// ─────────────────────────────────────────────────────────────
// 纯规则
// ─────────────────────────────────────────────────────────────

bool needs_downgrade(TypeRef type, int size) {
    if(!is_struct_type(type) && !is_union_type(type)) {
        return false;
    }
    return int_width_for(size) == 0;
}

int int_width_for(int size) {
    switch(size) {
        case 1: return 8;
        case 2: return 16;
        case 4: return 32;
        case 8: return 64;
        default: return 0;      // 0 = 走指针
    }
}

bool uses_sret(TypeRef ret_type, int size) {
    return needs_downgrade(ret_type, size);
}


// ─────────────────────────────────────────────────────────────
// 生成侧：签名降级 / sret 缓冲
//
// 只服务 extern_C 边界。被调方在 DLL 里（Crest 不为 extern_C 生成 body），
// 所以只需要处理"调用方这一半"。
// ─────────────────────────────────────────────────────────────

// 建 extern_C 函数的 LLVM 函数类型（参数按 ABI 降级，sret 走首参）
LLVMTypeRef gen_abi_func_type(TypeRef func_type) {
    auto& fparams = func_type->function_info.param_types;
    TypeRef fret = func_type->function_info.return_type;

    Array<LLVMTypeRef> params = make_array_capacity<LLVMTypeRef>(stage_allocator(), fparams.count + 1);

    // sret：>8 字节的聚合返回 → 首参是指向返回值的指针，函数本身返回 void
    int ret_size = size_of_type(fret);
    bool sret = uses_sret(fret, ret_size);
    if(sret) {
        params.push_back(LLVMPointerTypeInContext(g_llvm_session.ctx, 0));
    }

    isize fixed = fparams.count - (is_var_arg_function(func_type) ? 1 : 0);
    for(isize i = 0; i < fixed; i++) {
        TypeRef pt = fparams[i];
        int sz = size_of_type(pt);
        if(needs_downgrade(pt, sz)) {
            params.push_back(LLVMPointerTypeInContext(g_llvm_session.ctx, 0));
        } else if(is_struct_type(pt) || is_union_type(pt)) {
            params.push_back(LLVMIntTypeInContext(g_llvm_session.ctx, (unsigned)int_width_for(sz)));
        } else {
            params.push_back(get_llvm_type_from_type(pt));
        }
    }

    LLVMTypeRef ret_t = sret ? LLVMVoidTypeInContext(g_llvm_session.ctx)
                             : get_llvm_type_from_type(fret);
    return LLVMFunctionType(ret_t, params.data, (unsigned)params.count,
                            is_var_arg_function(func_type) ? 1 : 0);
}
