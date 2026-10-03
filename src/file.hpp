#pragma once

#include <string>
#include <vector>

#include "print.hpp"
#include "xoaop.h"
#include "path.hpp"

std::string get_program_path();

// 命令行参数统一成 UTF-8（Windows 下 argv 走 ANSI 代码页，非 ASCII 会错）
std::vector<std::string> get_utf8_args(int argc, char** argv);

// 环境变量值统一成 UTF-8（同上，getenv 也是 ANSI 代码页）
std::string get_utf8_env(const char *name);


xp_internal xpString file_to_string(char const *path, xpAllocator allocator) {
    FILE *file = nullptr;

#ifdef _WIN32
    file = _wfopen(std::filesystem::path(as_u8(xp_string_c(path))).c_str(), L"rb");
#else
    file = fopen(path, "rb");
#endif
    if(file == nullptr) {
        println_err("Read File Failed: {}", path);
    }

    fseek(file, 0, SEEK_END);
    isize size = ftell(file);
    rewind(file);

    xpString str = xp_make_string_capacity(allocator, nullptr, size);
    fread(str.c_str, 1, size, file);

    str.length = xp_strlen_c(str.c_str);

    fclose(file);

    return str;
}
