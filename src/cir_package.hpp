#pragma once

#include <optional>
#include <cstring>
#include <functional>

#include "xoaop.h"
#include "array.hpp"
#include "value.hpp"
#include "span.hpp"
#include "scope.hpp"

#include "stable_ordered_array.hpp"
#include "cir_instruction_ref.hpp"
#include "cir_inst.hpp"

struct Package;



struct CIRBlock {
    StableOrderedArray<CIRInstruction> insts;
    CIRBlockRef self = -1;          // 本块号（create_block 填）
    RefN<Package> package_ref;   // 所属包
    
    bool is_comptime;
    bool immediate_eval;
    bool is_loop;                // 循环块标记（旧 Loop 指令已合并进 Block）

    isize push_back_inst(CIRInstruction inst);

    
    struct InstRefIter {
        CIRInstructionRef ref;

        CIRInstructionRef operator*() const { return ref; }
        InstRefIter& operator++() { ref.advance(); return *this; }

        bool operator!=(const InstRefIter& o) const { return ref != o.ref; }
        bool operator==(const InstRefIter& o) const { return ref == o.ref; }
    };

    InstRefIter begin() { return {CIRInstructionRef{self, 0, package_ref.index}}; }
    InstRefIter end()   { return {CIRInstructionRef{self, (isize)insts.count(), package_ref.index}}; }
};


enum class CIRResultState: u32 {
    NothingYet,  // 还没有计算结果
    OnlyType,   // 仅仅只有类型推导
    WholeValue,  // 计算出实际值了
    Error        // 本指令分析出错，结果不可用，下游应跳过
};

enum class CIRValueKind : u8 {
    RValue,  // 实际值（参与运算、传参）
    LValue,  // 位置句柄（可被 Load/Store/FieldPtr/IndexPtr 操作）
};

struct CIRInstResult {
    CIRResultState state = CIRResultState::NothingYet;
    CIRValueKind value_kind = CIRValueKind::RValue;


    std::optional<TypeRef> implicit_type = std::nullopt;

    static CIRInstResult make_value(TypeRef t, Value v, CIRValueKind vk = CIRValueKind::RValue) {
        CIRInstResult r;
        r.state = CIRResultState::WholeValue;
        r.outstanding_type = t;
        r.val = v;
        r.value_kind = vk;
        return r;
    }

    static CIRInstResult make_value(Value v) {
        CIRInstResult r;
        r.state = CIRResultState::WholeValue;
        r.outstanding_type = v.type;
        r.val = v;
        r.value_kind = CIRValueKind::RValue;
        return r;
    }

    static CIRInstResult make_type_only(TypeRef t, CIRValueKind vk = CIRValueKind::RValue) {
        CIRInstResult r;
        r.state = CIRResultState::OnlyType;
        r.outstanding_type = t;
        r.val.type = t;
        r.value_kind = vk;
        return r;
    }


    static CIRInstResult make_error() {
        CIRInstResult r;
        r.state = CIRResultState::Error;
        return r;
    }

    TypeRef type() const;
    TypeRef actual_type() const;
    Value actual_val() const;

    void set_type(TypeRef new_type);
    void set_actual_type(TypeRef new_type);
    void set_val(Value new_val);

private:
    Value val;
    TypeRef outstanding_type = nullptr;
};


struct CIRResultInstance {

    static CIRResultInstance make(xpAllocator allocator);

    CIRInstResult* result_ptr_of(CIRInstructionRef ref);
    CIRInstResult& result_of(CIRInstructionRef ref);

    template<typename F>
    CIRInstResult& result_of_or(CIRInstructionRef ref, F&& factory) {
        return results.get_or_insert(ref, static_cast<F&&>(factory));
    }

private:
    xpHashMap<CIRInstructionRef, CIRInstResult> results;
};







struct CIRPackage {
    RefN<Package> package_ref;   // 本包在 context()->all_packages 中的编号


    CIRBlockRef top_blk;
    Array<CIRBlock> blocks;

    RefN<Scope> package_scope;   // 包级 scope（顶层驱动的初始 scope）

    Array<xpString> string_literals;


    xpHashMap<CIRInstructionRef, CIRInstResult> results;
    xpHashMap<FuncCallKey, CIRResultInstance> result_instances;
    Array<FuncCallKey> comptime_func_calls;


    CIRInstruction* inst(CIRInstructionRef ref);
    const CIRInstruction* inst(CIRInstructionRef ref) const;
    CIRBlock* block(CIRBlockRef ref);
    const CIRBlock* block(CIRBlockRef ref) const;

    Ref<CIRResultInstance> get_result_instance(FuncCallKey key);

    CIRInstResult& result_of(CIRInstructionRef ref, Ref<CIRResultInstance> instance = {});

    CIRBlockRef create_block(bool is_comptime, bool immediate_eval, bool is_loop);
};



CIRPackage make_cir_package(xpAllocator allocator);

struct CIRResultContext;

bool is_pure_comptime_func(CIRFunctionDeclInfo& func, const CIRResultContext& ctx);
void dump_cir_package(CIRPackage *file);





struct CIRResultContext {
    static CIRResultContext create(CIRPackage *pkg);

    CIRPackage *pkg() const { return _pkg; }
    void set_pkg(CIRPackage *new_pkg) { _pkg = new_pkg; }

    void enter_call(FuncCallKey key);
    void enter_call_instance(Ref<CIRResultInstance> instance);
    void exit_call();
    bool in_call() const { return _call_instance.key.func_decl_pc != INVALID_INST; }
    Ref<CIRResultInstance> call_instance() const { return _call_instance; }
    const FuncCallKey &call_key() const;

    CIRInstResult &result_of(CIRInstructionRef ref) const;

private:
    CIRPackage *_pkg = nullptr;
    Ref<CIRResultInstance> _call_instance;
    std::optional<FuncCallKey> _call_key;
};