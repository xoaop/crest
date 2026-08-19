#include "context.hpp"

static Context global_context;

Context *context() {
    return &global_context;
}



RefN<Package> add_package(Context *ctx, Package pkg) {
    ctx->all_packages.push_back(pkg);
    return RefN<Package>{ctx->all_packages.count - 1};
}

Package *try_access_val(const Ref<Package> &r) {
    if (r.index < 0) return nullptr;
    return &context()->all_packages[r.index];
}

Scope *try_access_val(const Ref<Scope> &r) {
    if (r.index < 0 || r.index >= context()->all_scopes.count) return nullptr;
    return &context()->all_scopes[r.index];
}


CIRInstruction* inst(CIRInstructionRef ref) {
    ASSERT(ref.pkg_index >= 0);
    return context()->all_packages[ref.pkg_index].cir_package.inst(ref);
}
