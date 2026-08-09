#ifndef CREST_ERROR_MSG_HPP
#define CREST_ERROR_MSG_HPP

#include <format>

#include "print.hpp"
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
    bool has_location = true;   // false = 无行号错误（启动/参数等），print_msg 跳过位置渲染
};



struct ErrorReporter {
private:
    void add_error_msg(ErrorLevel level, bool has_location, Span highlight_span, SourceCode src_code, xpString formatted_msg);

    template <typename... Args>
    void report(ErrorLevel level, Span highlight_span, SourceCode src_code, std::format_string<Args...> fmt, Args&&... args) {
        std::string formatted = std::format(fmt, std::forward<Args>(args)...);
        add_error_msg(level, true, highlight_span, src_code,
            xp_make_string_capacity(permanent_allocator(), formatted.data(), formatted.size()));
    }

    // 无行号版本：启动/参数等没有源码位置可指的错误
    template <typename... Args>
    void report(ErrorLevel level, std::format_string<Args...> fmt, Args&&... args) {
        std::string formatted = std::format(fmt, std::forward<Args>(args)...);
        add_error_msg(level, false, Span{-1, -1}, SourceCode{},
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

    // 无行号错误：打印时跳过 file:line 与源码渲染，只打彩色级别行
    template <typename... Args>
    void report_error(std::format_string<Args...> fmt, Args&&... args) {
        report(ErrorLevel::Error, fmt, std::forward<Args>(args)...);
    }

    void print_msg();

    Array<ErrorMsg> error_msgs;
    isize warning_count;
    isize error_count;
};


ErrorReporter make_error_reporter(xpAllocator allocator);


// 彩色级别标签: Error→红, Warning→蓝（打在 stderr）
void print_colored_level(ErrorLevel level);


// 渲染一行错误消息: [彩色]Level[/reset]: msg（无位置前缀）。
// print_msg（带 file:line:col:）与 err（无位置）共用，Error 红 / Warning 蓝。
template <typename... Args>
void print_error_line(ErrorLevel level, std::format_string<Args...> fmt, Args&&... args) {
    print_colored_level(level);
    println_err(": {}", std::format(fmt, std::forward<Args>(args)...));
}


// 无行号错误：直接打 stderr，不进 reporter（不计数/不汇总）。
// 供启动/参数等没有源码位置可指的错误使用。
template <typename... Args>
void err(std::format_string<Args...> fmt, Args&&... args) {
    print_error_line(ErrorLevel::Error, fmt, std::forward<Args>(args)...);
}


#endif