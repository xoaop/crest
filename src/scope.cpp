#include "scope.hpp"

#include "analyser.hpp"


Scope make_scope(Scope *parent, ScopeType type, xpAllocator allocator) {
    Scope sc = {};
    sc.scope_type = type;
    sc.parent = parent;
    sc.symbols = make_symbol_table(allocator);

    return sc;
}

Scope *alloc_scope(Scope *parent, ScopeType type, xpAllocator allocator) {
    Scope *sc = (Scope *)xp_alloc(allocator, sizeof(Scope));
    *sc = make_scope(parent, type, allocator);

    return sc;
}

void free_scope(Scope *scope) {
    free_symbol_table(&scope->symbols);
}


Scope *get_upper_scope_with_type(Scope *scope, ScopeType type) {
    Scope *upper = scope->parent;
    while (upper != NULL) {
        if(upper->scope_type == type) {
            return upper;
        }

        upper = upper->parent;
    }

    return NULL;
}


bool add_symbol_to_scope(Scope *scope, xpString symbol_ident, SymbolInfo info) {
    return add_symbol(&scope->symbols, symbol_ident, info);
}

SymbolInfo *find_symbol_curr(Scope *scope, xpString symbol_ident) {
    XP_ASSERT_DEFAULT(scope != NULL);
    
    return find_symbol(&scope->symbols, symbol_ident);
}


SymbolInfo *find_symbol_until(ScopeType top_scope_type, Scope *scope, xpString symbol_ident) {
    XP_ASSERT_DEFAULT(scope != NULL);
    
    Scope *curr = scope;
    while (curr != NULL) {
        SymbolInfo *info = find_symbol_curr(curr, symbol_ident);
        if(info != NULL) {
            return info;
        }

        if(curr->scope_type == top_scope_type) {
            break;
        }

        curr = curr->parent;
    }

    return NULL;
}

SymbolInfo *find_symbol_until_global(Scope *scope, xpString symbol_ident) {
    return find_symbol_until(ScopeType::Global, scope, symbol_ident);
}





SymbolInfo *find_symbol_curr_spec_v(Scope *scope, xpString symbol_ident, TypeKind type_kind) {
    XP_ASSERT_DEFAULT(scope != NULL);
    
    SymbolInfo *info = find_symbol_curr(scope, symbol_ident);

    if(info == NULL) {
        return NULL;
    }

    if(info->value.type->kind != type_kind) {
        return NULL;
    }

    return info;
}


SymbolInfo *find_symbol_until_spec_v(ScopeType top_scope_type, Scope *scope, xpString symbol_ident, TypeKind type_kind) {
    XP_ASSERT_DEFAULT(scope != NULL);
    
    SymbolInfo *info = find_symbol_until(top_scope_type, scope, symbol_ident);

    if(info == NULL) {
        return NULL;
    }

    if(info->value.type->kind != type_kind) {
        return NULL;
    }

    return info;
}