#include "target.hpp"

#include <string>
#include <vector>

#include <llvm-c/Core.h>
#include <llvm-c/TargetMachine.h>


static std::vector<std::string> split_dash(const std::string& s) {
    std::vector<std::string> parts;
    size_t start = 0;
    while(start <= s.size()) {
        const size_t dash = s.find('-', start);
        if(dash == std::string::npos) {
            parts.push_back(s.substr(start));
            break;
        }
        parts.push_back(s.substr(start, dash - start));
        start = dash + 1;
    }
    return parts;
}

// triple 分量 → 枚举
static TargetArch parse_arch(const std::string& s) {
    if(s == "x86_64" || s == "amd64") {
        return TargetArch::X86_64;
    }
    if(s == "i386" || s == "i486" || s == "i586" || s == "i686") {
        return TargetArch::X86;
    }
    if(s == "aarch64" || s == "arm64") {
        return TargetArch::AArch64;
    }
    if(s.starts_with("arm") || s.starts_with("thumb")) {
        return TargetArch::Arm;
    }
    return TargetArch::Unknown;
}

static TargetOS parse_os(const std::string& s) {
    if(s == "windows" || s == "win32" || s.starts_with("mingw")) {
        return TargetOS::Windows;
    }
    if(s == "linux" || s == "android") {
        return TargetOS::Linux;
    }
    if(s == "darwin" || s == "macosx" || s == "ios" || s == "tvos" || s == "watchos") {
        return TargetOS::Darwin;
    }
    if(s == "freebsd") {
        return TargetOS::FreeBSD;
    }
    return TargetOS::Unknown;
}

static TargetEnv parse_env(const std::string& s) {
    if(s == "msvc" || s == "itanium") {
        return TargetEnv::MSVC;
    }
    if(s.starts_with("gnu")) {
        return TargetEnv::GNU;
    }
    if(s.starts_with("musl")) {
        return TargetEnv::Musl;
    }
    return TargetEnv::Unknown;
}

// CLI 词表 → 枚举
static bool parse_arch_name(const std::string& s, TargetArch& out) {
    if(s == "x86_64")  { out = TargetArch::X86_64;  return true; }
    if(s == "x86")     { out = TargetArch::X86;     return true; }
    if(s == "aarch64") { out = TargetArch::AArch64; return true; }
    if(s == "arm")     { out = TargetArch::Arm;     return true; }
    return false;
}

static bool parse_os_name(const std::string& s, TargetOS& out) {
    if(s == "windows") { out = TargetOS::Windows; return true; }
    if(s == "linux")   { out = TargetOS::Linux;   return true; }
    if(s == "darwin")  { out = TargetOS::Darwin;  return true; }
    if(s == "freebsd") { out = TargetOS::FreeBSD; return true; }
    return false;
}

static bool parse_env_name(const std::string& s, TargetEnv& out) {
    if(s == "msvc") { out = TargetEnv::MSVC; return true; }
    if(s == "gnu")  { out = TargetEnv::GNU;  return true; }
    if(s == "musl") { out = TargetEnv::Musl; return true; }
    return false;
}

// 枚举 → triple 分量
static const char* triple_arch(TargetArch arch) {
    switch(arch) {
        case TargetArch::X86_64:  return "x86_64";
        case TargetArch::X86:     return "i686";
        case TargetArch::AArch64: return "aarch64";
        case TargetArch::Arm:     return "armv7";
        case TargetArch::Unknown: break;
    }
    return "";
}

static const char* triple_os(TargetOS os) {
    switch(os) {
        case TargetOS::Windows:  return "windows";
        case TargetOS::Linux:    return "linux";
        case TargetOS::Darwin:   return "darwin";
        case TargetOS::FreeBSD:  return "freebsd";
        case TargetOS::Unknown:  break;
    }
    return "";
}

static const char* triple_env(TargetEnv env) {
    switch(env) {
        case TargetEnv::MSVC:    return "msvc";
        case TargetEnv::GNU:     return "gnu";
        case TargetEnv::Musl:    return "musl";
        case TargetEnv::Unknown: break;
    }
    return "";
}

const char* target_arch_name(TargetArch arch) {
    switch(arch) {
        case TargetArch::X86_64:  return "x86_64";
        case TargetArch::X86:     return "x86";
        case TargetArch::AArch64: return "aarch64";
        case TargetArch::Arm:     return "arm";
        case TargetArch::Unknown: break;
    }
    return "unknown";
}

const char* target_os_name(TargetOS os) {
    switch(os) {
        case TargetOS::Windows:  return "windows";
        case TargetOS::Linux:    return "linux";
        case TargetOS::Darwin:   return "darwin";
        case TargetOS::FreeBSD:  return "freebsd";
        case TargetOS::Unknown:  break;
    }
    return "unknown";
}

const char* target_env_name(TargetEnv env) {
    switch(env) {
        case TargetEnv::MSVC:    return "msvc";
        case TargetEnv::GNU:     return "gnu";
        case TargetEnv::Musl:    return "musl";
        case TargetEnv::Unknown: break;
    }
    return "unknown";
}

TargetInfo parse_target_triple(const char* triple) {
    TargetInfo info;
    if(!triple) {
        return info;
    }

    char *normalized = LLVMNormalizeTargetTriple(triple);
    const std::string t = normalized ? normalized : triple;
    if(normalized) {
        LLVMDisposeMessage(normalized);
    }

    const std::vector<std::string> parts = split_dash(t);
    if(parts.size() > 0) {
        info.arch = parse_arch(parts[0]);
    }
    if(parts.size() > 2) {
        info.os = parse_os(parts[2]);
    }
    if(parts.size() > 3) {
        info.env = parse_env(parts[3]);
    }
    // os 段写作 mingw32 时隐含 gnu 环境
    if(info.os == TargetOS::Windows && info.env == TargetEnv::Unknown && parts.size() > 2
       && parts[2].starts_with("mingw")) {
        info.env = TargetEnv::GNU;
    }
    return info;
}

bool parse_target_spec(const char* spec, TargetInfo& out) {
    const std::vector<std::string> parts = split_dash(spec ? spec : "");

    TargetInfo info;
    if(parts.size() != 3
       || !parse_arch_name(parts[0], info.arch)
       || !parse_os_name(parts[1], info.os)
       || !parse_env_name(parts[2], info.env)) {
        return false;
    }
    out = info;
    return true;
}

std::string make_target_triple(const TargetInfo& info) {
    const char* arch = triple_arch(info.arch);
    const char* os = triple_os(info.os);
    if(!*arch || !*os) {
        return "";
    }

    const char* vendor = "unknown";
    if(info.os == TargetOS::Windows) {
        vendor = "pc";
    } else if(info.os == TargetOS::Darwin) {
        vendor = "apple";
    }

    std::string triple = std::string(arch) + "-" + vendor + "-" + os;
    if(const char* env = triple_env(info.env); *env) {
        triple += "-";
        triple += env;
    }
    return triple;
}
