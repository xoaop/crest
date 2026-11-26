#pragma once

#include <functional>

#include "cir_instruction_ref.hpp"
#include "cir_result_instance_ref.hpp"


struct CIRInstResult;


// 引用 CIRPackage 中某个指令的结果值
// 值总在某张 CIRResultInstance 里：result_instance 为空 → 包级那张；否则 → 实例自己那张
struct CIRInstResultRef {
    static const CIRInstResultRef INVALID_REF;

    CIRInstructionRef inst_ref;
    CIRResultInstanceRef result_instance;

    static CIRInstResultRef init(CIRInstructionRef ref, CIRResultInstanceRef ri = {});

    // 结果槽: get-or-create, 拿不到值看 state
    CIRInstResult *operator->() const;

    // 只查不算: 沿实例链查，找不到返回 null
    const CIRInstResult* get_result() const;

    const CIRInstruction& inst() const;

    bool operator==(const CIRInstResultRef &other) const {
        return inst_ref == other.inst_ref
            && result_instance == other.result_instance;
    }
};


template<>
struct std::hash<CIRInstResultRef> {
    usize operator()(const CIRInstResultRef& key) const {
        u64 h = static_cast<u64>(std::hash<CIRInstructionRef>{}(key.inst_ref));
        h = xp_hash_combine_u64(h, static_cast<u64>(std::hash<CIRResultInstanceRef>{}(key.result_instance)));
        return h;
    }
};


inline const CIRInstResultRef CIRInstResultRef::INVALID_REF{};
