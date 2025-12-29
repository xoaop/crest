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

    return is_equal_type(a.type, b.type);
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


b8 add_symbol(Array<SymbolTable> *symbol_table_stack, xpString name, SymbolInfo info) {
    SymbolTable *current_table = &(*symbol_table_stack)[symbol_table_stack->count - 1];
    return add_symbol(current_table, name, info);
}

SymbolInfo *find_symbol(Array<SymbolTable> *symbol_table_stack, xpString name) {
    for(isize i = symbol_table_stack->count - 1; i >= 0; i--) {
        SymbolTable *table = &(*symbol_table_stack)[i];
        SymbolInfo *info = find_symbol(table, name);
        if(info != NULL) {
            return info;
        }
    }

    return NULL;
}