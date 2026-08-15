#ifndef CREST_CIR_INSTRUCTION_REF_HPP
#define CREST_CIR_INSTRUCTION_REF_HPP

#include "xoaop.h"
#include "array.hpp"
#include "print.hpp"
#include <format>
#include <functional>
#include <optional>

struct CIRPackage;
struct CIRInstResult;
struct CIRResultInstance;
struct CIRInstruction;
using CIRResultInstanceRef = CIRResultInstance*;
using CIRBlockRef = isize;


constexpr CIRBlockRef INVALID_BLOCK = -1;
constexpr isize INVALID_INST_INDEX = -1;



struct CIRInstructionRef {
    isize       pkg_index = -1;
    CIRBlockRef block_ref = INVALID_BLOCK;
    isize       inst_index = INVALID_INST_INDEX;

    constexpr CIRInstructionRef() : block_ref(INVALID_BLOCK), inst_index(INVALID_INST_INDEX) {}
    explicit CIRInstructionRef(CIRBlockRef blk, isize idx, isize pkg_index = -1) : block_ref(blk), inst_index(idx), pkg_index(pkg_index) {
        ASSERT(blk != INVALID_BLOCK && idx != INVALID_INST_INDEX);
    }
    explicit CIRInstructionRef(CIRBlockRef blk) : block_ref(blk), inst_index(INVALID_INST_INDEX) {
        ASSERT(blk != INVALID_BLOCK);
    }

    // 块内推进：只动 inst_index，block_ref 不变（不跨 Block）
    void advance(isize n = 1) { 
        inst_index += n; 
    }

    void retreat(isize n = 1) { 
        inst_index -= n; 
    }

    CIRInstructionRef next(isize n = 1) const {
        return CIRInstructionRef{block_ref, inst_index + n, pkg_index};
    }
    CIRInstructionRef prev(isize n = 1) const {
        return CIRInstructionRef{block_ref, inst_index - n, pkg_index};
    }

    auto operator<=>(const CIRInstructionRef&) const = default;
};

const CIRInstructionRef INVALID_INST = CIRInstructionRef{};



template<>
struct std::hash<CIRInstructionRef> {
    usize operator()(const CIRInstructionRef& r) const {
        u64 h = xp_hash_combine_u64((u64)r.block_ref, (u64)r.inst_index);
        h = xp_hash_combine_u64(h, (u64)r.pkg_index);
        return h;
    }
};

template<>
struct std::formatter<CIRInstructionRef> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const CIRInstructionRef& r, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}.{}", r.block_ref, r.inst_index);
    }
};


// 引用 CIRPackage 中某个指令的结果值
// result_instance == nullptr → 值在 cir_package->results[inst_ref] 中
// result_instance != nullptr → 值在 result_instance->results[inst_ref] 中
struct CIRInstResultRef {
    CIRPackage*                            cir_package;
    CIRInstructionRef                      inst_ref;
    std::optional<CIRResultInstanceRef>      result_instance;

    static CIRInstResultRef make(CIRPackage* pkg, CIRInstructionRef ref, std::optional<CIRResultInstanceRef> ri = std::nullopt);

    const CIRInstResult* get_result() const;

    const CIRInstruction* inst() const;

    bool operator==(const CIRInstResultRef& other) const {
        return cir_package == other.cir_package
            && inst_ref == other.inst_ref
            && result_instance.has_value() == other.result_instance.has_value()
            && (!result_instance.has_value() || result_instance.value() == other.result_instance.value());
    }
};

template<>
struct std::hash<CIRInstResultRef> {
    usize operator()(const CIRInstResultRef& key) const {
        u64 h = (u64)(usize)key.cir_package;
        h = xp_hash_combine_u64(h, (u64)key.inst_ref.block_ref);
        h = xp_hash_combine_u64(h, (u64)key.inst_ref.inst_index);
        h = xp_hash_combine_u64(h, (u64)(usize)(key.result_instance ? *key.result_instance : nullptr));
        return h;
    }
};

// 函数调用键：定位一次 comptime 函数调用（声明 + 实例 + 参数引用）
struct FuncCallKey {
    CIRInstructionRef                        func_decl_pc;
    std::optional<CIRResultInstanceRef>        func_instance;
    Array<CIRInstResultRef>                  comptime_arg_refs;

    u64 hash() const;
    bool operator==(const FuncCallKey& other) const;
};

template<>
struct std::hash<FuncCallKey> {
    usize operator()(const FuncCallKey& key) const {
        return key.hash();
    }
};

#endif
