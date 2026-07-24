#ifndef CREST_SYMBOL_HPP
#define CREST_SYMBOL_HPP

#include <optional>

#include "xoaop.h"
#include "common.hpp"
#include "cir_key.hpp"

#include "value.hpp"


struct AstFile;
struct Package;
struct Ast;

enum class SymbolState {
    Unsolved, // 还未解析
    Solving,  // 正在解析中, 用于检测循环依赖
    Solved    // 已经解析完成
};

inline const char* to_string(SymbolState state) {
    switch(state) {
        case SymbolState::Unsolved: return "Unsolved"; // 还未解析
        case SymbolState::Solving:  return "Solving";  // 正在解析中
        case SymbolState::Solved:   return "Solved";   // 已经解析完成
        default: return "Unknown";
    }
}


enum class ValueStoreType {
    Nothing,
    InSymbolInfo, 
    InCIRInstruction,
};

struct SymbolInfo {
    xpString name;
    SymbolState state;
    Package *package;
    AstFile *file;
    Ast *ast;



    ValueStoreType value_store_type;

public: // @NOTE: 注意使用
    union {
        Value value;
        CIRInstResultRef inst_key; // 该符号是由哪个CIR指令定义的, 用于在求值过程中定位到唯一的符号定义, 以便处理循环依赖等情况
    };

public:
    SymbolInfo();
    SymbolInfo(const SymbolInfo& other);
    SymbolInfo& operator=(const SymbolInfo& other);

    CIRInstResultRef val_as_inst_key() const;
    CIRInstResult result(std::optional<FuncCallKey> key = {}) const;
    void val(Value new_val);
    void val(CIRInstResultRef new_key);

    bool is_var_decl();
    bool is_const_decl();
    bool is_const_decl_and_func();
};

SymbolInfo make_symbol(xpString name, Value value, Package *package, AstFile *file, Ast *ast);
SymbolInfo make_symbol(xpString name, Package *package, AstFile *file, Ast *ast);




struct SymbolTable {

    SymbolInfo *operator[](xpString name);


    xpHashMap<xpString, SymbolInfo> symbols;
};
SymbolTable make_symbol_table(xpAllocator allocator);
void free_symbol_table(SymbolTable *table);

b8 add_symbol(SymbolTable *table, xpString name, SymbolInfo info);
SymbolInfo *find_symbol(SymbolTable *table, xpString name);



struct SymbolInfoRef {
    SymbolTable *table;
    xpString name;


    SymbolInfo* operator()() const {
        if(table == nullptr) {
            return nullptr;
        }
        
        return find_symbol(table, name);
    }
};




// formatter
template<>
struct std::formatter<SymbolInfo> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const SymbolInfo& symbol_info, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "SymbolInfo {{ name: '{}', state '{}' }}",
            symbol_info.name,
            to_string(symbol_info.state)
        );
    }
};

template<>
struct std::formatter<SymbolTable> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const SymbolTable& symbol_table, FormatContext& ctx) const {
        const xpHashMapEntry<xpString, SymbolInfo> *entry = nullptr;
        for (isize i = xp_hash_map_first_entry(&symbol_table.symbols, &entry); entry != nullptr; i = xp_hash_map_next_entry(&symbol_table.symbols, i, &entry)) {
            std::format_to(ctx.out(), "  '{}': {}\n", entry->key, entry->value);
        }
        return ctx.out();
    }
};


#endif // CREST_SYMBOL_HPP