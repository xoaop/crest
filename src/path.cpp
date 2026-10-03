#include "path.hpp"

#include <string>
#include <filesystem>

// ! TODO: AI GENERATE
xpString normalize_path(xpString path, xpAllocator allocator) {
    const std::u8string normalized = std::filesystem::path(as_u8(path)).lexically_normal().generic_u8string();

    return xp_make_string(allocator, (const char *)normalized.c_str());
}


// ! TODO: AI GENERATE
Array<xpString> scan_crest_files(const char *dir_path, xpAllocator allocator) {
    Array<xpString> crest_files = make_array<xpString>(allocator);

    std::error_code ec;
    const std::filesystem::path dir(as_u8(xp_string_c(dir_path)));
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".cst") {
            crest_files.push_back(xp_make_string(allocator, (const char *)entry.path().generic_u8string().c_str()));
        }
    }

    return crest_files;
}

// ! TODO: AI GENERATE
xpString concat_path(xpString base_path, xpString relative_path, xpAllocator allocator) {
    const std::filesystem::path combined =
        std::filesystem::path(as_u8(base_path)) / std::filesystem::path(as_u8(relative_path));

    return xp_make_string(allocator, (const char *)combined.lexically_normal().generic_u8string().c_str());
}

// ! TODO: AI GENERATE
xpString get_last_component_of_path(xpString path, xpAllocator allocator) {
    const std::u8string filename = std::filesystem::path(as_u8(path)).filename().generic_u8string();

    return xp_make_string(allocator, (const char *)filename.c_str());
}


// ! TODO: AI GENERATE
bool is_file(xpString path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path(as_u8(path)), ec);
}

// ! TODO: AI GENERATE
bool is_directory(xpString path) {
    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::path(as_u8(path)), ec);
}

// ! TODO: AI GENERATE
bool is_path_exists(xpString path) {
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(as_u8(path)), ec);
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
