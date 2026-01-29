#ifndef CREST_SCOPE_HPP
#define CREST_SCOPE_HPP

#include "xoaop.h"

#include "symbol.hpp"


enum class ScopeType {
    Package, 
    File, 
    Function,  // 函数的Block
    Block,     // 不是函数的Block
    LoopBlock, // 循环块 
};

struct Scope {
    ScopeType scope_type;

    SymbolTable symbols;
    Scope *parent;
};


Scope make_scope(Scope *parent, ScopeType type, xpAllocator allocator);



#endif