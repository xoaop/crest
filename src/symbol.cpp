#include "symbol.hpp"


//
// SymbolInfo, etc.
//

SymbolInfo make_symbol_info(SymbolType type) {
    SymbolInfo info = {};
    info.type = type;
    return info;
}






//
// Symbol Table
//

SymbolTable global_symbol_table;

SymbolTable* symbol_table() {
    return &global_symbol_table;
}

void symbol_table_init() {
    global_symbol_table.symbols = xp_hash_map_make<xpString, SymbolInfo>(permanent_allocator());
}

SymbolTable make_symbol_table(xpAllocator allocator) {
    SymbolTable table = {};
    table.symbols = xp_hash_map_make<xpString, SymbolInfo>(allocator);
    return table;
}

void free_symbol_table(SymbolTable *table) {
    xp_hash_map_free(table->symbols);
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

