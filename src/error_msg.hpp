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
    // 内部实现：添加已格式化好的错误消息
    void add_error_msg(ErrorLevel level, Span highlight_span, SourceCode src_code, xpString formatted_msg);

public:
    void report_args(ErrorLevel level, Span highlight_span, SourceCode src_code, const char *fmt, va_list args) {
        std::string formatted = std::vformat(fmt, std::make_format_args(args));
        add_error_msg(level, highlight_span, src_code,
            xp_make_string_capacity(permanent_allocator(), formatted.data(), formatted.size()));
    }

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

    void print_msg();

    Array<ErrorMsg> error_msgs;
    isize warning_count;
    isize error_count;
};


ErrorReporter make_error_reporter(xpAllocator allocator);


#endif  