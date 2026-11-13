#pragma once

#include "ref.hpp"
#include "slot_map.hpp"

struct CIRPackage;


template<>
struct Ref<CIRPackage> : RefBase<CIRPackage> {
    SlotIndex key;

    constexpr Ref() = default;
    explicit constexpr Ref(SlotIndex k) : key(k) {}

    CIRPackage &resolve() const;

    bool operator==(const Ref<CIRPackage> &other) const {
        return key == other.key;
    }
};


template<>
struct std::hash<Ref<CIRPackage>> {
    usize operator()(const Ref<CIRPackage> &r) const {
        return std::hash<SlotIndex>{}(r.key);
    }
};
