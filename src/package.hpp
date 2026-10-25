#pragma once

#include "xoaop.h"
#include "symbol.hpp"
#include "ast.hpp"
#include "ast_file.hpp"
#include "scope.hpp"
#include "slot_map.hpp"


struct CIRPackage;


struct Package {
    GlobalStringIndex path;

    // TODO: 换成公共或独有的枚举, 而不是借用 SymbolState
    SymbolState state = SymbolState::Unsolved;

    xpArena stage_arena;
    xpAllocator stage_allocator;

    //
    // ast build
    //
    Array<AstFile> ast_files;
    Ref<Scope> package_scope;


    //
    // cir build
    //
    SlotIndex cir_package_key;
};

Package make_package(GlobalStringIndex path, xpAllocator allocator);
