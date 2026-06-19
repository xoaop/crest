#ifndef XOAOP_ARRAY_H
#define XOAOP_ARRAY_H

#include "xoaop.h"
#include <utility>

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

    const T& operator[](isize index) const {
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

    void pop_back() {
        array_pop_back(this);
    }


    void clear() {
        array_clear(this);
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

    T& back() {
        XP_ASSERT(count > 0);
        return data[count - 1];
    }

    const T& back() const {
        XP_ASSERT(count > 0);
        return data[count - 1];
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
Array<T> make_array_reserved(xpAllocator allocator, isize count) {
    Array<T> array = make_array_len<T>(allocator, count);
    array.count = count;
    return array;
}

template<typename T>
void array_free(Array<T>* array) {
    array_clear(array); // 先析构所有元素
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
        // 缩容时析构多余元素
        if(new_capacity < array->count) {
            for (isize i = new_capacity; i < array->count; i++) {
                array->data[i].~T();
            }
            array->count = new_capacity;
        }
        return array;
    }

    T* new_data = cast(T*)xp_alloc(array->allocator, new_capacity * sizeof(T));

    // 移动原有元素到新内存
    if(array->data != NULL) {
        for (isize i = 0; i < array->count; i++) {
            new (&new_data[i]) T(std::move(array->data[i]));
            array->data[i].~T();
        }
        xp_free(array->allocator, array->data);
    }

    array->data = new_data;
    array->capacity = new_capacity;

    return array;
}

template<typename T>
T *array_push_back(Array<T> *array, T elem) {
    if(array->count == array->capacity) {
        array_resize(array, ARRAY_NEW_CAPACITY(array->capacity));
    }

    new (&array->data[array->count]) T(std::move(elem)); // 构造元素
    array->count += 1;

    return &array->data[array->count - 1];
}

template<typename T>
T array_pop_back(Array<T> *array) {
    XP_ASSERT_DEFAULT(array->count > 0);

    T p = std::move(array->data[array->count - 1]); // 移动元素
    array->data[array->count - 1].~T(); // 析构原位置
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

    // 从后往前移动元素，避免覆盖
    for (isize i = array->count; i > index; i--) {
        new (&array->data[i]) T(std::move(array->data[i-1]));
        array->data[i-1].~T();
    }

    new (&array->data[index]) T(std::move(elem)); // 构造新元素
    array->count += 1;
    return array;
}

// !TEST
template<typename T>
Array<T>* array_insert(Array<T> *array, isize index, T *elems, isize count) {
    if(array->count + count > array->capacity) { // 修复：容量不足时才扩容
        array_resize(array, ARRAY_NEW_CAPACITY(array->capacity) + count); // NOTE: + count 防止新容量不够大
    }

    // 先移动后面的元素腾出空间
    for (isize i = array->count + count - 1; i >= index + count; i--) {
        new (&array->data[i]) T(std::move(array->data[i - count]));
        array->data[i - count].~T();
    }

    // 逐个构造新元素
    for (isize i = 0; i < count; i++) {
        new (&array->data[index + i]) T(elems[i]);
    }

    array->count += count;

    return array;
}


template<typename T>
T array_rm(Array<T> *array, isize index) {
    XP_ASSERT_DEFAULT(index >= 0 && index < array->count);

    T rm = std::move(array->data[index]); // 移动要返回的元素
    array->data[index].~T(); // 析构被删除的元素

    // 往前移动后面的元素
    for (isize i = index; i < array->count - 1; i++) {
        new (&array->data[i]) T(std::move(array->data[i+1]));
        array->data[i+1].~T();
    }

    array->count -= 1;
    return rm;
}


// NOTE(xoaop): 这个不考虑元素顺序
template<typename T>
T array_rm_f(Array<T> *array, isize index) {
    XP_ASSERT_DEFAULT(index >= 0 && index < array->count);

    T rm = std::move(array->data[index]); // 移动要返回的元素
    array->data[index].~T(); // 析构被删除的元素

    // 如果不是最后一个元素，把最后一个元素移过来
    if(array->count > 1 && index != array->count - 1) {
        new (&array->data[index]) T(std::move(array->data[array->count - 1]));
        array->data[array->count - 1].~T();
    }

    array->count -= 1;
    return rm;
}

template<typename T>
void array_clear(Array<T> *array) {
    for (isize i = 0; i < array->count; i++) {
        array->data[i].~T();
    }
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

// 注意：仅适用于平凡可构造类型（POD），非POD类型使用会导致未定义行为
template<typename T>
Array<T> make_array_as_buffer(xpAllocator allocator, isize len) {
    Array<T> array = make_array_len<T>(allocator, len);
    array.count = len;

    return array;
}

// 注意：仅适用于平凡可拷贝类型（POD），非POD类型使用会导致未定义行为
template<typename T>
void array_memcpy(Array<T> *dst, isize dst_start, T *src, isize src_count) {
    XP_ASSERT_DEFAULT(dst_start + src_count <= dst->capacity);
    memcpy(dst->data + dst_start, src, src_count * sizeof(T));
}


// 把dst当作环形数组, src不是
// 注意：仅适用于平凡可拷贝类型（POD），非POD类型使用会导致未定义行为
template<typename T>
void array_memcpy_dst_loop(Array<T> *dst, isize dst_start, T *src, isize src_count) {
    for(isize i = 0; i < src_count; i++) {
        isize dst_index = (dst_start + i) % dst->capacity;
        isize src_index = i;

        dst->data[dst_index] = src[src_index];
    }
}

// 把dst当作环形数组, src不是
// 注意：仅适用于平凡可拷贝类型（POD），非POD类型使用会导致未定义行为
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