#pragma once

// 统一输出层：业务代码不直接碰 std::print/printf，统一经此选流向 + 调试门控
//
// 流向约定（对齐主流编译器）：
//   print_out/println_out = stdout —— 正常结果（帮助、构建产物路径等）
//   print_err/println_err = stderr —— 一切诊断（错误/警告）与调试输出
//
// 编译期门控（Release 不定义 CREST_DEBUG → 全部编译掉）：
//   DEBUG_LOG   —— 调试构建总是显示（仅 CREST_DEBUG 门控）
//   DEBUG_TRACE —— 调试构建 + -trace 才显示（另受 g_trace_enabled 门控）
//   DEBUG_PANIC/ASSERT_MSG/ASSERT —— 调试构建总是生效，失败直接中止

#include <print>
#include <format>


template <typename... Args>
void print_out(std::format_string<Args...> fmt, Args&&... args) {
    std::print(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void println_out(std::format_string<Args...> fmt, Args&&... args) {
    std::println(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void print_err(std::format_string<Args...> fmt, Args&&... args) {
    std::print(stderr, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void println_err(std::format_string<Args...> fmt, Args&&... args) {
    std::println(stderr, fmt, std::forward<Args>(args)...);
}


#if defined(CREST_DEBUG)
    #include <source_location>
    #include <cstdlib>

    inline bool g_trace_enabled = false;

    template <typename... Args>
    inline void debug_impl(
        const char *tag,
        std::source_location loc,
        std::format_string<Args...> fmt, Args&&... args
    ) {
        println_err("[{}] {}:{}: {}",
            tag, loc.file_name(), loc.line(),
            std::format(fmt, std::forward<Args>(args)...));
    }

    #define DEBUG_LOG(fmt, ...)  \
        do { ::debug_impl("DEBUG", std::source_location::current(), fmt, ##__VA_ARGS__); } while(0)
    #define DEBUG_TRACE(fmt, ...) \
        do { if(::g_trace_enabled) ::debug_impl("TRACE", std::source_location::current(), fmt, ##__VA_ARGS__); } while(0)

    #define DEBUG_PANIC(fmt, ...) \
        do { \
            ::debug_impl("PANIC", std::source_location::current(), fmt, ##__VA_ARGS__); \
            std::abort(); \
        } while(0)

    #define ASSERT_MSG(cond, fmt, ...) \
        do { \
            if (!(cond)) { \
                ::debug_impl("ASSERT_MSG", std::source_location::current(), fmt, ##__VA_ARGS__); \
                std::abort(); \
            } \
        } while(0)

    #define ASSERT(cond) \
        do { \
            if (!(cond)) { \
                ::debug_impl("ASSERT", std::source_location::current(), "Assertion failed: {}", #cond); \
                std::abort(); \
            } \
        } while(0)

#else
    #define DEBUG_LOG(...)    ((void)0)
    #define DEBUG_TRACE(...)  ((void)0)
    #define DEBUG_PANIC(...)  ((void)0)
    #define ASSERT_MSG(cond, fmt, ...) ((void)0)
    #define ASSERT(cond) ((void)0)
#endif
