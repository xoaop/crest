#include "symbol.hpp"


//
// SymbolInfo, etc.
//

SymbolInfo make_symbol(xpString name, Value value, Package *package, AstFile *file, Ast *ast) {
    SymbolInfo info = {};
    info.name = name;
    info.value = value;
    info.package = package;
    info.file = file;
    info.ast = ast;
    info.state = SymbolState::Solved;
    return info;
}

SymbolInfo make_symbol(xpString name, Package *package, AstFile *file, Ast *ast) {
    SymbolInfo info = make_symbol(name, make_value(), package, file, ast);
    info.state = SymbolState::Unsolved;
    return info;
}


//
// Symbol Table
//


SymbolTable make_symbol_table(xpAllocator allocator) {
    SymbolTable table = {};
    table.symbols = xp_hash_map_make<xpString, SymbolInfo>(allocator);
    return table;
}

void free_symbol_table(SymbolTable *table) {
    xp_hash_map_free(table->symbols);
}

SymbolInfo *SymbolTable::operator[](xpString name) {
    return find_symbol(this, name);
}


b8 add_symbol(SymbolTable *table, xpString name, SymbolInfo info) {
    xpHashMap<xpString, SymbolInfo> *map = &table->symbols;
    if(xp_hash_map_get(*map, name) != NULL) {
        return false;
    }

    xp_hash_map_insert(map, name, info);
    return true;
}


SymbolInfo *find_symbol(SymbolTable *table, xpString name) {
    xpHashMap<xpString, SymbolInfo> *map = &table->symbols;
    SymbolInfo *info = xp_hash_map_get(*map, name);
    return info;
}

