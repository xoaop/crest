#pragma once

#include <string>

#include "xoaop.h"

std::string get_program_path();


xp_internal xpString file_to_string(char const *path, xpAllocator allocator) {
    FILE *file = NULL;

    if(fopen_s(&file, path, "rb")) {
        printf("Read File Failed: %s\n", path);
    }

    fseek(file, 0, SEEK_END);
    isize size = ftell(file);
    rewind(file);

    xpString str = xp_make_string_capacity(allocator, NULL, size);
    fread(str.c_str, 1, size, file);

    str.length = xp_strlen_c(str.c_str);

    fclose(file);

    return str;
}
