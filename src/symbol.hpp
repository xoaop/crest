#ifndef CREST_SYMBOL_HPP
#define CREST_SYMBOL_HPP

#include "xoaop.h"
#include "common.hpp"

#include "value.hpp"


struct AstFile;
struct Package;
struct Ast;


struct SymbolInfo {
    xpString name;
    Value value;
    Package *package;
    AstFile *file;
};

SymbolInfo make_symbol(xpString name, Value value, Package *package, AstFile *file);



struct SymbolTable {

    SymbolInfo *operator[](xpString name);


    xpHashMap<xpString, SymbolInfo> symbols;
};
SymbolTable make_symbol_table(xpAllocator allocator);
void free_symbol_table(SymbolTable *table);




b8 add_symbol(SymbolTable *table, xpString name, SymbolInfo info);
SymbolInfo *find_symbol(SymbolTable *table, xpString name);


#endif // CREST_SYMBOL_HPP