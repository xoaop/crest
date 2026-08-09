#include "print.hpp"

#include "error_msg.hpp"


void ErrorReporter::add_error_msg(ErrorLevel level, bool has_location, Span highlight_span, SourceCode src_code, xpString formatted_msg) {
    ErrorMsg msg;
    msg.level = level;
    msg.has_location = has_location;
    msg.highlight_span = highlight_span;
    msg.src_code = src_code;
    msg.msg = std::move(formatted_msg); // 直接转移，零拷贝

    error_msgs.push_back(msg);

    if(level == ErrorLevel::Error) {
        error_count += 1;
    } else if(level == ErrorLevel::Warning) {
        warning_count += 1;
    }
}

// 颜色常量（文件作用域，print_msg / print_error_line 共用）
static constexpr const char *COLOR_RESET = "\033[0m";
static constexpr const char *COLOR_RED   = "\033[31m";
static constexpr const char *COLOR_GREEN = "\033[32m";
static constexpr const char *COLOR_BLUE  = "\033[34m";


// 彩色级别标签: Error→红, Warning→蓝
void print_colored_level(ErrorLevel level) {
    print_err("{}{}{}",
        level == ErrorLevel::Error ? COLOR_RED : COLOR_BLUE,
        level == ErrorLevel::Error ? "Error" : "Warning",
        COLOR_RESET);
}


void ErrorReporter::print_msg() {
    if(error_msgs.count == 0) {
        return;
    }

    println_err("\n{} error(s), {} warning(s) found:\n", error_count, warning_count);

    for (size_t i = 0; i < error_msgs.count; ++i) {
        ErrorMsg *msg = &error_msgs[i];

        // 无行号错误：只打彩色级别行，跳过 file:line:col + 源码 + caret
        if(!msg->has_location) {
            print_error_line(msg->level, "{}", msg->msg);
            continue;
        }

        auto start = cal_line_column_index_of_byte_pos(msg->src_code, msg->highlight_span.start);
        auto end = cal_line_column_index_of_byte_pos(msg->src_code, msg->highlight_span.end);

        print_err("{}:{}:{}: ", msg->src_code.file_path, start.first, start.second);
        print_error_line(msg->level, "{}", msg->msg);

        xpString line_str = get_line_str_of_pos(msg->src_code, msg->highlight_span.start, xp_heap_allocator());
        defer(xp_string_free(line_str));

        print_err("{}", line_str); // 不需要换行，原行字符串已经带换行

        println_err("");

        for(isize j = 0; j < start.second - 1; j++) {
            print_err(" ");
        }
        // 统一开启绿色，输出所有^后再重置，避免冗余控制码
        print_err("{}", COLOR_GREEN);
        for(isize j = 0; j < end.second - start.second; j++) {
            print_err("^");
        }
        println_err("{}", COLOR_RESET);
    }
}

ErrorReporter make_error_reporter(xpAllocator allocator) {
    ErrorReporter reporter = {};
    reporter.error_msgs = make_array<ErrorMsg>(allocator);
    return reporter;
}