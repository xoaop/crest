#pragma once

#include <format>
#include <meta>
#include <string>
#include <type_traits>

#include "xoaop.h"
#include "ref.hpp"

struct Package;
struct Scope;
struct Ast;


//
// Memory Allocator
//
xpAllocator permanent_allocator();
xpAllocator temp_allocator();

// @deprecated: 现在只有 llvm backend 使用了, 后面换成Package的stage_allocator即可
xpAllocator stage_allocator();

void global_allocators_init();
void global_allocators_free();



// 递归深度守卫
constexpr isize MAX_RECURSION_DEPTH = 100;

extern isize recursion_depth;
extern bool recursion_limit_reported;

struct RecursionGuard {
    bool exceeded;

    RecursionGuard();
    ~RecursionGuard();
};



template<typename T> requires std::is_enum_v<T>
const char *to_string(const T value) {
    constexpr static auto enums = std::define_static_array(std::meta::enumerators_of(^^T));
    template for(constexpr auto& e: enums) {
        if(value == [:e:]) {
            return std::meta::identifier_of(e).data();
        }
    }

    UNREACHABLE();
    return nullptr;
}
