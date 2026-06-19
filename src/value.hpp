#ifndef CREST_VALUE_HPP
#define CREST_VALUE_HPP


#include "xoaop.h"
#include "array.hpp"
#include "cir_key.hpp"


struct Value;
struct Ast;
struct Type;
using TypeRef = Type *;



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


enum class ActualValueType {
    Nothing,
    Integer,
    Float,
    Bool,
    String,
    Struct,
    Array,
    Function,
};

struct Value {
public:
    Value();
    Value(const Value& other);
    Value& operator=(const Value& other);

    Value set_type(TypeRef new_type);

    TypeRef type;


    ActualValueType actual_type() const;

    void integer_val(i128 int_val);
    void float_val(double float_val);
    void bool_val(bool bool_val);
    void string_val(xpString str_val);
    void struct_fields_val(Array<Value> field_values);
    void array_element_values(Array<Value> elem_values);
    void func_val(xpString func_name);

    i128 integer_val() const;
    float float_val() const;
    bool bool_val() const;
    xpString string_val() const;
    Array<Value> struct_fields_val() const;
    Value struct_field_val(isize index) const;
    Value struct_field_val(xpString field_name) const;
    Array<Value> array_element_values() const;
    Value array_element_val(isize index) const;
    void func_val_key(CIRInstUniqueKey key);




    bool is_null = false;
private:
    ActualValueType actual_value_type = ActualValueType::Nothing;
    union {
        i128 integer_value;

        double float_value;

        bool bool_value;

        xpString string_value;

        Array<Value> struct_or_array_fields;

        FuncValue func_value;
    };

public:
    FuncValue func_val() const;


    friend Value clone_value(Value& v, xpAllocator allocator);
};


//
// Value Makers
//
Value make_value();
Value make_value(TypeRef type);


//
// Value Utils
//

Value clone_value(Value& v, xpAllocator allocator);


TypeRef extract_type_from_val_as_type(Value v);




enum class ProgressType {
    Undefined,
    InProgress,
    Finished,
};

struct TypeProgress {

    static Value Undefined();
    static Value InProgress();
    static Value Finished();
};







template<>
struct std::formatter<Value> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const Value& v, std::format_context& ctx) const {
        switch (v.actual_type()) {
            case ActualValueType::Integer:
                return std::format_to(ctx.out(), "{}", v.integer_val());
            case ActualValueType::Float:
                return std::format_to(ctx.out(), "{}", v.float_val());
            case ActualValueType::Bool:
                return std::format_to(ctx.out(), "{}", v.bool_val());
            default:
                return std::format_to(ctx.out(), "(unimplemented)");
        }
    }
};




#endif // CREST_VALUE_HPP