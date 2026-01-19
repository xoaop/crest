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
bool is_equal_type(Type a, Type b);


struct StructField;

struct Type {
    TypeKind kind;
    xpString type_name; // 目前仅用于struct等复合类型的记录
    
    union {
        
        // 函数
        struct {
            Array<Type> param_types;
            Type *return_type;
        } function_info;
        
        // 指针
        Type *pointed_type;
        
        // 结构体
        Array<StructField> struct_fields;
        
        
        // 数组
        struct {
            Type *element_type;
            isize count;
        } array_info;
    };

    bool operator== (const Type &other) const {
        return is_equal_type(*this, other);
    }
};


struct StructField {
    xpString name;
    Type type;
};










Type make_type(TypeKind kind);
Type *alloc_type(xpAllocator allocator, TypeKind kind);

Type copy_type(Type *src);

Type make_pointer_type(TypeKind base_type_kind, isize level_of_pointer);
Type make_pointer_type(Type base_type, isize level_of_pointer);
Type make_pointer_type(Type pointed_type);
Type get_pointed_type(Type pointer_type);
Type get_innermost_type_of_pointer(Type pointer_type);

Type make_struct_type();
Type make_struct_type(xpString name);

bool is_equal_type(Type a, Type b);
bool is_integer_type(Type type);
bool is_integer_or_bool_type(Type type);
bool is_signed_type(Type type);
bool is_signed_or_bool_type(Type type);
bool is_unsigned_type(Type type);
bool is_float_type(Type type);
bool is_certain_type(Type type);
bool is_pointer_type(Type type);
bool is_struct_type(Type type);


bool is_basic_type_kind(TypeKind kind);
bool is_complex_type_kind(TypeKind kind);
bool is_easy_type_kind(TypeKind kind);
bool is_hard_type_kind(TypeKind kind);



Type get_common_type(Type a, Type b);


bool check_literal_overflow(TypeKind type_kind, i128 result, double dresult);


void point_to(Type *type, Type *pointed_type);
void struct_add_member(Type *type, xpString name, Type member_type);





void print_type(Type type);






// TODO: 大更新, 把ast里的类型信息都改成TypeRef
// 所有详细类型信息都放在type表里
// 统一类型获取接口
// 解决结构体字段信息获取需要额外操作问题
typedef Type *TypeRef;


void init_type_table();



struct TypeTable {
    xpHashSet<Type> type_set;
};




TypeRef basic_type(TypeKind kind);
TypeRef pointer_type(TypeRef pointed_type);
TypeRef function_type(Array<TypeRef> param_types, TypeRef return_type);
TypeRef struct_type(xpString name);


TypeRef get_type(Type type);
TypeRef add_type(Type type);



template<>
usize xp_hash_func(Type *type);
