#pragma once

#include <functional>

#include "ref.hpp"
#include "cir_instruction_ref.hpp"
#include "cir_result_instance_ref.hpp"

struct CIRPackage;
struct CIRInstResult;


// 引用 CIRPackage 中某个指令的结果值
// 值总在某张 CIRResultInstance 里：result_instance 为空 → 包级那张；否则 → 实例自己那张
template<>
struct Ref<CIRInstResult> : RefBase<CIRInstResult> {
    Ref<CIRPackage>                        cir_package;
    CIRInstructionRef                      inst_ref;
    Ref<CIRResultInstance>                 result_instance;

    static Ref init(CIRPackage* pkg, CIRInstructionRef ref, Ref<CIRResultInstance> ri = {});

    // 结果槽: get-or-create, 拿不到值看 state
    CIRInstResult &resolve() const;

    // 只查不算: instance 表里没建就返回 null
    CIRInstResult* get_result() const;

    const CIRInstruction& inst() const;

    bool operator==(const Ref& other) const {
        return cir_package == other.cir_package
            && inst_ref == other.inst_ref
            && result_instance == other.result_instance;
    }
};


template<>
struct std::hash<Ref<CIRInstResult>> {
    usize operator()(const Ref<CIRInstResult>& key) const {
        u64 h = static_cast<u64>(std::hash<Ref<CIRPackage>>{}(key.cir_package));
        h = xp_hash_combine_u64(h, static_cast<u64>(key.inst_ref.block_ref));
        h = xp_hash_combine_u64(h, static_cast<u64>(key.inst_ref.inst_index));
        h = xp_hash_combine_u64(h, static_cast<u64>(std::hash<Ref<CIRResultInstance>>{}(key.result_instance)));
        return h;
    }
};
