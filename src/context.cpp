#include "context.hpp"

static Context global_context;

Context *context() {
    return &global_context;
}



PackageRef add_package(Context *ctx, Package pkg) {
    ctx->all_packages.push_back(pkg);
    return ctx->all_packages.count - 1;
}

Package *package_by_ref(PackageRef ref) {
    return &context()->all_packages[ref];
}


CIRInstruction* inst(CIRInstructionRef ref) {
    ASSERT(ref.pkg_index >= 0);
    return context()->all_packages[ref.pkg_index].cir_package.inst(ref);
}
