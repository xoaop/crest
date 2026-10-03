#include "context.hpp"

static Context global_context;

Context *context() {
    return &global_context;
}



Ref<Package> add_package(Context *ctx, Package pkg) {
    ctx->all_packages.push_back(pkg);
    return Ref<Package>{ctx->all_packages.count - 1};
}

template<>
Package *Ref<Package>::resolve() const {
    if (this->index < 0) return nullptr;
    return &context()->all_packages[this->index];
}

template<>
Scope *Ref<Scope>::resolve() const {
    if (this->index < 0 || this->index >= context()->all_scopes.count) return nullptr;
    return &context()->all_scopes[this->index];
}


const CIRInstruction& inst(CIRInstructionRef ref) {
    ASSERT(ref.pkg_index >= 0);
    return context()->all_packages[ref.pkg_index].cir_package.inst(ref);
}
