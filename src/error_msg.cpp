#include "print.hpp"

#include "error_msg.hpp"


void ErrorReporter::add_error_msg(ErrorLevel level, bool has_location, Span highlight_span, SourceCode src_code, xpString formatted_msg) {
    // 计数照旧，但超过上限就不再记录 —— 单个大输入能刷出上万条错误
    if(level == ErrorLevel::Error) {
        error_count += 1;
    } else if(level == ErrorLevel::Warning) {
        warning_count += 1;
    }

    if(error_msgs.count >= MAX_REPORTED_MSGS) {
        return;
    }

    ErrorMsg msg;
    msg.level = level;
    msg.has_location = has_location;
    msg.highlight_span = highlight_span;
    msg.src_code = src_code;
    msg.msg = std::move(formatted_msg); // 直接转移，零拷贝

    error_msgs.push_back(msg);
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

        // 超宽行只渲染高亮附近的一段。整行回显与光标都是 O(行宽)，
        // 几千字符的单行配上成百条错误会打出几十 MB。
        static constexpr isize MAX_ECHO_COLS = 160;
        static constexpr isize ECHO_CONTEXT = 60;

        const isize line_len = line_str.length;
        const isize hl_start = start.second - 1;   // 0-based
        const isize hl_end   = end.second - 1;

        isize win_start = 0;
        isize win_end = line_len;
        if(line_len > MAX_ECHO_COLS) {
            win_start = hl_start > ECHO_CONTEXT ? hl_start - ECHO_CONTEXT : 0;
            win_end = win_start + MAX_ECHO_COLS;
            if(win_end > line_len) { win_end = line_len; }
        }
        const isize prefix_len = win_start > 0 ? 3 : 0;

        if(prefix_len > 0) { print_err("..."); }
        xpString window_str = xp_make_string_capacity(xp_heap_allocator(),
            line_str.c_str + win_start, win_end - win_start);
        defer(xp_string_free(window_str));
        print_err("{}", window_str);
        if(win_end < line_len) { print_err("..."); }

        println_err("");

        // 光标：夹到窗口内，避免打出上万字符的缩进/尖号
        const isize caret_start = hl_start < win_start ? win_start : hl_start;
        const isize caret_end   = hl_end >= win_end ? win_end - 1 : hl_end;
        const isize caret_count = caret_end > caret_start ? caret_end - caret_start : 0;

        for(isize j = 0; j < prefix_len + (caret_start - win_start); j++) {
            print_err(" ");
        }
        print_err("{}", COLOR_GREEN);
        for(isize j = 0; j < caret_count; j++) {
            print_err("^");
        }
        println_err("{}", COLOR_RESET);
    }

    const isize hidden = error_count + warning_count - error_msgs.count;
    if(hidden > 0) {
        println_err("\n（另有 {} 条诊断未显示）", hidden);
    }
}

ErrorReporter make_error_reporter(xpAllocator allocator) {
    ErrorReporter reporter = {};
    reporter.error_msgs = make_array<ErrorMsg>(allocator);
    return reporter;
}