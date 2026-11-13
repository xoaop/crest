#include "symbol_info_ref.hpp"
#include "scope.hpp"


SymbolInfo& Ref<SymbolInfo>::resolve() const {
    ASSERT(scope != Ref<Scope>::INVALID_REF);

    SymbolInfo *info = find_symbol(&scope.unwrap().symbols, name);
    ASSERT(info != nullptr);
    return *info;
}
