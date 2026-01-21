#pragma once

#include "xoaop.h"
#include "array.hpp"
#include "common.hpp"



enum TypeKind {
    Type_Undefined = 0,

    
    Type_i8,
    Type_i32,
    Type_i64,
    Type_u8,
    Type_u32,
    Type_u64,
    Type_f32,
    Type_f64,
    
    Type_bool, // i1 in llvm
    Type_void,

    Type_function,

    Type_pointer,

    Type_struct,

    Type_array,


    Type_untyped_int, // 用于常量表达式推导阶段的字面量类型
    Type_untyped_float,

    Type_uncertain, // 用于类型推导阶段, 表示类型还不确定

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