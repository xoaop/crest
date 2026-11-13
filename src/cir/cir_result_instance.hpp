#pragma once

#include "xoaop.h"
#include "cir_inst_result.hpp"
#include "cir_instruction_ref.hpp"
#include "cir_result_instance_ref.hpp"


struct CIRResultInstance {

    static CIRResultInstance init(xpAllocator allocator, Ref<CIRResultInstance> parent);
    static CIRResultInstance init_root(xpAllocator allocator);

    const CIRInstResult* result_ptr_of(CIRInstructionRef ref) const;
    CIRInstResult& result_of(CIRInstructionRef ref);

    template<typename F>
    CIRInstResult& result_of_or(CIRInstructionRef ref, F&& factory) {
        return results.get_or_insert(ref, static_cast<F&&>(factory));
    }

    // 沿词法父实例链查结果，找不到返回 null；根实例的 parent 为空
    const CIRInstResult* find_along_chain(CIRInstructionRef ref) const;

private:
    xpHashMap<CIRInstructionRef, CIRInstResult> results;
    Ref<CIRResultInstance> parent;
};
