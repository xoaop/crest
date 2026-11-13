#include "context.hpp"

static Context global_context;

Context *context() {
    return &global_context;
}


Ref<CIRPackage> get_cir_package(Ref<Package> pkg_ref) {
    Package *pkg = &pkg_ref.unwrap();

    if(pkg->cir_package == Ref<CIRPackage>::INVALID_REF) {
        CIRPackage cir = make_cir_package(permanent_allocator());
        cir.package_scope = pkg->package_scope;
        cir.package_ref = pkg_ref;

        Ref<CIRPackage> ref{context()->all_cir_packages.insert(cir)};

        CIRPackage &created = context()->all_cir_packages[ref.key];
        created.self_ref = ref;
        created.top_blk = created.create_block(true, true, false, false);

        pkg->cir_package = ref;
    }

    return pkg->cir_package;
}



Ref<Package> add_package(Context *ctx, Package pkg) {
    ctx->all_packages.push_back(pkg);
    return Ref<Package>{ctx->all_packages.count - 1};
}



const CIRInstruction& inst(CIRInstructionRef ref) {
    ASSERT(ref.pkg != Ref<CIRPackage>::INVALID_REF);
    return ref.pkg->inst(ref);
}


GlobalStringIndex get_or_insert_global_string(const char *str) {
    return get_or_insert_global_string(context()->global_strings, str, permanent_allocator());
}

GlobalStringIndex get_or_insert_global_string(const char *str, size_t count) {
    return get_or_insert_global_string(context()->global_strings, str, count, permanent_allocator());
}

GlobalStringIndex get_or_insert_global_string(const xpString& str) {
    return get_or_insert_global_string(context()->global_strings, str, permanent_allocator());
}


StringRef get_global_string(GlobalStringIndex index) {
    return get_global_string(context()->global_strings, index);
}