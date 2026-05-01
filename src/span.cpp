#include "span.hpp"
#include <print>

Span make_span(isize start, isize end) {
    Span span;
    span.start = start;
    span.end = end;

    return span;
}


Span merge(Span a, Span b) {
    Span merged = {};

    if(a.start < b.start) {
        merged.start = a.start;
    } else {
        merged.start = b.start;
    }

    if(a.end > b.end) {
        merged.end = a.end;
    } else {
        merged.end = b.end;
    }

    return merged;
}


void print_span(SourceCode src_code, Span span) {
    auto start = cal_line_column_index_of_byte_pos(src_code, span.start);
    auto end = cal_line_column_index_of_byte_pos(src_code, span.end);

    std::print("{}:{}:{} - {}:{}",
        src_code.file_path,
        start.first, start.second,
        end.first, end.second
    );
}



