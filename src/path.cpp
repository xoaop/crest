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

    for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".cst") {
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

    std::filesystem::path combined = base / relative;
    std::filesystem::path normalized = combined.lexically_normal();

    auto generic = normalized.generic_string();

    return xp_make_string_capacity(allocator, generic.c_str(), (isize)generic.length());
}

// ! TODO: AI GENERATE
xpString get_last_component_of_path(xpString path, xpAllocator allocator) {
    std::filesystem::path p{std::string(path.c_str, (size_t)path.length)};
    auto filename = p.filename().string();

    return xp_make_string_capacity(allocator, filename.c_str(), (isize)filename.length());
}


// ! TODO: AI GENERATE
bool is_file(xpString path) {
    std::filesystem::path p{std::string(path.c_str, (size_t)path.length)};
    return std::filesystem::is_regular_file(p);
}

// ! TODO: AI GENERATE
bool is_directory(xpString path) {
    std::filesystem::path p{std::string(path.c_str, (size_t)path.length)};
    return std::filesystem::is_directory(p);
}

// ! TODO: AI GENERATE
bool is_path_exists(xpString path) {
    std::filesystem::path p{std::string(path.c_str, (size_t)path.length)};
    return std::filesystem::exists(p);
}

// ! TODO: AI GENERATE
bool is_existing_file(xpString path) {
    return is_path_exists(path) && is_file(path);
}

// ! TODO: AI GENERATE
bool is_existing_directory(xpString path) {
    return is_path_exists(path) && is_directory(path);
}




Path::Path(xpString raw_path_str, xpAllocator allocator) {
    
}


Path::~Path() {

}