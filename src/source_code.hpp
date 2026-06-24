#ifndef CREST_SOURCE_CODE_HPP
#define CREST_SOURCE_CODE_HPP

#include "xoaop.h"
#include "array.hpp"


typedef isize BytePos;

struct SourceCode {
    xpString file_path;
    xpString code_string;
    Array<BytePos> line_start_indices;
    isize line_offset;  // 代码片段在原文件中的起始行偏移（0-indexed），正常文件为0
};

SourceCode make_source_code(xpString file_path, xpString code_string, Array<BytePos> line_start_indices);
SourceCode make_source_code(xpString file_path, xpString code_string, xpAllocator allocator);

xpPair<BytePos, BytePos> cal_line_column_idx(Array<BytePos> line_start_indices, BytePos byte_pos);
xpPair<BytePos, BytePos> cal_line_column_index_of_byte_pos(SourceCode src_code, BytePos byte_pos);

xpString get_line_str_of_pos(SourceCode code, BytePos pos, xpAllocator allocator);

bool is_same_src_code(const SourceCode *a, const SourceCode *b);

#endif
