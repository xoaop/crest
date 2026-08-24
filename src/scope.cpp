#include "scope.hpp"
#include "cir_builder.hpp"
#include "type.hpp"

#include "print.hpp"

struct Ast;

SymbolInfo& Scope::operator[](xpString name) {
    SymbolInfo *info = find_symbol_until_global(this, name);
    XP_ASSERT_DEFAULT(info != nullptr);
    return *info;
}



Scope make_scope(Ref<Scope> parent, ScopeType type, xpAllocator allocator) {
    Scope sc = {};
    sc.scope_type = type;
    sc.parent = parent;
    sc.symbols = make_symbol_table(allocator);
    sc.ast_to_scope = xp_hash_map_make<Ast*, Ref<Scope>>(allocator);
    sc.children = make_array<Ref<Scope>>(allocator);
    return sc;
}


Ref<Scope> alloc_scope(Array<Scope> *all_scopes, Ref<Scope> parent, ScopeType type, xpAllocator allocator, Ast *owner_ast) {
    all_scopes->push_back(make_scope(parent, type, allocator));
    Ref<Scope> self{all_scopes->count - 1};

    if(parent != Ref<Scope>::INVALID_REF) {
        add_sub_scope(&(*all_scopes)[parent.index], self, owner_ast);
    }
    return self;
}

void add_sub_scope(Scope *parent, Ref<Scope> child, Ast *owner_ast) {
    if(owner_ast != nullptr) {
        xp_hash_map_insert(&parent->ast_to_scope, owner_ast, child);
    }

    parent->children.push_back(child);
}


Scope *get_upper_scope_with_type(Scope *scope, ScopeType type) {
    Scope *upper = scope;
    while (upper != NULL) {
        if(upper->scope_type == type) {
            return upper;
        }

        Ref<Scope> parent = upper->parent;
        upper = (parent == Ref<Scope>::INVALID_REF) ? NULL : &parent.unwrap();
    }

    return NULL;
}


bool add_symbol_to_scope(Scope *scope, xpString symbol_ident, SymbolInfo info) {
    return add_symbol(&scope->symbols, symbol_ident, info);
}

bool add_symbol_to_scope(Scope *scope, SymbolInfo info) {
    return add_symbol(&scope->symbols, info.name, info);
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

        Ref<Scope> parent = curr->parent;
        curr = (parent == Ref<Scope>::INVALID_REF) ? NULL : &parent.unwrap();
    }

    return NULL;
}

SymbolInfo *find_symbol_until_global(Scope *scope, xpString symbol_ident) {
    return find_symbol_until(ScopeType::Global, scope, symbol_ident);
}


Ref<SymbolInfo> find_symbol_until_global_ref(Ref<Scope> scope, xpString symbol_ident) {
    Ref<Scope> curr = scope;
    while (curr != Ref<Scope>::INVALID_REF) {
        Scope &s = curr.unwrap();
        SymbolInfo *info = find_symbol(&s.symbols, symbol_ident);
        if (info != nullptr) {
            return Ref<SymbolInfo>{
                .scope = curr,
                .name = symbol_ident
            };
        }
        curr = s.parent;
    }
    return Ref<SymbolInfo>::INVALID_REF;
}



SymbolInfo *find_symbol_curr_spec_v(Scope *scope, xpString symbol_ident, TypeKind type_kind) {
    XP_ASSERT_DEFAULT(scope != NULL);

    SymbolInfo *info = find_symbol_curr(scope, symbol_ident);

    if(info == NULL) {
        return NULL;
    }

    if(info->result().type()->kind != type_kind) {
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

    if(info->result().type()->kind != type_kind) {
        return NULL;
    }

    return info;
}

Ref<Scope> try_enter_scope(Scope *parent, Ast *ast_for_child_scope) {
    XP_ASSERT_DEFAULT(parent != NULL && ast_for_child_scope != NULL);

    Ref<Scope> *child_scope = xp_hash_map_get(parent->ast_to_scope, ast_for_child_scope);
    if(child_scope != nullptr) {
        return *child_scope;
    }

    return Ref<Scope>::INVALID_REF;
}

Ref<Scope> try_exit_scope(Scope *current) {
    XP_ASSERT_DEFAULT(current != NULL);
    return current->parent;
}


void print_scope_tree(Scope *scope, int indent) {
#if defined(CREST_DEBUG)
    if (!scope) return;

    auto print_indent = [](int n) {
        for (int i = 0; i < n; i++) print_err("  ");
    };

    print_indent(indent);
    println_err("{}", *scope);

    for(Ref<Scope> sub_scope : scope->children) {
        print_scope_tree(&sub_scope.unwrap(), indent + 1);
    }
#endif
}
