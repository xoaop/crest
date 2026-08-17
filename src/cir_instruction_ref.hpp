#ifndef CREST_CIR_INSTRUCTION_REF_HPP
#define CREST_CIR_INSTRUCTION_REF_HPP

#include "xoaop.h"
#include "array.hpp"
#include "print.hpp"
#include "ref.hpp"
#include <format>
#include <functional>
#include <optional>

struct CIRPackage;
struct CIRInstResult;
struct CIRResultInstance;
struct CIRInstruction;
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


template<> struct Ref<CIRInstResult>;

struct FuncCallKey {
    CIRInstructionRef                        func_decl_pc;
    Array<Ref<CIRInstResult>>                comptime_arg_refs = {};

    u64 hash() const;
    bool operator==(const FuncCallKey& other) const;
};

template<>
struct std::hash<FuncCallKey> {
    usize operator()(const FuncCallKey& key) const {
        return key.hash();
    }
};


CIRResultInstance* try_access_val(const Ref<CIRResultInstance>& r);

template<>
struct Ref<CIRResultInstance> : RefBase<CIRResultInstance> {
    FuncCallKey key;

    bool operator==(const Ref& other) const { return key == other.key; }
};


// 引用 CIRPackage 中某个指令的结果值
// result_instance == INVALID_REF → 值在 cir_package->results[inst_ref] 中
// result_instance != INVALID_REF → 值在 result_instance->results[inst_ref] 中
CIRInstResult* try_access_val(const Ref<CIRInstResult>& r);

template<>
struct Ref<CIRInstResult> : RefBase<CIRInstResult> {
    CIRPackage*                            cir_package = nullptr;
    CIRInstructionRef                      inst_ref;
    Ref<CIRResultInstance>                 result_instance;

    static Ref make(CIRPackage* pkg, CIRInstructionRef ref, Ref<CIRResultInstance> ri = {});

    CIRInstResult* get_result() const;

    const CIRInstruction* inst() const;

    bool operator==(const Ref& other) const {
        return cir_package == other.cir_package
            && inst_ref == other.inst_ref
            && result_instance == other.result_instance;
    }
};

template<>
struct std::hash<Ref<CIRInstResult>> {
    usize operator()(const Ref<CIRInstResult>& key) const {
        u64 h = (u64)(usize)key.cir_package;
        h = xp_hash_combine_u64(h, (u64)key.inst_ref.block_ref);
        h = xp_hash_combine_u64(h, (u64)key.inst_ref.inst_index);
        h = xp_hash_combine_u64(h, std::hash<FuncCallKey>{}(key.result_instance.key));
        return h;
    }
};

#endif
