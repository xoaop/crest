#include "path.hpp"

#include <filesystem>

// ! TODO: AI GENERATE
xpString normalize_path(xpString path, xpAllocator allocator) {
    
    // 归一化
    std::filesystem::path p{std::string(path.c_str, (size_t)path.length)};
    std::filesystem::path normalized_path = p.lexically_normal();

    // 转为通用格式
    auto generic = normalized_path.generic_string();

    return xp_make_string_capacity(allocator, generic.c_str(), (isize)generic.length());
}


// ! TODO: AI GENERATE
Array<xpString> scan_crest_files(const char *dir_path, xpAllocator allocator) {
    Array<xpString> crest_files = make_array<xpString>(allocator);

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".crest") {
            xpString file_path = xp_make_string_capacity(allocator, entry.path().string().c_str(), (isize)entry.path().string().length());
            array_push_back(&crest_files, file_path);
        }
    }

    return crest_files;
}

// ! TODO: AI GENERATE
xpString concat_path(xpString base_path, xpString relative_path, xpAllocator allocator) {
    std::filesystem::path base{std::string(base_path.c_str, (size_t)base_path.length)};
    std::filesystem::path relative{std::string(relative_path.c_str, (size_t)relative_path.length)};

    std::filesystem::path combined = base.parent_path() / relative;
    std::filesystem::path normalized = combined.lexically_normal();

    auto generic = normalized.generic_string();

    return xp_make_string_capacity(allocator, generic.c_str(), (isize)generic.length());
}