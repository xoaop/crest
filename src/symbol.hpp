#ifndef CREST_SYMBOL_HPP
#define CREST_SYMBOL_HPP

#include <optional>
#include <functional>
#include <type_traits>

#include "xoaop.h"
#include "common.hpp"
#include "cir_instruction_ref.hpp"
#include "ref.hpp"

#include "value.hpp"


struct AstFile;
struct Package;
struct Ast;
struct Scope;

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
    Ref<Package> package;
    AstFile *file;
    Ast *ast;



    ValueStoreType value_store_type;

public: // @NOTE: 注意使用
    union {
        Value value;
        Ref<CIRInstResult> inst_key; // 该符号是由哪个CIR指令定义的, 用于在求值过程中定位到唯一的符号定义, 以便处理循环依赖等情况
    };

public:
    SymbolInfo();
    SymbolInfo(const SymbolInfo& other);
    SymbolInfo& operator=(const SymbolInfo& other);

    Ref<CIRInstResult> val_as_inst_key() const;
    CIRInstResult result(std::optional<FuncCallKey> key = std::nullopt) const;
    void val(Value new_val);
    void val(Ref<CIRInstResult> new_key);

    bool is_var_decl();
    bool is_const_decl();
    bool is_const_decl_and_func();
};

SymbolInfo make_symbol(xpString name, Value value, Ref<Package> package, AstFile *file, Ast *ast);
SymbolInfo make_symbol(xpString name, Ref<Package> package, AstFile *file, Ast *ast);




struct SymbolTable {

    SymbolInfo *operator[](xpString name);


    xpHashMap<xpString, SymbolInfo> symbols;
};
SymbolTable make_symbol_table(xpAllocator allocator);
void free_symbol_table(SymbolTable *table);

b8 add_symbol(SymbolTable *table, xpString name, SymbolInfo info);
SymbolInfo *find_symbol(SymbolTable *table, xpString name);



SymbolInfo *try_access_val(const Ref<SymbolInfo> &r);

template<>
struct Ref<SymbolInfo> : RefBase<SymbolInfo> {
    Ref<Scope> scope = Ref<Scope>::INVALID_REF;
    xpString name;

    bool operator==(const Ref<SymbolInfo> &other) const {
        return scope == other.scope && name == other.name;
    }
};


template<>
struct std::hash<Ref<SymbolInfo>> {
    size_t operator()(const Ref<SymbolInfo> &r) const {
        u64 h = xp_hash_combine_u64(0, (u64)(usize)r.scope.index);
        h = xp_hash_combine_u64(h, std::hash<xpString>()(r.name));
        return h;
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
        for (const auto& entry : symbol_table.symbols) {
            std::format_to(ctx.out(), "  '{}': {}\n", entry.key, entry.value);
        }
        return ctx.out();
    }
};


#endif // CREST_SYMBOL_HPP