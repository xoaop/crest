#pragma once

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

    bool is_block() const { 
        return inst_index == INVALID_INST_INDEX;
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

    // $T 实例: comptime_arg_refs 按其结果的类型去重, 而非按指令身份 ——
    // sum_down(3i32) 和递归里的 sum_down(n-1i32) 是不同指令, 必须落同一实例。
    // 也让泛型实例与同一函数的编译期调用实例不互相命中。
    bool                                     is_generic_instance = false;

    u64 hash() const;
    bool operator==(const FuncCallKey& other) const;
};

template<>
struct std::hash<FuncCallKey> {
    usize operator()(const FuncCallKey& key) const {
        return key.hash();
    }
};


// Ref 特化依赖 CIRInstructionRef，放在文件末尾避免循环
#include "cir_result_instance_ref.hpp"
#include "cir_inst_result_ref.hpp"
