#include "symbol_info_ref.hpp"

#include "scope.hpp"


SymbolInfo *SymbolInfoRef::operator->() const {
    ASSERT(scope != ScopeRef::INVALID_REF);

    SymbolInfo *info = find_symbol(&scope->symbols, name);
    ASSERT(info != nullptr);
    return info;
}
