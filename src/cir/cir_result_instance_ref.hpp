#pragma once

#include <functional>

#include "ref.hpp"
#include "cir_package_ref.hpp"

struct CIRPackage;
struct CIRResultInstance;


template<>
struct Ref<CIRResultInstance> : RefBase<CIRResultInstance> {
    Ref<CIRPackage> cir_package;
    isize index = -1;

    CIRResultInstance &resolve() const;

    bool operator==(const Ref& other) const {
        return cir_package == other.cir_package && index == other.index;
    }
};


template<>
struct std::hash<Ref<CIRResultInstance>> {
    usize operator()(const Ref<CIRResultInstance>& r) const {
        u64 h = (u64)std::hash<Ref<CIRPackage>>{}(r.cir_package);
        h = xp_hash_combine_u64(h, (u64)r.index);
        return h;
    }
};
