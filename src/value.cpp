#include "value.hpp"
#include "type.hpp"






Value::Value() {
    type = undefined_type();
}



Value Value::set_type(TypeRef new_type) {
    type = new_type;
    return *this;
}


Value make_value() {
    Value v = {};
    v.type = undefined_type();
    v.actual_kind = ActualValueKind::None; // 初始化默认值，避免未定义行为
    return v;
}



Value make_value(TypeRef type) {
    Value v = make_value();
    v.type = type;
    return v;
}



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

void Value::set_package_value(Package* pkg) {
    type = package_type(pkg);
    actual_kind = ActualValueKind::Package;
}

void Value::set_type_value(TypeRef type_value) {
    type = type_value;
    actual_kind = ActualValueKind::Type;
}




i128 Value::get_integer() const {
    XP_ASSERT_DEFAULT(is_integer_stored());
    return integer_value;
}

double Value::get_float() const {
    XP_ASSERT_DEFAULT(is_float_stored());
    return float_value;
}

bool Value::get_bool() const {
    XP_ASSERT_DEFAULT(is_bool_stored());
    return bool_value;
}

Value* Value::get_pointer() const {
    XP_ASSERT_DEFAULT(is_pointer_stored());
    return pointed_value;
}

xpString Value::get_string() const {
    XP_ASSERT_DEFAULT(is_string_stored());
    return string_value;
}

Array<Value> Value::get_struct_fields() const {
    XP_ASSERT_DEFAULT(is_struct_stored());
    return struct_field_values;
}

Array<Value> Value::get_array_elements() const {
    XP_ASSERT_DEFAULT(is_array_stored());
    return array_element_values;
}

Ast* Value::get_function_ast() const {
    XP_ASSERT_DEFAULT(is_function_stored());
    return function_value.function_ast;
}

bool Value::get_function_is_extern_c() const {
    XP_ASSERT_DEFAULT(is_function_stored());
    return function_value.is_extern_c;
}

i128 Value::get_enum_value() const {
    XP_ASSERT_DEFAULT(is_enum_type(type));
    XP_ASSERT_DEFAULT(is_integer_stored());
    return integer_value;
}


Package* Value::get_package_value() const {
    XP_ASSERT_DEFAULT(is_package_type(type));
    return type->package_info;
}

TypeRef Value::get_type_value() const {
    XP_ASSERT_DEFAULT(is_type_type(type));
    return type->self_type_info;
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