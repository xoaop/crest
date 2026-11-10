#include "symbol_info_ref.hpp"
#include "scope.hpp"


SymbolInfo& Ref<SymbolInfo>::resolve() const {
    ASSERT(scope.is_valid());

    SymbolInfo *info = find_symbol(&scope.unwrap().symbols, name);
    ASSERT(info != nullptr);
    return *info;
}
