#include "link.hpp"

#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#include "context.hpp"
#include "target.hpp"
#include "path.hpp"
#include "file.hpp"
#include "print.hpp"
#include "error_msg.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

bool target_is_windows() {
    return context()->target.os == TargetOS::Windows;
}

bool target_uses_msvc() {
    const TargetInfo& t = context()->target;
    return t.os == TargetOS::Windows && t.env != TargetEnv::GNU;
}

std::string target_exe_suffix() {
    return target_is_windows() ? ".exe" : "";
}

bool parse_crt_arg(const char* s, LinkCrt& out) {
    if(!s) {
        return false;
    }
    if(strcmp(s, "static") == 0) {
        out = LinkCrt::Static;
        return true;
    }
    if(strcmp(s, "dynamic") == 0) {
        out = LinkCrt::Dynamic;
        return true;
    }
    if(strcmp(s, "static-debug") == 0) {
        out = LinkCrt::StaticDebug;
        return true;
    }
    if(strcmp(s, "dynamic-debug") == 0) {
        out = LinkCrt::DynamicDebug;
        return true;
    }
    return false;
}

bool parse_subsystem_arg(const char* s, LinkSubsystem& out) {
    if(!s) {
        return false;
    }
    if(strcmp(s, "console") == 0) {
        out = LinkSubsystem::Console;
        return true;
    }
    if(strcmp(s, "windows") == 0) {
        out = LinkSubsystem::Windows;
        return true;
    }
    return false;
}

#ifdef _WIN32

static std::string run_capture(const std::string& cmd) {
    std::string out;
    FILE* pipe = _popen(cmd.c_str(), "r");
    if(!pipe) {
        return out;
    }
    char buf[512];
    while(fgets(buf, sizeof(buf), pipe)) {
        out += buf;
    }
    _pclose(pipe);
    return out;
}

static void trim_ws(std::string& s) {
    auto is_space = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    };
    while(!s.empty() && is_space((unsigned char)s.back())) {
        s.pop_back();
    }
    size_t i = 0;
    while(i < s.size() && is_space((unsigned char)s[i])) {
        i += 1;
    }
    if(i > 0) {
        s.erase(0, i);
    }
}

static std::string find_vs_install() {
    namespace fs = std::filesystem;

    const std::string pf86 = get_utf8_env("ProgramFiles(x86)");
    if(pf86.empty()) {
        return "";
    }
    const fs::path vswhere = std::filesystem::path(as_u8(pf86)) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
    std::error_code ec;
    if(!fs::is_regular_file(vswhere, ec)) {
        return "";
    }

    std::string cmd = "\"" + vswhere.string() + "\""
        " -latest -products *"
        " -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
        " -property installationPath";
    std::string out = run_capture(cmd);
    trim_ws(out);
    return out;
}

static std::string find_vs_lld_link(const std::string& vs) {
    namespace fs = std::filesystem;
    if(vs.empty()) {
        return "";
    }
    const fs::path p = fs::path(vs) / "VC" / "Tools" / "Llvm" / "x64" / "bin" / "lld-link.exe";
    std::error_code ec;
    return fs::is_regular_file(p, ec) ? p.string() : "";
}

static std::vector<int> version_key(const std::string& s) {
    std::vector<int> key;
    int cur = 0;
    bool in_num = false;
    for(char c : s) {
        if(c >= '0' && c <= '9') {
            cur = cur * 10 + (c - '0');
            in_num = true;
        } else {
            if(in_num) {
                key.push_back(cur);
            }
            cur = 0;
            in_num = false;
        }
    }
    if(in_num) {
        key.push_back(cur);
    }
    return key;
}

// 从 parent 里挑版本号最大的子目录，拼上 tail 后必须是已存在目录
static std::string highest_version_dir(const std::string& parent, const std::filesystem::path& tail) {
    namespace fs = std::filesystem;

    std::error_code ec;
    if(!fs::is_directory(parent, ec)) {
        return "";
    }
    std::vector<int> best_key;
    std::string best_path;
    for(const auto& e : fs::directory_iterator(parent, ec)) {
        if(!e.is_directory()) {
            continue;
        }
        const fs::path full = e.path() / tail;
        if(!fs::is_directory(full, ec)) {
            continue;
        }
        const auto key = version_key(e.path().filename().string());
        if(best_path.empty() || key > best_key) {
            best_key = key;
            best_path = full.string();
        }
    }
    return best_path;
}

static std::string read_sdk_root() {
    const char* subkeys[] = {
        "SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots",
        "SOFTWARE\\WOW6432Node\\Microsoft\\Windows Kits\\Installed Roots",
    };
    for(const char* sk : subkeys) {
        HKEY key = nullptr;
        if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, sk, 0, KEY_READ, &key) != ERROR_SUCCESS) {
            continue;
        }
        char buf[MAX_PATH] = {0};
        DWORD sz = sizeof(buf);
        LONG rc = RegQueryValueExA(key, "KitsRoot10", nullptr, nullptr, (LPBYTE)buf, &sz);
        RegCloseKey(key);
        if(rc != ERROR_SUCCESS) {
            continue;
        }
        std::string s = buf;
        trim_ws(s);
        while(!s.empty() && (s.back() == '\\' || s.back() == '/')) {
            s.pop_back();
        }
        return s;
    }
    return "";
}

// lld-link 优先级：bin/ → CREST_LINKER → VS 自带 → PATH
static std::string find_lld_link(const std::string& vs) {
    namespace fs = std::filesystem;

    {
        const fs::path bin = context()->compiler_path / "bin";
        for(const char* n : {"lld-link.exe", "lld-link"}) {
            const fs::path p = bin / n;
            std::error_code ec;
            if(fs::is_regular_file(p, ec)) {
                return p.string();
            }
        }
    }

    const std::string env = get_utf8_env("CREST_LINKER");
    if(!env.empty()) {
        return env;
    }

    {
        std::string vs_lld = find_vs_lld_link(vs);
        if(!vs_lld.empty()) {
            return vs_lld;
        }
    }

    return "lld-link";   // 兜底：交给 PATH
}

static std::string target_arch() {
    switch(context()->target.arch) {
        case TargetArch::X86_64:  return "x64";
        case TargetArch::X86:     return "x86";
        case TargetArch::AArch64: return "arm64";
        case TargetArch::Arm:     return "arm";
        case TargetArch::Unknown: return "";
    }
    return "";
}

static std::vector<std::string> msvc_lib_dirs(const std::string& vs) {
    namespace fs = std::filesystem;

    std::vector<std::string> dirs;
    const std::string arch = target_arch();
    if(arch.empty()) {
        return dirs;
    }

    if(!vs.empty()) {
        std::string msvc = highest_version_dir(
            (fs::path(vs) / "VC" / "Tools" / "MSVC").string(), fs::path("lib") / arch);
        if(!msvc.empty()) {
            dirs.push_back(msvc);
        }
    }

    std::string sdk = read_sdk_root();
    if(!sdk.empty()) {
        std::string libroot = (fs::path(sdk) / "Lib").string();
        std::string ucrt = highest_version_dir(libroot, fs::path("ucrt") / arch);
        if(!ucrt.empty()) {
            dirs.push_back(ucrt);
        }
        std::string um = highest_version_dir(libroot, fs::path("um") / arch);
        if(!um.empty()) {
            dirs.push_back(um);
        }
    }

    return dirs;
}

static std::string crt_lib_name(LinkCrt crt) {
    switch(crt) {
        case LinkCrt::Dynamic:      return "msvcrt";
        case LinkCrt::StaticDebug:  return "libcmtd";
        case LinkCrt::DynamicDebug: return "msvcrtd";
        case LinkCrt::Static:       break;
    }
    return "libcmt";
}

static std::string subsystem_name(LinkSubsystem sub) {
    return sub == LinkSubsystem::Windows ? "windows" : "console";
}

#endif  // _WIN32

static std::string find_cc_driver() {
    const std::string env = get_utf8_env("CREST_LINKER");
    if(!env.empty()) {
        return env;
    }

#ifdef _WIN32
    const char* probe = "where %s >nul 2>&1";
    const char* names[] = {"g++", "gcc", "clang"};
#else
    const char* probe = "command -v %s >/dev/null 2>&1";
    const char* names[] = {"cc", "gcc", "clang"};
#endif

    for(const char* n : names) {
        char cmd[160];
        snprintf(cmd, sizeof(cmd), probe, n);
        if(std::system(cmd) == 0) {
            return n;
        }
    }
    return "cc";
}

bool link_objects(const LinkRequest& req) {
    if(req.obj_paths.count <= 0) {
        return true;
    }

    std::string cmd;

    if(target_uses_msvc()) {
#ifdef _WIN32
        const std::string vs = find_vs_install();
        std::string lld = find_lld_link(vs);
        const std::string lld_arg = lld.contains(' ') ? ("\"" + lld + "\"") : lld;

        cmd = lld_arg + " /nologo /out:\"" + req.output_binary_path + "\"";
        for(const auto& obj : req.obj_paths) {
            cmd += " \"" + std::string(obj.c_str) + "\"";
        }
        cmd += " /defaultlib:" + crt_lib_name(context()->link_crt)
             + " /subsystem:" + subsystem_name(context()->link_subsystem);
        for(const auto& d : msvc_lib_dirs(vs)) {
            cmd += " /libpath:\"" + d + "\"";
        }
        println_out("[link] linker = {}", lld);
#else
        err("linking an MSVC target requires Windows (no lld-link driver here)");
        return false;
#endif
    } else {
        std::string cc = find_cc_driver();
        const std::string cc_arg = cc.contains(' ') ? ("\"" + cc + "\"") : cc;

        cmd = cc_arg;
        for(const auto& obj : req.obj_paths) {
            cmd += " \"" + std::string(obj.c_str) + "\"";
        }
        cmd += " -o \"" + req.output_binary_path + "\"";
        if(target_is_windows() && context()->link_subsystem == LinkSubsystem::Windows) {
            cmd += " -mwindows";
        }
        println_out("[link] linker = {}", cc);
    }

    println_out("[link] {}", cmd);

    // 外层再套一对引号：cmd /c 会剥掉首尾引号并在第一个空格处截断
    std::string sys_cmd = cmd;
#ifdef _WIN32
    if(!sys_cmd.empty() && sys_cmd.front() == '"') {
        sys_cmd = "\"" + sys_cmd + "\"";
    }
#endif

    int rc = std::system(sys_cmd.c_str());
    if(rc != 0) {
        err("linking failed: linker exited with code {}", rc);
        return false;
    }
    return true;
}
