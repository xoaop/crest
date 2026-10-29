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

enum class LinkOutputKind {
    Executable,
    SharedLibrary,
    StaticLibrary,
};

// 一次链接请求：把一组 .o 链成可执行文件 / 共享库，或归档成静态库
struct LinkRequest {
    Array<xpString> obj_paths;          // gen_ir_all_packages 产出的 .o 路径
    std::string     output_binary_path;
    LinkOutputKind  kind = LinkOutputKind::Executable;
    std::string     import_lib_path;    // Windows 共享库的导入库；空串表示不生成
};

bool parse_crt_arg(const char* s, LinkCrt& out);
bool parse_subsystem_arg(const char* s, LinkSubsystem& out);

// 目标是否走 lld-link（-crt / -subsystem 只对它有意义）
bool target_uses_msvc();

// 目标是否是 Windows 系（cc 分支下 -subsystem 靠它决定 -mwindows）
bool target_is_windows();

bool link_objects(const LinkRequest& req);

std::string target_exe_suffix();

// 库文件名遵循各平台约定：libfoo.a / foo.lib、libfoo.so / libfoo.dylib / foo.dll
std::string target_static_lib_name(const std::string& stem);
std::string target_shared_lib_name(const std::string& stem);
// Windows 共享库的导入库；非 Windows 返回空串
std::string target_import_lib_name(const std::string& stem);
