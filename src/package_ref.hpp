#pragma once

#include <functional>

#include "xoaop.h"


struct Package;


struct PackageRef {
    static const PackageRef INVALID_REF;

    isize index = -1;

    constexpr PackageRef() = default;
    explicit constexpr PackageRef(isize idx) : index(idx) {}

    Package *operator->() const;
    Package &operator*() const {
        return *operator->();
    }

    bool operator==(const PackageRef &other) const {
        return index == other.index;
    }
};


template<>
struct std::hash<PackageRef> {
    usize operator()(const PackageRef &r) const {
        return (usize)(u64)r.index;
    }
};


inline const PackageRef PackageRef::INVALID_REF{};
