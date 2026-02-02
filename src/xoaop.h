/* 
    Everything about my library code in C/CPP
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




#if defined(__clang__) || defined(__GNUC__)
    typedef __int128_t i128;
#elif defined(_MSC_VER) 
    /* MSVC does not support __int128, so i128 is not defined */
#endif

void print_i128(i128 n);


/*
    常用宏 MACROS
*/



#define xp_internal static // 局部链接
#define xp_local_insist static // 函数内静态生命周期变量
#define xp_global static // 文件内静态生命周期变量


//TODO(xoaop): consider
#ifdef NULL
#undef NULL
#endif

#ifndef NULL
    #if defined(__cplusplus)
        #if __cplusplus >= 201103L
            #define NULL nullptr
        #else
            #define NULL 0
        #endif
    #else
        #define NULL ((void *)0)
    #endif
#endif


#define XP_ASSERT(exp) do {                                     \
    if(!(exp))                                                  \
        __builtin_trap();                                       \
} while(0)

#define XP_ASSERT_MSG(exp, format, ...) do {                    \
    if(!(exp)) {                                                \
        printf("Assert FAILED at %s:%d: ", __FILE__, __LINE__); \
        printf(format, __VA_ARGS__);                            \
        __builtin_trap();                                       \
    }                                                           \
} while(0)

#define XP_ASSERT_DEFAULT(exp) XP_ASSERT_MSG(exp, "")


#define XP_TODO XP_ASSERT_DEFAULT(0)


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

xp_define b32 xp_str_to_num_base(char const *str, u32 base, u64* result);
xp_define b32 xp_str_to_integer(char const *str, i128* result);


#define xp_array_len(array) (sizeof(array) / sizeof(array[0]))


/*
    内存操作
*/


xp_define void xp_memcpy_dst_loop(void *dst, isize dst_capacity, const void *src, isize src_count);






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
    void* data;
} xpAllocator;

xp_define void* xp_alloc(xpAllocator allocator, isize size);
xp_define void xp_free(xpAllocator allocator, void* ptr);
xp_define void* xp_realloc(xpAllocator allocator, void* ptr, isize old_size, isize new_size);
xp_define void xp_free_all(xpAllocator allocator);

xp_define void xp_zero(void *ptr, isize size);



// Heap Allocator包装
XP_ALLOCATOR_PROC(xp_heap_allocator_proc);
xp_define xpAllocator xp_heap_allocator();



// Arena Allocator内存分配器

// 8MB
#define MEMORY_BLOCK_DEFAULT_MIN_SIZE (8ll * 1024ll * 1024ll)

#define ALIGNMENT_DEFAULT (8ll)

typedef struct xpMemoryBlock {
	struct xpMemoryBlock* prev;
    struct xpMemoryBlock* next;

	u8* base; 
	isize size;
	isize used;
} xpMemoryBlock;

typedef struct xpArena {
    xpMemoryBlock* curr_block;
	isize block_size;


    b8 malloc;
} xpArena;

xp_define void xp_arena_init_default(xpArena *arena);
xp_define void xp_arena_init(xpArena *arena, isize block_size);

xp_define xpArena xp_arena_default();
xp_define xpArena xp_arena_make(isize block_size);

XP_ALLOCATOR_PROC(xp_arena_allocator_proc);
xp_define xpAllocator xp_arena_allocator(xpArena* arena);

xp_define xpAllocator xp_arena_allocator_default();

xp_define void xp_arena_allocator_clear(xpAllocator allocator);

xp_define void xp_arena_allocator_debug_print(xpAllocator allocator);


// TODO: Heap Record Allocator内存分配器




/*
    字符串, 切片
*/

xp_define isize xp_strlen_c(char const *str);
xp_define char *xp_find_first_non_space(const char* str);







// NOTE(xoaop): ONLY ASCII NOW
typedef struct xpString {
    xpAllocator allocator;
    isize length;
    isize capacity;
    char *c_str;

#if defined(__cplusplus)
    bool operator== (xpString other) const;
#endif // __cplusplus


} xpString;



typedef struct xpSlice {
    void *data;
    isize len;
} xpSlice;


xp_define xpString xp_string_c(char const *str);
xp_define xpString xp_make_string(xpAllocator allocator, char const *str);
xp_define xpString xp_make_string_capacity(xpAllocator allocator, void const *str, isize capacity);
xp_define xpString xp_make_string_zero();
xp_define xpString xp_make_string_from_slice(xpAllocator allocator, xpSlice slice);
xp_define xpString xp_string_copy(xpAllocator allocator, xpString string);
xp_define void xp_string_free(xpString string);
xp_define b32 xp_string_cmp(xpString a, xpString b);
xp_define b32 xp_string_equal(xpString a, xpString b);
xp_define xpString *xp_string_extend(xpString *string, isize extended_size);
xp_define isize xp_string_find_char(xpString str, char c);
xp_define xpString *xp_string_insert(isize pos, xpString *str, xpString inserted_str);
xp_define xpString *xp_string_append(xpString *str, xpString appended_str);
xp_define xpString xp_isize_to_string(isize value, xpAllocator allocator);
xp_define xpString xp_string_replace_char(xpString string, char old_char, char new_char, xpAllocator allocator);



xp_define xpSlice xp_slice_make(void *data, isize len);
xp_define xpSlice xp_slice_make_from_string(xpString string, isize begin, isize len);



/*
    哈希映射函数
*/
xp_define u32 xp_murmur_hash3_32(const void* data, usize length, usize seed);
xp_define u64 xp_hash_combine_u64(u64 old_hash, u64 new_value);


/*
    平台抽象
*/


#if defined(__cplusplus)
} // for decl
#endif



/*
    CPP PART
*/
#if defined(__cplusplus)



//
// char32_t 操作
//
size_t xp_strlen_char32(const char32_t* s);






/*
Macros
*/

// 为了标识实现作为模板函数的接口
#define IMPL_TP template<>



template<typename T>
void xp_zero(T* ptr) {
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


/*
Defer In CPP
*/

#if __cplusplus >= 201703L

#include <functional> // TODO(xoaop): REMOVE
template<typename F>
struct xpDeferWrapper {
    
    xpDeferWrapper(F defer_func): defer_func(defer_func) {
        
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

/*
Option
*/
enum class xpOptionEnum {
    None,
    Some 
};

template<typename T>
struct xpOption {
    public:
    
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
    
    
    private:
    xpOptionEnum kind;
    T value;
};




/*
HashMap
*/

//hash函数接口定义
template<typename K>
usize xp_hash_func(K *key);


template<typename K, typename V>
struct xpHashMapEntry {
    K key;
    V value;
    b8 used;
};

template<typename K, typename V>
struct xpHashMap {
    
    xpAllocator allocator;
    xpHashMapEntry<K, V> *entries;
    
    isize count;
    isize capacity;
};


template<typename K, typename V>
xpHashMap<K, V> xp_hash_map_make(xpAllocator allocator) {
    xpHashMap<K, V> hash_map = {};
    hash_map.allocator = allocator;
    hash_map.entries = NULL;
    
    hash_map.count = 0;
    hash_map.capacity = 0;
    
    return hash_map;
}

template<typename K, typename V>
void xp_hash_map_free(xpHashMap<K, V> map) {
    xp_free(map.allocator, map.entries);
}

template<typename K, typename V>
xpHashMap<K, V> xp_hash_map_copy(xpHashMap<K, V> *o, xpAllocator allocator) {
    xpHashMap<K, V> copy = *o;
    copy.entries = xp_alloc_array<xpHashMapEntry<K, V>>(allocator, copy.capacity);
    memcpy(copy.entries, o->entries, sizeof(xpHashMapEntry<K, V>) * copy.capacity);
    
    return copy;
}



template<typename K, typename V>
void xp_hash_map_extend(xpHashMap<K, V> *map, isize new_capacity) {
    XP_ASSERT(new_capacity > map->capacity);
    
    if(map->entries == NULL) {
        map->entries = (xpHashMapEntry<K, V> *) xp_alloc(map->allocator, sizeof(xpHashMapEntry<K, V>) * new_capacity);
        xp_zero(map->entries, sizeof(xpHashMapEntry<K, V>) * new_capacity);
    } else {
        // Rehash
        xpHashMapEntry<K, V> *new_entries = (xpHashMapEntry<K, V> *) xp_alloc(map->allocator, sizeof(xpHashMapEntry<K, V>) * new_capacity);
        for(isize i = 0; i < new_capacity; ++i) {
            new_entries[i].used = false;
        }
        
        for(isize i = 0; i < map->capacity; ++i) {
            xpHashMapEntry<K, V> *old_entry = &map->entries[i];
            if(old_entry->used) {
                usize hash_value = xp_hash_func(&old_entry->key);
                usize index = hash_value % new_capacity;
                // 线性探测
                while(new_entries[index].used) {
                    index = (index + 1) % new_capacity;
                }
                new_entries[index] = *old_entry;
            }
        }
        xp_free(map->allocator, map->entries);
        
        map->entries = new_entries;
    }
    
    map->capacity = new_capacity;
    return;
}


template<typename K, typename V>
V *xp_hash_map_insert(xpHashMap<K, V> *map, K key, V value) {
    if(map->count >= map->capacity) {
        xp_hash_map_extend(map, map->capacity + map->capacity / 2 + 1);
    }
    usize hash_value = xp_hash_func(&key);
    
    usize index = hash_value % map->capacity;
    usize original_index = index;
    do {
        xpHashMapEntry<K, V> *entry = &map->entries[index];
        if(entry->used == false) {
            entry->key = key;
            entry->value = value;
            entry->used = true;
            
            map->count += 1;
            return &entry->value;
        } else if(entry->key == key) {
            entry->value = value;
            return NULL;
        }
        
        index = (index + 1) % map->capacity;
    } while(index != original_index);
    
    //NOTE: FULL MAP
    XP_ASSERT(0);
    return NULL;
}


template<typename K, typename V>
xpHashMapEntry<K, V> *xp_hash_map_get_entry(xpHashMap<K, V> map, K key) {
    if(map.count == 0) {
        return NULL;
    }
    
    usize hash_value = xp_hash_func(&key);
    usize index = hash_value % map.capacity;
    usize original_index = index;
    do {
        xpHashMapEntry<K, V> *entry = &map.entries[index];
        if(entry->used == false) {
            return NULL;
        } else if(entry->key == key) {
            return entry;
        }
        
        index = (index + 1) % map.capacity;
    } while(index != original_index);
    
    return NULL;
}


template<typename K, typename V>
V *xp_hash_map_get(xpHashMap<K, V> map, K key) {
    xpHashMapEntry<K, V>* entry = xp_hash_map_get_entry(map, key);
    return entry ? &entry->value : nullptr;
}

template<typename K, typename V>
V *xp_hash_map_set(xpHashMap<K, V> *map, K key, V value) {
    xpHashMapEntry<K, V> *entry;
    if((entry = xp_hash_map_get_entry(map, key)) != NULL) {
        entry->value = value;
        return entry->value;
    }
    return NULL;
}

template<typename K, typename V>
b32 xp_hash_map_remove(xpHashMap<K, V> *map, K key) {
    xpHashMapEntry<K, V> *entry;
    if((entry = xp_hash_map_get_entry(*map, key)) != NULL) {
        entry->used = false;
        map->count -= 1;
        return true;
    }
    return false;
}

template<typename K, typename V>
isize xp_hash_map_first_entry(xpHashMap<K, V> *map, xpHashMapEntry<K, V> **first_entry) {
    for(isize i = 0; i < map->capacity; i++) {
        if(map->entries[i].used == true) {
            *first_entry = &map->entries[i];
            return i;
        }
    }
    
    *first_entry = NULL;
    return -1;
}

template<typename K, typename V>
isize xp_hash_map_next_entry(xpHashMap<K, V> *map, isize curr_pos, xpHashMapEntry<K, V> **next_entry) {
    for(isize i = curr_pos + 1; i < map->capacity; i++) {
        if(map->entries[i].used == true) {
            *next_entry = &map->entries[i];
            return i;
        }
    }
    
    *next_entry = NULL;
    return -1;
}



/*
HashSet
*/

template<typename K>
struct xpHashSetEntry {
    K key;
    b8 used;
};


template<typename K>
struct xpHashSet {
    xpAllocator allocator;
    xpHashSetEntry<K> *entries;
    
    isize count;
    isize capacity;
};

template<typename K>
xpHashSet<K> xp_hash_set_make(xpAllocator allocator) {
    xpHashSet<K> hash_set = {};
    hash_set.allocator = allocator;
    hash_set.entries = NULL;
    
    hash_set.count = 0;
    hash_set.capacity = 0;
    
    return hash_set;
}

template<typename K>
void xp_hash_set_free(xpHashSet<K> set) {
    xp_free(set.allocator, set.entries);
}

template<typename K>
void xp_hash_set_clear(xpHashSet<K> *set) {
    set->count = 0;
}

template<typename K>
xpHashSet<K> xp_hash_set_copy(xpHashSet<K> *set, xpAllocator allocator) {
    xpHashSet<K> copy = *set;
    copy.entries = xp_alloc_array<xpHashSetEntry<K>>(allocator, copy.capacity);
    memcpy(copy.entries, set->entries, sizeof(xpHashSetEntry<K>) * copy.capacity);
    
    return copy;
}


template<typename K>
void xp_hash_set_extend(xpHashSet<K> *set, isize new_capacity) {
    XP_ASSERT_DEFAULT(new_capacity > set->capacity);
    
    if(set->entries == NULL) {
        set->entries = (xpHashSetEntry<K> *) xp_alloc(set->allocator, sizeof(xpHashSetEntry<K>) * new_capacity);
        xp_zero(set->entries, sizeof(xpHashSetEntry<K>) * new_capacity);
    } else {
        // Rehash
        xpHashSetEntry<K> *new_entries = (xpHashSetEntry<K> *) xp_alloc(set->allocator, sizeof(xpHashSetEntry<K>) * new_capacity);
        for(isize i = 0; i < new_capacity; ++i) {
            new_entries[i].used = false;
        }
        
        for(isize i = 0; i < set->capacity; ++i) {
            xpHashSetEntry<K> *old_entry = &set->entries[i];
            if(old_entry->used) {
                usize hash_value = xp_hash_func(&old_entry->key);
                usize index = hash_value % new_capacity;
                // 线性探测
                while(new_entries[index].used) {
                    index = (index + 1) % new_capacity;
                }
                new_entries[index] = *old_entry;
            }
        }
        xp_free(set->allocator, set->entries);
        
        set->entries = new_entries;
    }
    
    set->capacity = new_capacity;
    return;
}


template<typename K>
K *xp_hash_set_insert(xpHashSet<K> *set, K key) {
    if(set->count == set->capacity) {
        xp_hash_set_extend(set, set->capacity + set->capacity / 2 + 1);
    }
    usize hash_value = xp_hash_func(&key);
    
    usize index = hash_value % set->capacity;
    usize original_index = index;
    
    
    do {
        xpHashSetEntry<K> *entry = &set->entries[index];
        if(entry->used == false) {
            entry->key = key;
            entry->used = true;
            
            set->count += 1;
            return &entry->key;
        } else if(entry->key == key) {
            return NULL;
        }
        index = (index + 1) % set->capacity;
    } while(index != original_index);
    
    //NOTE: FULL SET
    XP_ASSERT_DEFAULT(0);
    return NULL;
}

template<typename K>
xpHashSetEntry<K> *xp_hash_set_get_entry(xpHashSet<K> *set, K key) {
    if(set->count == 0) {
        return NULL;
    }
    
    usize hash_value = xp_hash_func(&key);
    
    usize index = hash_value % set->capacity;
    usize original_index = index;
    do {
        xpHashSetEntry<K> *entry = &set->entries[index];
        if(entry->used == false) {
            return NULL;
        } else if(entry->key == key) {
            return entry;
        }
        
        index = (index + 1) % set->capacity;
    } while(index != original_index);
    
    return NULL;
}

template<typename K>
K *xp_hash_set_get(xpHashSet<K> *set, K key) {
    xpHashSetEntry<K> *entry = xp_hash_set_get_entry(set, key);
    return entry ? &entry->key : NULL;
}


template<typename K>
b32 xp_hash_set_find(xpHashSet<K> *set, K key) {
    xpHashSetEntry<K> *entry = NULL;
    if((entry = xp_hash_set_get_entry(set, key)) != NULL) {
        return true;
    }
    
    return false;
}


template<typename K>
b32 xp_hash_set_remove(xpHashSet<K> *set, K key) {
    xpHashSetEntry<K> *entry = NULL;
    if((entry = xp_hash_set_get_entry(set, key)) != NULL) {
        entry->used = false;
        return true;
    }
    
    return false;
}




//
// xpInterningTable 实现
//
// 这个数据结构用于任意类型的唯一化存储, 保证每个值只存储一份, 且指针永不失效

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
};

template<typename T, size_t CAPACITY>
void xp_interning_table_init(xpInterningTable<T, CAPACITY> *table) {
    xp_arena_init_default(&table->arena);
    table->allocator = xp_arena_allocator(&table->arena);
    
    table->count = 0;
}

template<typename T, size_t CAPACITY>
void xp_interning_table_free(xpInterningTable<T, CAPACITY> *table) {
    xp_free_all(table->allocator);
}

template<typename T, size_t CAPACITY>
xpInterningEntry<T> *xp_interning_table_get_entry(xpInterningTable<T, CAPACITY> *table, T key) {
    if(table->count == 0) {
        return NULL;
    }
    
    usize hash_value = xp_hash_func(&key);
    
    usize index = hash_value % xpInterningTable<T, CAPACITY>::capacity;
    usize original_index = index;
    do {
        xpInterningEntry<T> *entry = &table->buckets[index];
        if(entry->used == false) {
            return NULL;
        } else if(*(entry->key_ptr) == (key)) {
            return entry;
        }
        
        index = (index + 1) % xpInterningTable<T, CAPACITY>::capacity;
    } while(index != original_index);
    
    return NULL;
}

template<typename T, size_t CAPACITY>
T *xp_interning_table_get(xpInterningTable<T, CAPACITY> *table, T key) {
    xpInterningEntry<T> *entry = xp_interning_table_get_entry(table, key);
    return entry ? entry->key_ptr : NULL;
}

template<typename T, size_t CAPACITY>
T *xp_interning_table_insert(xpInterningTable<T, CAPACITY> *table, T key) {
    if(table->count >= table->capacity) {
        return NULL; // 满了
    }
    
    usize hash_value = xp_hash_func(&key);
    usize index = hash_value % xpInterningTable<T, CAPACITY>::capacity;
    usize original_index = index;
    
    do {
        xpInterningEntry<T> *entry = &table->buckets[index];
        if(entry->used == false) {
            T *stored_key_ptr = xp_alloc<T>(table->allocator);
            *stored_key_ptr = key;
            
            entry->key_ptr = stored_key_ptr;
            entry->used = true;
            
            table->count += 1;
            return stored_key_ptr;
        } else if(*(entry->key_ptr) == (key)) {
            return NULL; // 重复插入当作失败处理
        }
        
        index = (index + 1) % xpInterningTable<T, CAPACITY>::capacity;
    } while(index != original_index);
    
    //NOTE: FULL MAP
    XP_ASSERT_DEFAULT(0);
    return NULL;
}



//
// xpString CPP 相关操作声明
//

xpString xp_string_concat_mid(xpString a, xpString b, xpOption<xpString> middle, xpAllocator allocator);




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
	return cast(u8 *)((p + (alignment-1)) &~ (alignment-1));
}

u8 *xp_align_down(u8 *value, isize alignment) {
    uintptr p;

    XP_ASSERT(xp_is_power_of_two(alignment));

    p = cast(uintptr)value;
    return cast(u8 *)((p) &~ (alignment-1));
}

isize xp_align_up_isize(isize value, isize alignment) {
    XP_ASSERT_DEFAULT(xp_is_power_of_two(alignment));
    return (value + (alignment - 1)) &~ (alignment - 1);
}



b32 xp_is_power_of_two(isize x) {
    if(x <= 0) {
        return false;
    }
    return !(x & (x-1));
}

b32 xp_is_space(char c) {
    if( c == ' ' ||
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
    if( c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9' ||
        c == 'a' || c == 'b' || c == 'c' || c == 'd' || c == 'e' || c == 'f' ||
        c == 'A' || c == 'B' || c == 'C' || c == 'D' || c == 'E' || c == 'F'
    ) {
        return true;
    } else {
        return false;
    }
}

//NOTE(xoaop): 没有检查合法
b32 xp_str_to_num_base(char const *str, u32 base, u64* result) {
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

b32 xp_str_to_integer(char const *str, i128 *result) {
    XP_ASSERT_DEFAULT(result != NULL);

    isize len = xp_strlen_c(str);

    XP_ASSERT_MSG(len > 0, "xp_str_to_integer() meet zero len str!\n");

    b32 success = false;
    u64 r = 0;
    if(str[0] == '0') {
        if(len <= 1) {
            success = true;
            r = str[0] - '0';
        }
        switch (str[1]) {
        case 'x':
            if(!(len > 2)) break;
            success = xp_str_to_num_base(&str[2], 16, &r);
            break;
        case 'b':
            if(!(len > 2)) break;
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



//
// 内存操作
//

void xp_memcpy_dst_loop(void *dst, isize dst_capacity, const void *src, isize src_count) {
    XP_ASSERT_DEFAULT(dst != NULL && src != NULL && dst_capacity > 0);


    u8 *dst_u8 = cast(u8 *)dst;
    u8 *src_u8 = cast(u8 *)src;
    for(isize i = 0; i < src_count; i++) {
        dst_u8[i % dst_capacity] = src_u8[i];
    }
}



//
// 内存分配器
//


void* xp_alloc(xpAllocator allocator, isize size) {
    return allocator.proc(allocator.data, xpAlloc, NULL, size, 0);
}
void xp_free(xpAllocator allocator, void* ptr) {
    allocator.proc(allocator.data, xpFree, ptr, (isize)0, 0);
}
void* xp_realloc(xpAllocator allocator, void* ptr, isize old_size, isize new_size) {
    return allocator.proc(allocator.data, xpRealloc, ptr, old_size, new_size);
}
void xp_free_all(xpAllocator allocator) {
    allocator.proc(allocator.data, xpFreeAll, NULL, (isize)0, 0);
}

void xp_zero(void *ptr, isize size) {
    if(ptr != NULL) {
        u8 *p = cast(u8 *)ptr;
        for(isize i = 0; i < size; i++) {
            p[i] = 0;
        }
    }
}

// NOTE(xoaop): 包装堆内存分配
XP_ALLOCATOR_PROC(xp_heap_allocator_proc) {
    void* alloc_result = NULL;
    switch (type)
    {
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
xpAllocator xp_heap_allocator() {
    xpAllocator allocator;
    allocator.proc = xp_heap_allocator_proc;
    allocator.data = NULL;
    return allocator;
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


xpAllocator xp_arena_allocator(xpArena* arena) {
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

xp_internal void* xp_arena_alloc_item(xpArena* arena, isize size);
xp_internal void xp_arena_free_all(xpArena* arena);


XP_ALLOCATOR_PROC(xp_arena_allocator_proc) {
    xpArena* arena = cast(xpArena*)allocator_data;
    
	void* alloc_result = NULL;
    switch (type)
    {
    case xpAlloc:
        alloc_result = xp_arena_alloc_item(arena, size);
        break;
    case xpFree:
        // NOTE(xoaop): No Free in Arena Allocator 
        break;
    case xpRealloc:
        if(ptr == NULL) {
            XP_ASSERT_DEFAULT(0);
        }

        if(size == 0) {
            alloc_result = NULL;
        } else if (size <= old_size){
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

xp_internal void *xp_arena_alloc_item(xpArena* arena, isize size) {
    XP_ASSERT(size >= 0);
    if(size == 0) {
        return NULL;
    }
    
    void* alloc_result = NULL;
    
    b8 found = false;
    
    xpMemoryBlock start_block;
    start_block.next = arena->curr_block;
    do {
        if(start_block.next == NULL) {
            break;
        }

        xpMemoryBlock* curr_block = start_block.next;

        // NOTE(xoaop): 对齐
        isize available_size = curr_block->size - (xp_align_up(curr_block->base + curr_block->used, ALIGNMENT_DEFAULT) - curr_block->base);
        if(available_size >= size) {
            found = true;
            arena->curr_block = curr_block;
            break;
        }

        start_block.next = curr_block->next;

    } while(start_block.next != NULL);


    
    // 如果没有合适的内存块, 新建
    if(!found) {
        isize alloc_size = size + xp_align_up_isize(sizeof(xpMemoryBlock), ALIGNMENT_DEFAULT);

        isize alloc_block_count = alloc_size / arena->block_size + 1;
        
        // TODO(xoaop): 换成虚拟内存页分配, 不用heap_allocator
        // NOTE(xoaop): 我想这个指针该是对齐的, 不然会有问题
        xpMemoryBlock* new_block = cast(xpMemoryBlock *)xp_alloc(xp_heap_allocator(), alloc_block_count * arena->block_size);
    
        new_block->base = cast(u8 *)new_block;
        
        new_block->prev = arena->curr_block;
        new_block->next = NULL;

        new_block->size = alloc_block_count * arena->block_size;
        new_block->used = sizeof(xpMemoryBlock);
        
        if(arena->curr_block != NULL) {
            arena->curr_block->next = new_block;
        }

        arena->curr_block = new_block;
    }

    alloc_result = arena->curr_block->base + arena->curr_block->used;
    arena->curr_block->used = xp_align_up(arena->curr_block->base + arena->curr_block->used, ALIGNMENT_DEFAULT) - arena->curr_block->base + size;

    return alloc_result;
}


xp_internal void xp_arena_free_all(xpArena* arena) {

    // 先到最后面
    while(arena->curr_block != NULL) {
        arena->curr_block = arena->curr_block->next;
    }
    // 再从后到前
    while(arena->curr_block != NULL) {
        xpMemoryBlock* curr = arena->curr_block;
        arena->curr_block = curr->prev;
        //TODO(xoaop): 换成虚拟内存页分配, 不用heap_allocator
        xp_free(xp_heap_allocator(), curr->base);
    }

    // *NOTE(xoaop): 这是用malloc分配的, 别忘了释放
    if(arena->malloc) {
        free(arena);
    }

}

void xp_arena_allocator_clear(xpAllocator allocator) {
    XP_ASSERT_DEFAULT(allocator.data); // basic check, but not enough, maybe there are other types of allocators that have data pointer

    xpArena *arena = cast(xpArena *)allocator.data;

    if(arena->curr_block == NULL) {
        return;
    }

    for(;;) {
        xpMemoryBlock *curr = arena->curr_block;
        curr->used = sizeof(xpMemoryBlock);

        if(curr->prev == NULL) {
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
    
    if(block == NULL) {
        printf("Empty Arena\n");
        return;
    }

    isize curr_block_index = 0;
    while(block->prev != NULL) {
        block = block->prev;
        curr_block_index += 1;
    }

    printf("Current Block Index: %lld\n", curr_block_index);

    curr_block_index = 0;
    while(block != NULL) {
        printf("Block %lld: size = %lld, used = %lld\n", curr_block_index, block->size, block->used);
        block = block->next;
        curr_block_index += 1;
    }

    return;
}


//
// 字符串实现
//
isize xp_strlen_c(char const *str) {
    isize len = 0;
    for(; str[len] != '\0'; len += 1);

    //NOTE: for unknown wrong, maybe unnessery
    XP_ASSERT(len >= 0);
    return len;
}

void xp_strncpy_c(char *dst, char *src, isize len) {
    isize index = 0;
    for(; index < len && src[index] != '\0'; index += 1) {
        dst[index] = src[index];
    }

    //NOTE(xoaop): 兼容c字符串
    dst[len - 1] = '\0';
}


char *xp_find_first_non_space(const char *str) {
    XP_ASSERT_DEFAULT(str != NULL);
    while (*str && isspace(cast(unsigned char)*str)) {
        str++;
    }
    return cast(char *)str;
}



xpString xp_string_c(char const *str) {
    XP_ASSERT_DEFAULT(str != NULL);

    xpString string = {
        .allocator = {NULL, NULL},
        .length = xp_strlen_c(str),
        .capacity = xp_strlen_c(str),
        .c_str = cast(char *)str
    };
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
    if(str != NULL) {
        length = xp_strlen_c(cast(char const *)str);
    }
    if(length > capacity) {
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

xpString xp_make_string_zero() {
    xpString string;
    string.allocator = {NULL, NULL};
    string.c_str = NULL;
    string.length = 0;
    string.capacity = 0;
    return string;
}


void xp_string_free(xpString string) {
    if(string.allocator.proc != NULL) {
        xp_free(string.allocator, string.c_str);
    }
}

b32 xp_string_cmp(xpString a, xpString b) {
    if(a.length > b.length) {
        return 1;
    } else if(a.length < b.length) {
        return -1;
    }

    for(isize i = 0; i < a.length; i++) {
        if(a.c_str[i] != b.c_str[i]) {
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
    if(extended_size <= 0) {
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
    for(isize i = 0; i < str.length; i++) {
        if(str.c_str[i] == c) {
            return i;
        }
    }
    return -1;
}

xpString *xp_string_insert(isize pos, xpString *str, xpString inserted_str) {
    XP_ASSERT_DEFAULT(pos >= 0);
    XP_ASSERT_DEFAULT(pos <= str->length);

    // 1. 扩容，确保插入后长度不会超过 capacity
    if(str->length + inserted_str.length > str->capacity) {
        xp_string_extend(str, inserted_str.length + (str->length + inserted_str.length - str->capacity));
    }

    XP_ASSERT_DEFAULT(str->length + inserted_str.length <= str->capacity);

    // 2. 后移原有数据
    if(pos < str->length) {
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

    for(isize i = 0; i < new_string.length; i++) {
        if(new_string.c_str[i] == old_char) {
            new_string.c_str[i] = new_char;
        }
    }

    return new_string;
}


xpString xp_make_string_from_slice(xpAllocator allocator, xpSlice slice) {
    return xp_make_string_capacity(allocator, slice.data, slice.len);
}


xpSlice xp_slice_make(void *data, isize len) {
    xpSlice slice = {
        .data = data,
        .len = len
    };
    return slice;
}

xpSlice xp_slice_make_from_string(xpString string, isize begin, isize len) {
    if((len < 0 || begin < 0) || ((begin + len) > string.length)) {
        XP_ASSERT(0);
    }
    
    xpSlice slice = {};
    slice.data = string.c_str + begin;
    slice.len = len;

    return slice;
}


//
// 哈希映射函数实现
//


//NOTE: 可以移到常用函数里
xp_internal u32 rotate_left(u32 value, i32 shift) {
    const i32 bits = sizeof(u32) * 8;  // 32 位
    shift %= bits;  // 确保 shift 在 0 到 31 之间
    return (value << shift) | (value >> (bits - shift));
}

u32 xp_murmur_hash3_32(const void* data, usize length, usize seed) {
    const u8* key = cast(const u8*)(data);
    const usize nblocks = length / 4;

    u32 h1 = seed;

    const u32 c1 = 0xcc9e2d51;
    const u32 c2 = 0x1b873593;

    // 处理 4 字节块
    const u32* blocks = cast(u32*)key;
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
    const u8* tail = key + nblocks * 4;
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




//!NOTE(xoaop): GENERATED BY AI
void print_i128(i128 n) {
    if (n == 0) {
        printf("0");
        return;
    }
    char buf[50] = {0};
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


#if defined(__cplusplus)
}
#endif

//
//
// CPP PART IMPLEMENTATION
//
//


#if defined(__cplusplus)

// xpString的CPP部分

bool xpString::operator== (xpString other) const {
    b32 xp_string_cmp(xpString a, xpString b);
    return !xp_string_cmp(*this, other);
}



xpString xp_string_concat_mid(xpString a, xpString b, xpOption<xpString> middle, xpAllocator allocator) {
    xpString result = xp_string_copy(allocator, a);

    if(middle.has_value()) {
        xp_string_append(&result, middle.unwrap());
    }

    xp_string_append(&result, b);

    return result;
}



//
// 常见类型的hash函数实现
//

template<>
usize xp_hash_func<i32>(i32 *key) {
    return xp_murmur_hash3_32(key, sizeof(i32), 0);
}

template<>
usize xp_hash_func<i64>(i64 *key) {
    return xp_murmur_hash3_32(key, sizeof(i64), 0);
}

template<>
usize xp_hash_func<u32>(u32 *key) {
    return xp_murmur_hash3_32(key, sizeof(u32), 0);
}

template<>
usize xp_hash_func<u64>(u64 *key) {
    return xp_murmur_hash3_32(key, sizeof(u64), 0);
}

template<>
usize xp_hash_func<f32>(f32 *key) {
    u32 bits;
    memcpy(&bits, key, sizeof(f32));
    return xp_murmur_hash3_32(&bits, sizeof(u32), 0);
}

template<>
usize xp_hash_func<f64>(f64 *key) {
    u64 bits;
    memcpy(&bits, key, sizeof(f64));
    return xp_murmur_hash3_32(&bits, sizeof(u64), 0);
}

template<>
usize xp_hash_func<char>(char *key) {
    return cast(usize)(*key);
}

template<>
usize xp_hash_func<xpString>(xpString *key) {
    return xp_murmur_hash3_32(key->c_str, cast(usize) key->length, 0);
}



size_t xp_strlen_char32(const char32_t* s) {
    if (!s) return 0;
    size_t len = 0;
    while (s[len] != U'\0') {
        ++len;
    }
    return len;
}


#endif





#endif // XOAOP_IMPLEMENTATION



#endif // XOAOP_H