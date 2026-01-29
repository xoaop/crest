#include "scope.hpp"


Scope make_scope(Scope *parent, ScopeType type, xpAllocator allocator) {
    Scope sc = {};
    sc.scope_type = type;
    sc.parent = parent;
    sc.symbols = make_symbol_table(allocator);

    return sc;
}