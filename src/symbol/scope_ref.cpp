#include "scope_ref.hpp"

#include "context.hpp"


Scope *ScopeRef::operator->() const {
    ASSERT(index >= 0 && index < context()->all_scopes.count);
    return &context()->all_scopes[index];
}
