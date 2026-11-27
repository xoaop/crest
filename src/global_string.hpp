#pragma once

#include <vector>
#include <unordered_map>

#include "xoaop.h"

using GlobalStringIndex = size_t;

constexpr GlobalStringIndex GLOBAL_STRING_NONE = static_cast<GlobalStringIndex>(-1);

struct GlobalStringArray {
    std::vector<xpString> strings;
    std::unordered_map<xpString, GlobalStringIndex> string_index_map;
};



GlobalStringIndex get_or_insert_global_string(GlobalStringArray& global_strings, const char *str, xpAllocator allocator);
GlobalStringIndex get_or_insert_global_string(GlobalStringArray& global_strings, const char *str, size_t count, xpAllocator allocator);
GlobalStringIndex get_or_insert_global_string(GlobalStringArray& global_strings, const xpString& str, xpAllocator allocator);

const xpString get_global_string(const GlobalStringArray& global_strings, GlobalStringIndex index);


struct KnownGlobalStrings {
    GlobalStringIndex builtin;
    GlobalStringIndex align;
    GlobalStringIndex under_is_file_scope;
    GlobalStringIndex type;
    GlobalStringIndex string;
    GlobalStringIndex main;
    GlobalStringIndex data;
    GlobalStringIndex count;
    GlobalStringIndex typeof_builtin;
    GlobalStringIndex sizeof_builtin;
    GlobalStringIndex field_count_builtin;
    GlobalStringIndex get_field_builtin;
    GlobalStringIndex compile_error_builtin;
    GlobalStringIndex empty;
};

KnownGlobalStrings make_known_global_strings(GlobalStringArray& global_strings, xpAllocator allocator);


// 从 context 上的全局字符串表取文本
const xpString get_global_string(GlobalStringIndex index);