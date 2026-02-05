#ifndef CREST_SPAN_HPP
#define CREST_SPAN_HPP

#include "xoaop.h"




struct Span {
    isize start;
    isize end;
};

Span make_span(isize start, isize end);



#endif