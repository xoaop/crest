#ifndef CREST_COMMON_H
#define CREST_COMMON_H

#include "xoaop.h"
#include "ref.hpp"

struct Package;
struct Scope;

Package *try_access_val(const Ref<Package> &r);
Scope *try_access_val(const Ref<Scope> &r);


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



#endif