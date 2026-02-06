#ifndef CREST_SOURCE_CODE_HPP
#define CREST_SOURCE_CODE_HPP

#include "xoaop.h"
#include "array.hpp"


typedef isize BytePos;

struct SourceCode {
    xpString code_string;
    Array<BytePos> line_start_indices;
};

SourceCode make_source_code(xpString code_string, Array<BytePos> line_start_indices);
SourceCode make_source_code(xpString code_string, xpAllocator allocator);

xpPair<BytePos, BytePos> cal_line_column_idx(Array<BytePos> line_start_indices, BytePos byte_pos);
xpPair<BytePos, BytePos> cal_line_column_index_of_byte_pos(SourceCode *src_code, BytePos byte_pos);

#endif
