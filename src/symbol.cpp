#include "symbol.hpp"


//
// SymbolInfo, etc.
//

// SymbolInfo make_symbol_info(SymbolType type) {
//     SymbolInfo info = {};
//     info.type = type;
//     return info;
// }

bool is_equal_symbol_info(SymbolInfo a, SymbolInfo b) {
    if(xp_string_cmp(a.name, b.name) != 0) {
        return false;
    }

    return a.type == b.type;
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

