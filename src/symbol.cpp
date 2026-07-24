#include "symbol.hpp"

#include "ast.hpp"

#include "cir_builder.hpp"

#include <cstring>

#include "error_msg.hpp"

//
// SymbolInfo, etc.
//

SymbolInfo::SymbolInfo() {
    value_store_type = ValueStoreType::Nothing;
    value = make_value();
}

SymbolInfo::SymbolInfo(const SymbolInfo& other) {
    memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(SymbolInfo));
}

SymbolInfo& SymbolInfo::operator=(const SymbolInfo& other) {
    if (this == &other) return *this;
    memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(SymbolInfo));
    return *this;
}

CIRInstResult SymbolInfo::result(std::optional<FuncCallKey> key) const {
    if(value_store_type == ValueStoreType::InSymbolInfo) {
        CIRInstResult r;
        r.set_type(value.type);
        r.set_val(value);
        return r;
    }
    if(value_store_type == ValueStoreType::InCIRInstruction) {

        if(key.has_value()) {
            auto *instance = xp_hash_map_get(inst_key.cir_package->result_instances, key.value());
            if(instance) {
                auto *res = xp_hash_map_get((*instance)->results, inst_key.inst_ref);
                if(res) {
                    return *res;
                }
            }
        }
        return inst_key.cir_package->results[inst_key.inst_ref];
    }

    return CIRInstResult{};
}

CIRInstResultRef SymbolInfo::val_as_inst_key() const {
    XP_ASSERT_DEFAULT(value_store_type == ValueStoreType::InCIRInstruction);
    return inst_key;
}

void SymbolInfo::val(Value new_val) {
    value_store_type = ValueStoreType::InSymbolInfo;
    value = new_val;
}
void SymbolInfo::val(CIRInstResultRef new_key) {
    value_store_type = ValueStoreType::InCIRInstruction;
    inst_key = new_key;
}



bool SymbolInfo::is_var_decl() {
    if(ast == nullptr) return false;

    return ast->type == AstType_VariableDecl || ast->type == AstType_ParamDecl;
}

bool SymbolInfo::is_const_decl() {
    if(ast == nullptr) return false;

    return ast->type == AstType_ConstDecl;
}

bool SymbolInfo::is_const_decl_and_func() {
    if(ast == nullptr) return false;

    return ast->type == AstType_ConstDecl && 
           ast->ConstDecl.value_ast->type == AstType_FunctionDeclValue;
}


SymbolInfo make_symbol(xpString name, Value value, Package *package, AstFile *file, Ast *ast) {
    SymbolInfo info{};
    info.name = name;
    info.val(value);
    info.package = package;
    info.file = file;
    info.ast = ast;
    info.state = SymbolState::Solved;
    return info;
}

SymbolInfo make_symbol(xpString name, Package *package, AstFile *file, Ast *ast) {
    SymbolInfo info = make_symbol(name, make_value(), package, file, ast);
    info.state = SymbolState::Unsolved;

    info.value_store_type = ValueStoreType::Nothing;
    info.value = make_value();
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

