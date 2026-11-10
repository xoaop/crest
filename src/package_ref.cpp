#include "context.hpp"


template<>
Package& Ref<Package>::resolve() const {
    ASSERT(index >= 0 && index < context()->all_packages.count);
    return context()->all_packages[index];
}
