#include "context.hpp"


template<>
Scope& Ref<Scope>::resolve() const {
    ASSERT(index >= 0 && index < context()->all_scopes.count);
    return context()->all_scopes[index];
}
