#ifndef CREST_SYMBOL_HPP
#define CREST_SYMBOL_HPP

#include "xoaop.h"
#include "common.hpp"

#include "type.hpp"


#define SYMBOL_TYPES                             \
    SYMBOL_TYPE(Function, struct {               \
        xpString name;                           \
        Array<Type> param_types;                 \
        Type return_type;                        \
    })                                           \
    SYMBOL_TYPE(VariableDecl, struct {           \
        xpString name;                           \
        Type type;                               \
    })                                           \
/**/


#define SYMBOL_TYPE(type_name, ...) SymbolType_##type_name,
enum SymbolType {
    SYMBOL_TYPES
};
#undef SYMBOL_TYPE



#define SYMBOL_TYPE(type_name, struct_def) typedef struct_def XP_JOIN_2(SymbolInfo, type_name);
SYMBOL_TYPES
#undef SYMBOL_TYPE



enum ScopeType {
    ScopeType_Global,
    ScopeType_Local,
};


struct SymbolInfo {
    xpString name;
    // TODO(xoaop): Additional symbol information
    ScopeType scope;
    
    SymbolType type;

    union {
        #define SYMBOL_TYPE(type_name, ...) XP_JOIN_2(SymbolInfo, type_name) type_name;
        SYMBOL_TYPES
        #undef SYMBOL_TYPE
    };

};

SymbolInfo make_symbol_info(SymbolType type);







struct SymbolTable {

    SymbolInfo *operator[](xpString name);


    xpHashMap<xpString, SymbolInfo> symbols;
};
SymbolTable make_symbol_table(xpAllocator allocator);
void free_symbol_table(SymbolTable *table);


SymbolTable* symbol_table();
void symbol_table_init();


b8 add_symbol(SymbolTable *table, xpString name, SymbolInfo info);
SymbolInfo *find_symbol(SymbolTable *table, xpString name);

b8 add_symbol(Array<SymbolTable> *symbol_table_stack, xpString name, SymbolInfo info);
SymbolInfo *find_symbol(Array<SymbolTable> *symbol_table_stack, xpString name);

#endif // CREST_SYMBOL_HPP