#pragma once

#include "array.hpp"
#include <string>

enum class LinkCrt {
    Static,
    Dynamic,
    StaticDebug,
    DynamicDebug,
};

enum class LinkSubsystem {
    Console,
    Windows,
};

// 一次链接请求：把一组 .o 链成一个可执行文件
struct LinkRequest {
    Array<xpString> obj_paths;          // gen_ir_all_packages 产出的 .o 路径
    std::string     output_binary_path;
};

bool parse_crt_arg(const char* s, LinkCrt& out);
bool parse_subsystem_arg(const char* s, LinkSubsystem& out);

// 目标是否走 lld-link（-crt / -subsystem 只对它有意义）
bool target_uses_msvc();

// 目标是否是 Windows 系（cc 分支下 -subsystem 靠它决定 -mwindows）
bool target_is_windows();

bool link_objects(const LinkRequest& req);

std::string target_exe_suffix();
