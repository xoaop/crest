#ifndef CREST_CIR_KEY_HPP
#define CREST_CIR_KEY_HPP

#include "xoaop.h"
#include "array.hpp"
#include <functional>
#include <optional>

struct CIRPackage;
struct Package;
struct CIRInstResult;
struct CIRResultInstance;
struct CIRInstruction;
using CIRInstructionRef = isize;

// 引用 CIRPackage 中某个指令的结果值
// result_instance == nullptr → 值在 cir_package->results[inst_ref] 中
// result_instance != nullptr → 值在 result_instance->results[inst_ref] 中
struct CIRInstResultRef {
    CIRPackage*                            cir_package;
    CIRInstructionRef                      inst_ref;
    std::optional<CIRResultInstance*>      result_instance;

    static CIRInstResultRef make(CIRPackage* pkg, CIRInstructionRef ref, std::optional<CIRResultInstance*> ri = std::nullopt);

    const CIRInstResult* get_result() const;

    const CIRInstruction* inst() const;

    bool operator==(const CIRInstResultRef& other) const {
        return cir_package == other.cir_package
            && inst_ref == other.inst_ref
            && result_instance.has_value() == other.result_instance.has_value()
            && (!result_instance.has_value() || result_instance.value() == other.result_instance.value());
    }
};

template<>
struct std::hash<CIRInstResultRef> {
    usize operator()(const CIRInstResultRef& key) const {
        u64 h = (u64)(usize)key.cir_package;
        h = xp_hash_combine_u64(h, (u64)key.inst_ref);
        h = xp_hash_combine_u64(h, (u64)(usize)(key.result_instance ? *key.result_instance : nullptr));
        return h;
    }
};

template<>
inline usize xp_hash_func(CIRInstResultRef *key) {
    return std::hash<CIRInstResultRef>{}(*key);
}

struct FuncCallKey {
    CIRInstructionRef                        func_decl_pc;
    Array<CIRInstResultRef>                  comptime_arg_refs;
    std::optional<CIRResultInstance*>        func_instance;

    u64 hash() const;
    bool operator==(const FuncCallKey& other) const;
};

template<>
inline usize xp_hash_func(FuncCallKey *key) {
    return key->hash();
}

#endif
