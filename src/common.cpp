#include "xoaop.h"

#include "common.hpp"
#include "context.hpp"


xpArena permanent_arena;
xpArena temp_arena;
xpArena stage_arena;


xp_global xpAllocator permanent_memory;
xp_global xpAllocator temp_memory;
xp_global xpAllocator stage_memory;

xpAllocator permanent_allocator() {
    return permanent_memory;
}

xpAllocator temp_allocator() {
    return temp_memory;
}

xpAllocator stage_allocator() {
    return stage_memory;
}


void global_allocators_init() {
    xp_arena_init_default(&permanent_arena);
    xp_arena_init_default(&temp_arena);
    xp_arena_init_default(&stage_arena);

    permanent_memory = xp_arena_allocator(&permanent_arena);
    temp_memory = xp_arena_allocator(&temp_arena);
    stage_memory = xp_arena_allocator(&stage_arena);
}


void global_allocators_free() {
    xp_free_all(permanent_memory);
    xp_free_all(temp_memory);
    xp_free_all(stage_memory);
}







xpString rename_ident(xpHashMap<xpString, isize> *identifier_map, xpString ident) {
    isize *exist_count = nullptr;
    if((exist_count = xp_hash_map_get(*identifier_map, ident)) != nullptr) {
        int len = snprintf(nullptr, 0, "%s.%td", ident.c_str, *exist_count);
        char *new_ident = cast(char *)xp_alloc(permanent_allocator(), len + 1);
        snprintf(new_ident, len + 1, "%s.%td", ident.c_str, *exist_count);
        *exist_count += 1;
        return xp_string_c(new_ident);
    } else {
        isize new_count = 0;
        int len = snprintf(nullptr, 0, "%s.%td", ident.c_str, new_count);
        char *new_ident = cast(char *)xp_alloc(permanent_allocator(), len + 1);
        snprintf(new_ident, len + 1, "%s.%td", ident.c_str, new_count);

        xpString new_str = xp_string_c(new_ident);
        new_count +=1 ;
        xp_hash_map_insert(identifier_map, ident, new_count);
        return new_str;
    }
}

isize recursion_depth = 0;
bool recursion_limit_reported = false;

// 阈值可用环境变量覆盖
static isize recursion_limit() {
    static isize v = [] {
        const char *s = getenv("CREST_MAX_RECURSION_DEPTH");
        return s ? (isize)atoll(s) : MAX_RECURSION_DEPTH;
    }();
    return v;
}

RecursionGuard::RecursionGuard() {
    recursion_depth += 1;
    exceeded = recursion_depth > recursion_limit();

    if(exceeded && !recursion_limit_reported) {
        recursion_limit_reported = true;
        context()->reporter.report_error("嵌套层级过深（超过 {} 层），无法继续解析", recursion_limit());
    }
}

RecursionGuard::~RecursionGuard() {
    recursion_depth -= 1;
}
