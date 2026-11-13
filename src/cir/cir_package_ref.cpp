#include "cir_package_ref.hpp"
#include "context.hpp"


CIRPackage& Ref<CIRPackage>::resolve() const {
    return context()->all_cir_packages[key];
}
