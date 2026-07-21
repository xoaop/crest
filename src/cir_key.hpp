#ifndef CREST_CIR_KEY_HPP
#define CREST_CIR_KEY_HPP

#include "xoaop.h"
#include "array.hpp"
#include <optional>

struct CIRPackage;
struct Package;
struct CIRInstResult;
struct CIRResultInstance;
using CIRInstructionRef = isize;

// 定位唯一的 CIR 指令定义，用于跨包引用
struct CIRInstUniqueKey {
    CIRPackage       *cir_package;
    Package           *package;
    CIRInstructionRef  defining_inst;

    u64 hash() const {
        u64 h = (u64)(usize)cir_package;
        h = xp_hash_combine_u64(h, (u64)(usize)package);
        h = xp_hash_combine_u64(h, (u64)defining_inst);
        return h;
    }

    bool operator==(const CIRInstUniqueKey& other) const {
        return cir_package == other.cir_package
            && package == other.package
            && defining_inst == other.defining_inst;
    }
};

template<>
inline usize xp_hash_func(CIRInstUniqueKey *key) {
    return key->hash();
}

// 引用 CIRPackage 中某个指令的结果值
// result_instance == nullptr → 值在 cir_package->results[inst_ref] 中
// result_instance != nullptr → 值在 result_instance->results[inst_ref] 中
struct CIRInstResultRef {
    CIRPackage*                            cir_package;
    CIRInstructionRef                      inst_ref;
    std::optional<CIRResultInstance*>      result_instance;

    // 工厂：从 (pkg, inst_ref, 可选 result_instance) 创建
    static CIRInstResultRef make(CIRPackage* pkg, CIRInstructionRef ref,
                                  std::optional<CIRResultInstance*> ri = std::nullopt);

    // 访问：从指定位置读取结果值
    const CIRInstResult* get_result() const;
};

struct FuncCallKey {
    CIRInstructionRef         func_decl_pc;
    Array<CIRInstResultRef>   comptime_arg_refs;

    u64 hash() const;
    bool operator==(const FuncCallKey& other) const;
};

template<>
inline usize xp_hash_func(FuncCallKey *key) {
    return key->hash();
}

struct FuncValue {
    xpString           name;
    CIRInstUniqueKey   func_key;
};

#endif
