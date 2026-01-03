#pragma once

#include "xoaop.h"
#include "array.hpp"
#include "common.hpp"

enum TypeKind {
    Type_Undefined,

    
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

};

TypeKind string_to_type_kind(xpString str);


struct Type {
    TypeKind kind;

    union {
        struct {
            Array<Type> param_types;
            Type *return_type;
        } function_info;

        Type *pointed_type;

        Array<Type> struct_members;
        
        struct {
            Type *element_type;
            isize count;
        } array_info;
    };
};



Type make_type(TypeKind kind);
Type *alloc_type(xpAllocator allocator, TypeKind kind);

Type make_pointer_type(TypeKind base_type_kind, isize level_of_pointer);
Type make_pointer_type(Type pointed_type);
Type get_pointed_type(Type pointer_type);

bool is_equal_type(Type a, Type b);
int size_of_type(Type type);
bool is_integer_type(Type type);
bool is_integer_or_bool_type(Type type);
bool is_signed_type(Type type);
bool is_signed_or_bool_type(Type type);
bool is_unsigned_type(Type type);
bool is_float_type(Type type);
bool is_certain_type(Type type);
bool is_pointer_type(Type type);
Type get_common_type(Type a, Type b);


bool check_literal_overflow(TypeKind type_kind, i128 result, double dresult);


void point_to(Type *type, Type *pointed_type);
void struct_add_member(Type *type, Type member_type);








void print_type(Type type);