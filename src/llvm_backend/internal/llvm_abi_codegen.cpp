// C ABI 边界在 LLVM IR 层的降级。
//
// llvm_abi.cpp 管“规则”：平台分类、签名计划、参数属性，纯计算、不生成 IR。
// 本文件管“落地”：按 plan 在源码形状与 ABI 形状之间搬运参数和返回值。
//
//   定义点（callee）：gen_ir_param_value_by_abi / gen_ir_ret_by_abi
//   调用点（caller）：gen_ir_sret_slot_by_abi / gen_ir_call_args_by_abi / gen_ir_call_result_by_abi
//   公共搬运：gen_ir_aggregate_address / gen_ir_aggregate_value / gen_ir_load_chunks / gen_ir_store_chunks
//
// 只有 extern_C 边界用到这些；Crest 内部调用两边自洽，不受影响。

#include "internal/llvm_generator.hpp"
#include "internal/llvm_abi.hpp"

#include "llvm-c/Core.h"


// ── 聚合体在内存与 IR 值之间的搬运 ──────────────────────────────────────

// 把聚合实参落成内存地址。
// 实参在 LLVM 里可能是内存地址（局部变量、Deref 等），也可能是聚合值
// （常量 struct 走 LLVMConstNamedStruct）。C ABI 降级需要按字节读块或传指针，
// 所以先统一成地址——clang 也是这么做的。
LLVMValueRef LLVMGenerator::gen_ir_aggregate_address(LLVMValueRef val) {
    if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMPointerTypeKind) {
        return val;
    }

    LLVMTypeRef t = LLVMTypeOf(val);

    LLVMValueRef addr = insert_alloca_before_last_inst_which_is_br(curr_state.entry, "abitmp", t);
    LLVMBuildStore(unit.builder, val, addr);

    return addr;
}

// 反过来：地址 / 聚合值统一成聚合值（值原样返回，地址才 load）
LLVMValueRef LLVMGenerator::gen_ir_aggregate_value(LLVMValueRef val, LLVMTypeRef agg_type) {
    if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMPointerTypeKind) {
        return LLVMBuildLoad2(unit.builder, agg_type, val, "abiload");
    }

    return val;
}

// 寄存器类聚合体在内存和 IR 参数/返回值之间按 8 字节块转换。
// 块内偏移按 k*8 写死：Crest 没有 packed，也没有 16 字节对齐成员。
// 对齐显式按 1 写，避免后端从 GEP 推出高于实际布局的假设。
LLVMValueRef LLVMGenerator::gen_ir_chunk_address(LLVMValueRef base, int k) {
    LLVMTypeRef i8_t = LLVMInt8TypeInContext(g_llvm_session.ctx);
    LLVMTypeRef i64_t = LLVMInt64TypeInContext(g_llvm_session.ctx);

    LLVMValueRef off = LLVMConstInt(i64_t, (u64)(k * 8), 0);

    return LLVMBuildInBoundsGEP2(unit.builder, i8_t, base, &off, 1, "abiptr");
}

Array<LLVMValueRef> LLVMGenerator::gen_ir_load_chunks(LLVMValueRef base, LLVMTypeRef const* types, int n) {
    auto out = make_array_capacity<LLVMValueRef>(stage_allocator(), n);

    for(int k = 0; k < n; k++) {
        LLVMValueRef chunk = LLVMBuildLoad2(unit.builder, types[k], gen_ir_chunk_address(base, k), "abichunk");
        LLVMSetAlignment(chunk, 1);

        out.push_back(chunk);
    }

    return out;
}

void LLVMGenerator::gen_ir_store_chunks(LLVMValueRef base, LLVMValueRef const* vals, int n) {
    for(int k = 0; k < n; k++) {
        LLVMValueRef st = LLVMBuildStore(unit.builder, vals[k], gen_ir_chunk_address(base, k));
        LLVMSetAlignment(st, 1);
    }
}


// ── 定义点 ─────────────────────────────────────────────────────────────

// 入口：extern_C 定义的形参在 IR 里是 ABI 形状——sret 占首位、聚合拆成 1~2 块、
// 按值传的大聚合是一个指针。这里按 plan 拼回源码参数的值，调用方再存进参数 alloca。
LLVMValueRef LLVMGenerator::gen_ir_param_value_by_abi(const ABIFunctionPlan& plan, unsigned slot) {
    // IR 参数下标 = sret 占位 + 前面各形参占的 LLVM 参数个数（源码下标 ≠ IR 下标）
    unsigned idx = plan.sret ? 1 : 0;
    for(unsigned i = 0; i < slot && i < (unsigned)plan.params.count; i++) {
        idx += (unsigned)plan.params[i].ntypes;
    }

    XP_ASSERT_DEFAULT(slot < (unsigned)plan.params.count);
    const ABIParamPlan& pp = plan.params[slot];

    switch(pp.kind) {
    case ABIParamPlan::Kind::Scalar:
        return LLVMGetParam(curr_state.curr_function, idx);

    case ABIParamPlan::Kind::ByValPtr: {
        // 指针传参：SysV 的 byval 由后端复制，MS x64 拿的就是调用方的副本，直接读出值
        LLVMValueRef ptr = LLVMGetParam(curr_state.curr_function, idx);

        return LLVMBuildLoad2(unit.builder, get_llvm_type_from_type(pp.source_type), ptr, "abiparam");
    }

    case ABIParamPlan::Kind::Aggregate: {
        // 寄存器类聚合体散成块：写回内存再整体读出
        LLVMTypeRef agg_t = get_llvm_type_from_type(pp.source_type);
        LLVMValueRef tmp = insert_alloca_before_last_inst_which_is_br(curr_state.entry, "abiparam", agg_t);

        LLVMValueRef chunks[2] = {nullptr, nullptr};
        for(int k = 0; k < pp.ntypes; k++) {
            chunks[k] = LLVMGetParam(curr_state.curr_function, idx + (unsigned)k);
        }

        gen_ir_store_chunks(tmp, chunks, pp.ntypes);

        return LLVMBuildLoad2(unit.builder, agg_t, tmp, "abiparam");
    }
    }

    UNREACHABLE();
    return nullptr;
}

// 出口：extern_C 定义的返回。sret 写进隐藏指针后 ret void；
// 寄存器类聚合体拆块返回；空聚合体按 void。
void LLVMGenerator::gen_ir_ret_by_abi(const ABIFunctionPlan& plan, LLVMValueRef ret_val) {
    if(ret_val == nullptr) {
        LLVMBuildRetVoid(unit.builder);
        return;
    }

    LLVMTypeRef agg_t = get_llvm_type_from_type(plan.ret_type);

    if(plan.sret) {
        // 隐藏出参排在首位：调用方给的缓冲就是返回值落点
        LLVMValueRef sret_ptr = LLVMGetParam(curr_state.curr_function, 0);
        LLVMValueRef st = LLVMBuildStore(unit.builder, gen_ir_aggregate_value(ret_val, agg_t), sret_ptr);
        LLVMSetAlignment(st, (unsigned)type_align_of(plan.ret_type));

        LLVMBuildRetVoid(unit.builder);
        return;
    }

    if(plan.ret_chunks > 0) {
        // 寄存器类聚合体：从内存逐块读出，打包成 fn_type 的返回类型
        Array<LLVMValueRef> chunks = gen_ir_load_chunks(
            gen_ir_aggregate_address(ret_val), plan.ret_chunk_types, plan.ret_chunks);

        if(plan.ret_chunks == 1) {
            LLVMBuildRet(unit.builder, chunks[0]);
        } else {
            LLVMTypeRef chunk_types[2] = {plan.ret_chunk_types[0], plan.ret_chunk_types[1]};
            LLVMTypeRef pair_t = LLVMStructTypeInContext(g_llvm_session.ctx, chunk_types, 2, 0);

            LLVMValueRef pair = LLVMGetUndef(pair_t);
            pair = LLVMBuildInsertValue(unit.builder, pair, chunks[0], 0, "abiret");
            pair = LLVMBuildInsertValue(unit.builder, pair, chunks[1], 1, "abiret");

            LLVMBuildRet(unit.builder, pair);
        }
        return;
    }

    // void / 空聚合体（C 无法表达零大小返回）
    if(plan.ret_type->kind == Type_void
        || ((is_struct_type(plan.ret_type) || is_union_type(plan.ret_type)) && type_size_of(plan.ret_type) == 0)) {
        LLVMBuildRetVoid(unit.builder);
        return;
    }

    LLVMBuildRet(unit.builder, ret_val);
}


// ── 调用点 ─────────────────────────────────────────────────────────────

// sret：聚合返回走隐藏出参，调用方分配缓冲，调用时作为首参传入
LLVMValueRef LLVMGenerator::gen_ir_sret_slot_by_abi(const ABIFunctionPlan& plan) {
    if(!plan.sret) {
        return nullptr;
    }

    return insert_alloca_before_last_inst_which_is_br(
        curr_state.entry, "sret", get_llvm_type_from_type(plan.ret_type));
}

// 把一个源码实参降级成对应的 ABI 实参：标量 1 个值，聚合 1~2 个块，大聚合 1 个指针
Array<LLVMValueRef> LLVMGenerator::gen_ir_call_args_by_abi(const ABIFunctionPlan& plan,
                                                     const ABIParamPlan& pp, LLVMValueRef arg_val) {
    auto out = make_array_capacity<LLVMValueRef>(stage_allocator(), 2);

    switch(pp.kind) {
    case ABIParamPlan::Kind::Scalar:
        out.push_back(arg_val);
        break;

    case ABIParamPlan::Kind::ByValPtr: {
        // 聚合体走内存：SysV 带 byval（后端负责复制），MS 是裸指针
        LLVMValueRef addr = gen_ir_aggregate_address(arg_val);

        if(plan.abi == ABIKind::MSX64) {
            // MS x64：被调方可以改写参数内存，调用方必须先复制一份
            LLVMTypeRef agg_t = get_llvm_type_from_type(pp.source_type);
            LLVMValueRef copy = insert_alloca_before_last_inst_which_is_br(
                curr_state.entry, "abicopy", agg_t);

            LLVMBuildStore(unit.builder, gen_ir_aggregate_value(addr, agg_t), copy);
            addr = copy;
        }

        out.push_back(addr);
    } break;

    case ABIParamPlan::Kind::Aggregate: {
        // 寄存器类聚合体：按 8 字节块从内存读出
        Array<LLVMValueRef> chunks = gen_ir_load_chunks(
            gen_ir_aggregate_address(arg_val), pp.types, pp.ntypes);

        FOR(chunks) out.push_back(it);
    } break;
    }

    return out;
}

// 调用收尾：把 LLVMBuildCall 的结果还原成源码返回值。
// sret_slot 是调用前分配的隐藏缓冲（无 sret 时为 nullptr）。
LLVMValueRef LLVMGenerator::gen_ir_call_result_by_abi(const ABIFunctionPlan& plan, LLVMValueRef call_result,
                                                LLVMValueRef sret_slot) {
    if(sret_slot != nullptr) {
        // sret：函数本身返回 void，从缓冲把聚合值读出来当结果
        return LLVMBuildLoad2(unit.builder, get_llvm_type_from_type(plan.ret_type), sret_slot, "sretval");
    }

    if(plan.ret_chunks > 0) {
        // 寄存器类聚合体返回：把返回的块写回聚合体再整体读出
        LLVMValueRef tmp = insert_alloca_before_last_inst_which_is_br(
            curr_state.entry, "abiret", get_llvm_type_from_type(plan.ret_type));

        LLVMValueRef vals[2] = {nullptr, nullptr};
        for(int k = 0; k < plan.ret_chunks; k++) {
            vals[k] = plan.ret_chunks == 1
                ? call_result
                : LLVMBuildExtractValue(unit.builder, call_result, (unsigned)k, "abichunk");
        }

        gen_ir_store_chunks(tmp, vals, plan.ret_chunks);

        return LLVMBuildLoad2(unit.builder, get_llvm_type_from_type(plan.ret_type), tmp, "abiagg");
    }

    if((is_struct_type(plan.ret_type) || is_union_type(plan.ret_type)) && type_size_of(plan.ret_type) == 0) {
        // 空聚合体：C 无法表达，按无返回值处理
        return LLVMGetUndef(get_llvm_type_from_type(plan.ret_type));
    }

    return call_result;
}
