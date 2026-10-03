#include "file.hpp"



#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <memory>
#include <vector>

std::string get_program_path() {
    // 方法1：先尝试固定大小缓冲区（避免 nullptr,0 在某些环境下失败）
    std::vector<wchar_t> buffer(MAX_PATH + 1);
    DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    
    if (len == 0) {
        DWORD err = GetLastError();
        if (err == ERROR_INSUFFICIENT_BUFFER) {
            // 缓冲区太小，需要更大空间：再次调用获取所需大小
            DWORD needed = GetModuleFileNameW(nullptr, nullptr, 0);
            if (needed == 0) {
                // 二次失败，放弃
                return "";
            }
            buffer.resize(needed + 1);
            len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (len == 0) {
                return "";
            }
        } else {
            // 其他错误
            return "";
        }
    }
    
    // 确保以 null 结尾（API 已保证，但预防）
    buffer[len] = L'\0';
    
    // 转换为 UTF-8
    int needed_utf8 = WideCharToMultiByte(CP_UTF8, 0, buffer.data(), len, nullptr, 0, nullptr, nullptr);
    if (needed_utf8 <= 0) return "";
    std::string utf8_path(needed_utf8, '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer.data(), len, &utf8_path[0], needed_utf8, nullptr, nullptr);
    return utf8_path;
}

#elif defined(__linux__)


#include <unistd.h>
#include <limits.h>

std::string get_program_path() {
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len == -1) {
        return "";
    }
    buffer[len] = '\0';
    return std::string(buffer);
}



#elif defined(__APPLE__)


#include <mach-o/dyld.h>
#include <limits.h>
#include <stdlib.h>

std::string get_program_path() {
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) != 0) {
        // 缓冲区不足，size 已更新为所需长度
        // 可以重新分配更大的缓冲区，为简单起见，返回空
        return "";
    }
    // _NSGetExecutablePath 可能返回相对路径或含 ".." 的路径，用 realpath 规范化
    char resolved[PATH_MAX];
    if (realpath(buffer, resolved) == nullptr) {
        return "";
    }
    return std::string(resolved);
}

#else
    #error "Unsupported platform"
#endif


std::vector<std::string> get_utf8_args(int argc, char** argv) {
    std::vector<std::string> args;

#ifdef _WIN32
    int wide_argc = 0;
    wchar_t **wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
    if(!wide_argv) {
        return args;
    }
    for(int i = 0; i < wide_argc; i++) {
        const int needed = WideCharToMultiByte(
            CP_UTF8, 0, wide_argv[i], -1, nullptr, 0, nullptr, nullptr);
        if(needed <= 0) {
            args.push_back("");
            continue;
        }
        std::string utf8((size_t)needed, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, utf8.data(), needed, nullptr, nullptr);
        utf8.resize((size_t)needed - 1);   // 去掉结尾 NUL
        args.push_back(utf8);
    }
    LocalFree(wide_argv);
#else
    for(int i = 0; i < argc; i++) {
        args.push_back(argv[i]);
    }
#endif

    return args;
}


std::string get_utf8_env(const char *name) {
#ifdef _WIN32
    const int name_len = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
    if(name_len <= 0) {
        return "";
    }
    std::wstring wide_name((size_t)name_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name.data(), name_len);

    const wchar_t *wide_value = _wgetenv(wide_name.c_str());
    if(!wide_value) {
        return "";
    }
    const int needed = WideCharToMultiByte(
        CP_UTF8, 0, wide_value, -1, nullptr, 0, nullptr, nullptr);
    if(needed <= 0) {
        return "";
    }
    std::string utf8((size_t)needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide_value, -1, utf8.data(), needed, nullptr, nullptr);
    utf8.resize((size_t)needed - 1);   // 去掉结尾 NUL
    return utf8;
#else
    const char *value = std::getenv(name);
    return value ? std::string(value) : std::string();
#endif
}