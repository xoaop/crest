#include "context.hpp"

static Context global_context;

Context *context() {
    return &global_context;
}


CIRPackage &get_cir_package(Ref<Package> pkg_ref) {
    Package *pkg = &pkg_ref.unwrap();

    if(!pkg->cir_package_key.is_valid()) {
        CIRPackage cir = make_cir_package(permanent_allocator());
        cir.package_scope = pkg->package_scope;
        cir.package_ref = pkg_ref;
        cir.top_blk = cir.create_block(true, true, false, false);

        pkg->cir_package_key = context()->all_cir_packages.insert(cir);
    }

    return context()->all_cir_packages[pkg->cir_package_key];
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
    return get_cir_package(Ref<Package>(ref.pkg_index)).inst(ref);
}
