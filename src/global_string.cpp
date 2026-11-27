#include "global_string.hpp"

GlobalStringIndex get_or_insert_global_string(GlobalStringArray& global_strings, const char *str, size_t count, xpAllocator allocator) {
    const auto key = xp_make_string_count(allocator, str, static_cast<isize>(count));

    auto it = global_strings.string_index_map.find(key);
    if(it != global_strings.string_index_map.end()) {
        xp_string_free(key);
        return it->second;
    }

    global_strings.strings.push_back(key);
    global_strings.string_index_map[key] = global_strings.strings.size() - 1;

    return global_strings.strings.size() - 1;
}

GlobalStringIndex get_or_insert_global_string(GlobalStringArray& global_strings, const char *str, xpAllocator allocator) {
    return get_or_insert_global_string(global_strings, str, strlen(str), allocator);
}


GlobalStringIndex get_or_insert_global_string(GlobalStringArray& global_strings, const xpString& str, xpAllocator allocator) {
    return get_or_insert_global_string(global_strings, str, static_cast<size_t>(xp_string_count(str)), allocator);
}

const xpString get_global_string(const GlobalStringArray& global_strings, GlobalStringIndex index) {
    return global_strings.strings[index];
}

KnownGlobalStrings make_known_global_strings(GlobalStringArray& global_strings, xpAllocator allocator) {
    KnownGlobalStrings known;

    known.builtin = get_or_insert_global_string(global_strings, "builtin", allocator);
    known.align = get_or_insert_global_string(global_strings, "align", allocator);
    known.under_is_file_scope = get_or_insert_global_string(global_strings, "under_is_file_scope", allocator);
    known.type = get_or_insert_global_string(global_strings, "type", allocator);
    known.string = get_or_insert_global_string(global_strings, "string", allocator);
    known.main = get_or_insert_global_string(global_strings, "main", allocator);
    known.data = get_or_insert_global_string(global_strings, "data", allocator);
    known.count = get_or_insert_global_string(global_strings, "count", allocator);
    known.typeof_builtin = get_or_insert_global_string(global_strings, "typeof", allocator);
    known.sizeof_builtin = get_or_insert_global_string(global_strings, "sizeof", allocator);
    known.field_count_builtin = get_or_insert_global_string(global_strings, "field_count", allocator);
    known.get_field_builtin = get_or_insert_global_string(global_strings, "get_field", allocator);
    known.compile_error_builtin = get_or_insert_global_string(global_strings, "compile_error", allocator);
    known.empty = get_or_insert_global_string(global_strings, "", allocator);

    return known;
}