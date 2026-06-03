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
    None,
    Integer,
    Float,
    Bool,
    Pointer,
    Struct,
    Array,
    Function,
    String,
    Package,
    Type,
};


struct Value {
public:
    Value();

    Value set_type(TypeRef new_type);

    // set_actual_value 函数族
    void set_integer_value(i128 value);
    void set_float_value(double value);
    void set_bool_value(bool value);
    void set_pointer_value(Value* pointed_value);
    void set_string_value(xpString value);
    void set_struct_value(Array<Value> field_values);
    void set_array_value(Array<Value> element_values);
    void set_function_value(Ast* function_ast, bool is_extern_c);
    void set_package_value(Package* pkg);
    void set_type_value(TypeRef type_value);

    // 辅助：安全获取实际值类型
    bool is_integer_stored() const { return actual_kind == ActualValueKind::Integer; }
    bool is_float_stored() const { return actual_kind == ActualValueKind::Float; }
    bool is_bool_stored() const { return actual_kind == ActualValueKind::Bool; }
    bool is_pointer_stored() const { return actual_kind == ActualValueKind::Pointer; }
    bool is_string_stored() const { return actual_kind == ActualValueKind::String; }
    bool is_struct_stored() const { return actual_kind == ActualValueKind::Struct; }
    bool is_array_stored() const { return actual_kind == ActualValueKind::Array; }
    bool is_function_stored() const { return actual_kind == ActualValueKind::Function; }

    // 类型安全的成员getter函数
    i128 get_integer() const;
    double get_float() const;
    bool get_bool() const;
    Value* get_pointer() const;
    xpString get_string() const;
    Array<Value> get_struct_fields() const;
    Array<Value> get_array_elements() const;
    Ast* get_function_ast() const;
    bool get_function_is_extern_c() const;
    i128 get_enum_value() const;
    Package* get_package_value() const;
    TypeRef get_type_value() const;



    TypeRef type;
    ActualValueKind actual_kind = ActualValueKind::None;


private:
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

    
    // 友元声明：仅允许克隆函数访问私有成员
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




template<>
struct std::formatter<Value> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const Value& v, std::format_context& ctx) const {
        switch (v.actual_kind) {
        case ActualValueKind::None:
            return std::format_to(ctx.out(), "(none)");
        case ActualValueKind::Integer:
            return std::format_to(ctx.out(), "{}", v.get_integer());
        case ActualValueKind::Float:
            return std::format_to(ctx.out(), "{}", v.get_float());
        case ActualValueKind::Bool:
            return std::format_to(ctx.out(), "{}", v.get_bool());
        case ActualValueKind::Pointer:
            return std::format_to(ctx.out(), "{}", static_cast<void*>(v.get_pointer()));
        case ActualValueKind::String:
            return std::format_to(ctx.out(), "\"{}\"", v.get_string());
        case ActualValueKind::Struct: {
            auto fields = v.get_struct_fields();
            auto out = std::format_to(ctx.out(), "{{");
            for (isize i = 0; i < fields.count; i++) {
                if (i > 0) out = std::format_to(out, ", ");
                out = std::format_to(out, "{}", fields[i]);
            }
            return std::format_to(out, "}}");
        }
        case ActualValueKind::Array: {
            auto elems = v.get_array_elements();
            auto out = std::format_to(ctx.out(), "[");
            for (isize i = 0; i < elems.count; i++) {
                if (i > 0) out = std::format_to(out, ", ");
                out = std::format_to(out, "{}", elems[i]);
            }
            return std::format_to(out, "]");
        }
        case ActualValueKind::Function:
            return std::format_to(ctx.out(), "(function {:p})", static_cast<void*>(v.get_function_ast()));
        case ActualValueKind::Package:
            return std::format_to(ctx.out(), "(package {:p})", static_cast<void*>(v.get_package_value()));
        case ActualValueKind::Type:
            return std::format_to(ctx.out(), "(type {:p})", static_cast<void*>(v.get_type_value()));
        }
        return std::format_to(ctx.out(), "(unknown)");
    }
};




#endif // CREST_VALUE_HPP