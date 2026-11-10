#include "cir_package_ref.hpp"
#include "context.hpp"


template<>
CIRPackage& Ref<CIRPackage>::resolve() const {
    ASSERT(index >= 0 && index < context()->all_packages.count);

    const SlotIndex key = context()->all_packages[index].cir_package_key;
    CIRPackage *pkg = context()->all_cir_packages.try_get(key);
    ASSERT(pkg != nullptr);
    return *pkg;
}
