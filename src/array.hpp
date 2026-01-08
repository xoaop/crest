#ifndef XOAOP_ARRAY_H
#define XOAOP_ARRAY_H

#include "xoaop.h"

#define ARRAY_NEW_CAPACITY(old) (cast(isize) (old + old / 2 + 1))


/*
    Array
*/

template<typename T>
struct Array {

    T *data;
    xpAllocator allocator;
    isize count;
    isize capacity;

    
    T& operator[](isize index) {
        XP_ASSERT(index >= 0 && index < count);
        return data[index];
    }


    // 妥协
    // Array(xpAllocator allocator) {
    //     *this = make_array<T>(allocator);
    // }



    void push_back(T elem) {
        array_push_back(this, elem);
    }


    void clear() {
        count = 0;
    }


    T *begin() {
        return data;
    }

    T *end() {
        return data + count;
    }

    b8 full() {
        return count == capacity;
    }
};



template<typename T>
Array<T> make_array(xpAllocator allocator) {
    Array<T> array;
    array.allocator = allocator;
    array.data = NULL;
    array.count = 0;
    array.capacity = 0;

    return array;
}

template<typename T>
Array<T> make_array_len(xpAllocator allocator, isize len) {
    Array<T> array = make_array<T>(allocator);

    array.data = (T*)xp_alloc(allocator, len * sizeof(T));
    array.count = 0;
    array.capacity = len;

    return array;
}

template<typename T>
void array_free(Array<T>* array) {
    xp_free(array->allocator, array->data);

    array->data = NULL;
    array->count = 0;
    array->capacity = 0;
}


template<typename T>
Array<T> array_copy(Array<T>* src, xpAllocator allocator) {
    Array<T> new_array = make_array<T>(allocator);

    for(isize i = 0; i < src->count; i++) {
        array_push_back(&new_array, src->data[i]);
    }

    return new_array;
}


template<typename T>
Array<T>* array_resize(Array<T>* array, isize new_capacity) {
    if(array->capacity >= new_capacity) {
        return array;
    }

    if(array->data != NULL) {
        array->data = cast(T *)xp_realloc(array->allocator, array->data, new_capacity * sizeof(T), array->capacity * sizeof(T));
    } else {
        array->data = cast(T *)xp_alloc(array->allocator, new_capacity * sizeof(T));
    }

    
    //若容量小于先前的元素数量
    array->capacity = new_capacity;
    if(array->capacity < array->count) {
        array->count = array->capacity;
    }

    return array;
}

template<typename T>
T *array_push_back(Array<T> *array, T elem) {
    if(array->count == array->capacity) {
        array_resize(array, ARRAY_NEW_CAPACITY(array->capacity));
    }

    array->data[array->count] = elem;
    array->count += 1;

    return &array->data[array->count - 1];
}

template<typename T>
T array_pop_back(Array<T> *array) {
    XP_ASSERT_DEFAULT(array->count > 0);

    T p = array->data[array->count - 1];
    array->count -= 1;
    return p;
}


template<typename T>
Array<T>* array_push_back(Array<T> *array, T *elems, isize count) {
    for(isize i = 0; i < count; i++) {
        array_push_back(array, elems[i]);
    }

    return array;
}


template<typename T>
Array<T>* array_insert(Array<T> *array, isize index, T elem) {
    XP_ASSERT_DEFAULT(index >= 0 && index <= array->count);

    if(array->count == array->capacity) {
        array_resize(array, ARRAY_NEW_CAPACITY(array->capacity));
    }

    memmove(array->data + index + 1, array->data + index, (array->count - index) * sizeof(T));
    array->data[index] = elem;
    array->count += 1;
    return array;
}

// !TEST
template<typename T>
Array<T>* array_insert(Array<T> *array, isize index, T *elems, isize count) {
    if(array->count + count < array->capacity) {
        array_resize(array, ARRAY_NEW_CAPACITY(array->capacity) + count); // NOTE: + count 防止新容量不够大
    }

    memmove(array->data + index + count, array->data + index, (array->count - index) * sizeof(T));
    memcpy(array->data + index, elems, count);
    array->count += count;

    return array;
}


template<typename T>
T array_rm(Array<T> *array, isize index) {
    XP_ASSERT_DEFAULT(index >= 0 && index < array->count);

    T rm = array->data[index];
    
    // 最后一个元素
    if(index == array->count - 1) {
        array->count -= 1;
        return rm;
    }
    
    
    memmove(array->data + index, array->data + index + 1, (array->count - index - 1) * sizeof(T));
    array->count -= 1;
    return rm;
}


// NOTE(xoaop): 这个不考虑元素顺序
template<typename T>
T array_rm_f(Array<T> *array, isize index) {
    XP_ASSERT_DEFAULT(index >= 0 && index < array->count);

    T rm = array->data[index];
    // 如果只有一个元素，直接减少count
    if(array->count > 1) {
        array->data[index] = array->data[array->count - 1];
    }

    array->count -= 1;
    return rm;
}

template<typename T>
void array_clear(Array<T> *array) {
    array->count = 0;
    return;
}

template<typename T>
isize array_find_next_index(Array<T> *array, T elem) {
    for(isize i = 0; i < array->count; i++) {
        if(array->data[i] == elem) {
            return i;
        }
    }

    return -1;
}


template<typename T>
Array<T> array_cut(xpAllocator allocator, Array<T> *array, isize start) {
    XP_ASSERT_DEFAULT(start < array->count);

    Array<T> new_array = make_array(allocator);

    for(isize i = start; i < array->count; i++) {
        array_push_back(&new_array, array->data[i]);
    }

    return new_array;
}



//
// Array As Buffer
//

template<typename T>
Array<T> make_array_as_buffer(xpAllocator allocator, isize len) {
    Array<T> array = make_array_len<T>(allocator, len);
    array.count = len;
    
    return array;
}

template<typename T>
void array_memcpy(Array<T> *dst, isize dst_start, T *src, isize src_count) {
    XP_ASSERT_DEFAULT(dst_start + src_count <= dst->capacity);
    memcpy(dst->data + dst_start, src, src_count * sizeof(T));
}


// 把dst当作环形数组, src不是
template<typename T>
void array_memcpy_dst_loop(Array<T> *dst, isize dst_start, T *src, isize src_count) {
    for(isize i = 0; i < src_count; i++) {
        isize dst_index = (dst_start + i) % dst->capacity; 
        isize src_index = i;
        
        dst->data[dst_index] = src[src_index];
    }
}

// 把dst当作环形数组, src不是
template<typename T>
void array_memcpy_dst_loop(Array<T> *dst, isize dst_start, Array<T> *src, isize src_count) {
    XP_ASSERT_DEFAULT(src_count <= src->capacity);
    array_memcpy_dst_loop(dst, dst_start, src->data, src_count);
}



// 把src当作环形数组, dst不是
// template<typename T>
// void array_memcpy_src_loop(Array<T> *dst, isize dst_start, T *src, isize src_start, isize src_count) {
//     XP_ASSERT_DEFAULT(dst_start + src_count <= dst->capacity);
    
//     for(isize i = 0; i < src_count; i++) {
//         isize dst_index = dst_start + i; 
//         isize src_index = (src_start + i) % src->capacity;
        
//         dst->data[dst_index] = src->data[src_index];
//     }
// }



//
// Array<u32> As String
//
Array<u32> make_array_as_string(xpAllocator allocator, const char32_t *string);

void array_push_back_str(Array<u32> *array, const char32_t *str);

isize array_memcpy_dst_loop(Array<u32> *dst, isize dst_start, const char32_t *src);


/*
Slice
*/

template<typename T>
struct Slice {
    xpSlice _slice;
};

template<typename T>
Slice<T> slice_make_from_array(Array<T> array) {

}



#endif