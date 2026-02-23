#ifndef CREST_VALUE_HPP
#define CREST_VALUE_HPP


#include "xoaop.h"
#include "type.hpp"
#include "array.hpp"

struct Value;




enum class ValueState {
    Unsolved,   // 未求值
    Solving,    // 正在求值, 可能会遇到循环依赖
    Solved,     // 已经求值完毕

    Error,      // 求值过程中发生错误
};

enum class ValueErrorKind {
    UsingRuntimeValue,
    TypeError,
    Overflow,
    CircularDependency,
    DivideByZero,
    OperatorError,
    Other,
};


struct Value {

    Value(): type(NULL), state(ValueState::Unsolved) {}

    Value set_type(TypeRef new_type);
    Value set_is_runtime(bool is_or_not);
    Value set_value_state(ValueState new_state);

    bool has_error();


    TypeRef type = NULL;
    ValueState state = ValueState::Unsolved; // 主要用于求值过程中检测循环依赖
    ValueErrorKind error_kind; // 当state == Error时, 该字段表示错误的具体类型
    
    bool is_runtime_value = false; // 是否是运行时值, 主要用于区分常量和非常量, 以及在求值过程中区分是否需要求值

    Ast *val_ast; // 该值对应的AST节点

    union {

        i128 integer_value;

        double float_value;

        bool bool_value;

        Value *pointed_value;

        Array<Value> struct_field_values;

        Array<Value> array_element_values;

        struct {
            Ast *function_ast;
            bool is_extern_c;
        } function_value;

        xpString string_value;
    };

};


//
// Value Makers
//
Value make_value();
Value make_value(ValueState state);
Value make_comptime_sovled_val(TypeRef type);
Value make_value_for_var_decl(TypeRef type);
Value make_value(TypeRef type);
Value make_value(TypeRef type, bool is_runtime);
Value make_error_value(ValueErrorKind error_kind);

//
// Value Getters
//

i128 get_integer_value(Value& v);
double get_float_value(Value& v);
bool get_bool_value(Value& v);
Value* get_pointer_value(Value& v);
xpString get_string_value(Value& v);
TypeRef get_type_value(Value& v);
Package* get_package_value(Value& v);
Array<Value> get_struct_field_values(Value& v);
Array<Value> get_array_element_values(Value& v);
Ast* get_function_value(Value& v);


//
// Value Utils
//

Value clone_value(Value& v, xpAllocator allocator);


#endif // CREST_VALUE_HPP