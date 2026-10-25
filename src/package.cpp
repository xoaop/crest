#include "package.hpp"
#include "cir_builder.hpp"


Package make_package(GlobalStringIndex path, xpAllocator allocator) {
    Package p = {};
    p.path = path;

    p.ast_files = make_array<AstFile>(allocator);
    return p;
}
