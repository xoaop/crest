#include "cir_package_ref.hpp"

#include "context.hpp"


CIRPackage *CIRPackageRef::operator->() const {
    return &context()->all_cir_packages[key];
}
