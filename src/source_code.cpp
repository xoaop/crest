#include "source_code.hpp"

SourceCode make_source_code(xpString code_string, Array<isize> line_start_indices) {
    SourceCode source_code;
    source_code.code_string = code_string;
    source_code.line_start_indices = line_start_indices;

    return source_code;
}


SourceCode make_source_code(xpString code_string, xpAllocator allocator) {
    SourceCode source_code = make_source_code(code_string, make_array<BytePos>(allocator));
    
    // 计算行起始位置
    BytePos count = cast(BytePos) code_string.length;
    array_push_back(&source_code.line_start_indices, cast(BytePos)0); // 第一行起始位置是0
    for(BytePos i = 0; i < count; i++) {
        if(code_string.c_str[i] == '\n') {
            array_push_back(&source_code.line_start_indices, i + 1);
        }
    }


    return source_code;
}



void cal_line_column_index_of_byte_pos(SourceCode *src_code, BytePos byte_pos, BytePos *out_line_index, BytePos *out_column_index) {
    isize line_count = cast(BytePos) src_code->line_start_indices.count;

    isize left_idx = 0;
    isize right_idx = line_count;
    while(left_idx < right_idx) {
        BytePos mid_idx = left_idx + ((right_idx - left_idx) / 2);

        if(src_code->line_start_indices[mid_idx] <= byte_pos) {
            left_idx = mid_idx + 1;
        } else {
            right_idx = mid_idx;
        }
    }
    
    // 如果已经是第一行, 说明没有比byte_pos更大的行起始位置, 只能是第一行, 就不用减1了, 要不然得到的就是所在行的下一行，所以要减1
    if(left_idx - 1 >= 0) {
        left_idx = left_idx - 1;
    }

    BytePos line_start_pos = src_code->line_start_indices[left_idx];
    BytePos line_index = left_idx + 1; // 行号从1开始计数
    BytePos column_index = byte_pos - line_start_pos + 1;

    XP_ASSERT_DEFAULT(out_column_index != NULL && out_line_index != NULL);
    *out_line_index = line_index;
    *out_column_index = column_index;
    return;
}

