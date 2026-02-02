#ifndef CREST_PACKAGE_HPP
#define CREST_PACKAGE_HPP

#include "xoaop.h"
#include "symbol.hpp"
#include "ast.hpp"
#include "ast_file.hpp"
#include "scope.hpp"


struct Package {
    xpString path;

    Array<AstFile> ast_files;

    Scope package_scope;
};

Package make_package(xpString path, xpAllocator allocator);



#endif