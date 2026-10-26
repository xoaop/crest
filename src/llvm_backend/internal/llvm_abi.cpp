#include "internal/llvm_abi.hpp"

#include <cstring>

// 目标选择

ABIKind current_abi(TargetOS target) {
    // 目前只有 Windows 走 MS x64；其余目标按 System V x86-64 处理。
    // （非本机 target 的 LLVM 后端尚未初始化，-target 交叉编译会在更早处失败。）
    return target == TargetOS::Windows ? ABIKind::MSX64
                                       : ABIKind::SysVX64;
}


// Microsoft x64 规则（按大小一刀切）

static int ms_int_width_for(int size) {
    switch(size) {
        case 1: return 8;
        case 2: return 16;
        case 4: return 32;
        case 8: return 64;
        default: return 0;      // 0 = 走指针
    }
}


// System V x86-64 分类

struct ChunkAcc {
    bool any_int = false;
    unsigned f32_count = 0;
    bool f64 = false;
};

static void classify_into(TypeRef t, isize offset, ChunkAcc acc[2], bool& memory) {
    if(t == nullptr) {
        memory = true;
        return;
    }

    isize size = type_size_of(t);
    if(size == 0) {
        return;
    }
    if(size > 16 || offset < 0 || offset >= 16) {
        memory = true;
        return;
    }

    // SysV：未对齐字段 → MEMORY
    isize align = type_align_of(t);
    if(align > 1 && (offset % align) != 0) {
        memory = true;
        return;
    }

    switch(t->kind) {
    case Type_struct: {
        FOR(t->struct_info.struct_fields) {
            TypeRef ft = it.type;
            classify_into(ft, offset + field_offset_in_struct(t, it_index), acc, memory);
            if(memory) return;
        }
        return;
    }
    case Type_union: {
        Ref<Scope> scope = t->union_info.union_scope;
        for(const auto& entry : *scope) {
            TypeRef ft = union_field_type(t, entry.value.name);
            classify_into(ft, offset, acc, memory);
            if(memory) return;
        }
        return;
    }
    case Type_array: {
        TypeRef et = t->array_info.element_type;
        isize es = type_size_of(et);
        for(usize i = 0; i < t->array_info.count; i++) {
            classify_into(et, offset + (isize)i * es, acc, memory);

            if(memory) {
                return;
            }
        }
        return;
    }
    case Type_enum: {
        acc[offset / 8].any_int = true;
        return;
    }
    case Type_f32: {
        acc[offset / 8].f32_count++;
        return;
    }
    case Type_f64: {
        acc[offset / 8].f64 = true;
        return;
    }
    case Type_i8:  case Type_i16: case Type_i32: case Type_i64: case Type_isize:
    case Type_u8:  case Type_u16: case Type_u32: case Type_u64: case Type_usize:
    case Type_bool:
    case Type_pointer: {
        acc[offset / 8].any_int = true;
        return;
    }
    default:
        // void / function / type / var_arg 等不该出现在 C 参数里
        memory = true;
        return;
    }
}

SysVShape classify_sysv(TypeRef type) {
    SysVShape shape;

    isize size = type_size_of(type);
    if(size > 16) {
        shape.memory = true;
        return shape;
    }

    ChunkAcc acc[2];
    bool memory = false;
    classify_into(type, 0, acc, memory);
    if(memory) {
        shape.memory = true;
        return shape;
    }

    shape.count = (int)((size + 7) / 8);
    for(int i = 0; i < shape.count; i++) {
        SysVChunk& c = shape.chunks[i];
        isize remaining = size - (isize)i * 8;
        c.bytes = (unsigned)(remaining > 8 ? 8 : remaining);
        c.f64 = acc[i].f64;
        c.f32_count = acc[i].f32_count;
        c.is_sse = !acc[i].any_int;
        if(c.is_sse && !c.f64 && c.f32_count == 0) {
            // 纯填充块不应出现；按 INTEGER 兜底更安全
            c.is_sse = false;
        }
    }
    return shape;
}

static LLVMTypeRef sysv_chunk_type(const SysVChunk& c) {
    LLVMContextRef ctx = g_llvm_session.ctx;
    if(c.is_sse) {
        if(c.f64) {
            return LLVMDoubleTypeInContext(ctx);
        }
        if(c.f32_count >= 2) {
            return LLVMVectorType(LLVMFloatTypeInContext(ctx), 2);
        }
        return LLVMFloatTypeInContext(ctx);
    }
    return LLVMIntTypeInContext(ctx, c.bytes * 8);
}


// 标量扩展属性

static void scalar_ext_attrs(TypeRef t, bool& signext, bool& zeroext) {
    signext = false;
    zeroext = false;

    if(t == nullptr) {
        return;
    }

    if(t->kind == Type_bool) {
        zeroext = true;
        return;
    }

    if(is_integer_type(t)) {
        if(size_of_type(t) < 4) {
            if(is_signed_type(t)) {
                signext = true;
            } else {
                zeroext = true;
            }
        }

        return;
    }

    if(t->kind == Type_enum) {
        TypeRef et = t->enum_info.element_type;
        if(et != nullptr && size_of_type(et) < 4) {
            if(is_signed_type(et)) {
                signext = true;
            } else {
                zeroext = true;
            }
        }
    }
}


// 签名计划

ABIFunctionPlan make_c_abi_plan(TypeRef func_type, ABIKind abi) {
    LLVMContextRef ctx = g_llvm_session.ctx;

    ABIFunctionPlan plan;
    plan.abi = abi;
    plan.ret_type = func_type->function_info.return_type;

    const bool sysv = plan.abi == ABIKind::SysVX64;
    auto& fparams = func_type->function_info.param_types;
    const isize fixed = fparams.count - (is_var_arg_function(func_type) ? 1 : 0);

    plan.params = make_array_capacity<ABIParamPlan>(stage_allocator(), fixed);

    // SysV 寄存器计数（GPR / SSE 各自独立）
    int used_gpr = 0;
    int used_sse = 0;

    for(isize i = 0; i < fixed; i++) {
        ABIParamPlan pp;
        TypeRef pt = fparams[i];
        pp.source_type = pt;

        const bool is_agg = is_struct_type(pt) || is_union_type(pt);
        // extern_C 的数组形参已在类型检查阶段拒绝（C 会退化为指针）；
        // 数组作为 struct 字段时，由 classify_sysv 正常分类。

        if(!is_agg) {
            pp.kind = ABIParamPlan::Kind::Scalar;
            pp.types[0] = get_llvm_type_from_type(pt);
            pp.ntypes = 1;
            scalar_ext_attrs(pt, pp.signext, pp.zeroext);

            if(sysv) {
                if(is_float_type(pt)) used_sse++; else used_gpr++;
            }
        } else if(!sysv) {
            // MS x64：1/2/4/8 字节 → 同宽整数；其它 → 指针
            const int sz = (int)type_size_of(pt);
            const int w = ms_int_width_for(sz);
            if(w == 0) {
                pp.kind = ABIParamPlan::Kind::ByValPtr;
                pp.types[0] = LLVMPointerTypeInContext(ctx, 0);
                pp.ntypes = 1;
            } else {
                pp.kind = ABIParamPlan::Kind::Aggregate;
                pp.types[0] = LLVMIntTypeInContext(ctx, (unsigned)w);
                pp.ntypes = 1;
            }
        } else {
            // SysV：按 8 字节分块；寄存器不够或 MEMORY → 指针
            SysVShape s = classify_sysv(pt);
            int need_gpr = 0;
            int need_sse = 0;
            for(int k = 0; k < s.count; k++) {
                if(s.chunks[k].is_sse) need_sse++; else need_gpr++;
            }

            if(s.memory || used_gpr + need_gpr > 6 || used_sse + need_sse > 8) {
                pp.kind = ABIParamPlan::Kind::ByValPtr;
                pp.types[0] = LLVMPointerTypeInContext(ctx, 0);
                pp.ntypes = 1;
                pp.align = (int)type_align_of(pt);
            } else {
                pp.kind = ABIParamPlan::Kind::Aggregate;
                pp.ntypes = s.count;
                for(int k = 0; k < s.count; k++) {
                    pp.types[k] = sysv_chunk_type(s.chunks[k]);
                }
                used_gpr += need_gpr;
                used_sse += need_sse;
            }
        }

        plan.params.push_back(pp);
    }

    // ── 返回 ──
    LLVMTypeRef ret_llvm = nullptr;
    const bool agg_ret = is_struct_type(plan.ret_type) || is_union_type(plan.ret_type);

    if(plan.ret_type->kind == Type_void) {
        ret_llvm = LLVMVoidTypeInContext(ctx);
    } else if(agg_ret) {
        const int sz = (int)type_size_of(plan.ret_type);
        if(sz == 0) {
            // C 无法表达空聚合体；按无返回值处理
            ret_llvm = LLVMVoidTypeInContext(ctx);
        } else if(!sysv) {
            const int w = ms_int_width_for(sz);
            if(w == 0) {
                plan.sret = true;
                ret_llvm = LLVMVoidTypeInContext(ctx);
            } else {
                plan.ret_chunks = 1;
                plan.ret_chunk_types[0] = LLVMIntTypeInContext(ctx, (unsigned)w);
                ret_llvm = plan.ret_chunk_types[0];
            }
        } else {
            SysVShape s = classify_sysv(plan.ret_type);
            if(s.memory) {
                plan.sret = true;
                ret_llvm = LLVMVoidTypeInContext(ctx);
            } else if(s.count == 1) {
                plan.ret_chunks = 1;
                plan.ret_chunk_types[0] = sysv_chunk_type(s.chunks[0]);
                ret_llvm = plan.ret_chunk_types[0];
            } else {
                plan.ret_chunks = 2;
                plan.ret_chunk_types[0] = sysv_chunk_type(s.chunks[0]);
                plan.ret_chunk_types[1] = sysv_chunk_type(s.chunks[1]);
                LLVMTypeRef chunks[2] = {plan.ret_chunk_types[0], plan.ret_chunk_types[1]};
                ret_llvm = LLVMStructTypeInContext(ctx, chunks, 2, 0);
            }
        }
    } else {
        ret_llvm = get_llvm_type_from_type(plan.ret_type);
        scalar_ext_attrs(plan.ret_type, plan.ret_signext, plan.ret_zeroext);
    }

    // ── 参数 LLVM 类型列表 ──
    Array<LLVMTypeRef> llvm_params =
        make_array_capacity<LLVMTypeRef>(stage_allocator(), fixed + 2);
    if(plan.sret) {
        llvm_params.push_back(LLVMPointerTypeInContext(ctx, 0));
    }
    for(const auto& pp : plan.params) {
        for(int k = 0; k < pp.ntypes; k++) {
            llvm_params.push_back(pp.types[k]);
        }
    }

    plan.fn_type = LLVMFunctionType(
        ret_llvm, llvm_params.data, (unsigned)llvm_params.count,
        is_var_arg_function(func_type) ? 1 : 0);

    return plan;
}


// 属性

static LLVMAttributeRef make_enum_attr(const char* name, u64 value = 0) {
    unsigned kind = LLVMGetEnumAttributeKindForName(name, strlen(name));
    XP_ASSERT_DEFAULT(kind != 0);
    return LLVMCreateEnumAttribute(g_llvm_session.ctx, kind, value);
}

static LLVMAttributeRef make_type_attr(const char* name, LLVMTypeRef type) {
    unsigned kind = LLVMGetEnumAttributeKindForName(name, strlen(name));
    XP_ASSERT_DEFAULT(kind != 0);
    return LLVMCreateTypeAttribute(g_llvm_session.ctx, kind, type);
}

static void add_attr(LLVMValueRef target, unsigned index, LLVMAttributeRef attr, bool is_call) {
    if(is_call) {
        LLVMAddCallSiteAttribute(target, index, attr);
    } else {
        LLVMAddAttributeAtIndex(target, index, attr);
    }
}

void apply_c_abi_attrs(LLVMValueRef fn_or_call, const ABIFunctionPlan& plan, bool is_call) {
    if(plan.ret_signext) {
        add_attr(fn_or_call, LLVMAttributeReturnIndex, make_enum_attr("signext"), is_call);
    }
    if(plan.ret_zeroext) {
        add_attr(fn_or_call, LLVMAttributeReturnIndex, make_enum_attr("zeroext"), is_call);
    }

    unsigned index = 1;   // LLVM 形参下标从 1 开始

    if(plan.sret) {
        add_attr(fn_or_call, index,
                 make_type_attr("sret", get_llvm_type_from_type(plan.ret_type)), is_call);
        add_attr(fn_or_call, index, make_enum_attr("noalias"), is_call);
        index++;
    }

    for(const auto& pp : plan.params) {
        if(pp.kind == ABIParamPlan::Kind::ByValPtr) {
            // SysV 用 byval 让后端按内存传值；MS x64 的裸指针本来就表示按引用传
            if(plan.abi == ABIKind::SysVX64) {
                add_attr(fn_or_call, index,
                         make_type_attr("byval", get_llvm_type_from_type(pp.source_type)), is_call);
                if(pp.align > 0) {
                    add_attr(fn_or_call, index,
                             make_enum_attr("align", (u64)pp.align), is_call);
                }
            }
            index++;
        } else {
            for(int k = 0; k < pp.ntypes; k++) {
                if(pp.signext) {
                    add_attr(fn_or_call, index, make_enum_attr("signext"), is_call);
                }
                if(pp.zeroext) {
                    add_attr(fn_or_call, index, make_enum_attr("zeroext"), is_call);
                }
                index++;
            }
        }
    }
}
