#ifndef CREST_SYMBOL_HPP
#define CREST_SYMBOL_HPP

#include "xoaop.h"
#include "common.hpp"

#include "value.hpp"


struct AstFile;
struct Package;
struct Ast;


enum class SymbolState {
    Unsolved, // 还未解析
    Solving,  // 正在解析中, 用于检测循环依赖
    Solved    // 已经解析完成
};


struct SymbolInfo {
    xpString name;
    SymbolState state;
    Value value;
    Package *package;
    AstFile *file;
    Ast *ast;
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





// formatter
template<>
struct std::formatter<SymbolInfo> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const SymbolInfo& symbol_info, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "SymbolInfo {{ name: '{}', value: {} }}",
            symbol_info.name.as_c_str(),
            symbol_info.value
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