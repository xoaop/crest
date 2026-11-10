#pragma once

#include <functional>

#include "ref.hpp"

struct CIRPackage;
struct CIRResultInstance;


template<>
struct Ref<CIRResultInstance> : RefBase<CIRResultInstance> {
    Ref<CIRPackage> cir_package;
    isize index = -1;

    bool is_valid() const { return cir_package.is_valid() && index >= 0; }

    CIRResultInstance &resolve() const;

    bool operator==(const Ref& other) const {
        return cir_package == other.cir_package && index == other.index;
    }
};


template<>
struct std::hash<Ref<CIRResultInstance>> {
    usize operator()(const Ref<CIRResultInstance>& r) const {
        u64 h = (u64)r.cir_package.index;
        h = xp_hash_combine_u64(h, (u64)r.index);
        return h;
    }
};
