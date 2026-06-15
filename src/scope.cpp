#include "scope.hpp"
#include "type.hpp"

struct Ast;

SymbolInfo& SymbolIterator::operator*() const {
    return map->entries[pos].value;
}

SymbolIterator& SymbolIterator::operator++() {
    xpHashMapEntry<xpString, SymbolInfo> *next = nullptr;
    pos = xp_hash_map_next_entry(map, pos, &next);
    return *this;
}

bool SymbolIterator::operator!=(const SymbolIterator& other) const {
    return pos != other.pos;
}

SymbolIterator Scope::begin() {
    SymbolIterator it;
    it.map = &symbols.symbols;
    xpHashMapEntry<xpString, SymbolInfo> *first = nullptr;
    it.pos = xp_hash_map_first_entry(it.map, &first);
    return it;
}

SymbolIterator Scope::end() {
    return { &symbols.symbols, END_OF_HASH_MAP_INDEX };
}

SymbolInfo& Scope::operator[](xpString name) {
    SymbolInfo *info = find_symbol_until_global(this, name);
    XP_ASSERT_DEFAULT(info != nullptr);
    return *info;
}



Scope make_scope(Scope *parent, ScopeType type, xpAllocator allocator) {
    Scope sc = {};
    sc.scope_type = type;
    sc.parent = parent;
    sc.symbols = make_symbol_table(allocator);
    sc.ast_to_scope = xp_hash_map_make<Ast*, Scope*>(allocator);
    sc.children = make_array<Scope*>(allocator);
    return sc;
}


Scope *alloc_scope(Scope *parent, ScopeType type, xpAllocator allocator, Ast *owner_ast) {
    Scope *sc = (Scope *)xp_alloc(allocator, sizeof(Scope));

    *sc = make_scope(parent, type, allocator);

    if(parent != nullptr) {
        add_sub_scope(parent, sc, owner_ast);
    }
    return sc;
}

void add_sub_scope(Scope *parent, Scope *child, Ast *owner_ast) {
    XP_ASSERT_DEFAULT(parent != nullptr && child != nullptr);
    
    if(owner_ast != nullptr) {
        xp_hash_map_insert(&parent->ast_to_scope, owner_ast, child);
    }

    array_push_back(&parent->children, child);
}

void free_scope(Scope *scope) {
    free_symbol_table(&scope->symbols);
    // 释放当前Scope的子映射表
    xp_hash_map_free(scope->ast_to_scope);
    array_free(&scope->children);
}


Scope *get_upper_scope_with_type(Scope *scope, ScopeType type) {
    Scope *upper = scope;
    while (upper != NULL) {
        if(upper->scope_type == type) {
            return upper;
        }

        upper = upper->parent;
    }

    return NULL;
}


Scope *find_scope_by_ast(Scope *current_scope, Ast *ast) {
    if (!current_scope || !ast) return nullptr;

    auto opt = xp_hash_map_get(current_scope->ast_to_scope, ast);
    if (opt != nullptr) {
        return *opt;
    }

    return find_scope_by_ast(current_scope->parent, ast);
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

    if(info->val().type->kind != type_kind) {
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

    if(info->val().type->kind != type_kind) {
        return NULL;
    }

    return info;
}

Scope *try_enter_scope(Scope *parent, Ast *ast_for_child_scope) {
    XP_ASSERT_DEFAULT(parent != NULL && ast_for_child_scope != NULL);

    Scope **child_scope = xp_hash_map_get(parent->ast_to_scope, ast_for_child_scope);
    if(child_scope != nullptr) {
        return *child_scope;
    }

    return nullptr;
}

Scope *try_exit_scope(Scope *current) {
    XP_ASSERT_DEFAULT(current != NULL);
    return current->parent; // 可能返回nullptr, 调用者需要检查, 不能退出全局作用域
}


Scope *enter_scope(Scope *parent, Ast *ast_for_child_scope) {
    Scope *child_scope = try_enter_scope(parent, ast_for_child_scope);
    XP_ASSERT_DEFAULT(child_scope != NULL);
    return child_scope;
}

Scope *exit_scope(Scope *current) {
    Scope *parent = try_exit_scope(current);
    XP_ASSERT_DEFAULT(parent != NULL);
    return parent;
}


void print_scope_tree(Scope *scope, int indent) {
    if (!scope) return;

    auto print_indent = [](int n) {
        for (int i = 0; i < n; i++) std::print("  ");
    };

    print_indent(indent);
    std::println("{}", *scope);

    for(Scope *sub_scope : scope->children) {
        print_scope_tree(sub_scope, indent + 1);
    }
}