#include "source_code.hpp"

SourceCode make_source_code(xpString file_path, xpString code_string, Array<isize> line_start_indices) {
    SourceCode source_code;
    source_code.file_path = file_path;
    source_code.code_string = code_string;
    source_code.line_start_indices = line_start_indices;

    return source_code;
}


SourceCode make_source_code(xpString file_path, xpString code_string, xpAllocator allocator) {
    SourceCode source_code = make_source_code(file_path, code_string, make_array<BytePos>(allocator));
    
    // 计算行起始位置
    BytePos count = cast(BytePos) code_string.length;
    array_push_back(&source_code.line_start_indices, cast(BytePos)0); // 第一行起始位置是0
    for(BytePos i = 0; i < count; i++) {
        if(code_string.c_str[i] == '\n') {
            array_push_back(&source_code.line_start_indices, i + 1); // 下一行的起始位置是当前字符的下一个位置
        }
    }


    return source_code;
}


xpPair<BytePos, BytePos> cal_line_column_idx(Array<BytePos> line_start_indices, BytePos byte_pos) {
    isize line_count = line_start_indices.count;
    if(line_count == 0) {
        return xp_make_pair((BytePos)1, (BytePos)1);
    }

    isize left_idx = 0;
    isize right_idx = line_count;
    while(left_idx < right_idx) {
        BytePos mid_idx = left_idx + ((right_idx - left_idx) / 2);

        if(line_start_indices[mid_idx] <= byte_pos) {
            left_idx = mid_idx + 1;
        } else {
            right_idx = mid_idx;
        }
    }
    
    // 如果已经是第一行, 说明没有比byte_pos更大的行起始位置, 只能是第一行, 就不用减1了, 要不然得到的就是所在行的下一行，所以要减1
    if(left_idx - 1 >= 0) {
        left_idx = left_idx - 1;
    }

    BytePos line_start_pos = line_start_indices[left_idx];
    BytePos line_index = left_idx + 1; // 行号从1开始计数
    BytePos column_index = byte_pos - line_start_pos + 1;

    xpPair line_column = xp_make_pair(line_index, column_index);
    return line_column;
}


xpPair<BytePos, BytePos> cal_line_column_index_of_byte_pos(SourceCode src_code, BytePos byte_pos) {
    return cal_line_column_idx(src_code.line_start_indices, byte_pos);
}

xpString get_line_str_of_pos(SourceCode code, BytePos pos, xpAllocator allocator) {

    auto line_column = cal_line_column_index_of_byte_pos(code, pos);
    BytePos line_index = line_column.first;
    BytePos column_index = line_column.second;
    isize line_count = code.line_start_indices.count;

    if(line_count == 0) {
        return xp_make_string_capacity(allocator, nullptr, 0);
    }


    BytePos line_start_pos = code.line_start_indices[line_index - 1]; // line_index从1开始计数，所以要减1得到正确的行起始位置
    BytePos line_end_pos = (line_index < line_count) ? code.line_start_indices[line_index] - 1 : code.code_string.length; // 如果是最后一行，行结束位置就是code_string的长度，否则就是下一行的起始位置减1

    xpString line_str = xp_make_string_capacity(allocator, code.code_string.c_str + line_start_pos, line_end_pos - line_start_pos);
    return line_str;
}



bool is_same_src_code(const SourceCode *a, const SourceCode *b) {
    return xp_string_equal(a->file_path, b->file_path);
}