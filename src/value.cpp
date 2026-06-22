#include "value.hpp"
#include "type.hpp"

#include <cstring>

Value::Value() {
    type = undefined_type();
}

Value::Value(const Value& other) {
    memcpy((void*)this, &other, sizeof(Value));
}

Value& Value::operator=(const Value& other) {
    if (this == &other) return *this;
    memcpy((void*)this, &other, sizeof(Value));
    return *this;
}

Value Value::set_type(TypeRef new_type) {
    type = new_type;
    return *this;
}

Value make_value() {
    Value v = {};
    v.type = undefined_type();
    return v;
}

Value make_value(TypeRef type) {
    Value v = make_value();
    v.type = type;
    return v;
}

ActualValueType Value::actual_type() const {
    return actual_value_type;
}


void Value::integer_val(i128 int_val) {
    actual_value_type = ActualValueType::Integer;
    integer_value = int_val;
}

void Value::float_val(double fval) {
    actual_value_type = ActualValueType::Float;
    float_value = fval;
}

void Value::bool_val(bool bval) {
    actual_value_type = ActualValueType::Bool;
    bool_value = bval;
}

void Value::string_val(xpString str_val) {
    actual_value_type = ActualValueType::String;
    string_value = str_val;
}

void Value::struct_fields_val(Array<Value> field_values) {
    actual_value_type = ActualValueType::Struct;
    struct_or_array_fields = field_values;
}

void Value::array_element_values(Array<Value> elem_values) {
    actual_value_type = ActualValueType::Array;
    struct_or_array_fields = elem_values;
}

void Value::func_val(xpString func_name) {
    actual_value_type = ActualValueType::Function;
    this->func_value.name = func_name;
}

void Value::func_val_key(CIRInstUniqueKey key) {
    this->func_value.func_key = key;
}


i128 Value::integer_val() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Integer);
    return integer_value;
}

float Value::float_val() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Float);
    return float_value;
}

bool Value::bool_val() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Bool);
    return bool_value;
}

xpString Value::string_val() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::String);
    return string_value;
}

Array<Value> Value::struct_fields_val() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Struct);
    return struct_or_array_fields;
}

Value Value::struct_field_val(isize index) const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Struct);
    XP_ASSERT_DEFAULT(is_struct_type(type));
    XP_ASSERT_DEFAULT(index >= 0 && index < type->struct_info.struct_fields.count);

    return struct_fields_val()[index];
}

Value Value::struct_field_val(xpString field_name) const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Struct);
    XP_ASSERT_DEFAULT(is_struct_type(type));


    for(isize i = 0; i < type->struct_info.struct_fields.count; i++) {
        auto field = type->struct_info.struct_fields[i];

        if(xp_string_equal(field.name, field_name)) {
            return struct_field_val(i);
        }
    }

    XP_ASSERT_MSG(false, "struct field not found");
    return Value();
}

Array<Value> Value::array_element_values() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Array);
    return struct_or_array_fields;
}

Value Value::array_element_val(isize index) const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Array);
    XP_ASSERT_DEFAULT(is_array_type(type));
    XP_ASSERT_DEFAULT(index >= 0 && index < type->array_info.count);

    return struct_or_array_fields[index];
}

FuncValue Value::func_val() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Function);
    XP_ASSERT_DEFAULT(is_function_type(type));
    return func_value;
}


TypeRef extract_type_from_val_as_type(Value v) {
    XP_ASSERT_DEFAULT(is_type_type(v.type));
    return v.type->self_type_info;
}




Value TypeProgress::Undefined() {
    Value v = make_value();
    v.set_type(easy_type(Type_untyped_int));
    v.integer_val(static_cast<i128>(ProgressType::Undefined));
    return v;
}

Value TypeProgress::InProgress() {
    Value v = make_value();
    v.set_type(easy_type(Type_untyped_int));
    v.integer_val(static_cast<i128>(ProgressType::InProgress));
    return v;
}

Value TypeProgress::Finished() {
    Value v = make_value();
    v.set_type(easy_type(Type_untyped_int));
    v.integer_val(static_cast<i128>(ProgressType::Finished));
    return v;
}
