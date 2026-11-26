#include "package_ref.hpp"

#include "context.hpp"


Package *PackageRef::operator->() const {
    ASSERT(index >= 0 && index < context()->all_packages.count);
    return &context()->all_packages[index];
}
