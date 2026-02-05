#include "span.hpp"

Span make_span(isize start, isize end) {
    Span span;
    span.start = start;
    span.end = end;

    return span;
}