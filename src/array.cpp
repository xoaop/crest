#include "array.hpp"

Array<u32> make_array_as_string(xpAllocator allocator, const char32_t *string) {
    isize str_len = xp_strlen_char32(string);
    
    Array<u32> array = make_array_as_buffer<u32>(allocator, str_len);

    array_memcpy(&array, 0, cast(u32 *)string, str_len);

    return array;
}


void array_push_back_str(Array<u32> *array, const char32_t *str) {
    if(str == NULL) {
        return;
    }

    isize i = 0;
    while(str[i] != U'\0') {
        array->push_back((u32)(str[i++]));
    }
}

isize array_memcpy_dst_loop(Array<u32> *dst, isize dst_start, const char32_t *src) {
    isize len = xp_strlen_char32(src);
    array_memcpy_dst_loop<u32>(dst, dst_start, cast(u32 *)src, len);
    return len;
}