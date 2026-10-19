#include "symbol.hpp"

#include "scope.hpp"

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
            auto *entry = xp_hash_map_get(inst_key.cir_package->result_instance_map, key.value());
            if(entry != nullptr) {
                auto *instance = &inst_key.cir_package->result_instances[entry->index];
                auto *res = instance->result_ptr_of(inst_key.inst_ref);
                if(res) {
                    return *res;
                }
            }
        }
        auto *res = inst_key.cir_package->results.result_ptr_of(inst_key.inst_ref);
        if(res) return *res;
    }

    return CIRInstResult{};
}

Ref<CIRInstResult> SymbolInfo::val_as_inst_key() const {
    XP_ASSERT_DEFAULT(value_store_type == ValueStoreType::InCIRInstruction);
    return inst_key;
}

void SymbolInfo::val(Value new_val) {
    value_store_type = ValueStoreType::InSymbolInfo;
    value = new_val;
}
void SymbolInfo::val(Ref<CIRInstResult> new_key) {
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
           ast->ConstDecl.value_ast != nullptr &&
           ast->ConstDecl.value_ast->type == AstType_FunctionDeclValue;
}

bool SymbolInfo::is_built_in() {
    if(ast == nullptr) return false;

    return ast->type == AstType_ConstDecl && ast->ConstDecl.is_builtin;
}


SymbolInfo make_symbol(xpString name, Value value, Ref<Package> package, AstFile *file, Ast *ast) {
    SymbolInfo info{};
    info.name = name;
    info.val(value);
    info.package = package;
    info.file = file;
    info.ast = ast;
    info.state = SymbolState::Solved;
    return info;
}

SymbolInfo make_symbol(xpString name, Ref<Package> package, AstFile *file, Ast *ast) {
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
    ASSERT(allocator.proc != xp_heap_allocator().proc); // TODO: 加个重载，而不是比proc

    SymbolTable table = {};
    table.symbols = xp_hash_map_make<xpString, SymbolInfo>(allocator);

    return table;
}


void add_symbol(SymbolTable *table, xpString name, SymbolInfo info) {
    xp_hash_map_insert(&table->symbols, name, info);
}


SymbolInfo *find_symbol(SymbolTable *table, xpString name) {
    return xp_hash_map_get(table->symbols, name);
}

SymbolInfo *Ref<SymbolInfo>::resolve() const {
    if(this->scope == Ref<Scope>::INVALID_REF) {
        return nullptr;
    }
    return find_symbol(&this->scope.unwrap().symbols, this->name);
}

