/*
    Everything about my library code in C/CPP


    Config:
    - XOAOP_IMPLEMENTATION: 定义后会包含实现部分，建议在一个源文件中定义
    - XOAOP_I128_SUPPORT: 定义后会启用对i128类型的支持，提供相关函数和常量
    - XP_HEAP_RECORD_ENABLE: 定义后会启用带监控的堆分配器功能，提供相关API和统计功能
    
*/


#ifndef XOAOP_H
#define XOAOP_H


#if defined(__cplusplus)
extern "C" { // for decl
#endif

/*
    链接选项
*/

#ifdef XP_EXPORT_SYMBOL
#define XP_EXPORT __declspec(dllexport)
#else 
#define XP_EXPORT
#endif

#define xp_define XP_EXPORT



/*
    头文件
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <cassert>

/*
Type Definitions
*/

#include <stddef.h> // NOTE: only for ptrdiff_t now
    typedef unsigned char u8;
    typedef unsigned short u16;
    typedef unsigned int u32;
    typedef unsigned long long u64;
    typedef size_t usize;
    typedef char i8;
    typedef short i16;
    typedef int i32;
    typedef long long i64;
    typedef ptrdiff_t isize;
    typedef float f32;
    typedef double f64;


    typedef i8 b8;
    typedef i16 b16;
    typedef i32 b32;

    typedef usize uintptr;


#if defined(XOAOP_I128_SUPPORT)

#if defined(__clang__) || defined(__GNUC__)
    typedef __int128_t i128;
#elif defined(_MSC_VER) 
    error("i128 not supported on MSVC");
#endif


#define I128_MIN (((i128)1) << 127)
#define I128_MAX ((((~(i128)0) >> 1)))


xp_define void print_i128(i128 n);

xp_define bool xp_check_i128_add_overflow(i128 a, i128 b, i128 *result);
xp_define bool xp_check_i128_sub_overflow(i128 a, i128 b, i128 *result);
xp_define bool xp_check_i128_mul_overflow(i128 a, i128 b, i128 *result);
xp_define bool xp_check_i128_div_overflow(i128 a, i128 b, i128 *result);
xp_define bool xp_check_i128_mod_overflow(i128 a, i128 b, i128 *result);
xp_define bool xp_check_i128_neg_overflow(i128 a, i128 *result);




xp_define b32 xp_str_to_integer(char const *str, i128 *result);

#endif // XOAOP_I128_SUPPORT


bool xp_check_f64_is_inf(f64 value);



/*
    常用宏 MACROS
*/



#define xp_internal static // 局部链接
#define xp_local_insist static // 函数内静态生命周期变量
#define xp_global static // 文件内静态生命周期变量




// 平台统一的trap宏
#if defined(_MSC_VER)
#define XP_TRAP() __debugbreak()
#else
#define XP_TRAP() __builtin_trap()
#endif

#define XP_ASSERT(exp) do {                                     \
    if(!(exp))                                                  \
        XP_TRAP();                                              \
} while(0)

#define XP_ASSERT_MSG(exp, format, ...) do {                    \
    if(!(exp)) {                                                \
        fprintf(stderr, "\nAssert FAILED at %s:%d: ", __FILE__, __LINE__); \
        fprintf(stderr, format, ##__VA_ARGS__);                   \
        fflush(stderr);                                           \
        XP_TRAP();                                              \
    }                                                           \
} while(0)

#define XP_ASSERT_DEFAULT(exp) XP_ASSERT_MSG(exp, "")


#define XP_TODO() XP_ASSERT_DEFAULT(0)
#define UNREACHABLE() XP_ASSERT_MSG(0, "unreachable\n")


#ifndef cast
#define cast(new_type) (new_type)
#endif


#define XP_GET_PTR_FROM_MEMBER(member_ptr, parent_type, member_name) \
    ((parent_type *)((char *)(member_ptr) - offsetof(parent_type, member_name)))


#define XP_MAX(a, b) (a > b ? a : b)
#define XP_MIN(a, b) (a > b ? b : a)



// 常用常量
#define I32_MAX (cast(i32)(2147483647))
#define I32_MIN (cast(i32)(-2147483647 - 1))
#define I64_MAX (cast(i64)(9223372036854775807ll))
#define I64_MIN (cast(i64)(-9223372036854775807ll - 1ll))
#define U32_MAX (cast(u32)(4294967295u))
#define U32_MIN (cast(u32)(0u))
#define U64_MAX (cast(u64)(18446744073709551615ull))
#define U64_MIN (cast(u64)(0ull))


#define XP_PI 3.14159265358979323846f


#define XP_JOIN_2(x, y) x##y



/*
    常见函数
*/

xp_define u8 *xp_align_up(u8 *value, isize alignment);
xp_define u8 *xp_align_down(u8 *value, isize alignment);
xp_define isize xp_align_up_isize(isize value, isize alignment);
xp_define b32 xp_is_power_of_two(isize x);

xp_define b32 xp_is_space(char c);
xp_define b32 xp_is_digit_base_all(char c);

xp_define b32 xp_str_to_num_base(char const *str, u32 base, u64 *result);



#define xp_array_len(array) (sizeof(array) / sizeof(array[0]))


    /*
        内存操作
    */


xp_define void xp_memcpy_dst_loop(void *dst, isize dst_capacity, isize dst_offset, const void *src, isize src_count);






/*
    内存分配器API
*/
typedef enum xpAllocationType {
    xpAlloc,
    xpFree,
    xpFreeAll,
    xpRealloc,
} xpAllocationType;


#define XP_ALLOCATOR_PROC(name) void* name(    \
    void *allocator_data,                      \
    xpAllocationType type,                     \
    void *ptr,                                 \
    isize size,                                \
    isize old_size                             \
)
typedef XP_ALLOCATOR_PROC(xp_allocator_proc);


typedef struct xpAllocator {
    xp_allocator_proc *proc;

    // NOTE(xoaop): 为了实现泛型, 只能使用void* 指针, 这块内存的分配只能用堆内存分配
    void *data;
} xpAllocator;

xp_define void *xp_alloc(xpAllocator allocator, isize size);
xp_define void xp_free(xpAllocator allocator, void *ptr);
xp_define void *xp_realloc(xpAllocator allocator, void *ptr, isize new_size, isize old_size);
xp_define void xp_free_all(xpAllocator allocator);

xp_define void xp_zero(void *ptr, isize size);



// Heap Allocator包装
XP_ALLOCATOR_PROC(xp_heap_allocator_proc);
xp_define xpAllocator xp_pure_heap_allocator();
xp_define xpAllocator xp_heap_allocator();

// 默认分配器别名
#define xp_default_allocator() xp_heap_allocator()





// Arena Allocator内存分配器

// 8MB
#define MEMORY_BLOCK_DEFAULT_MIN_SIZE (8ll * 1024ll * 1024ll)

#define ALIGNMENT_DEFAULT (8ll)

typedef struct xpMemoryBlock {
    struct xpMemoryBlock *prev;
    struct xpMemoryBlock *next;

    u8 *base;
    isize size;
    isize used;
} xpMemoryBlock;

typedef struct xpArena {
    xpMemoryBlock *curr_block;
    isize block_size;


    b8 malloc;
} xpArena;

// ArenaSave 用来保存当前arena allocator的状态, 以便之后恢复, 适用于函数内临时分配的内存
typedef struct xpArenaSave {
    xpMemoryBlock *block;
    isize used;
} xpArenaSave;



xp_define void xp_arena_init_default(xpArena *arena);
xp_define void xp_arena_init(xpArena *arena, isize block_size);

xp_define xpArena xp_arena_default();
xp_define xpArena xp_arena_make(isize block_size);

XP_ALLOCATOR_PROC(xp_arena_allocator_proc);
xp_define xpAllocator xp_arena_allocator(xpArena *arena);

xp_define xpAllocator xp_arena_allocator_default();

xp_define void xp_arena_free_all(xpArena *arena);
xp_define void xp_arena_allocator_clear(xpAllocator allocator);

xp_define void xp_arena_allocator_debug_print(xpAllocator allocator);

// Arena Save/Restore 功能
xp_define xpArenaSave xp_arena_save(xpArena *arena);
xp_define void xp_arena_restore(xpArena *arena, xpArenaSave save);
xp_define void xp_arena_free_to_save(xpArena *arena, xpArenaSave save);

// 针对xpAllocator的便利函数
xp_define xpArenaSave xp_arena_allocator_save(xpAllocator allocator);
xp_define void xp_arena_allocator_restore(xpAllocator allocator, xpArenaSave save);
xp_define void xp_arena_allocator_free_to_save(xpAllocator allocator, xpArenaSave save);



/*
    平台抽象
*/


#if defined(__cplusplus)
} // for decl
#endif


// === 子模块 includes ===
// === begin: xoaop_defer.h ===


#if defined(__cplusplus) && __cplusplus >= 201703L

template<typename F>
struct xpDeferWrapper {

    xpDeferWrapper(F defer_func) : defer_func(defer_func) {

    }
    ~xpDeferWrapper() {
        defer_func();
    }
    F defer_func;
};

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x) DEFER_2(x, __COUNTER__)
#define defer(code) auto DEFER_3(__deFer__) = xpDeferWrapper([&]() { code; })

#endif // __cplusplus >= 201703L

// === end: xoaop_defer.h ===
// === begin: xoaop_pair.h ===


#if defined(__cplusplus)

template<typename A, typename B>
struct xpPair {
    A first;
    B second;
};

template<typename A, typename B>
xpPair<A, B> xp_make_pair(A first, B second) {
    xpPair<A, B> pair = {};
    pair.first = first;
    pair.second = second;
    return pair;
}

#endif // __cplusplus

// === end: xoaop_pair.h ===
// === begin: xoaop_option.h ===


#if defined(__cplusplus)

enum class xpOptionEnum {
    None,
    Some
};

template<typename T>
struct xpOption {
public:

    static xpOption<T> none() {
        return xpOption();
    }

    static xpOption<T> some(T value) {
        return xpOption(value);
    }

    xpOption() : kind(xpOptionEnum::None) {}
    xpOption(T val) : kind(xpOptionEnum::Some), value(val) {}

    bool has_value() {
        return kind == xpOptionEnum::Some;
    }

    bool is_none() {
        return kind == xpOptionEnum::None;
    }

    T unwrap() {
        XP_ASSERT_DEFAULT(kind == xpOptionEnum::Some);
        return value;
    }


    bool operator==(const xpOption<T> &other) const {
        if (kind != other.kind) {
            return false;
        }
        if (kind == xpOptionEnum::None) {
            return true; // 两个都是None
        }
        return value == other.value; // 比较Some的值
    }


private:
    xpOptionEnum kind;
    T value;
};

#endif // __cplusplus

// === end: xoaop_option.h ===
// === begin: xoaop_result.h ===


#include <concepts>

#if defined(__cplusplus)

template<typename F, typename I>
concept MatchHandler = requires(F handler, I && input) {
    { handler(input) } -> std::same_as<void>;
};


enum class xpResultType {
    Ok,
    Err
};


template<typename OkType, typename ErrType>
struct xpResult {
    static_assert(!std::is_same_v<OkType, ErrType>, "OkType and ErrType must be different types");


    static xpResult<OkType, ErrType> ok(OkType value) {
        return xpResult(value);
    }

    static xpResult<OkType, ErrType> err(ErrType error) {
        return xpResult(error);
    }

    bool is_ok() const {
        return type == xpResultType::Ok;
    }

    bool is_err() const {
        return type == xpResultType::Err;
    }

    OkType as_ok() const {
        XP_ASSERT_DEFAULT(is_ok());
        return ok_val;
    }

    ErrType as_err() const {
        XP_ASSERT_DEFAULT(is_err());
        return err_val;
    }


private:

    xpResult(OkType ok_value) : type(xpResultType::Ok), ok_val(ok_value) {}
    xpResult(ErrType err_value) : type(xpResultType::Err), err_val(err_value) {}

    xpResultType type;
    union {
        OkType ok_val;
        ErrType err_val;
    };
};

template<typename OkType, typename ErrType>
void match(xpResult<OkType, ErrType> &result, MatchHandler<OkType> auto &&ok_handler, MatchHandler<ErrType> auto &&err_handler) {
    if (result.is_ok()) {
        ok_handler(result.as_ok());
    } else if (result.is_err()) {
        err_handler(result.as_err());
    } else {
        UNREACHABLE();
    }
}

#endif // __cplusplus

// === end: xoaop_result.h ===
// === begin: xoaop_string.h ===


#include <string>
#include <string_view>
#if __cplusplus >= 202300L
#include <format>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/*
    字符串, 切片
*/

xp_define isize xp_strlen_c(char const *str);
xp_define char *xp_find_first_non_space(const char *str);


// NOTE(xoaop): ONLY ASCII NOW
typedef struct xpString {
    xpAllocator allocator;
    isize length;
    isize capacity;
    char *c_str;

#if defined(__cplusplus)
        xpString() = default;
        xpString(const char* str);
        xpString(xpAllocator allocator, const char* str);
        bool operator== (xpString other) const;
        char operator[] (isize index) const;
        char *as_c_str() const;
#endif // __cplusplus


} xpString;



typedef struct xpSlice {
    void *data;
    isize len;
} xpSlice;


xp_define xpString xp_string_c(char const *str);
xp_define xpString xp_make_string(xpAllocator allocator, char const *str);
xp_define xpString xp_make_string_capacity(xpAllocator allocator, void const *str, isize capacity);
xp_define xpString xp_make_string_count(xpAllocator allocator, char const *str, isize count);
xp_define xpString xp_make_string_zero();
xp_define xpString xp_string_to_c_style(xpString string, xpAllocator allocator);
xp_define xpString xp_make_string_from_slice(xpAllocator allocator, xpSlice slice);
xp_define xpString xp_string_copy(xpAllocator allocator, xpString string);
xp_define void xp_string_free(xpString string);
xp_define b32 xp_string_cmp(xpString a, xpString b);
xp_define b32 xp_string_equal(xpString a, xpString b);
xp_define xpString *xp_string_extend(xpString *string, isize extended_size);
xp_define isize xp_string_find_char(xpString str, char c);
xp_define xpString *xp_string_insert(isize pos, xpString *str, xpString inserted_str);
xp_define xpString *xp_string_append(xpString *str, xpString appended_str);
xp_define xpString *xp_string_append_char(xpString *str, char c);
xp_define xpString xp_isize_to_string(isize value, xpAllocator allocator);
xp_define xpString xp_string_replace_char(xpString string, char old_char, char new_char, xpAllocator allocator);



xp_define xpSlice xp_slice_make(void *data, isize len);
xp_define xpSlice xp_slice_make_from_string(xpString string, isize begin, isize len);

#if defined(__cplusplus)
}
#endif


#if defined(__cplusplus)

//
// xpString CPP 相关操作声明
//

xpString xp_string_concat_mid(xpString a, xpString b, xpOption<xpString> middle, xpAllocator allocator);


#if __cplusplus >= 202300L

template<>
struct std::formatter<xpString> : std::formatter<std::string_view> {
    using std::formatter<std::string_view>::parse;


    auto format(const xpString& s, std::format_context& ctx) const {
        return std::formatter<std::string_view>::format(
            std::string_view(s.c_str, static_cast<size_t>(s.length)),
            ctx
        );
    }
};

#endif // C++23

#endif // __cplusplus


#if defined(XOAOP_IMPLEMENTATION) && !defined(XOAOP_STRING_IMPLEMENTATION_DONE)
#define XOAOP_STRING_IMPLEMENTATION_DONE

#if defined(__cplusplus)
extern "C" {
#endif


//
// 字符串实现
//
isize xp_strlen_c(char const *str) {
    isize len = 0;
    for (; str[len] != '\0'; len += 1);

    //NOTE: for unknown wrong, maybe unnessery
    XP_ASSERT(len >= 0);
    return len;
}

void xp_strncpy_c(char *dst, const char *src, isize len) {
    if (len <= 0 || dst == NULL || src == NULL) {
        return;
    }

    isize index = 0;
    for (; index < len && src[index] != '\0'; index += 1) {
        dst[index] = src[index];
    }

    //NOTE(xoaop): 兼容c字符串
    dst[index] = '\0'; // 正确：在实际拷贝结束位置补零，而不是强制在len-1位置
}


char *xp_find_first_non_space(const char *str) {
    XP_ASSERT_DEFAULT(str != NULL);
    while (*str && isspace(cast(unsigned char) * str)) {
        str++;
    }
    return cast(char *)str;
}



xpString xp_string_c(char const *str) {
    XP_ASSERT_DEFAULT(str != NULL);

    xpString string;
    string.allocator = {NULL, NULL};
    string.length = xp_strlen_c(str);
    string.capacity = xp_strlen_c(str);
    string.c_str = cast(char *)str;
    return string;
}


xpString xp_make_string(xpAllocator allocator, char const *str) {
    return xp_make_string_capacity(allocator, str, xp_strlen_c(str));
}


xpString xp_make_string_capacity(xpAllocator allocator, void const *str, isize capacity) {
    XP_ASSERT_DEFAULT(capacity >= 0);


    xpString string;
    string.allocator = allocator;
    string.c_str = cast(char *) xp_alloc(string.allocator, capacity + 1); // NOTE: +1是为了末尾的'\0'
    string.capacity = capacity;

    // 计算实际长度
    isize length = 0;
    if (str != NULL) {
        length = xp_strlen_c(cast(char const *)str);
    }
    if (length > capacity) {
        length = capacity;
    }
    string.length = length;


    // 清空空间
    memset(string.c_str, '\0', string.capacity + 1); // NOTE: +1是为了末尾的'\0'
    // 复制字符串内容
    memcpy(string.c_str, str, string.length);

    // 末尾补上'\0'
    // string.c_str[length] = '\0';
    return string;
}

xpString xp_make_string_count(xpAllocator allocator, char const *str, isize count) {
    XP_ASSERT_DEFAULT(count >= 0);

    xpString string = xp_make_string_zero();
    string.allocator = allocator;

    // 计算实际长度
    isize actual_count = 0;
    if (str != NULL) {
        actual_count = xp_strlen_c(cast(char const *)str);
    }

    isize length = 0;
    if (actual_count < count) {
        length = actual_count;
    } else {
        length = count;
    }
    string.length = length;

    string.c_str = cast(char *) xp_alloc(string.allocator, length + 1); // NOTE: +1是为了末尾的'\0'
    string.capacity = length;

    // 复制字符串内容
    memcpy(string.c_str, str, string.length);

    // 末尾补上'\0'
    string.c_str[string.capacity] = '\0';


    return string;
}

xpString xp_make_string_zero() {
    xpString string;
    string.allocator = { NULL, NULL };
    string.c_str = NULL;
    string.length = 0;
    string.capacity = 0;
    return string;
}

xpString xp_string_to_c_style(xpString string, xpAllocator allocator) {
    xpString c_style_string = xp_make_string_count(allocator, string.c_str, string.length);
    return c_style_string;
}


void xp_string_free(xpString string) {
    if (string.allocator.proc != NULL) {
        xp_free(string.allocator, string.c_str);
    }
}

b32 xp_string_cmp(xpString a, xpString b) {
    if (a.length > b.length) {
        return 1;
    } else if (a.length < b.length) {
        return -1;
    }

    for (isize i = 0; i < a.length; i++) {
        if (a.c_str[i] != b.c_str[i]) {
            return (a.c_str[i] > b.c_str[i]) ? 1 : -1;
        }
    }

    return 0;
}

b32 xp_string_equal(xpString a, xpString b) {
    return xp_string_cmp(a, b) == 0;
}

xpString xp_string_copy(xpAllocator allocator, xpString string) {
    xpString string_copy;

    string_copy.allocator = allocator;
    string_copy.length = string.length;
    string_copy.capacity = string.length;

    string_copy.c_str = cast(char *) xp_alloc(string_copy.allocator, string_copy.capacity + 1);
    memcpy(string_copy.c_str, string.c_str, string.length);

    string_copy.c_str[string.length] = '\0'; // 末尾补上'\0'
    return string_copy;
}


xpString *xp_string_extend(xpString *string, isize extended_size) {
    if (extended_size <= 0) {
        return string;
    }

    char *new_c_str = cast(char *)xp_alloc(
        string->allocator,
        string->capacity + extended_size + 1 // +1是为了末尾的'\0'
    );
    memcpy(new_c_str, string->c_str, string->length);
    xp_free(string->allocator, string->c_str);

    string->c_str = new_c_str;

    string->capacity = string->capacity + extended_size;

    string->c_str[string->length] = '\0'; // 末尾补上'\0'


    return string;
}

isize xp_string_find_char(xpString str, char c) {
    for (isize i = 0; i < str.length; i++) {
        if (str.c_str[i] == c) {
            return i;
        }
    }
    return -1;
}

xpString *xp_string_insert(isize pos, xpString *str, xpString inserted_str) {
    XP_ASSERT_DEFAULT(pos >= 0);
    XP_ASSERT_DEFAULT(pos <= str->length);

    // 1. 扩容，确保插入后长度不会超过 capacity
    if (str->length + inserted_str.length > str->capacity) {
        xp_string_extend(str, inserted_str.length + (str->length + inserted_str.length - str->capacity));
    }

    XP_ASSERT_DEFAULT(str->length + inserted_str.length <= str->capacity);

    // 2. 后移原有数据
    if (pos < str->length) {
        isize moved_size = str->length - pos;
        memmove(str->c_str + pos + inserted_str.length, str->c_str + pos, moved_size);
    }

    // 3. 插入新字符串
    memcpy(str->c_str + pos, inserted_str.c_str, inserted_str.length);

    str->length += inserted_str.length;

    XP_ASSERT_DEFAULT(str->length <= str->capacity);

    // 4. 末尾补零
    str->c_str[str->length] = '\0';


    return str;
}

xpString *xp_string_append_char(xpString *str, char c) {
    xpString char_str = xp_make_string_zero();
    char_str.c_str = &c;
    char_str.length = 1;
    char_str.capacity = 1;

    return xp_string_append(str, char_str);
}

xpString *xp_string_append(xpString *str, xpString appended_str) {
    return xp_string_insert(str->length, str, appended_str);
}



xpString xp_isize_to_string(isize value, xpAllocator allocator) {
    isize len = snprintf(NULL, 0, "%lld", value);

    xpString string = xp_make_string_capacity(allocator, NULL, len);
    snprintf(string.c_str, string.capacity + 1, "%lld", value);

    string.length = len;

    return string;
}

xpString xp_string_replace_char(xpString string, char old_char, char new_char, xpAllocator allocator) {
    xpString new_string = xp_string_copy(allocator, string);

    for (isize i = 0; i < new_string.length; i++) {
        if (new_string.c_str[i] == old_char) {
            new_string.c_str[i] = new_char;
        }
    }

    return new_string;
}


xpString xp_make_string_from_slice(xpAllocator allocator, xpSlice slice) {
    return xp_make_string_capacity(allocator, slice.data, slice.len);
}


xpSlice xp_slice_make(void *data, isize len) {
    xpSlice slice;
    slice.data = data;
    slice.len = len;
    return slice;
}

xpSlice xp_slice_make_from_string(xpString string, isize begin, isize len) {
    if ((len < 0 || begin < 0) || ((begin + len) > string.length)) {
        XP_ASSERT(0);
    }

    xpSlice slice = {};
    slice.data = string.c_str + begin;
    slice.len = len;

    return slice;
}


#if defined(__cplusplus)
}
#endif


#if defined(__cplusplus)

//
// xpString CPP 部分实现
//


xpString::xpString(const char* str) {
    *this = xp_string_c(str);
}

xpString::xpString(xpAllocator allocator, const char* str) {
    *this = xp_make_string(allocator, str);
}

bool xpString::operator== (xpString other) const {
    b32 xp_string_cmp(xpString a, xpString b);
    return !xp_string_cmp(*this, other);
}

char xpString::operator[] (isize index) const {
    XP_ASSERT_DEFAULT(index >= 0 && index < length);
    return c_str[index];
}

char *xpString::as_c_str() const {
    // 目前都是c风格
    return c_str;
}



xpString xp_string_concat_mid(xpString a, xpString b, xpOption<xpString> middle, xpAllocator allocator) {
    xpString result = xp_string_copy(allocator, a);

    if (middle.has_value()) {
        xp_string_append(&result, middle.unwrap());
    }

    xp_string_append(&result, b);

    return result;
}

#endif // __cplusplus

#endif // XOAOP_IMPLEMENTATION

// === end: xoaop_string.h ===
// === begin: xoaop_hash.h ===


#include <string>
#include <functional>

/*
    哈希映射函数 (C API)
*/

#if defined(__cplusplus)
extern "C" {
#endif

xp_define u32 xp_murmur_hash3_32(const void *data, usize length, usize seed);
xp_define u64 xp_hash_combine_u64(u64 old_hash, u64 new_value);

#if defined(__cplusplus)
}
#endif


#if defined(__cplusplus)


// std::hash<xpString> 特化 — 支持 xpString 作为 HashMap/HashSet 的 key
namespace std {
template<>
struct hash<xpString> {
    size_t operator()(const xpString& s) const noexcept {
        return xp_murmur_hash3_32(s.c_str, static_cast<usize>(s.length), 0);
    }
};
}


// 哈希表槽位状态
typedef enum xpHashSlotState {
    XP_HASH_SLOT_EMPTY = 0,    // 空槽位
    XP_HASH_SLOT_USED = 1,     // 已使用
    XP_HASH_SLOT_TOMBSTONE = 2 // 墓碑：已删除，探测时跳过但不终止
} xpHashSlotState;


// 通用线性探测结果（不依赖具体条目类型）
struct LinearProbeResult {
    isize found_index = -1;       // 找到的匹配条目的索引（-1表示未找到）
    isize first_tombstone = -1;   // 第一个遇到的墓碑位置的索引（-1表示没有）
    isize first_empty = -1;       // 第一个空槽位置的索引（-1表示没有）
};


// 纯算法层面的通用线性探测：不依赖具体数据结构，只需要探测回调
// 回调函数签名：xpHashSlotState probe_callback(isize index, bool* out_key_match)
// 返回值：当前索引的槽位状态；out_key_match输出当前索引的key是否匹配目标key
// 注意: capacity 必须是 2 的幂，使用位运算代替取模
template<typename ProbeCallback>
LinearProbeResult linear_probe(usize hash_value, isize capacity, ProbeCallback&& callback) {
    LinearProbeResult result = {};

    if (capacity <= 0) {
        return result;
    }

    usize mask = static_cast<usize>(capacity) - 1;
    usize index = hash_value & mask;
    usize original_index = index;

    // 至少执行一次探测
    for (;;) {
        bool key_match = false;
        xpHashSlotState state = callback(static_cast<isize>(index), &key_match);

        if (state == XP_HASH_SLOT_EMPTY) {
            // 记录第一个空槽位置
            if (result.first_empty == -1) {
                result.first_empty = static_cast<isize>(index);
            }

            // 探测链结束，没有找到匹配
            break;
        } else if (state == XP_HASH_SLOT_TOMBSTONE) {
            // 记录第一个墓碑位置
            if (result.first_tombstone == -1) {
                result.first_tombstone = static_cast<isize>(index);
            }

        } else if (state == XP_HASH_SLOT_USED && key_match) {
            // 找到匹配的key
            result.found_index = static_cast<isize>(index);
            break;
        }

        index = (index + 1) & mask;

        // 防止无限循环
        if (index == original_index) {
            break;
        }
    }

    return result;
}

#define END_OF_HASH_MAP_INDEX -1

#endif // __cplusplus


#if defined(XOAOP_IMPLEMENTATION) && !defined(XOAOP_HASH_IMPLEMENTATION_DONE)
#define XOAOP_HASH_IMPLEMENTATION_DONE

#if defined(__cplusplus)
extern "C" {
#endif

//
// 哈希映射函数实现
//


//NOTE: 可以移到常用函数里
xp_internal u32 rotate_left(u32 value, i32 shift) {
    const i32 bits = sizeof(u32) * 8;  // 32 位
    shift %= bits;  // 确保 shift 在 0 到 31 之间
    return (value << shift) | (value >> (bits - shift));
}

u32 xp_murmur_hash3_32(const void *data, usize length, usize seed) {
    const u8 *key = cast(const u8 *)(data);
    const usize nblocks = length / 4;

    u32 h1 = seed;

    const u32 c1 = 0xcc9e2d51;
    const u32 c2 = 0x1b873593;

    // 处理 4 字节块
    const u32 *blocks = cast(u32 *)key;
    for (usize i = 0; i < nblocks; ++i) {
        u32 k1 = blocks[i];

        k1 *= c1;
        k1 = rotate_left(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = rotate_left(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

    // 处理剩余字节
    const u8 *tail = key + nblocks * 4;
    u32 k1 = 0;

    switch (length & 3) {
    case 3: k1 ^= tail[2] << 16;
    case 2: k1 ^= tail[1] << 8;
    case 1: k1 ^= tail[0];
        k1 *= c1;
        k1 = rotate_left(k1, 15);
        k1 *= c2;
        h1 ^= k1;
    }

    // 最终混合
    h1 ^= length;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

// 一个简单的hash combine函数
u64 xp_hash_combine_u64(u64 old_hash, u64 new_value) {
    static const u64 HASH_SEED = 0x517cc1b727220a95;
    static const u64 K = 0x9e3779b97f4a7c15;

    u64 z = new_value + K + (old_hash << 6) + (old_hash >> 2);
    z ^= (z >> 33);
    z *= 0xff51afd7ed558ccd;
    z ^= (z >> 33);
    z *= 0xc4ceb9fe1a85ec53;
    z ^= (z >> 33);
    return old_hash ^ z;
}

#if defined(__cplusplus)
}
#endif


#endif // XOAOP_IMPLEMENTATION

// === end: xoaop_hash.h ===
// === begin: xoaop_hashmap.h ===


#if defined(__cplusplus)

template<typename K, typename V>
struct xpHashMapEntry {
    xpHashSlotState state;
    K key;
    V value;
};

template<typename K, typename V>
struct xpHashMapIterator;

template<typename K, typename V>
struct xpHashMap {

    xpAllocator allocator;
    xpHashMapEntry<K, V> *entries;
    isize count;
    isize capacity;

    // 方法声明 —— 实现在文件末尾 (C API 之后)
    static xpHashMap make(xpAllocator a);
    void free();
    xpHashMap copy(xpAllocator a) const;
    V *insert(K key, V value);
    xpHashMapEntry<K, V> *get_entry(K key) const;
    V& operator[](K key);
    b32 remove(K key);
    void clear();

    xpHashMapIterator<K, V> begin() const;
    xpHashMapIterator<K, V> end() const;
};

// --- 迭代器 ---

template<typename K, typename V>
struct xpHashMapIterator {
    const xpHashMapEntry<K, V>* entries;
    isize capacity;
    isize index;

    bool operator!=(const xpHashMapIterator& o) const { return index != o.index; }

    void operator++() {
        index++;
        while (index < capacity && entries[index].state != XP_HASH_SLOT_USED)
            index++;
    }

    const xpHashMapEntry<K, V>& operator*() const { return entries[index]; }
    const xpHashMapEntry<K, V>* operator->() const { return &entries[index]; }
};

// --- 内部: 扩容 ---

template<typename K, typename V>
void xp_hash_map_extend(xpHashMap<K, V> *map, isize new_capacity) {
    XP_ASSERT_MSG(new_capacity > map->capacity, "extend: new_capacity must be larger");

    if (map->entries == NULL) {
        map->entries = (xpHashMapEntry<K, V> *) xp_alloc(map->allocator, sizeof(xpHashMapEntry<K, V>) * new_capacity);
        for (isize i = 0; i < new_capacity; ++i) {
            map->entries[i].state = XP_HASH_SLOT_EMPTY;
        }
    } else {
        xpHashMapEntry<K, V> *new_entries = (xpHashMapEntry<K, V> *) xp_alloc(map->allocator, sizeof(xpHashMapEntry<K, V>) * new_capacity);
        for (isize i = 0; i < new_capacity; ++i) {
            new_entries[i].state = XP_HASH_SLOT_EMPTY;
        }

        usize mask = static_cast<usize>(new_capacity) - 1;
        for (isize i = 0; i < map->capacity; ++i) {
            xpHashMapEntry<K, V> *old_entry = &map->entries[i];
            if (old_entry->state == XP_HASH_SLOT_USED) {
                usize hash_value = std::hash<K>{}(old_entry->key);
                usize index = hash_value & mask;
                while (new_entries[index].state == XP_HASH_SLOT_USED) {
                    index = (index + 1) & mask;
                }
                new (&new_entries[index].key) K(std::move(old_entry->key));
                new (&new_entries[index].value) V(std::move(old_entry->value));
                new_entries[index].state = XP_HASH_SLOT_USED;

                old_entry->key.~K();
                old_entry->value.~V();
                old_entry->state = XP_HASH_SLOT_EMPTY;
            }
        }
        xp_free(map->allocator, map->entries);
        map->entries = new_entries;
    }

    map->capacity = new_capacity;
}

// --- 内部: 线性探测 ---

template<typename K, typename V>
LinearProbeResult xp_hash_map_linear_probe(const xpHashMap<K, V> *map, K key) {
    if (map->capacity == 0 || map->entries == nullptr) {
        return {};
    }

    usize hash_value = std::hash<K>{}(key);

    return linear_probe(hash_value, map->capacity, [&](isize index, bool* out_key_match) {
        const xpHashMapEntry<K, V>* entry = &map->entries[index];
        *out_key_match = false;
        if (entry->state == XP_HASH_SLOT_USED) {
            *out_key_match = (entry->key == key);
        }
        return entry->state;
    });
}

// === C API ===

template<typename K, typename V>
xpHashMap<K, V> xp_hash_map_make(xpAllocator allocator) {
    xpHashMap<K, V> map = {};
    map.allocator = allocator;
    return map;
}

template<typename K, typename V>
void xp_hash_map_free(xpHashMap<K, V> map) {
    if (map.entries != NULL) {
        for (isize i = 0; i < map.capacity; ++i) {
            if (map.entries[i].state == XP_HASH_SLOT_USED) {
                map.entries[i].key.~K();
                map.entries[i].value.~V();
            }
        }
        xp_free(map.allocator, map.entries);
    }
}

template<typename K, typename V>
xpHashMap<K, V> xp_hash_map_copy(const xpHashMap<K, V> *o, xpAllocator allocator) {
    xpHashMap<K, V> copy = {};
    copy.allocator = allocator;
    copy.count = o->count;
    copy.capacity = o->capacity;

    if (o->capacity > 0 && o->entries != NULL) {
        copy.entries = xp_alloc_array<xpHashMapEntry<K, V>>(allocator, copy.capacity);
        for (isize i = 0; i < copy.capacity; ++i) {
            copy.entries[i].state = o->entries[i].state;
            if (o->entries[i].state == XP_HASH_SLOT_USED) {
                new (&copy.entries[i].key) K(o->entries[i].key);
                new (&copy.entries[i].value) V(o->entries[i].value);
            }
        }
    }

    return copy;
}

template<typename K, typename V>
V *xp_hash_map_insert(xpHashMap<K, V> *map, K key, V value) {
    if (map->capacity == 0 || (double)map->count / (double)map->capacity >= 0.7) {
        isize new_cap = map->capacity == 0 ? 8 : map->capacity * 2;
        xp_hash_map_extend(map, new_cap);
    }

    auto probe_result = xp_hash_map_linear_probe(map, key);

    if (probe_result.found_index != -1) {
        map->entries[probe_result.found_index].value = value;
        return nullptr;
    }

    isize insert_index = -1;
    if (probe_result.first_tombstone != -1) {
        insert_index = probe_result.first_tombstone;
    } else if (probe_result.first_empty != -1) {
        insert_index = probe_result.first_empty;
    }

    XP_ASSERT_MSG(insert_index != -1, "insert: map is full after expansion");
    if (insert_index == -1) {
        return nullptr;
    }

    xpHashMapEntry<K, V>* insert_entry = &map->entries[insert_index];
    new (&insert_entry->key) K(key);
    new (&insert_entry->value) V(value);
    insert_entry->state = XP_HASH_SLOT_USED;

    map->count += 1;
    return &insert_entry->value;
}

template<typename K, typename V>
xpHashMapEntry<K, V> *xp_hash_map_get_entry(xpHashMap<K, V> map, K key) {
    if (map.count == 0 || map.capacity == 0 || map.entries == nullptr) {
        return NULL;
    }

    auto probe_result = xp_hash_map_linear_probe(&map, key);
    if (probe_result.found_index == -1) {
        return nullptr;
    }
    return &map.entries[probe_result.found_index];
}

template<typename K, typename V>
V *xp_hash_map_get(xpHashMap<K, V> map, K key) {
    xpHashMapEntry<K, V> *entry = xp_hash_map_get_entry(map, key);
    return entry ? &entry->value : nullptr;
}

template<typename K, typename V>
V *xp_hash_map_set(xpHashMap<K, V> *map, K key, V value) {
    xpHashMapEntry<K, V> *entry = xp_hash_map_get_entry(*map, key);
    if (entry != NULL) {
        entry->value = value;
        return &entry->value;
    }
    return NULL;
}

template<typename K, typename V>
b32 xp_hash_map_remove(xpHashMap<K, V> *map, K key) {
    xpHashMapEntry<K, V> *entry = xp_hash_map_get_entry(*map, key);
    if (entry != NULL) {
        entry->key.~K();
        entry->value.~V();
        entry->state = XP_HASH_SLOT_TOMBSTONE;
        map->count -= 1;
        return true;
    }
    return false;
}

template<typename K, typename V>
void xp_hash_map_clear(xpHashMap<K, V> *map) {
    if (map->entries == NULL) return;

    for (isize i = 0; i < map->capacity; ++i) {
        if (map->entries[i].state == XP_HASH_SLOT_USED) {
            map->entries[i].key.~K();
            map->entries[i].value.~V();
            map->entries[i].state = XP_HASH_SLOT_EMPTY;
        } else if (map->entries[i].state == XP_HASH_SLOT_TOMBSTONE) {
            map->entries[i].state = XP_HASH_SLOT_EMPTY;
        }
    }

    map->count = 0;
}

// --- 方法实现 (调用上面的 C API) ---

template<typename K, typename V>
xpHashMap<K, V> xpHashMap<K, V>::make(xpAllocator a) {
    return xp_hash_map_make<K, V>(a);
}

template<typename K, typename V>
void xpHashMap<K, V>::free() {
    xp_hash_map_free(*this);
}

template<typename K, typename V>
xpHashMap<K, V> xpHashMap<K, V>::copy(xpAllocator a) const {
    return xp_hash_map_copy(this, a);
}

template<typename K, typename V>
V *xpHashMap<K, V>::insert(K key, V value) {
    return xp_hash_map_insert(this, key, value);
}

template<typename K, typename V>
xpHashMapEntry<K, V> *xpHashMap<K, V>::get_entry(K key) const {
    return xp_hash_map_get_entry(*this, key);
}

template<typename K, typename V>
V& xpHashMap<K, V>::operator[](K key) {
    V *v = xp_hash_map_get(*this, key);
    XP_ASSERT_MSG(v != nullptr, "operator[]: key not found");
    return *v;
}

template<typename K, typename V>
b32 xpHashMap<K, V>::remove(K key) {
    return xp_hash_map_remove(this, key);
}

template<typename K, typename V>
void xpHashMap<K, V>::clear() {
    xp_hash_map_clear(this);
}

template<typename K, typename V>
xpHashMapIterator<K, V> xpHashMap<K, V>::begin() const {
    isize idx = 0;
    while (idx < capacity && entries[idx].state != XP_HASH_SLOT_USED)
        idx++;
    return {entries, capacity, idx};
}

template<typename K, typename V>
xpHashMapIterator<K, V> xpHashMap<K, V>::end() const {
    return {entries, capacity, capacity};
}

#endif // __cplusplus

// === end: xoaop_hashmap.h ===
// === begin: xoaop_hashset.h ===


#if defined(__cplusplus)

template<typename K>
struct xpHashSetEntry {
    xpHashSlotState state;
    K key;
};

template<typename K>
struct xpHashSetIterator;

template<typename K>
struct xpHashSet {
    xpAllocator allocator;
    xpHashSetEntry<K> *entries;
    isize count;
    isize capacity;

    // 方法声明 —— 实现在文件末尾 (C API 之后)
    static xpHashSet make(xpAllocator a);
    void free();
    xpHashSet copy(xpAllocator a) const;
    K *insert(K key);
    b32 contains(K key) const;
    const K& operator[](K key) const;
    b32 remove(K key);
    void clear();

    xpHashSetIterator<K> begin() const;
    xpHashSetIterator<K> end() const;
};

// --- 迭代器 ---

template<typename K>
struct xpHashSetIterator {
    const xpHashSetEntry<K>* entries;
    isize capacity;
    isize index;

    bool operator!=(const xpHashSetIterator& o) const { return index != o.index; }

    void operator++() {
        index++;
        while (index < capacity && entries[index].state != XP_HASH_SLOT_USED)
            index++;
    }

    const xpHashSetEntry<K>& operator*() const { return entries[index]; }
    const xpHashSetEntry<K>* operator->() const { return &entries[index]; }
};

// --- 内部: 线性探测 ---

template<typename K>
LinearProbeResult xp_hash_set_linear_probe(const xpHashSet<K> *set, K key) {
    if (set->capacity == 0 || set->entries == nullptr) {
        return {};
    }

    usize hash_value = std::hash<K>{}(key);
    return linear_probe(hash_value, set->capacity, [&](isize index, bool* out_key_match) {
        const xpHashSetEntry<K>* entry = &set->entries[index];
        *out_key_match = false;
        if (entry->state == XP_HASH_SLOT_USED) {
            *out_key_match = (entry->key == key);
        }
        return entry->state;
    });
}

// --- 内部: 扩容 ---

template<typename K>
void xp_hash_set_extend(xpHashSet<K> *set, isize new_capacity) {
    XP_ASSERT_MSG(new_capacity > set->capacity, "extend: new_capacity must be larger");

    if (set->entries == NULL) {
        set->entries = (xpHashSetEntry<K> *) xp_alloc(set->allocator, sizeof(xpHashSetEntry<K>) * new_capacity);
        for (isize i = 0; i < new_capacity; ++i) {
            set->entries[i].state = XP_HASH_SLOT_EMPTY;
        }
    } else {
        xpHashSetEntry<K> *new_entries = (xpHashSetEntry<K> *) xp_alloc(set->allocator, sizeof(xpHashSetEntry<K>) * new_capacity);
        for (isize i = 0; i < new_capacity; ++i) {
            new_entries[i].state = XP_HASH_SLOT_EMPTY;
        }

        usize mask = static_cast<usize>(new_capacity) - 1;
        for (isize i = 0; i < set->capacity; ++i) {
            xpHashSetEntry<K> *old_entry = &set->entries[i];
            if (old_entry->state == XP_HASH_SLOT_USED) {
                usize hash_value = std::hash<K>{}(old_entry->key);
                usize index = hash_value & mask;
                while (new_entries[index].state == XP_HASH_SLOT_USED) {
                    index = (index + 1) & mask;
                }
                new (&new_entries[index].key) K(std::move(old_entry->key));
                new_entries[index].state = XP_HASH_SLOT_USED;

                old_entry->key.~K();
                old_entry->state = XP_HASH_SLOT_EMPTY;
            }
        }
        xp_free(set->allocator, set->entries);
        set->entries = new_entries;
    }

    set->capacity = new_capacity;
}

// === C API ===

template<typename K>
xpHashSet<K> xp_hash_set_make(xpAllocator allocator) {
    xpHashSet<K> hash_set = {};
    hash_set.allocator = allocator;
    return hash_set;
}

template<typename K>
void xp_hash_set_free(xpHashSet<K> set) {
    if (set.entries != NULL) {
        for (isize i = 0; i < set.capacity; ++i) {
            if (set.entries[i].state == XP_HASH_SLOT_USED) {
                set.entries[i].key.~K();
            }
        }
        xp_free(set.allocator, set.entries);
    }
}

template<typename K>
xpHashSet<K> xp_hash_set_copy(xpHashSet<K> *set, xpAllocator allocator) {
    xpHashSet<K> copy = {};
    copy.allocator = allocator;
    copy.count = set->count;
    copy.capacity = set->capacity;

    if (set->capacity > 0 && set->entries != NULL) {
        copy.entries = xp_alloc_array<xpHashSetEntry<K>>(allocator, copy.capacity);
        for (isize i = 0; i < copy.capacity; ++i) {
            copy.entries[i].state = XP_HASH_SLOT_EMPTY;
        }

        for (isize i = 0; i < set->capacity; ++i) {
            const xpHashSetEntry<K> *src_entry = &set->entries[i];
            xpHashSetEntry<K> *dst_entry = &copy.entries[i];

            dst_entry->state = src_entry->state;
            if (src_entry->state == XP_HASH_SLOT_USED) {
                new (&dst_entry->key) K(src_entry->key);
            }
        }
    }

    return copy;
}

template<typename K>
K *xp_hash_set_insert(xpHashSet<K> *set, K key) {
    if (set->capacity == 0 || (double)set->count / (double)set->capacity >= 0.7) {
        isize new_cap = set->capacity == 0 ? 8 : set->capacity * 2;
        xp_hash_set_extend(set, new_cap);
    }

    auto probe_result = xp_hash_set_linear_probe(set, key);

    if (probe_result.found_index != -1) {
        return nullptr;
    }

    isize insert_index = -1;
    if (probe_result.first_tombstone != -1) {
        insert_index = probe_result.first_tombstone;
    } else if (probe_result.first_empty != -1) {
        insert_index = probe_result.first_empty;
    }

    XP_ASSERT_MSG(insert_index != -1, "insert: set is full after expansion");
    if (insert_index == -1) {
        return nullptr;
    }

    xpHashSetEntry<K>* insert_entry = &set->entries[insert_index];
    new (&insert_entry->key) K(key);
    insert_entry->state = XP_HASH_SLOT_USED;

    set->count += 1;
    return &insert_entry->key;
}

template<typename K>
xpHashSetEntry<K> *xp_hash_set_get_entry(xpHashSet<K> *set, K key) {
    if (set->count == 0 || set->capacity == 0 || set->entries == nullptr) {
        return NULL;
    }

    auto probe_result = xp_hash_set_linear_probe(set, key);
    if (probe_result.found_index == -1) {
        return nullptr;
    }
    return &set->entries[probe_result.found_index];
}

template<typename K>
K *xp_hash_set_get(xpHashSet<K> *set, K key) {
    xpHashSetEntry<K> *entry = xp_hash_set_get_entry(set, key);
    return entry ? &entry->key : NULL;
}

template<typename K>
b32 xp_hash_set_find(xpHashSet<K> *set, K key) {
    return xp_hash_set_get_entry(set, key) != NULL;
}

template<typename K>
b32 xp_hash_set_remove(xpHashSet<K> *set, K key) {
    xpHashSetEntry<K> *entry = xp_hash_set_get_entry(set, key);
    if (entry != NULL) {
        entry->key.~K();
        entry->state = XP_HASH_SLOT_TOMBSTONE;
        set->count -= 1;
        return true;
    }
    return false;
}

template<typename K>
void xp_hash_set_clear(xpHashSet<K> *set) {
    if (set->entries == NULL || set->capacity <= 0) {
        set->count = 0;
        return;
    }

    for (isize i = 0; i < set->capacity; ++i) {
        xpHashSetEntry<K> *entry = &set->entries[i];
        if (entry->state == XP_HASH_SLOT_USED) {
            entry->key.~K();
            entry->state = XP_HASH_SLOT_EMPTY;
        } else if (entry->state == XP_HASH_SLOT_TOMBSTONE) {
            entry->state = XP_HASH_SLOT_EMPTY;
        }
    }

    set->count = 0;
}

// --- 方法实现 (调用上面的 C API) ---

template<typename K>
xpHashSet<K> xpHashSet<K>::make(xpAllocator a) {
    return xp_hash_set_make<K>(a);
}

template<typename K>
void xpHashSet<K>::free() {
    xp_hash_set_free(*this);
}

template<typename K>
xpHashSet<K> xpHashSet<K>::copy(xpAllocator a) const {
    return xp_hash_set_copy(this, a);
}

template<typename K>
K *xpHashSet<K>::insert(K key) {
    return xp_hash_set_insert(this, key);
}

template<typename K>
b32 xpHashSet<K>::contains(K key) const {
    return xp_hash_set_find(const_cast<xpHashSet<K>*>(this), key);
}

template<typename K>
const K& xpHashSet<K>::operator[](K key) const {
    K *v = xp_hash_set_get(const_cast<xpHashSet<K>*>(this), key);
    XP_ASSERT_MSG(v != nullptr, "operator[]: key not found in set");
    return *v;
}

template<typename K>
b32 xpHashSet<K>::remove(K key) {
    return xp_hash_set_remove(this, key);
}

template<typename K>
void xpHashSet<K>::clear() {
    xp_hash_set_clear(this);
}

template<typename K>
xpHashSetIterator<K> xpHashSet<K>::begin() const {
    isize idx = 0;
    while (idx < capacity && entries[idx].state != XP_HASH_SLOT_USED)
        idx++;
    return {entries, capacity, idx};
}

template<typename K>
xpHashSetIterator<K> xpHashSet<K>::end() const {
    return {entries, capacity, capacity};
}

#endif // __cplusplus

// === end: xoaop_hashset.h ===
// === begin: xoaop_intern.h ===


#if defined(__cplusplus)

template<typename T>
struct xpInterningEntry {
    T *key_ptr;
    b8 used;
};


template<typename T, size_t CAPACITY>
struct xpInterningTable {

    xpArena arena; // 只能是独立的ArenaAllocator
    xpAllocator allocator; // 只能是独立的ArenaAllocator

    xpInterningEntry<T> buckets[CAPACITY];

    isize count;


    static constexpr size_t capacity = CAPACITY;
    static_assert((CAPACITY & (CAPACITY - 1)) == 0, "InterningTable CAPACITY must be a power of 2");
};

template<typename T, size_t CAPACITY>
void xp_interning_table_init(xpInterningTable<T, CAPACITY> *table) {
    xp_arena_init_default(&table->arena);
    table->allocator = xp_arena_allocator(&table->arena);

    for (isize i = 0; i < CAPACITY; ++i) {
        table->buckets[i].used = false;
    }
    table->count = 0;
}

template<typename T, size_t CAPACITY>
void xp_interning_table_free(xpInterningTable<T, CAPACITY> *table) {
    xp_free_all(table->allocator);
}


template<typename T, size_t CAPACITY>
LinearProbeResult xp_interning_table_linear_probe(xpInterningTable<T, CAPACITY> *table, T key) {
    usize hash_value = std::hash<T>{}(key);
    return linear_probe(hash_value, xpInterningTable<T, CAPACITY>::capacity, [&](isize index, bool* out_key_match) {
        xpInterningEntry<T>* entry = &table->buckets[index];
        // 只有已使用的条目才能比较key
        *out_key_match = false;
        if (entry->used) {
            *out_key_match = (*(entry->key_ptr) == key);
        }
        return entry->used ? XP_HASH_SLOT_USED : XP_HASH_SLOT_EMPTY;
    });
}


template<typename T, size_t CAPACITY>
xpInterningEntry<T> *xp_interning_table_get_entry(xpInterningTable<T, CAPACITY> *table, T key) {
    if(table->count == 0) {
        return NULL;
    }

    auto probe_result = xp_interning_table_linear_probe(table, key);
    if(probe_result.found_index == -1) {
        return nullptr;
    }

    return &table->buckets[probe_result.found_index];
}


template<typename T, size_t CAPACITY>
T *xp_interning_table_get(xpInterningTable<T, CAPACITY> *table, T key) {
    xpInterningEntry<T> *entry = xp_interning_table_get_entry(table, key);
    return entry ? entry->key_ptr : NULL;
}


template<typename T, size_t CAPACITY>
T *xp_interning_table_insert(xpInterningTable<T, CAPACITY> *table, T key) {
    if (table->count >= table->capacity) {
        return NULL; // 满了
    }

    auto probe_result = xp_interning_table_linear_probe(table, key);

    if(probe_result.found_index == -1){
        xpInterningEntry<T> *entry = &table->buckets[probe_result.first_empty];

        T *stored_key_ptr = xp_alloc<T>(table->allocator);
        *stored_key_ptr = key;

        entry->key_ptr = stored_key_ptr;
        entry->used = true;

        table->count += 1;
        return stored_key_ptr;
    } else {
        return nullptr; // 重复插入当作失败处理
    }

    //NOTE: FULL MAP
    XP_ASSERT_DEFAULT(0);
    return NULL;
}

#endif // __cplusplus

// === end: xoaop_intern.h ===



/*
    CPP PART
*/
#if defined(__cplusplus)

//
// 头文件
//

#include <functional> // TODO(xoaop): REMOVE
#include <string>
#include <string_view>
#include <concepts>


#if __cplusplus >= 202300L

#include <print>

#endif // __cplusplus >= 202300L

namespace xoaop {

// ── HeapAllocator ── 全局堆分配器，所有实例等价，无状态
template<typename T>
struct HeapAllocator {
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_swap            = std::false_type;
    using is_always_equal                        = std::true_type;

    HeapAllocator() noexcept = default;

    template<typename U>
    HeapAllocator(const HeapAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        return static_cast<T*>(xp_alloc(xp_heap_allocator(), isize(n * sizeof(T))));
    }

    void deallocate(T* p, std::size_t) noexcept {
        xp_free(xp_heap_allocator(), p);
    }

    friend bool operator==(const HeapAllocator&, const HeapAllocator&) noexcept { return true; }
    friend bool operator!=(const HeapAllocator&, const HeapAllocator&) noexcept { return false; }
};

// ── ArenaAllocator ── Arena 分配器，有状态，绑定特定 arena
template<typename T>
struct ArenaAllocator {
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap            = std::true_type;
    using is_always_equal                        = std::false_type;

    xpAllocator allocator;

    ArenaAllocator() = delete;
    explicit ArenaAllocator(xpAllocator a) noexcept : allocator(a) {}

    template<typename U>
    ArenaAllocator(const ArenaAllocator<U>& other) noexcept : allocator(other.allocator) {}

    T* allocate(std::size_t n) {
        return static_cast<T*>(xp_alloc(allocator, isize(n * sizeof(T))));
    }

    void deallocate(T* p, std::size_t) noexcept {
        xp_free(allocator, p);
    }

    // 重置 arena 使用位置，保留已分配块以供复用（不归还堆）
    void clear() noexcept {
        xp_arena_allocator_clear(allocator);
    }

    // 释放 arena 所有内存块归还堆
    void free_all() noexcept {
        xp_free_all(allocator);
    }

    friend bool operator==(const ArenaAllocator& a, const ArenaAllocator& b) noexcept {
        return a.allocator.proc == b.allocator.proc && a.allocator.data == b.allocator.data;
    }
    friend bool operator!=(const ArenaAllocator& a, const ArenaAllocator& b) noexcept {
        return !(a == b);
    }
};

// 旧代码兼容别名
template<typename T>
using Allocator = HeapAllocator<T>;

}



//
// Heap Record Allocator 带监控的堆内存分配器
//

struct xpHeapRecordEntry {
    void* ptr;             // 内存块指针
    isize size;            // 分配大小
    const char* file;      // 分配所在文件（可选）
    isize line;            // 分配所在行号（可选）
    u64 timestamp;         // 分配时间戳（可选）
};

struct xpHeapRecordStats {
    isize total_allocs;    // 总分配次数
    isize total_frees;     // 总释放次数
    isize current_allocs;  // 当前未释放的分配次数
    isize total_bytes;     // 总分配字节数
    isize freed_bytes;     // 总释放字节数
    isize current_bytes;   // 当前未释放的字节数
    isize peak_bytes;      // 峰值内存使用
};

struct xpHeapRecordAllocator;

// 函数声明
xp_define xpAllocator xp_heap_record_allocator();                  // 获取全局单例监控分配器
xp_define xpAllocator xp_heap_record_allocator_create_new();      // 创建新的独立监控分配器
xp_define xpAllocator xp_heap_record_allocator_with_location_tracking(); // 获取全局单例（带位置跟踪）
xp_define xpAllocator xp_heap_record_allocator_create_new_with_location_tracking(); // 创建新的独立监控分配器（带位置跟踪）

// 针对全局单例的便利函数（不需要传分配器参数）
xp_define xpHeapRecordStats xp_heap_record_get_stats();
xp_define void xp_heap_record_print_stats();
xp_define void xp_heap_record_print_leaks();
xp_define isize xp_heap_record_check_leaks();
xp_define void xp_heap_record_destroy_global(b8 report_leaks);    // 销毁全局单例

// 针对独立分配器的函数
xp_define xpHeapRecordStats xp_heap_record_get_stats(xpAllocator allocator);
xp_define void xp_heap_record_print_stats(xpAllocator allocator);
xp_define void xp_heap_record_print_leaks(xpAllocator allocator);
xp_define isize xp_heap_record_check_leaks(xpAllocator allocator);
xp_define void xp_heap_record_destroy(xpAllocator allocator, b8 report_leaks);

//
// char32_t 操作
//
size_t xp_strlen_char32(const char32_t *s);






/*
Macros
*/

// 为了标识实现作为模板函数的接口
#define IMPL_TP template<>



template<typename T>
void xp_zero(T *ptr) {
    memset(ptr, 0, sizeof(T));
}

template<typename T>
T *xp_alloc(xpAllocator allocator) {
    return (T *)xp_alloc(allocator, sizeof(T));
}

template<typename T>
T *xp_alloc_array(xpAllocator allocator, isize capacity) {
    return cast(T *)xp_alloc(allocator, sizeof(T) * capacity);
}

template<typename T>
void xp_zero_array(void *array, isize capacity) {
    xp_zero(array, sizeof(T) * capacity);
}


// RAII wrapper for automatic arena state restore when leaving scope
// 自动恢复arena状态的RAII封装，离开作用域时自动调用恢复函数
class xpAutoArenaRestore {
public:
    enum class Mode {
        RestoreOnly,    // 仅重置指针，不释放后续内存块，性能更高
        FreeToSave      // 释放保存点之后分配的所有内存块，适合大内存临时分配
    };

    // 从xpArena*构造
    explicit xpAutoArenaRestore(xpArena* arena, Mode mode = Mode::RestoreOnly)
        : arena_(arena), mode_(mode), cancelled_(false) {
        XP_ASSERT_DEFAULT(arena_ != nullptr);
        save_ = xp_arena_save(arena_);
    }

    // 从xpAllocator构造
    explicit xpAutoArenaRestore(xpAllocator allocator, Mode mode = Mode::RestoreOnly)
        : mode_(mode), cancelled_(false) {
        XP_ASSERT_DEFAULT(allocator.proc == xp_arena_allocator_proc);
        XP_ASSERT_DEFAULT(allocator.data != nullptr);
        arena_ = static_cast<xpArena*>(allocator.data);
        save_ = xp_arena_save(arena_);
    }

    // 禁止拷贝
    xpAutoArenaRestore(const xpAutoArenaRestore&) = delete;
    xpAutoArenaRestore& operator=(const xpAutoArenaRestore&) = delete;

    // 支持移动
    xpAutoArenaRestore(xpAutoArenaRestore&& other) noexcept
        : arena_(other.arena_), save_(other.save_), mode_(other.mode_), cancelled_(other.cancelled_) {
        other.cancelled_ = true; // 转移所有权
    }

    xpAutoArenaRestore& operator=(xpAutoArenaRestore&& other) noexcept {
        if (this != &other) {
            // 先恢复自己的状态
            if (!cancelled_) {
                DoRestore();
            }
            // 转移状态
            arena_ = other.arena_;
            save_ = other.save_;
            mode_ = other.mode_;
            cancelled_ = other.cancelled_;
            other.cancelled_ = true;
        }
        return *this;
    }

    // 取消自动恢复，提交当前的分配
    void Cancel() {
        cancelled_ = true;
    }

    // 手动触发恢复（如果需要提前恢复）
    void Restore() {
        DoRestore();
        cancelled_ = true;
    }

    ~xpAutoArenaRestore() {
        DoRestore();
    }

private:
    void DoRestore() {
        if (!cancelled_ && arena_ != nullptr) {
            if (mode_ == Mode::RestoreOnly) {
                xp_arena_restore(arena_, save_);
            } else {
                xp_arena_free_to_save(arena_, save_);
            }
        }
    }

    xpArena* arena_ = nullptr;
    xpArenaSave save_ = {};
    Mode mode_ = Mode::RestoreOnly;
    bool cancelled_ = false;
};





#if __cplusplus >= 202300L


#if defined(XOAOP_I128_SUPPORT)

template<>
struct std::formatter<i128>: std::formatter<std::string_view> {
    using std::formatter<std::string_view>::parse;

    auto format(const i128& value, std::format_context& ctx) const {
        // 将i128转换为字符串
        char buffer[64]; // 足够大以容纳i128的字符串表示

        char *ptr = buffer + sizeof(buffer) - 1;
        *ptr = '\0'; // null-terminate

        i128 temp = value < 0 ? -value : value; // 处理负数
        // 逐位转换为字符串
        do {
            --ptr;
            *ptr = '0' + (temp % 10);
            temp /= 10;
        } while (temp != 0);

        if(value < 0) {
            --ptr;
            *ptr = '-';
        }
        
        
        return std::formatter<std::string_view>::format(std::string_view(ptr, buffer + sizeof(buffer) - ptr - 1), ctx);
    }
};



#endif // XOAOP_i128_SUPPORT



#endif // C++23






#endif // __cplusplus





//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
// 实现
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//



#if defined(XOAOP_IMPLEMENTATION) && !defined(XOAOP_IMPLEMENTATION_DONE)
#define XOAOP_IMPLEMENTATION_DONE



#if defined(__cplusplus)
extern "C" {
#endif


//
// 常见函数
//

u8 *xp_align_up(u8 *ptr, isize alignment) {
    uintptr p;

    XP_ASSERT(xp_is_power_of_two(alignment));

    p = cast(uintptr)ptr;
    return cast(u8 *)((p + (alignment - 1)) & ~(alignment - 1));
}

u8 *xp_align_down(u8 *value, isize alignment) {
    uintptr p;

    XP_ASSERT(xp_is_power_of_two(alignment));

    p = cast(uintptr)value;
    return cast(u8 *)((p) & ~(alignment - 1));
}

isize xp_align_up_isize(isize value, isize alignment) {
    XP_ASSERT_DEFAULT(xp_is_power_of_two(alignment));
    return (value + (alignment - 1)) & ~(alignment - 1);
}



b32 xp_is_power_of_two(isize x) {
    if (x <= 0) {
        return false;
    }
    return !(x & (x - 1));
}

b32 xp_is_space(char c) {
    if (c == ' ' ||
        c == '\n' ||
        c == '\r' ||
        c == '\t' ||
        c == '\v' ||
        c == '\f'
        ) {
        return true;
    } else {
        return false;
    }
}

b32 xp_is_digit_base_all(char c) {
    if (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9' ||
        c == 'a' || c == 'b' || c == 'c' || c == 'd' || c == 'e' || c == 'f' ||
        c == 'A' || c == 'B' || c == 'C' || c == 'D' || c == 'E' || c == 'F'
        ) {
        return true;
    } else {
        return false;
    }
}

//NOTE(xoaop): 没有检查合法
b32 xp_str_to_num_base(char const *str, u32 base, u64 *result) {
    u64 num = 0;
    isize i = 0;
    while (str[i] != '\0') {
        u64 n = 0;
        switch (str[i]) {
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
            n = str[i] - '0';
            break;
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            n = str[i] - 'a' + 10;
            break;
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            n = str[i] - 'A' + 10;
            break;
        default:
            return false;
            break;
        }

        num = (num * base) + n;
        i += 1;
    }

    *result = num;
    return true;
}

#if defined(XOAOP_I128_SUPPORT)

b32 xp_str_to_integer(char const *str, i128 *result) {
    XP_ASSERT_DEFAULT(result != NULL);

    isize len = xp_strlen_c(str);

    XP_ASSERT_MSG(len > 0, "xp_str_to_integer() meet zero len str!\n");

    b32 success = false;
    u64 r = 0;
    if (str[0] == '0') {
        if (len <= 1) {
            success = true;
            r = str[0] - '0';
        }
        switch (str[1]) {
        case 'x':
            if (!(len > 2)) break;
            success = xp_str_to_num_base(&str[2], 16, &r);
            break;
        case 'b':
            if (!(len > 2)) break;
            success = xp_str_to_num_base(&str[2], 2, &r);
            break;
        default:
            success = xp_str_to_num_base(&str[1], 8, &r);
            break;
        }
    } else {
        success = xp_str_to_num_base(&str[0], 10, &r);
    }

    *result = cast(i128) r;
    return success;
}

#endif // XOAOP_I128_SUPPORT


//
// 内存操作
//

void xp_memcpy_dst_loop(void *dst, isize dst_capacity, isize dst_offset, const void *src, isize src_count) {
    XP_ASSERT_DEFAULT(dst != NULL && src != NULL && dst_capacity > 0);
    XP_ASSERT_DEFAULT(dst_offset >= 0 && dst_offset < dst_capacity);


    u8 *dst_u8 = cast(u8 *)dst;
    u8 *src_u8 = cast(u8 *)src;
    for (isize i = 0; i < src_count; i++) {
        dst_u8[(dst_offset + i) % dst_capacity] = src_u8[i];
    }
}



//
// 内存分配器
//


void *xp_alloc(xpAllocator allocator, isize size) {
    return allocator.proc(allocator.data, xpAlloc, NULL, size, 0);
}
void xp_free(xpAllocator allocator, void *ptr) {
    allocator.proc(allocator.data, xpFree, ptr, (isize)0, 0);
}
void *xp_realloc(xpAllocator allocator, void *ptr, isize new_size, isize old_size) {
    return allocator.proc(allocator.data, xpRealloc, ptr, new_size, old_size);
}
void xp_free_all(xpAllocator allocator) {
    allocator.proc(allocator.data, xpFreeAll, NULL, (isize)0, 0);
    allocator.data = NULL;
}

void xp_zero(void *ptr, isize size) {
    if (ptr != NULL) {
        u8 *p = cast(u8 *)ptr;
        for (isize i = 0; i < size; i++) {
            p[i] = 0;
        }
    }
}

// NOTE(xoaop): 包装堆内存分配
XP_ALLOCATOR_PROC(xp_heap_allocator_proc) {
    void *alloc_result = NULL;
    switch (type) {
    case xpAlloc:
        alloc_result = malloc(size);
        break;
    case xpFree:
        free(ptr);
        break;
    case xpRealloc:
        alloc_result = realloc(ptr, size);
        break;
    case xpFreeAll:
        // NOTE(xoaop): 堆内存不存在释放全部
        // XP_ASSERT_DEFAULT(0);
        break;
    default:
        XP_ASSERT(0);
        break;
    }

    return alloc_result;
}


xpAllocator xp_pure_heap_allocator() {
    xpAllocator allocator;
    allocator.proc = xp_heap_allocator_proc;
    allocator.data = NULL;
    return allocator;
}




xpAllocator xp_heap_allocator() {
#if defined(XP_HEAP_RECORD_ENABLE) && defined(__cplusplus)
    // 启用监控模式（仅C++支持），返回全局单例
    #ifdef XP_HEAP_RECORD_TRACK_LOCATIONS
        return xp_heap_record_allocator_with_location_tracking();
    #else
        return xp_heap_record_allocator();
    #endif
#else
    // 默认使用普通堆分配器，每次都返回新的实例（和原有行为一致）
    return xp_pure_heap_allocator();
#endif
}


// Arena Allocator
void xp_arena_init_default(xpArena *arena) {
    *arena = xp_arena_default();
}


void xp_arena_init(xpArena *arena, isize block_size) {
    *arena = xp_arena_make(block_size);
}

xpArena xp_arena_default() {
    return xp_arena_make(MEMORY_BLOCK_DEFAULT_MIN_SIZE);
}

xpArena xp_arena_make(isize block_size) {
    xpArena arena;
    arena.curr_block = NULL;
    arena.block_size = block_size;
    arena.malloc = false;

    return arena;
}


xpAllocator xp_arena_allocator(xpArena *arena) {
    xpAllocator allocator;
    allocator.proc = xp_arena_allocator_proc;
    allocator.data = arena;

    return allocator;
};

xp_define xpAllocator xp_arena_allocator_default() {
    xpAllocator allocator;
    allocator.proc = xp_arena_allocator_proc;

    // *NOTE(xoaop): malloc
    xpArena *arena = cast(xpArena *)malloc(sizeof(xpArena));
    *arena = xp_arena_default();

    // *NOTE(xoaop): 这个决定有没有内存泄漏
    arena->malloc = true;

    allocator.data = arena;


    return allocator;
}

xp_internal void *xp_arena_alloc_item(xpArena *arena, isize size);



XP_ALLOCATOR_PROC(xp_arena_allocator_proc) {
    xpArena *arena = cast(xpArena *)allocator_data;

    void *alloc_result = NULL;
    switch (type) {
    case xpAlloc:
        alloc_result = xp_arena_alloc_item(arena, size);
        break;
    case xpFree:
        // NOTE(xoaop): No Free in Arena Allocator 
        break;
    case xpRealloc:
        if (ptr == NULL) {
            XP_ASSERT_DEFAULT(0);
        }

        if (size == 0) {
            alloc_result = NULL;
        } else if (size <= old_size) {
            alloc_result = ptr;
        } else {
            alloc_result = xp_arena_alloc_item(arena, size);
            memmove(alloc_result, ptr, old_size);
        }
        break;
    case xpFreeAll:
        xp_arena_free_all(arena);
        break;
    default:
        XP_ASSERT(0);
        break;
    }

    return alloc_result;
}

xp_internal void *xp_arena_alloc_item(xpArena *arena, isize size) {
    XP_ASSERT(size >= 0);
    if (size == 0) {
        return NULL;
    }

    void *alloc_result = NULL;

    b8 found = false;

    xpMemoryBlock *prev_block = NULL;
    while (arena->curr_block != NULL) {
        xpMemoryBlock *curr_block = arena->curr_block;

        // NOTE(xoaop): 对齐
        isize available_size = curr_block->size - (xp_align_up(curr_block->base + curr_block->used, ALIGNMENT_DEFAULT) - curr_block->base);
        if (available_size >= size) {
            found = true;
            break;
        }

        arena->curr_block = curr_block->next;
        prev_block = curr_block;
    }


    // 如果没有合适的内存块, 新建
    if (!found) {
        arena->curr_block = prev_block;

        isize alloc_size = size + xp_align_up_isize(sizeof(xpMemoryBlock), ALIGNMENT_DEFAULT);

        isize alloc_block_count = (alloc_size + arena->block_size - 1) / arena->block_size;

        // TODO(xoaop): 换成虚拟内存页分配, 不用heap_allocator
        // NOTE(xoaop): 我想这个指针该是对齐的, 不然会有问题
        xpMemoryBlock *new_block = cast(xpMemoryBlock *)xp_alloc(xp_heap_allocator(), alloc_block_count * arena->block_size);

        new_block->base = cast(u8 *)new_block;

        new_block->prev = arena->curr_block;
        new_block->next = NULL;

        new_block->size = alloc_block_count * arena->block_size;
        new_block->used = sizeof(xpMemoryBlock);

        if (arena->curr_block != NULL) {
            arena->curr_block->next = new_block;
        }

        arena->curr_block = new_block;
    }

    alloc_result = arena->curr_block->base + arena->curr_block->used;
    arena->curr_block->used = xp_align_up(arena->curr_block->base + arena->curr_block->used, ALIGNMENT_DEFAULT) - arena->curr_block->base + size;

    return alloc_result;
}


void xp_arena_free_all(xpArena *arena) {

    // 先到最前面
    while (arena->curr_block != NULL && arena->curr_block->prev != NULL) {
        arena->curr_block = arena->curr_block->prev;
    }

    // 再从前往后释放
    xpMemoryBlock *curr = arena->curr_block;
    xpMemoryBlock *next = NULL;
    while (curr != NULL) {
        next = curr->next;
        //TODO(xoaop): 换成虚拟内存页分配, 不用heap_allocator
        xp_free(xp_heap_allocator(), curr->base);

        curr = next;
    }

    // *NOTE(xoaop): 若这是用malloc分配的, 别忘了释放
    if (arena->malloc) {
        free(arena);
    }

}

void xp_arena_allocator_clear(xpAllocator allocator) {
    XP_ASSERT_DEFAULT(allocator.data); // basic check, but not enough, maybe there are other types of allocators that have data pointer

    xpArena *arena = cast(xpArena *)allocator.data;

    if (arena->curr_block == NULL) {
        return;
    }

    for (;;) {
        xpMemoryBlock *curr = arena->curr_block;
        curr->used = sizeof(xpMemoryBlock);

        if (curr->prev == NULL) {
            break;
        }

        arena->curr_block = arena->curr_block->prev;
    }

}

void xp_arena_allocator_debug_print(xpAllocator allocator) {
    XP_ASSERT_DEFAULT(allocator.data); // basic check
    xpArena *arena = cast(xpArena *)allocator.data;

    xpMemoryBlock *block = arena->curr_block;

    printf("Arena Allocator Debug Print:\n");

    if (block == NULL) {
        printf("Empty Arena\n");
        return;
    }

    isize curr_block_index = 0;
    while (block->prev != NULL) {
        block = block->prev;
        curr_block_index += 1;
    }

    printf("Current Block Index: %lld\n", curr_block_index);

    curr_block_index = 0;
    while (block != NULL) {
        printf("Block %lld: size = %lld, used = %lld\n", curr_block_index, block->size, block->used);
        block = block->next;
        curr_block_index += 1;
    }

    return;
}

// Arena Save/Restore 实现
xpArenaSave xp_arena_save(xpArena *arena) {
    XP_ASSERT_DEFAULT(arena != NULL);

    xpArenaSave save = {0};
    if (arena->curr_block != NULL) {
        save.block = arena->curr_block;
        save.used = arena->curr_block->used;
    }

    return save;
}

void xp_arena_restore(xpArena *arena, xpArenaSave save) {
    XP_ASSERT_DEFAULT(arena != NULL);

    // 如果保存的block为NULL，说明当时arena是空的
    if (save.block == NULL) {
        // 释放所有块
        xp_arena_free_all(arena);
        arena->curr_block = NULL;
        return;
    }

    // 恢复到保存的block
    arena->curr_block = save.block;
    // 恢复used值
    arena->curr_block->used = save.used;

    // 注意：这个函数不会释放在save之后分配的内存块，只是重置指针
    // 如果需要释放内存，请使用xp_arena_free_to_save
}

void xp_arena_free_to_save(xpArena *arena, xpArenaSave save) {
    XP_ASSERT_DEFAULT(arena != NULL);

    // 如果保存的block为NULL，释放所有内存
    if (save.block == NULL) {
        xp_arena_free_all(arena);
        arena->curr_block = NULL;
        return;
    }

    // 先移动到保存的block
    arena->curr_block = save.block;

    // 释放所有在save.block之后的内存块
    xpMemoryBlock *curr = save.block->next;
    while (curr != NULL) {
        xpMemoryBlock *next = curr->next;
        xp_free(xp_heap_allocator(), curr->base);
        curr = next;
    }

    // 断开后续块的链接
    save.block->next = NULL;

    // 恢复used值
    save.block->used = save.used;
}

// 针对xpAllocator的便利函数实现
xpArenaSave xp_arena_allocator_save(xpAllocator allocator) {
    XP_ASSERT_DEFAULT(allocator.proc == xp_arena_allocator_proc);
    XP_ASSERT_DEFAULT(allocator.data != NULL);
    xpArena *arena = cast(xpArena *)allocator.data;
    return xp_arena_save(arena);
}

void xp_arena_allocator_restore(xpAllocator allocator, xpArenaSave save) {
    XP_ASSERT_DEFAULT(allocator.proc == xp_arena_allocator_proc);
    XP_ASSERT_DEFAULT(allocator.data != NULL);
    xpArena *arena = cast(xpArena *)allocator.data;
    xp_arena_restore(arena, save);
}

void xp_arena_allocator_free_to_save(xpAllocator allocator, xpArenaSave save) {
    XP_ASSERT_DEFAULT(allocator.proc == xp_arena_allocator_proc);
    XP_ASSERT_DEFAULT(allocator.data != NULL);
    xpArena *arena = cast(xpArena *)allocator.data;
    xp_arena_free_to_save(arena, save);
}




#if defined(XOAOP_I128_SUPPORT)

//!NOTE(xoaop): GENERATED BY AI
void print_i128(i128 n) {
    if (n == 0) {
        printf("0");
        return;
    }
    char buf[50] = { 0 };
    int i = 49;
    bool neg = false;
    if (n < 0) {
        neg = true;
        n = -n;
    }
    while (n > 0) {
        buf[--i] = '0' + (n % 10);
        n /= 10;
    }
    if (neg) {
        buf[--i] = '-';
    }
    printf("%s", &buf[i]);
}

bool xp_check_i128_add_overflow(i128 a, i128 b, i128 *result) {
    return __builtin_add_overflow(a, b, result);
}

bool xp_check_i128_sub_overflow(i128 a, i128 b, i128 *result) {
    return __builtin_sub_overflow(a, b, result);
}

bool xp_check_i128_mul_overflow(i128 a, i128 b, i128 *result) {
    return __builtin_mul_overflow(a, b, result);
}

bool xp_check_i128_div_overflow(i128 a, i128 b, i128 *result) {
    if (b == 0) {
        return true; // 除以零溢出
    }
    if (a == I128_MIN && b == -1) {
        return true; // 最小值除以 -1 溢出
    }

    *result = a / b;

    return false;
}

bool xp_check_i128_mod_overflow(i128 a, i128 b, i128 *result) {
    if (b == 0) {
        return true; // 除以零溢出
    }
    if (a == I128_MIN && b == -1) {
        *result = 0; // 最小值 mod -1 的结果是 0
        return false;
    }

    *result = a % b;

    return false;
}

bool xp_check_i128_neg_overflow(i128 a, i128 *result) {
    if (a == I128_MIN) {
        return true; // 最小值取反溢出
    }

    *result = -a;

    return false;
}
#endif // XOAOP_I128_SUPPORT

bool xp_check_f64_is_inf(f64 value) {
    return isinf(value);
}





#if defined(__cplusplus)
}
#endif

//
//
// CPP PART IMPLEMENTATION
//
//


#if defined(__cplusplus)

//
// Heap Record Allocator 实现
//

struct xpHeapRecordAllocator {
    xpAllocator backing_allocator;  // 底层实际分配器
    xpHashMap<void*, xpHeapRecordEntry> records;  // 分配记录
    xpHeapRecordStats stats;        // 统计信息
    b8 track_locations;             // 是否跟踪分配位置（文件/行号）
    b8 auto_report_on_destroy;      // 销毁时自动报告内存泄漏
};

// 内部：获取当前时间戳（简单实现）
u64 xp_heap_record_get_timestamp() {
    // 这里可以根据需要替换为更精确的时间实现
    static u64 counter = 0;
    return counter++;
}

// 监控分配器的核心处理函数
XP_ALLOCATOR_PROC(xp_heap_record_allocator_proc) {
    xpHeapRecordAllocator* record_allocator = cast(xpHeapRecordAllocator*)allocator_data;
    XP_ASSERT_DEFAULT(record_allocator != NULL);

    void* result = NULL;

    switch (type) {
        case xpAlloc: {
            // 从底层分配器分配内存
            result = xp_alloc(record_allocator->backing_allocator, size);

            if (result != NULL) {
                // 记录分配信息
                xpHeapRecordEntry entry = {};
                entry.ptr = result;
                entry.size = size;
                entry.timestamp = xp_heap_record_get_timestamp();

                // 如果启用了位置跟踪，这里可以获取调用栈信息
                // （目前简化实现，位置信息需要编译器支持或宏包装）
                entry.file = NULL;
                entry.line = 0;

                // 插入到记录中
                xp_hash_map_insert(&record_allocator->records, result, entry);

                // 更新统计信息
                record_allocator->stats.total_allocs++;
                record_allocator->stats.current_allocs++;
                record_allocator->stats.total_bytes += size;
                record_allocator->stats.current_bytes += size;

                // 更新峰值
                if (record_allocator->stats.current_bytes > record_allocator->stats.peak_bytes) {
                    record_allocator->stats.peak_bytes = record_allocator->stats.current_bytes;
                }
            }
            break;
        }

        case xpFree: {
            if (ptr != NULL) {
                // 查找记录
                xpHeapRecordEntry* entry = xp_hash_map_get(record_allocator->records, ptr);

                if (entry != NULL) {
                    // 更新统计信息
                    record_allocator->stats.total_frees++;
                    record_allocator->stats.current_allocs--;
                    record_allocator->stats.freed_bytes += entry->size;
                    record_allocator->stats.current_bytes -= entry->size;

                    // 从记录中移除
                    xp_hash_map_remove(&record_allocator->records, ptr);
                }

                // 执行实际释放
                xp_free(record_allocator->backing_allocator, ptr);
            }
            break;
        }

        case xpRealloc: {
            if (ptr == NULL) {
                // 相当于分配新内存
                result = xp_alloc(record_allocator->backing_allocator, size);
                if (result != NULL) {
                    xpHeapRecordEntry entry = {};
                    entry.ptr = result;
                    entry.size = size;
                    entry.timestamp = xp_heap_record_get_timestamp();
                    entry.file = NULL;
                    entry.line = 0;

                    xp_hash_map_insert(&record_allocator->records, result, entry);

                    record_allocator->stats.total_allocs++;
                    record_allocator->stats.current_allocs++;
                    record_allocator->stats.total_bytes += size;
                    record_allocator->stats.current_bytes += size;

                    if (record_allocator->stats.current_bytes > record_allocator->stats.peak_bytes) {
                        record_allocator->stats.peak_bytes = record_allocator->stats.current_bytes;
                    }
                }
            } else {
                // 查找旧记录
                xpHeapRecordEntry* old_entry = xp_hash_map_get(record_allocator->records, ptr);

                if (old_entry != NULL) {
                    // 执行实际重分配
                    result = xp_realloc(record_allocator->backing_allocator, ptr, size, old_size);

                    if (result != NULL) {
                        // 移除旧记录
                        xp_hash_map_remove(&record_allocator->records, ptr);

                        // 更新统计信息
                        record_allocator->stats.freed_bytes += old_entry->size;
                        record_allocator->stats.current_bytes -= old_entry->size;

                        // 添加新记录
                        xpHeapRecordEntry new_entry = {};
                        new_entry.ptr = result;
                        new_entry.size = size;
                        new_entry.timestamp = xp_heap_record_get_timestamp();
                        new_entry.file = old_entry->file;
                        new_entry.line = old_entry->line;

                        xp_hash_map_insert(&record_allocator->records, result, new_entry);

                        record_allocator->stats.total_allocs++;
                        record_allocator->stats.total_bytes += size;
                        record_allocator->stats.current_bytes += size;

                        if (record_allocator->stats.current_bytes > record_allocator->stats.peak_bytes) {
                            record_allocator->stats.peak_bytes = record_allocator->stats.current_bytes;
                        }
                    }
                } else {
                    // 找不到旧记录，直接透传给底层分配器（不跟踪）
                    result = xp_realloc(record_allocator->backing_allocator, ptr, size, old_size);
                }
            }
            break;
        }

        case xpFreeAll: {
            // 释放所有记录的内存
            for (const auto& entry : record_allocator->records) {
                xp_free(record_allocator->backing_allocator, entry.value.ptr);
            }

            // 清空记录和统计信息
            xp_hash_map_clear(&record_allocator->records);
            xp_zero(&record_allocator->stats, sizeof(xpHeapRecordStats));

            // 调用底层分配器的free_all（如果支持）
            xp_free_all(record_allocator->backing_allocator);
            break;
        }

        default: {
            XP_ASSERT_DEFAULT(0);
            break;
        }
    }

    return result;
}

// 创建新的独立监控分配器
xpAllocator xp_heap_record_allocator_create_new() {
    // 直接创建底层普通堆分配器，避免递归
    xpAllocator backing = xp_pure_heap_allocator();

    // 分配监控分配器结构体
    xpHeapRecordAllocator* record_allocator = cast(xpHeapRecordAllocator*)xp_alloc(backing, sizeof(xpHeapRecordAllocator));
    xp_zero(record_allocator, sizeof(xpHeapRecordAllocator));

    record_allocator->backing_allocator = backing;
    record_allocator->records = xp_hash_map_make<void*, xpHeapRecordEntry>(backing);
    record_allocator->track_locations = false;
    record_allocator->auto_report_on_destroy = true;

    // 初始化统计信息
    xp_zero(&record_allocator->stats, sizeof(xpHeapRecordStats));

    // 返回包装后的分配器
    xpAllocator result;
    result.proc = xp_heap_record_allocator_proc;
    result.data = record_allocator;

    return result;
}

// 获取全局单例监控分配器
xpAllocator xp_heap_record_allocator() {
    struct StaticAllocatorHolder {
        xpAllocator allocator;

        StaticAllocatorHolder() {
            allocator = xp_heap_record_allocator_create_new();
        }

        ~StaticAllocatorHolder() {
            // 程序退出时自动报告泄漏
            xp_heap_record_destroy(allocator, true);
        }
    };

    static StaticAllocatorHolder holder;
    return holder.allocator;
}

// 创建新的独立监控分配器（带位置跟踪）
xpAllocator xp_heap_record_allocator_create_new_with_location_tracking() {
    xpAllocator allocator = xp_heap_record_allocator_create_new();
    xpHeapRecordAllocator* record_allocator = cast(xpHeapRecordAllocator*)allocator.data;
    record_allocator->track_locations = true;
    return allocator;
}

// 获取全局单例监控分配器（带位置跟踪）
xpAllocator xp_heap_record_allocator_with_location_tracking() {
    struct StaticAllocatorHolder {
        xpAllocator allocator;

        StaticAllocatorHolder() {
            allocator = xp_heap_record_allocator_create_new_with_location_tracking();
        }

        ~StaticAllocatorHolder() {
            // 程序退出时自动报告泄漏
            xp_heap_record_destroy(allocator, true);
        }
    };

    static StaticAllocatorHolder holder;
    return holder.allocator;
}

// 销毁全局单例监控分配器
void xp_heap_record_destroy_global(b8 report_leaks) {
    // 由于使用了静态持有器，全局单例会在程序退出时自动销毁
    // 这个函数留作兼容性接口，手动触发报告
    xpAllocator instance = xp_heap_record_allocator();
    if (report_leaks) {
        xp_heap_record_check_leaks(instance);
    }
}

// 全局单例版本：获取统计信息
xpHeapRecordStats xp_heap_record_get_stats() {
    return xp_heap_record_get_stats(xp_heap_record_allocator());
}

// 全局单例版本：打印统计信息
void xp_heap_record_print_stats() {
    xp_heap_record_print_stats(xp_heap_record_allocator());
}

// 全局单例版本：打印内存泄漏
void xp_heap_record_print_leaks() {
    xp_heap_record_print_leaks(xp_heap_record_allocator());
}

// 全局单例版本：检查内存泄漏
isize xp_heap_record_check_leaks() {
    return xp_heap_record_check_leaks(xp_heap_record_allocator());
}

// 获取监控分配器的统计信息
xpHeapRecordStats xp_heap_record_get_stats(xpAllocator allocator) {
    XP_ASSERT_DEFAULT(allocator.proc == xp_heap_record_allocator_proc);
    XP_ASSERT_DEFAULT(allocator.data != NULL);

    xpHeapRecordAllocator* record_allocator = cast(xpHeapRecordAllocator*)allocator.data;
    return record_allocator->stats;
}

// 辅助函数：格式化字节大小
xp_internal void xp_format_bytes(isize bytes, char* buffer, isize buffer_size) {
    XP_ASSERT_DEFAULT(buffer != NULL && buffer_size > 0);

    if (bytes == 0) {
        snprintf(buffer, buffer_size, "0B");
        return;
    }

    isize remaining = bytes;
    isize gb = remaining / (1024 * 1024 * 1024);
    remaining %= (1024 * 1024 * 1024);

    isize mb = remaining / (1024 * 1024);
    remaining %= (1024 * 1024);

    isize kb = remaining / 1024;
    remaining %= 1024;

    isize b = remaining;

    buffer[0] = '\0';
    isize offset = 0;

    if (gb > 0) {
        offset += snprintf(buffer + offset, buffer_size - offset, "%lldGB", gb);
    }

    if (mb > 0) {
        if (offset > 0) {
            offset += snprintf(buffer + offset, buffer_size - offset, "+");
        }
        offset += snprintf(buffer + offset, buffer_size - offset, "%lldMB", mb);
    }

    if (kb > 0) {
        if (offset > 0) {
            offset += snprintf(buffer + offset, buffer_size - offset, "+");
        }
        offset += snprintf(buffer + offset, buffer_size - offset, "%lldKB", kb);
    }

    if (b > 0) {
        if (offset > 0) {
            offset += snprintf(buffer + offset, buffer_size - offset, "+");
        }
        snprintf(buffer + offset, buffer_size - offset, "%lldB", b);
    }
}

// 打印内存使用统计报告
void xp_heap_record_print_stats(xpAllocator allocator) {
    XP_ASSERT_DEFAULT(allocator.proc == xp_heap_record_allocator_proc);
    XP_ASSERT_DEFAULT(allocator.data != NULL);

    xpHeapRecordAllocator* record_allocator = cast(xpHeapRecordAllocator*)allocator.data;
    xpHeapRecordStats* stats = &record_allocator->stats;

    char total_bytes[32], freed_bytes[32], current_bytes[32], peak_bytes[32];
    xp_format_bytes(stats->total_bytes, total_bytes, sizeof(total_bytes));
    xp_format_bytes(stats->freed_bytes, freed_bytes, sizeof(freed_bytes));
    xp_format_bytes(stats->current_bytes, current_bytes, sizeof(current_bytes));
    xp_format_bytes(stats->peak_bytes, peak_bytes, sizeof(peak_bytes));

    printf("=== Heap Memory Allocator Stats ===\n");
    printf("Total allocations:    %lld\n", stats->total_allocs);
    printf("Total frees:          %lld\n", stats->total_frees);
    printf("Current allocations:  %lld\n", stats->current_allocs);
    printf("Total bytes allocated:%s\n", total_bytes);
    printf("Total bytes freed:    %s\n", freed_bytes);
    printf("Current bytes used:   %s\n", current_bytes);
    printf("Peak bytes used:      %s\n", peak_bytes);
    printf("===================================\n");
}

// 打印所有未释放的内存分配（内存泄漏报告）
void xp_heap_record_print_leaks(xpAllocator allocator) {
    XP_ASSERT_DEFAULT(allocator.proc == xp_heap_record_allocator_proc);
    XP_ASSERT_DEFAULT(allocator.data != NULL);

    xpHeapRecordAllocator* record_allocator = cast(xpHeapRecordAllocator*)allocator.data;

    if (record_allocator->stats.current_allocs == 0) {
        printf("No memory leaks detected!\n");
        return;
    }

    printf("Memory leak report: %lld unfreed allocations found\n", record_allocator->stats.current_allocs);
    printf("===================================\n");

    isize index = 0;

    for (const auto& entry : record_allocator->records) {
        char size_str[32];
        xp_format_bytes(entry.value.size, size_str, sizeof(size_str));

        printf("Leak #%lld: Address %p, Size: %s (alloc #%lld)",
               index++, entry.value.ptr, size_str, entry.value.timestamp);

        if (entry.value.file != NULL) {
            printf(" at %s:%lld", entry.value.file, entry.value.line);
        }
        printf("\n");
    }
    printf("===================================\n");
}

// 手动检查并报告内存泄漏，返回泄漏数量
isize xp_heap_record_check_leaks(xpAllocator allocator) {
    XP_ASSERT_DEFAULT(allocator.proc == xp_heap_record_allocator_proc);
    XP_ASSERT_DEFAULT(allocator.data != NULL);

    xpHeapRecordAllocator* record_allocator = cast(xpHeapRecordAllocator*)allocator.data;

    if (record_allocator->stats.current_allocs > 0) {
        xp_heap_record_print_leaks(allocator);
    }

    return record_allocator->stats.current_allocs;
}

// 销毁监控分配器，可选是否报告泄漏
void xp_heap_record_destroy(xpAllocator allocator, b8 report_leaks) {
    XP_ASSERT_DEFAULT(allocator.proc == xp_heap_record_allocator_proc);
    XP_ASSERT_DEFAULT(allocator.data != NULL);

    xpHeapRecordAllocator* record_allocator = cast(xpHeapRecordAllocator*)allocator.data;
    xpAllocator backing_allocator = record_allocator->backing_allocator;

    // 报告泄漏
    if (report_leaks || record_allocator->auto_report_on_destroy) {
        xp_heap_record_check_leaks(allocator);
    }

    // 释放所有未释放的内存
    xp_free_all(allocator);

    // 释放哈希表
    xp_hash_map_free(record_allocator->records);

    // 释放监控分配器本身
    xp_free(backing_allocator, record_allocator);
}

size_t xp_strlen_char32(const char32_t *s) {
    if (!s) return 0;
    size_t len = 0;
    while (s[len] != U'\0') {
        ++len;
    }
    return len;
}


#endif // __cplusplus





#endif // XOAOP_IMPLEMENTATION



#endif // XOAOP_H
