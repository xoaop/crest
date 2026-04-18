#ifndef CREST_SCOPE_HPP
#define CREST_SCOPE_HPP

#include "xoaop.h"

#include "symbol.hpp"



enum TypeKind: int;




enum class ScopeType {

    // 全局作用域, 包括:
    // 内置类型, 如string
    Global,

    // package作用域, 包括:
    // 包下所有文件的所有函数声明, 结构体声明
    Package,

    // 文件作用域， 包括:
    // import的package
    File,
    

    // 函数的Block, 包括:
    // 函数参数变量
    Function,  

    // 不是函数的Block
    Block,     

    // 循环块
    LoopBlock,
};

struct Scope {

    


    ScopeType scope_type;

    SymbolTable symbols;
    Scope *parent;
};


Scope make_scope(Scope *parent, ScopeType type, xpAllocator allocator);
Scope *alloc_scope(Scope *parent, ScopeType type, xpAllocator allocator);
void free_scope(Scope *scope);

Scope *get_upper_scope_with_type(Scope *scope, ScopeType type);

bool add_symbol_to_scope(Scope *scope, xpString symbol_ident, SymbolInfo info);
bool add_symbol_to_scope(Scope *scope, SymbolInfo info);

SymbolInfo *find_symbol_curr(Scope *scope, xpString symbol_ident);
SymbolInfo *find_symbol_until(ScopeType top_scope_type, Scope *scope, xpString symbol_ident);
SymbolInfo *find_symbol_until_global(Scope *scope, xpString symbol_ident);
SymbolInfo *find_symbol_curr_spec_v(Scope *scope, xpString symbol_ident, TypeKind type_kind);
SymbolInfo *find_symbol_until_spec_v(ScopeType top_scope_type, Scope *scope, xpString symbol_ident, TypeKind type_kind);

#endif