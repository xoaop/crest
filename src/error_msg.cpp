#include <print>

#include "error_msg.hpp"


void ErrorReporter::add_error_msg(ErrorLevel level, Span highlight_span, SourceCode src_code, xpString formatted_msg) {
    ErrorMsg msg;
    msg.level = level;
    msg.highlight_span = highlight_span;
    msg.src_code = src_code;
    msg.msg = std::move(formatted_msg); // 直接转移，零拷贝

    array_push_back(&error_msgs, msg);

    if(level == ErrorLevel::Error) {
        error_count += 1;
    } else if(level == ErrorLevel::Warning) {
        warning_count += 1;
    }
}

void ErrorReporter::print_msg() {
    #define COLOR_RESET "\033[0m"
    #define COLOR_RED "\033[31m"
    #define COLOR_GREEN "\033[32m"
    #define COLOR_BLUE "\033[34m"

    if(error_msgs.count == 0) {
        return;
    }

    std::println("\n{} error(s), {} warning(s) found:\n", error_count, warning_count);

    for (size_t i = 0; i < error_msgs.count; ++i) {
        ErrorMsg *msg = &error_msgs[i];
        std::string_view level_str = "";
        std::string_view color_code = "";
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

        std::println("{}:{}:{}: {}{}{}: {}",
            msg->src_code.file_path,
            start.first, start.second,
            color_code,
            level_str,
            COLOR_RESET,
            msg->msg
        );


        xpString line_str = get_line_str_of_pos(msg->src_code, msg->highlight_span.start, xp_heap_allocator());
        defer(xp_string_free(line_str));

        std::print("{}", line_str); // 不需要换行，原行字符串已经带换行

        std::println("");

        for(isize i = 0; i < start.second - 1; i++) {
            std::print(" ");
        }
        // 统一开启绿色，输出所有^后再重置，避免冗余控制码
        std::print("{}", COLOR_GREEN);
        for(isize i = 0; i < end.second - start.second; i++) {
            std::print("^");
        }
        std::println("{}", COLOR_RESET);
    }
}

ErrorReporter make_error_reporter(xpAllocator allocator) {
    ErrorReporter reporter = {};
    reporter.error_msgs = make_array<ErrorMsg>(allocator);
    return reporter;
}