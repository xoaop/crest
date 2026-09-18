#include "stacktrace.hpp"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>

#include <format>
#include <mutex>

std::vector<std::string> xp_capture_stacktrace(int skip) {
    std::vector<std::string> frames;

    constexpr int MAX_FRAMES = 128;
    void *addrs[MAX_FRAMES];
    USHORT count = CaptureStackBackTrace((DWORD)skip, MAX_FRAMES, addrs, nullptr);

    // 符号处理器每进程只初始化一次；deferred 让它按需从 exe 旁边的 pdb 取符号
    static std::once_flag sym_init;
    std::call_once(sym_init, [] {
        HANDLE process = GetCurrentProcess();
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        SymInitialize(process, nullptr, TRUE);
    });

    HANDLE process = GetCurrentProcess();

    char buf[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO *sym = reinterpret_cast<SYMBOL_INFO *>(buf);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;

    for(USHORT i = 0; i < count; i++) {
        DWORD64 addr = reinterpret_cast<DWORD64>(addrs[i]);
        DWORD64 disp = 0;

        if(!SymFromAddr(process, addr, &disp, sym)) {
            frames.push_back(std::format("0x{:016x}", addr));
            continue;
        }

        std::string frame = sym->Name;

        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD line_disp = 0;
        if(SymGetLineFromAddr64(process, addr, &line_disp, &line)) {
            frame += std::format(" ({}:{})", line.FileName, line.LineNumber);
        }

        frames.push_back(std::move(frame));
    }

    return frames;
}

#else

#include <cxxabi.h>
#include <execinfo.h>
#include <cstdlib>

// "exe(_Z3foov+0x12) [0x...]" 里的符号段解成可读名字；解不开就原样留着
static std::string demangle_in(std::string raw) {
    std::size_t open = raw.find('(');
    std::size_t plus = raw.find('+', open == std::string::npos ? 0 : open);
    if(open == std::string::npos || plus == std::string::npos || plus <= open + 1) {
        return raw;
    }

    std::string mangled = raw.substr(open + 1, plus - open - 1);
    int status = 0;
    char *demangled = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
    if(demangled == nullptr) {
        return raw;
    }

    std::string result = raw.substr(0, open + 1) + demangled + raw.substr(plus);
    std::free(demangled);
    return result;
}

std::vector<std::string> xp_capture_stacktrace(int skip) {
    std::vector<std::string> frames;

    constexpr int MAX_FRAMES = 128;
    void *addrs[MAX_FRAMES];
    int count = backtrace(addrs, MAX_FRAMES);
    char **syms = backtrace_symbols(addrs, count);

    for(int i = skip; i < count; i++) {
        frames.push_back(syms ? demangle_in(syms[i]) : std::string("?"));
    }

    std::free(syms);
    return frames;
}

#endif
