#pragma once

#include "xoaop.h"
#include "array.hpp"
#include "common.hpp"

#define TYPE_KINDS                                      \
    TYPE_KIND(Undefined, "undefined")                   \
    TYPE_KIND(i8, "i8")                                 \
    TYPE_KIND(i32, "i32")                               \
    TYPE_KIND(i64, "i64")                               \
    TYPE_KIND(u8, "u8")                                 \
    TYPE_KIND(u32, "u32")                               \
    TYPE_KIND(u64, "u64")                               \
    TYPE_KIND(f32, "f32")                               \
    TYPE_KIND(f64, "f64")                               \
    TYPE_KIND(bool, "bool")                             \
    TYPE_KIND(void, "void")                             \
    TYPE_KIND(function, "function")                     \
    TYPE_KIND(pointer, "pointer")                       \
    TYPE_KIND(struct, "struct")                         \
    TYPE_KIND(array, "array")                           \
    TYPE_KIND(string, "string")                         \
    TYPE_KIND(untyped_int, "untyped_int")               \
    TYPE_KIND(untyped_float, "untyped_float")           \
    TYPE_KIND(uncertain, "uncertain")                   \
/**/


enum TypeKind {

#define TYPE_KIND(name, str) Type_##name,
    TYPE_KINDS
#undef TYPE_KIND

};

TypeKind string_to_type_kind(xpString str);

// 前向声明
struct Type;
typedef Type *TypeRef;

bool is_equal_type(Type a, Type b);


struct StructField;

struct Type {
    TypeKind kind;
    xpString type_name; // 目前仅用于struct等复合类型的记录
    
    union {
        
        // 函数
        struct {
            Array<TypeRef> param_types;
            TypeRef return_type;
        } function_info;
        
        // 指针
        TypeRef pointed_type;
        
        // 结构体
        Array<StructField> struct_fields;
        
        
        // 数组
        struct {
            TypeRef element_type;
            usize count;
        } array_info;
    };

    // *重要: 用于hash set比较Type, 不然会出问题
    bool operator== (const Type &other) const {
        return is_equal_type(*this, other);
    }
};


struct StructField {
    xpString name;
    TypeRef type;
};










Type make_type(TypeKind kind);
Type *alloc_type(xpAllocator allocator, TypeKind kind);

Type copy_type(Type *src);

Type make_pointer_type(TypeKind base_type_kind, isize level_of_pointer);
Type make_pointer_type(Type base_type, isize level_of_pointer);
Type make_pointer_type(Type pointed_type);
TypeRef get_pointed_type(TypeRef pointer_type);
TypeRef get_innermost_type_of_pointer(TypeRef pointer_type);

Type make_struct_type();
Type make_struct_type(xpString name);
Type make_struct_type(xpString name, Array<StructField> fields);

// bool is_equal_type(Type a, Type b);
bool is_integer_type(TypeRef type);
bool is_integer_or_bool_type(TypeRef type);
bool is_signed_type(TypeRef type);
bool is_signed_or_bool_type(TypeRef type);
bool is_unsigned_type(TypeRef type);
bool is_float_type(TypeRef type);
bool is_certain_type(TypeRef type);
bool is_pointer_type(TypeRef type);
bool is_struct_type(TypeRef type);
bool is_array_type(TypeRef type);

bool is_basic_type_kind(TypeKind kind);
bool is_complex_type_kind(TypeKind kind);
bool is_easy_type_kind(TypeKind kind);
bool is_hard_type_kind(TypeKind kind);



TypeRef get_common_type(TypeRef a, TypeRef b);


bool check_literal_overflow(TypeKind type_kind, i128 result, double dresult);


void point_to(Type *type, Type *pointed_type);
void struct_add_member(Type *type, xpString name, Type member_type);







void print_type(TypeRef type);






// TODO: 大更新, 把ast里的类型信息都改成TypeRef
// 所有详细类型信息都放在type表里
// 统一类型获取接口
// 解决结构体字段信息获取需要额外操作问题






static constexpr size_t TYPE_TABLE_CAPACITY = 16384;

struct TypeTable {
    xpInterningTable<Type, TYPE_TABLE_CAPACITY> type_interning_table;
};
void init_type_table();




TypeRef easy_type(TypeKind kind);
TypeRef pointer_type(TypeRef pointed_type);
TypeRef pointer_type(TypeRef pointed_type, isize level_of_pointer);
TypeRef function_type(Array<TypeRef> param_types, TypeRef return_type);
TypeRef array_type(TypeRef element_type, usize count);
TypeRef get_struct_type(xpString name);
TypeRef get_uncertain_type(xpString type_name);
TypeRef get_struct_or_uncertain_type(xpString name);
TypeRef undefined_type();


TypeRef get_or_add_type(Type type);
TypeRef get_type(Type type);
TypeRef add_type(Type type);
TypeRef add_uncertain_type(xpString type_name);
TypeRef add_struct_type(xpString name, Array<StructField> fields);
void update_uncertain_to_struct(TypeRef uncertain_type, Array<StructField> fields);

template<>
usize xp_hash_func(Type *type);


xpAllocator type_allocator();