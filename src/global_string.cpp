#include "global_string.hpp"

GlobalStringIndex get_or_insert_global_string(GlobalStringArray& global_strings, const char *str, size_t count, xpAllocator allocator) {
    auto it = global_strings.string_index_map.find(StringRef::init(str, count));
    
    if (it != global_strings.string_index_map.end()) {
        return it->second;
    } else {
        global_strings.strings.push_back(xp_make_string_count(allocator, str, static_cast<isize>(count)));

        const auto& new_string = global_strings.strings.back();
        auto new_string_ref = StringRef::init(new_string);
        GlobalStringIndex index = global_strings.strings.size() - 1;

        global_strings.string_index_map[new_string_ref] = index;
        
        return index;
    }
}

GlobalStringIndex get_or_insert_global_string(GlobalStringArray& global_strings, const char *str, xpAllocator allocator) {
    return get_or_insert_global_string(global_strings, str, strlen(str), allocator);
}


GlobalStringIndex get_or_insert_global_string(GlobalStringArray& global_strings, const xpString& str, xpAllocator allocator) {
    return get_or_insert_global_string(global_strings, str.c_str, static_cast<size_t>(str.length), allocator);
}

StringRef get_global_string(const GlobalStringArray& global_strings, GlobalStringIndex index) {
    return StringRef::init(global_strings.strings[index]);
}

size_t global_string_count(const GlobalStringArray& global_strings) {
    return global_strings.strings.size();
}

KnownGlobalStrings make_known_global_strings(GlobalStringArray& global_strings, xpAllocator allocator) {
    KnownGlobalStrings known;

    known.builtin = get_or_insert_global_string(global_strings, "builtin", allocator);
    known.under_is_file_scope = get_or_insert_global_string(global_strings, "under_is_file_scope", allocator);
    known.type = get_or_insert_global_string(global_strings, "type", allocator);
    known.string = get_or_insert_global_string(global_strings, "string", allocator);
    known.main = get_or_insert_global_string(global_strings, "main", allocator);
    known.data = get_or_insert_global_string(global_strings, "data", allocator);
    known.count = get_or_insert_global_string(global_strings, "count", allocator);
    known.typeof_builtin = get_or_insert_global_string(global_strings, "typeof", allocator);
    known.sizeof_builtin = get_or_insert_global_string(global_strings, "sizeof", allocator);
    known.empty = get_or_insert_global_string(global_strings, "", allocator);

    return known;
}