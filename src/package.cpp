#include "package.hpp"


Package make_package(xpString path, xpAllocator allocator) {
    Package p = {};
    p.path = path;

    p.ast_files = make_array<AstFile>(allocator);

    return p;
}
