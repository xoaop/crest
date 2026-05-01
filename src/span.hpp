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


#endif