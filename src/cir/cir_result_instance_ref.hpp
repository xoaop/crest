#pragma once

#include <functional>

#include "cir_package_ref.hpp"


struct CIRResultInstance;


struct CIRResultInstanceRef {
    static const CIRResultInstanceRef INVALID_REF;

    CIRPackageRef cir_package;
    isize index = -1;

    CIRResultInstance *operator->() const;

    bool operator==(const CIRResultInstanceRef &other) const {
        return cir_package == other.cir_package && index == other.index;
    }
};


template<>
struct std::hash<CIRResultInstanceRef> {
    usize operator()(const CIRResultInstanceRef &r) const {
        u64 h = (u64)std::hash<CIRPackageRef>{}(r.cir_package);
        h = xp_hash_combine_u64(h, (u64)r.index);
        return h;
    }
};


inline const CIRResultInstanceRef CIRResultInstanceRef::INVALID_REF{};
