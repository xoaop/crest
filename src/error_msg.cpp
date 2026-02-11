#include <cstdarg>

#include "error_msg.hpp"

void error_msg(Token *token, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if(token != NULL) {
        printf("Error at %s:%td:%td: ", token->file_path.c_str, token->line_index, token->column_index);
    }
    vprintf(fmt, args);
    printf("\n");

    va_end(args);
}




void ErrorReporter::report(ErrorLevel level, Span highlight_span, SourceCode src_code, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    ErrorMsg msg;
    msg.level = level;
    msg.highlight_span = highlight_span;
    msg.src_code = src_code;

    // 格式化错误信息
    char buffer[1024];
    isize count = vsnprintf(buffer, sizeof(buffer), fmt, args);
    msg.msg = xp_make_string_capacity(permanent_allocator(), buffer, count);

    array_push_back(&error_msgs, msg);

    va_end(args);

    return;
}

void ErrorReporter::report_error(Span highlight_span, SourceCode src_code, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    defer(va_end(args));

    report(ErrorLevel::Error, highlight_span, src_code, fmt, args);
}

void ErrorReporter::print_msg() {
    #define COLOR_RESET "\033[0m"
    #define COLOR_RED "\033[31m"
    #define COLOR_GREEN "\033[32m"
    #define COLOR_BLUE "\033[34m"

    
    for (size_t i = 0; i < error_msgs.count; ++i) {
        ErrorMsg *msg = &error_msgs[i];
        const char *level_str = "";
        const char *color_code = "";
        switch (msg->level) {
            case ErrorLevel::Error:
                level_str = "Error";
                color_code = COLOR_RED;
                break;
            case ErrorLevel::Warning:
                level_str = "Warning";
                color_code = COLOR_BLUE;
                break;
        }


        auto start = cal_line_column_index_of_byte_pos(msg->src_code, msg->highlight_span.start);
        auto end = cal_line_column_index_of_byte_pos(msg->src_code, msg->highlight_span.end);

        fprintf(stderr, "%s:%lld:%lld: %s%s%s: %s\n",
            msg->src_code.file_path.c_str,
            start.first, start.second,
            color_code,
            level_str,
            COLOR_RESET,
            msg->msg.c_str
        );


        xpString line_str = get_line_str_of_pos(msg->src_code, msg->highlight_span.start, xp_heap_allocator());
        defer(xp_string_free(line_str));

        fprintf(stderr, "%s", line_str.c_str);

        fprintf(stderr, "\n");

        for(isize i = 0; i < start.second - 1; i++) {
            fprintf(stderr, " ");
        }
        for(isize i = 0; i < end.second - start.second; i++) {
            fprintf(stderr, "%s^%s", COLOR_GREEN, COLOR_RESET);
        }
        fprintf(stderr, "\n");


    }
}

ErrorReporter make_error_reporter(xpAllocator allocator) {
    ErrorReporter reporter = {};
    reporter.error_msgs = make_array<ErrorMsg>(allocator);
    return reporter;
}