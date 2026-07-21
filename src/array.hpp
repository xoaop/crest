#ifndef XOAOP_ARRAY_H
#define XOAOP_ARRAY_H

#include "xoaop.h"
#include <type_traits>
#include <utility>




template<typename T>
struct ArrayRef {
    T *data;
    isize count;

    T& operator[](isize index) {
        XP_ASSERT(index >= 0 && index < count);
        return data[index];
    }

    const T& operator[](isize index) const {
        XP_ASSERT(index >= 0 && index < count);
        return data[index];
    }

    T *begin() { 
        return data; 
    }

    T *end() { 
        return data + count; 
    }

    const T *begin() const { 
        return data; 
    }
    
    const T *end() const { 
        return data + count; 
    }

    ArrayRef<T> slice(isize start) const {
        XP_ASSERT(start >= 0 && start <= count);
        return {data + start, count - start};
    }

    ArrayRef<T> slice(isize start, isize n) const {
        XP_ASSERT(start >= 0 && start + n <= count);
        return {data + start, n};
    }
};


template<typename T>
ArrayRef<T> array_ref(T *data, isize count) {
    return {data, count};
}

template<typename T>
ArrayRef<const T> array_ref(const T *data, isize count) {
    return {data, count};
}

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



    template<typename... Args>
    T *emplace_back(Args&&... args) {
        resize(count + 1);
        data[count - 1].~T();
        new (&data[count - 1]) T(std::forward<Args>(args)...);
        return &data[count - 1];
    }

    T *push_back(T elem) {
        resize(count + 1);
        data[count - 1] = std::move(elem);
        return &data[count - 1];
    }

    ArrayRef<T> push_back(T *elems, isize n) {
        for(isize i = 0; i < n; i++) {
            push_back(elems[i]);
        }
        return {data + count - n, n};
    }

    T pop_back() {
        XP_ASSERT_DEFAULT(count > 0);
        T p = std::move(data[count - 1]);
        data[count - 1].~T();
        count -= 1;
        return p;
    }

    void resize(isize new_count) {
        // 扩容
        if(new_count > capacity) {
            T* nd = cast(T*)xp_alloc(allocator, new_count * sizeof(T));
            if(data != NULL) {
                for(isize i = 0; i < count; i++) {
                    new(&nd[i]) T(std::move(data[i]));
                    data[i].~T();
                }
                xp_free(allocator, data);
            }
            data = nd;
            capacity = new_count;
        }

        // 缩容：析构多余元素
        for(isize i = new_count; i < count; i++) {
            data[i].~T();
        }

        // 扩增：默认构造新元素
        if constexpr (std::is_default_constructible_v<T>) {
            for(isize i = count; i < new_count; i++) {
                new (&data[i]) T();
            }
        }

        count = new_count;
    }


    void clear() {
        for (isize i = 0; i < count; i++) {
            data[i].~T();
        }
        count = 0;
    }

    Array<T>* insert(isize index, T elem) {
        XP_ASSERT_DEFAULT(index >= 0 && index <= count);
        resize(count + 1);
        for (isize i = count - 1; i > index; i--) {
            data[i] = std::move(data[i-1]);
        }
        data[index] = std::move(elem);
        return this;
    }

    Array<T>* insert(isize index, T *elems, isize n) {
        isize old_count = count;
        resize(old_count + n);
        for (isize i = old_count - 1; i >= index; i--) {
            data[i + n] = std::move(data[i]);
        }
        for (isize i = 0; i < n; i++) {
            data[index + i] = elems[i];
        }
        return this;
    }

    T rm(isize index) {
        XP_ASSERT_DEFAULT(index >= 0 && index < count);
        T r = std::move(data[index]);
        data[index].~T();
        for (isize i = index; i < count - 1; i++) {
            new (&data[i]) T(std::move(data[i+1]));
            data[i+1].~T();
        }
        count -= 1;
        return r;
    }

    T rm_f(isize index) {
        XP_ASSERT_DEFAULT(index >= 0 && index < count);
        T r = std::move(data[index]);
        data[index].~T();
        if(count > 1 && index != count - 1) {
            new (&data[index]) T(std::move(data[count - 1]));
            data[count - 1].~T();
        }
        count -= 1;
        return r;
    }

    isize find_next_index(T elem) {
        for(isize i = 0; i < count; i++) {
            if(data[i] == elem) {
                return i;
            }
        }
        return -1;
    }

    Array<T> copy(xpAllocator allocator) const {
        Array<T> new_array = make_array<T>(allocator);
        for(isize i = 0; i < count; i++) {
            new_array.push_back(data[i]);
        }
        return new_array;
    }

    Array<T> cut(xpAllocator allocator, isize start) const {
        XP_ASSERT_DEFAULT(start < count);
        Array<T> new_array = make_array<T>(allocator);
        for(isize i = start; i < count; i++) {
            new_array.push_back(data[i]);
        }
        return new_array;
    }


    T *begin() {
        return data;
    }

    T *end() {
        return data + count;
    }

    const T *begin() const {
        return data;
    }

    const T *end() const {
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
ArrayRef<T> array_ref(Array<T> &array) {
    return {array.data, array.count};
}

template<typename T>
ArrayRef<const T> array_ref(const Array<T> &array) {
    return {array.data, array.count};
}



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
Array<T> make_array_count(xpAllocator allocator, isize len) {
    Array<T> array = make_array<T>(allocator);
    array.resize(len);
    return array;
}

template<typename T>
Array<T> make_array_capacity(xpAllocator allocator, isize capacity) {
    Array<T> array = make_array<T>(allocator);

    array.data = (T*)xp_alloc(allocator, capacity * sizeof(T));
    array.count = 0;
    array.capacity = capacity;

    return array;
}


template<typename T>
void array_free(Array<T>* array) {
    array->clear();
    xp_free(array->allocator, array->data);

    array->data = NULL;
    array->count = 0;
    array->capacity = 0;
}


//
// Array As Buffer
//

// 注意：仅适用于平凡可构造类型（POD），非POD类型使用会导致未定义行为
template<typename T>
Array<T> make_array_as_buffer(xpAllocator allocator, isize len) {
    static_assert(std::is_trivially_default_constructible_v<T>,
                  "make_array_as_buffer only supports trivially default-constructible types (POD)");
    Array<T> array = make_array<T>(allocator);
    array.data = (T*)xp_alloc(allocator, len * sizeof(T));
    array.count = len;
    array.capacity = len;
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