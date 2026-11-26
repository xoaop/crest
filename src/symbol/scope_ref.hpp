#pragma once

#include <functional>

#include "xoaop.h"


struct Scope;


struct ScopeRef {
    static const ScopeRef INVALID_REF;

    isize index = -1;

    constexpr ScopeRef() = default;
    explicit constexpr ScopeRef(isize idx) : index(idx) {}

    Scope *operator->() const;
    Scope &operator*() const {
        return *operator->();
    }

    bool operator==(const ScopeRef &other) const {
        return index == other.index;
    }
};


template<>
struct std::hash<ScopeRef> {
    usize operator()(const ScopeRef &r) const {
        return (usize)(u64)r.index;
    }
};


inline const ScopeRef ScopeRef::INVALID_REF{};
