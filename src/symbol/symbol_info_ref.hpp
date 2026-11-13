#pragma once

#include <functional>

#include "ref.hpp"
#include "symbol.hpp"


template<>
struct Ref<SymbolInfo> : RefBase<SymbolInfo> {
    Ref<Scope> scope = Ref<Scope>::INVALID_REF;
    GlobalStringIndex name;

    SymbolInfo &resolve() const;

    bool operator==(const Ref<SymbolInfo> &other) const {
        return scope == other.scope && name == other.name;
    }
};


template<>
struct std::hash<Ref<SymbolInfo>> {
    size_t operator()(const Ref<SymbolInfo> &r) const {
        u64 h = xp_hash_combine_u64(0, (u64)(usize)r.scope.index);
        h = xp_hash_combine_u64(h, (u64)r.name);
        return h;
    }
};
