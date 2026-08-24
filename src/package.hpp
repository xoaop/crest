#ifndef CREST_PACKAGE_HPP
#define CREST_PACKAGE_HPP

#include "xoaop.h"
#include "symbol.hpp"
#include "ast.hpp"
#include "ast_file.hpp"
#include "scope.hpp"
#include "cir_builder.hpp"


struct CIRPackage;


struct Package {
    xpString path;

    // TODO: 换成公共或独有的枚举, 而不是借用 SymbolState
    SymbolState state = SymbolState::Unsolved;


    //
    // ast build
    //
    Array<AstFile> ast_files;
    Ref<Scope> package_scope;


    //
    // cir build
    //
    CIRPackage cir_package;
};

Package make_package(xpString path, xpAllocator allocator);



#endif