#ifndef CREST_ERROR_MSG_HPP
#define CREST_ERROR_MSG_HPP

#include <format>

#include "xoaop.h"
#include "array.hpp"
#include "span.hpp"
#include "source_code.hpp"
#include "tokenizer.hpp"


enum class ErrorLevel {
    Warning,
    Error,
};


struct ErrorMsg {
    ErrorLevel level;
    Span highlight_span;
    xpString msg;
    SourceCode src_code;
};



struct ErrorReporter {
private:
    void add_error_msg(ErrorLevel level, Span highlight_span, SourceCode src_code, xpString formatted_msg);

    template <typename... Args>
    void report(ErrorLevel level, Span highlight_span, SourceCode src_code, std::format_string<Args...> fmt, Args&&... args) {
        std::string formatted = std::format(fmt, std::forward<Args>(args)...);
        add_error_msg(level, highlight_span, src_code,
            xp_make_string_capacity(permanent_allocator(), formatted.data(), formatted.size()));
    }

    template <typename... Args>
    void report_error(Span highlight_span, SourceCode src_code, std::format_string<Args...> fmt, Args&&... args) {
        report(ErrorLevel::Error, highlight_span, src_code, std::move(fmt), std::forward<Args>(args)...);
    }

public:

    template <typename... Args>
    void report(ErrorLevel level, SourceLocation src_loc, std::format_string<Args...> fmt, Args&&... args) {
        report(level, src_loc.span, src_loc.src_code, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void report_error(SourceLocation src_loc, std::format_string<Args...> fmt, Args&&... args) {
        report(ErrorLevel::Error, src_loc.span, src_loc.src_code, fmt, std::forward<Args>(args)...);
    }

    void print_msg();

    Array<ErrorMsg> error_msgs;
    isize warning_count;
    isize error_count;
};


ErrorReporter make_error_reporter(xpAllocator allocator);


#if defined(CREST_DEBUG)
    #include <print>
    #include <source_location>

    template <typename... Args>
    inline void debug_impl(
        const char *tag,
        std::source_location loc,
        std::format_string<Args...> fmt, Args&&... args
    ) {
        std::println(stderr, "[{}] {}:{}: {}",
            tag, loc.file_name(), loc.line(),
            std::format(fmt, std::forward<Args>(args)...));
    }

    #define DEBUG_LOG(fmt, ...)  \
        ::debug_impl("DEBUG", std::source_location::current(), fmt, ##__VA_ARGS__)
    #define DEBUG_TRACE(fmt, ...) \
        ::debug_impl("TRACE", std::source_location::current(), fmt, ##__VA_ARGS__)

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


#endif  