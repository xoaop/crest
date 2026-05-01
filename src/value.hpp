#ifndef CREST_VALUE_HPP
#define CREST_VALUE_HPP


#include "xoaop.h"
#include "array.hpp"


struct Value;
struct Ast;
struct Package;
struct Type;
using TypeRef = Type *;



enum class ValueState {
    Unsolved,   // 未求值
    Solving,    // 正在求值, 可能会遇到循环依赖
    Solved,     // 已经求值完毕

    Error,      // 求值过程中发生错误
};

enum class ValueErrorKind {
    ErrorValue,
    UsingRuntimeValue,
    TypeError,
    Overflow,
    CircularDependency,
    DivideByZero,
    OperatorError,
    Other,
};

using ValueResult = xpResult<Value, ValueErrorKind>;


enum class ActualValueKind {
    Integer,
    Float,
    Bool,
    Pointer,
    Struct,
    Array,
    Function,
    String,
    Type,
    Package,
};


struct Value {

    Value(): type(NULL), state(ValueState::Unsolved) {}

    Value set_type(TypeRef new_type);
    Value set_is_runtime(bool is_or_not);
    Value set_value_state(ValueState new_state);

    bool has_error();

    // ========== set_actual_value 函数族 ==========
    // 设置整数值（包括enum底层值）
    void set_integer_value(i128 value);

    // 设置浮点数值
    void set_float_value(double value);

    // 设置布尔值
    void set_bool_value(bool value);

    // 设置指针值
    void set_pointer_value(Value* pointed_value);

    // 设置字符串值
    void set_string_value(xpString value);

    // 设置结构体字段值
    void set_struct_value(Array<Value> field_values);

    // 设置数组元素值
    void set_array_value(Array<Value> element_values);

    // 设置函数值
    void set_function_value(Ast* function_ast, bool is_extern_c);

    // ========== 辅助：安全获取实际值类型 ==========
    bool is_integer_stored() const { return actual_kind == ActualValueKind::Integer; }
    bool is_float_stored() const { return actual_kind == ActualValueKind::Float; }
    bool is_bool_stored() const { return actual_kind == ActualValueKind::Bool; }
    bool is_pointer_stored() const { return actual_kind == ActualValueKind::Pointer; }
    bool is_string_stored() const { return actual_kind == ActualValueKind::String; }
    bool is_struct_stored() const { return actual_kind == ActualValueKind::Struct; }
    bool is_array_stored() const { return actual_kind == ActualValueKind::Array; }
    bool is_function_stored() const { return actual_kind == ActualValueKind::Function; }


    TypeRef type = NULL;
    ValueState state = ValueState::Unsolved; // 主要用于求值过程中检测循环依赖
    // ValueErrorKind error_kind; // 当state == Error时, 该字段表示错误的具体类型

    bool is_runtime_value = false; // 是否是运行时值, 主要用于区分常量和非常量, 以及在求值过程中区分是否需要求值

    Ast *val_ast; // 该值对应的AST节点


    ActualValueKind actual_kind; // 该值的实际类型, 主要用于区分不同类型的值, 比如enum类型的值虽然底层是整数, 但实际类型是enum
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
Value make_error_value();

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
i128 get_enum_val_as_integer(Value& v);


//
// Value Utils
//

Value clone_value(Value& v, xpAllocator allocator);


#endif // CREST_VALUE_HPP