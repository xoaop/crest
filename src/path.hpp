#ifndef CREST_PATH_CPP
#define CREST_PATH_CPP

#include "xoaop.h"
#include "array.hpp"

#include <filesystem>

xpString normalize_path(xpString path, xpAllocator allocator);
Array<xpString> scan_crest_files(const char *dir_path, xpAllocator allocator);
xpString concat_path(xpString base_path, xpString relative_path, xpAllocator allocator);

xpString get_last_component_of_path(xpString path, xpAllocator allocator);

bool is_file(xpString path);

bool is_directory(xpString path);

bool is_path_exists(xpString path);

bool is_existing_file(xpString path);

bool is_existing_directory(xpString path);

std::filesystem::path to_path(xpString path);


struct Path {

    Path(xpString raw_path_str, xpAllocator allocator);
    ~Path();

private:
    std::filesystem::path internal_path;

};





#endif