#pragma once

#include <functional>

#include "slot_map.hpp"


struct CIRPackage;


struct CIRPackageRef {
    static const CIRPackageRef INVALID_REF;

    SlotIndex key;

    constexpr CIRPackageRef() = default;
    explicit constexpr CIRPackageRef(SlotIndex k) : key(k) {}

    CIRPackage *operator->() const;
    CIRPackage &operator*() const {
        return *operator->();
    }

    bool operator==(const CIRPackageRef &other) const {
        return key == other.key;
    }
};


template<>
struct std::hash<CIRPackageRef> {
    usize operator()(const CIRPackageRef &r) const {
        return std::hash<SlotIndex>{}(r.key);
    }
};


inline const CIRPackageRef CIRPackageRef::INVALID_REF{};
