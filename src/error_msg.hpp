#ifndef CREST_ERROR_MSG_HPP
#define CREST_ERROR_MSG_HPP



#include "xoaop.h"
#include "array.hpp"
#include "span.hpp"
#include "source_code.hpp"
#include "tokenizer.hpp"

void error_msg(Token *token, const char *fmt, ...);


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
    
    void report_args(ErrorLevel level, Span highlight_span, SourceCode src_code, const char *fmt, va_list args);
    void report(ErrorLevel level, Span highlight_span, SourceCode src_code, const char *fmt, ...);
    void report_error(Span highlight_span, SourceCode src_code, const char *fmt, ...);
    
    void print_msg();


    Array<ErrorMsg> error_msgs;


    isize warning_count;
    isize error_count;
};


ErrorReporter make_error_reporter(xpAllocator allocator);


#endif  