#include "value.hpp"


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




i128 get_integer_value(Value& v) {
    XP_ASSERT_DEFAULT(is_integer_or_untyped_type(v.type));

    return v.integer_value;
}

double get_float_value(Value& v) {
    XP_ASSERT_DEFAULT(is_float_or_untyped_type(v.type));

    return v.float_value;
}

bool get_bool_value(Value& v) {
    XP_ASSERT_DEFAULT(v.type->kind == Type_bool);

    return v.bool_value;
}

Value* get_pointer_value(Value& v) {
    XP_ASSERT_DEFAULT(is_pointer_type(v.type));

    return v.pointed_value;
}

xpString get_string_value(Value& v) {
    XP_ASSERT_DEFAULT(is_string_struct_type(v.type));

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

    return v.struct_field_values;
}

Array<Value> get_array_element_values(Value& v) {
    XP_ASSERT_DEFAULT(is_array_type(v.type));

    return v.array_element_values;
}

Ast *get_function_value(Value& v) {
    XP_ASSERT_DEFAULT(is_function_type(v.type));

    return v.function_value.function_ast;
}





Value clone_value(Value& v, xpAllocator allocator) {
    Value new_value = v;

    if(is_struct_type(v.type)) {
        new_value.struct_field_values = array_copy(&v.struct_field_values, allocator);
    } else if(is_array_type(v.type)) {
        new_value.array_element_values = array_copy(&v.array_element_values, allocator);
    }

    return new_value;
}