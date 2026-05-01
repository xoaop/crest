#include "value.hpp"
#include "type.hpp"

Value Value::set_is_runtime(bool is_or_not) {
    is_runtime_value = is_or_not;
    return *this;
}



Value Value::set_value_state(ValueState new_state) {
    state = new_state;
    return *this;
}

Value Value::set_type(TypeRef new_type) {
    type = new_type;
    return *this;
}


bool Value::has_error() {
    return state == ValueState::Error || type == error_type();
}


Value make_value() {
    Value v = {};
    v.type = undefined_type();
    v.state = ValueState::Unsolved;
    v.is_runtime_value = false;
    v.actual_kind = ActualValueKind::Integer; // 初始化默认值，避免未定义行为
    // v.error_kind = ValueErrorKind::Other;

    return v;
}

Value make_value(ValueState state) {
    return make_value().set_value_state(state);
}

Value make_comptime_sovled_val(TypeRef type) {
    return make_value(type).set_is_runtime(false).set_value_state(ValueState::Solved);
}

Value make_value_for_var_decl(TypeRef type) {
    return make_value(type).set_is_runtime(true).set_value_state(ValueState::Solved);
}


Value make_value(TypeRef type) {
    Value v = make_value();
    v.type = type;
    return v;
}

Value make_value(TypeRef type, bool is_runtime) {
    Value v = make_value(type);
    v.set_is_runtime(is_runtime);

    return v;
}

Value make_error_value() {
    Value v = make_value(ValueState::Error);
    v.type = error_type();
    return v;
}

// ========== set_actual_value 函数族实现 ==========
void Value::set_integer_value(i128 value) {
    integer_value = value;
    actual_kind = ActualValueKind::Integer;
}

void Value::set_float_value(double value) {
    float_value = value;
    actual_kind = ActualValueKind::Float;
}

void Value::set_bool_value(bool value) {
    bool_value = value;
    actual_kind = ActualValueKind::Bool;
}

void Value::set_pointer_value(Value* pointed_value) {
    this->pointed_value = pointed_value;
    actual_kind = ActualValueKind::Pointer;
}

void Value::set_string_value(xpString value) {
    string_value = value;
    actual_kind = ActualValueKind::String;
}

void Value::set_struct_value(Array<Value> field_values) {
    struct_field_values = field_values;
    actual_kind = ActualValueKind::Struct;
}

void Value::set_array_value(Array<Value> element_values) {
    array_element_values = element_values;
    actual_kind = ActualValueKind::Array;
}

void Value::set_function_value(Ast* function_ast, bool is_extern_c) {
    function_value.function_ast = function_ast;
    function_value.is_extern_c = is_extern_c;
    actual_kind = ActualValueKind::Function;
}




i128 get_integer_value(Value& v) {
    XP_ASSERT_DEFAULT(is_integer_or_untyped_type(v.type) || is_enum_type(v.type));
    XP_ASSERT_DEFAULT(v.is_integer_stored());

    return v.integer_value;
}

double get_float_value(Value& v) {
    XP_ASSERT_DEFAULT(is_float_or_untyped_type(v.type));
    XP_ASSERT_DEFAULT(v.is_float_stored());

    return v.float_value;
}

bool get_bool_value(Value& v) {
    XP_ASSERT_DEFAULT(v.type->kind == Type_bool);
    XP_ASSERT_DEFAULT(v.is_bool_stored());

    return v.bool_value;
}

Value* get_pointer_value(Value& v) {
    XP_ASSERT_DEFAULT(is_pointer_type(v.type));
    XP_ASSERT_DEFAULT(v.is_pointer_stored());

    return v.pointed_value;
}

xpString get_string_value(Value& v) {
    XP_ASSERT_DEFAULT(is_string_struct_type(v.type));
    XP_ASSERT_DEFAULT(v.is_string_stored());

    return v.string_value;
}

TypeRef get_type_value(Value& v) {
    XP_ASSERT_DEFAULT(is_type_type(v.type));

    return v.type->self_type_info;
}

Package* get_package_value(Value& v) {
    XP_ASSERT_DEFAULT(is_package_type(v.type));

    return v.type->package_info;
}

Array<Value> get_struct_field_values(Value& v) {
    XP_ASSERT_DEFAULT(is_struct_type(v.type));
    XP_ASSERT_DEFAULT(v.is_struct_stored());

    return v.struct_field_values;
}

Array<Value> get_array_element_values(Value& v) {
    XP_ASSERT_DEFAULT(is_array_type(v.type));
    XP_ASSERT_DEFAULT(v.is_array_stored());

    return v.array_element_values;
}

Ast *get_function_value(Value& v) {
    XP_ASSERT_DEFAULT(is_function_type(v.type));
    XP_ASSERT_DEFAULT(v.is_function_stored());

    return v.function_value.function_ast;
}


i128 get_enum_val_as_integer(Value& v) {
    XP_ASSERT_DEFAULT(is_enum_type(v.type));
    XP_ASSERT_DEFAULT(v.is_integer_stored());

    return v.integer_value;
}




Value clone_value(Value& v, xpAllocator allocator) {
    Value new_value = v;

    if(is_string_struct_type(v.type)) {
        new_value.string_value = v.string_value;
    } else if(is_struct_type(v.type) && !is_string_struct_type(v.type)) {
        // NOTE: 注意区分普通结构体和字符串(结构体)

        new_value.struct_field_values = array_copy(&v.struct_field_values, allocator);
    } else if(is_array_type(v.type)) {
        new_value.array_element_values = array_copy(&v.array_element_values, allocator);
    }

    return new_value;
}