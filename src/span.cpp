#include "span.hpp"

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