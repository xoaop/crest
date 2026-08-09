#include "span.hpp"

#include "print.hpp"

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


#if defined(CREST_DEBUG)
void print_span(SourceCode src_code, Span span) {
    auto start = cal_line_column_index_of_byte_pos(src_code, span.start);
    auto end = cal_line_column_index_of_byte_pos(src_code, span.end);

    print_err("{}:{}:{} - {}:{}",
        src_code.file_path,
        start.first, start.second,
        end.first, end.second
    );
}
#endif // CREST_DEBUG



SourceLocation::SourceLocation() : src_code(), span() {

}

SourceLocation::SourceLocation(SourceCode src_code, Span span) {
    this->src_code = src_code;
    this->span = span;
}

SourceLocation merge(const SourceLocation& a, const SourceLocation& b) {
    XP_ASSERT_MSG(is_same_src_code(&a.src_code, &b.src_code), "Cannot merge SourceLocations from different SourceCodes");

    SourceLocation merged{
        a.src_code,
        merge(a.span, b.span)
    };

    return merged;
}