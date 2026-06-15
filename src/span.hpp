#ifndef CREST_SPAN_HPP
#define CREST_SPAN_HPP

#include <format>
#include "xoaop.h"
#include "source_code.hpp"



struct Span {
    isize start;
    isize end;
};

Span make_span(isize start, isize end);

Span merge(Span a, Span b);

void print_span(SourceCode src_code, Span span);



// Span的格式化支持（仅打印字节偏移）
template<>
struct std::formatter<Span> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const Span& span, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "Span({}..{})", span.start, span.end);
    }
};


struct SourceLocation {
    SourceLocation();
    SourceLocation(SourceCode src_code, Span span);

    SourceCode src_code;
    Span span;
};

SourceLocation merge(const SourceLocation& a, const SourceLocation& b);


template<>
struct std::formatter<SourceLocation> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const SourceLocation& loc, std::format_context& ctx) const {
        auto start = cal_line_column_index_of_byte_pos(loc.src_code, loc.span.start);
        auto end = cal_line_column_index_of_byte_pos(loc.src_code, loc.span.end);

        return std::format_to(ctx.out(), "{}:{}:{} - {}:{}",
            loc.src_code.file_path,
            start.first, start.second,
            end.first, end.second
        );
    }
};


#endif