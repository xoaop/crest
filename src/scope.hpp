#ifndef CREST_SCOPE_HPP
#define CREST_SCOPE_HPP

#include "xoaop.h"

#include "symbol.hpp"
// 前向声明AST，不需要引入完整头文件
struct Ast;
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


    // struct块
    StructBlock,

    // enum块
    EnumBlock,

    // union块
    UnionBlock,
};

inline const char* to_string(ScopeType t) {
    switch(t) {
        case ScopeType::Global: return "Global";
        case ScopeType::Package: return "Package";
        case ScopeType::File: return "File";
        case ScopeType::Function: return "Function";
        case ScopeType::Block: return "Block";
        case ScopeType::LoopBlock: return "LoopBlock";
        case ScopeType::StructBlock: return "StructBlock";
        case ScopeType::EnumBlock: return "EnumBlock";
        case ScopeType::UnionBlock: return "UnionBlock";
        default: return "Unknown";
    }
}


struct Scope {
    ScopeType scope_type;

    SymbolTable symbols;
    Ref<Scope> parent;   // 根 scope 无父，可空

    xpHashMap<Ast*, Ref<Scope>> ast_to_scope;
    Array<Ref<Scope>> children;

    auto begin() { return symbols.symbols.begin(); }
    auto end()   { return symbols.symbols.end(); }

    SymbolInfo& operator[](xpString name);
};


Scope make_scope(Ref<Scope> parent, ScopeType type, xpAllocator allocator);
Ref<Scope> alloc_scope(Array<Scope> *all_scopes, Ref<Scope> parent, ScopeType type, xpAllocator allocator, Ast* owner_ast = nullptr);

void add_sub_scope(Scope *parent, Ref<Scope> child, Ast *owner_ast = nullptr);

Scope *get_upper_scope_with_type(Scope *scope, ScopeType type);

bool add_symbol_to_scope(Scope *scope, xpString symbol_ident, SymbolInfo info);
bool add_symbol_to_scope(Scope *scope, SymbolInfo info);

SymbolInfo *find_symbol_curr(Scope *scope, xpString symbol_ident);
SymbolInfo *find_symbol_until(ScopeType top_scope_type, Scope *scope, xpString symbol_ident);
SymbolInfo *find_symbol_until_global(Scope *scope, xpString symbol_ident);
SymbolInfo *find_symbol_curr_spec_v(Scope *scope, xpString symbol_ident, TypeKind type_kind);
SymbolInfo *find_symbol_until_spec_v(ScopeType top_scope_type, Scope *scope, xpString symbol_ident, TypeKind type_kind);

Ref<SymbolInfo> find_symbol_until_global_ref(Ref<Scope> scope, xpString symbol_ident);


Ref<Scope> try_enter_scope(Scope *parent, Ast *ast_for_child_scope);
Ref<Scope> try_exit_scope(Scope *current);

void print_scope_tree(Scope *scope, int indent = 0);








template<>
struct std::formatter<Scope> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const Scope& scope, std::format_context& ctx) const {
        const char *scope_type_string = nullptr;
        switch(scope.scope_type) {
            case ScopeType::Global: scope_type_string = "Global"; break;
            case ScopeType::Package: scope_type_string = "Package"; break;
            case ScopeType::File: scope_type_string = "File"; break;
            case ScopeType::Function: scope_type_string = "Function"; break;
            case ScopeType::Block: scope_type_string = "Block"; break;
            case ScopeType::LoopBlock: scope_type_string = "LoopBlock"; break;
            case ScopeType::StructBlock: scope_type_string = "StructBlock"; break;
            case ScopeType::EnumBlock: scope_type_string = "EnumBlock"; break;
            case ScopeType::UnionBlock: scope_type_string = "UnionBlock"; break;
        }

        return std::format_to(ctx.out(), "Scope {{ type: {}, symbols: [\n{}] }}",
            scope_type_string,
            scope.symbols
        );
    }
};









#endif