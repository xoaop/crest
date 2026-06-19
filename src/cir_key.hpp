#ifndef CREST_CIR_KEY_HPP
#define CREST_CIR_KEY_HPP

#include "xoaop.h"

struct CIRPackage;
struct Package;
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

struct FuncValue {
    xpString           name;
    CIRInstUniqueKey   func_key;
};

#endif
