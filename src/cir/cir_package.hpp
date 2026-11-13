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
#include "cir_package_ref.hpp"
#include "cir_result_instance.hpp"

struct Package;



struct CIRBlock {
    StableOrderedArray<CIRInstruction> insts;
    CIRBlockRef self = -1;          // 本块号（create_block 填）
    Ref<CIRPackage> cir_package;   // 所属包
    
    bool is_comptime;
    bool immediate_eval;
    bool is_loop;                // 循环块标记（旧 Loop 指令已合并进 Block）
    bool yields_value;           // 块作为表达式产出值：break 的值递送到块句柄当结果（含 TypeOnly 下的编译期常量）

    isize push_back_inst(CIRInstruction inst);

    
    struct InstRefIter {
        CIRInstructionRef ref;

        CIRInstructionRef operator*() const { return ref; }
        InstRefIter& operator++() { ref.advance(); return *this; }

        bool operator!=(const InstRefIter& o) const { return ref != o.ref; }
        bool operator==(const InstRefIter& o) const { return ref == o.ref; }
    };

    InstRefIter begin() const { return {CIRInstructionRef{self, 0, cir_package}}; }
    InstRefIter end()   const { return {CIRInstructionRef{self, (isize)insts.count(), cir_package}}; }
};








struct CIRPackage {
    Ref<CIRPackage> self_ref;   // 自身句柄
    Ref<Package> package_ref;   // 本包在 context()->all_packages 中的编号


    CIRBlockRef top_blk;
    Array<CIRBlock> blocks;

    Ref<Scope> package_scope;   // 包级 scope（顶层驱动的初始 scope）

    Array<GlobalStringIndex> string_literals;


    Array<CIRResultInstance> result_instances;   // [0] = 包级根实例
    xpHashMap<FuncCallKey, Ref<CIRResultInstance>> result_instance_map;


    CIRInstruction& inst_mut(CIRInstructionRef ref);
    const CIRInstruction& inst(CIRInstructionRef ref) const;
    CIRBlock& block_mut(CIRBlockRef ref);
    const CIRBlock& block(CIRBlockRef ref) const;

    Ref<CIRResultInstance> root_instance() const;
    Ref<CIRResultInstance> get_or_init_result_instance(FuncCallKey key, Ref<CIRResultInstance> parent);

    CIRInstResult& result_of(CIRInstructionRef ref, Ref<CIRResultInstance> instance = {});
    const CIRInstResult* find_result(CIRInstructionRef ref, Ref<CIRResultInstance> instance) const;
    const CIRInstResult* find_result_in_curr_instance_and_pkg(CIRInstructionRef ref, Ref<CIRResultInstance> instance) const;

    CIRBlockRef create_block(bool is_comptime, bool immediate_eval, bool is_loop, bool yields_value = false);
};



CIRPackage make_cir_package(xpAllocator allocator);


struct CIRResultContext;

bool is_pure_comptime_func(const CIRFunctionDeclInfo& func, const CIRResultContext& ctx);
void dump_cir_package(CIRPackage *file);





struct CIRResultContext {
    static CIRResultContext create(Ref<CIRPackage> pkg);

    Ref<CIRPackage> pkg() const { return _pkg; }
    void set_pkg(Ref<CIRPackage> new_pkg) { _pkg = new_pkg; }

    void enter_call(FuncCallKey key, Ref<CIRResultInstance> parent);
    void enter_call_instance(Ref<CIRResultInstance> instance);

    void exit_call();
    bool in_call() const { 
        return _call_instance != Ref<CIRResultInstance>::INVALID_REF;
    }

    Ref<CIRResultInstance> call_instance() const { 
        return _call_instance; 
    }

    const FuncCallKey &call_key() const;

    CIRInstResult &result_of(CIRInstructionRef ref) const;
    const CIRInstResult *result_ptr_of(CIRInstructionRef ref) const;
    const CIRInstResult *result_ptr_in_curr_instance_and_pkg(CIRInstructionRef ref) const;

private:
    Ref<CIRPackage> _pkg = Ref<CIRPackage>::INVALID_REF;
    Ref<CIRResultInstance> _call_instance;
    std::optional<FuncCallKey> _call_key;
};