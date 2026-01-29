#ifndef CREST_PATH_CPP
#define CREST_PATH_CPP

#include "xoaop.h"
#include "array.hpp"

xpString normalize_path(xpString path, xpAllocator allocator);
Array<xpString> scan_crest_files(const char *dir_path, xpAllocator allocator);
xpString concat_path(xpString base_path, xpString relative_path, xpAllocator allocator);


#endif