#ifndef CREST_SYMBOL_HPP
#define CREST_SYMBOL_HPP

#include "xoaop.h"
#include "common.hpp"

#include "type.hpp"


#define SYMBOL_TYPES                             \
/**/


#define SYMBOL_TYPE(type_name, ...) SymbolType_##type_name,
enum SymbolType {
    SYMBOL_TYPES
};
#undef SYMBOL_TYPE



#define SYMBOL_TYPE(type_name, struct_def) typedef struct_def XP_JOIN_2(SymbolInfo, type_name);
SYMBOL_TYPES
#undef SYMBOL_TYPE



enum class SymbolKind {
    FunctionDecl,
    StructDecl,
    PackageImport,

    VarDecl,
};



struct Package;

struct SymbolInfo {
    SymbolKind kind;
    xpString name;
    
    // TODO 现在先不区分作用域类型, 后面实现
    // ScopeType scope;
    
    // TODO 这个目前好像没什么用, 可以直接用Type
    // SymbolType type;
    
    TypeRef type;
    union {

        // function decl or struct decl

        // import
        Package *imported_package;
    };

};

// SymbolInfo make_symbol_info(SymbolType type);

bool is_equal_symbol_info(SymbolInfo a, SymbolInfo b);







struct SymbolTable {

    SymbolInfo *operator[](xpString name);


    xpHashMap<xpString, SymbolInfo> symbols;
};
SymbolTable make_symbol_table(xpAllocator allocator);
void free_symbol_table(SymbolTable *table);




b8 add_symbol(SymbolTable *table, xpString name, SymbolInfo info);
SymbolInfo *find_symbol(SymbolTable *table, xpString name);

b8 add_symbol(Array<SymbolTable> *symbol_table_stack, xpString name, SymbolInfo info);
SymbolInfo *find_symbol(Array<SymbolTable> *symbol_table_stack, xpString name);



// Type get_type_detail_if_have(SymbolTable *table, Type type);





#endif // CREST_SYMBOL_HPP