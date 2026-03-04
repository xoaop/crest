#ifndef CREST_SPAN_HPP
#define CREST_SPAN_HPP

#include "xoaop.h"
#include "source_code.hpp"



struct Span {
    isize start;
    isize end;
};

Span make_span(isize start, isize end);

Span merge(Span a, Span b);

void print_span(SourceCode src_code, Span span);



#endif