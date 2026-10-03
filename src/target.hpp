#pragma once

#include <string>

enum class TargetArch {
    X86_64,
    X86,
    AArch64,
    Arm,
    Unknown
};

enum class TargetOS {
    Windows,
    Linux,
    Darwin,
    FreeBSD,
    Unknown
};

enum class TargetEnv {
    MSVC,
    GNU,
    Musl,
    Unknown
};

struct TargetInfo {
    TargetArch arch = TargetArch::Unknown;
    TargetOS   os   = TargetOS::Unknown;
    TargetEnv  env  = TargetEnv::Unknown;
};

// 归一化 triple 后按 arch / vendor / os / env 分量解析
TargetInfo parse_target_triple(const char* triple);

// "-target" 的 <arch>-<os>-<env> 三段；任一段不在词表里返回 false
bool parse_target_spec(const char* spec, TargetInfo& out);

// 枚举 → LLVM triple（arch 和 os 未知时返回空串）
std::string make_target_triple(const TargetInfo& info);

// 枚举 → CLI 词表里的名字
const char* target_arch_name(TargetArch arch);
const char* target_os_name(TargetOS os);
const char* target_env_name(TargetEnv env);
