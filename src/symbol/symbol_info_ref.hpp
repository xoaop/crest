#pragma once

#include <functional>

#include "xoaop.h"
#include "scope_ref.hpp"
#include "symbol.hpp"


struct SymbolInfoRef {
    static const SymbolInfoRef INVALID_REF;

    ScopeRef scope = ScopeRef::INVALID_REF;
    GlobalStringIndex name;

    SymbolInfo *operator->() const;
    SymbolInfo &operator*() const {
        return *operator->();
    }

    bool operator==(const SymbolInfoRef &other) const {
        return scope == other.scope && name == other.name;
    }
};


template<>
struct std::hash<SymbolInfoRef> {
    size_t operator()(const SymbolInfoRef &r) const {
        u64 h = xp_hash_combine_u64(0, (u64)(usize)r.scope.index);
        h = xp_hash_combine_u64(h, (u64)r.name);
        return h;
    }
};


inline const SymbolInfoRef SymbolInfoRef::INVALID_REF{};
