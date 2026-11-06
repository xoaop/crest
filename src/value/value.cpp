#include "value.hpp"
#include "value_array.hpp"
#include "type.hpp"

#include "error_msg.hpp"
#include "context.hpp"
#include "print.hpp"

#include <cstring>

Value::Value() {
    // 清零整个对象：trivial 拷贝（memcpy）会复制 union 未初始化字节（UB），
    // Debug -O0 侥幸不崩，Release -O2 会暴露。构造时归零消除。
    memset(this, 0, sizeof(*this));
    type = undefined_type();
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

bool Value::equal(const Value& other) const {
    const auto kind = actual_type();
    if(kind != other.actual_type()) {
        return false;
    }
    switch(kind) {
    case ActualValueType::Nothing:
        return true;
    case ActualValueType::Integer:
        return integer_val() == other.integer_val();
    case ActualValueType::Float:
        return float_val() == other.float_val();
    case ActualValueType::Bool:
        return bool_val() == other.bool_val();
    case ActualValueType::Type:
        return type_val() == other.type_val();
    case ActualValueType::Pointer:
        return pointer_val().slot == other.pointer_val().slot;
    case ActualValueType::Function: {
        const auto self_func = is_unresolved_func_val() ? unresolved_func_val() : func_val();
        const auto other_func = other.is_unresolved_func_val() ? other.unresolved_func_val() : other.func_val();
        return self_func.func_key == other_func.func_key;
    }
    case ActualValueType::Package:
        return package_val() == other.package_val();
    case ActualValueType::Struct: {
        const auto self_fields = struct_fields_val();
        const auto other_fields = other.struct_fields_val();
        if(self_fields.count != other_fields.count) {
            return false;
        }
        FOR_0_TO(self_fields.count) {
            if(!self_fields[it].equal(other_fields[it])) {
                return false;
            }
        }
        return true;
    }
    case ActualValueType::Array: {
        const auto self_elems = array_element_values();
        const auto other_elems = other.array_element_values();
        if(self_elems.count != other_elems.count) {
            return false;
        }
        FOR_0_TO(self_elems.count) {
            if(!self_elems[it].equal(other_elems[it])) {
                return false;
            }
        }
        return true;
    }
    }
    return false;
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

void Value::struct_fields_val(Array<Value> field_values) {
    actual_value_type = ActualValueType::Struct;
    struct_or_array_fields = field_values;
}

void Value::array_element_values(Array<Value> elem_values) {
    actual_value_type = ActualValueType::Array;
    struct_or_array_fields = elem_values;
}

void Value::func_val(Ref<CIRInstResult> func_key) {
    actual_value_type = ActualValueType::Function;
    this->func_value.func_key = func_key;
}

void Value::func_val_key(Ref<CIRInstResult> key) {
    this->func_value.func_key = key;
}


i128 Value::integer_val() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Integer);
    return integer_value;
}

double Value::float_val() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Float);
    return float_value;
}

bool Value::bool_val() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Bool);
    return bool_value;
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


    FOR(type->struct_info.struct_fields) {
        auto field = it;

        if(field.name == get_or_insert_global_string(field_name)) {
            return struct_field_val(it_index);
        }
    }

    XP_ASSERT_MSG(false, "struct field not found");
    return Value();
}

Pointer slice_data_val(const Value& slice_value) {
    return slice_value.struct_field_val(0).pointer_val();
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

ValueRef Value::struct_field_ref(isize index) {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Struct);
    XP_ASSERT_DEFAULT(index >= 0 && index < struct_or_array_fields.count);

    return &struct_or_array_fields[index];
}

ValueRef Value::array_element_ref(isize index) {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Array);
    XP_ASSERT_DEFAULT(index >= 0 && index < struct_or_array_fields.count);

    return &struct_or_array_fields[index];
}

ValueRef Value::element_ref(isize index) {
    // 只有实际握着元素缓冲的值才有"元素"这回事
    if(actual_value_type != ActualValueType::Array
       || index < 0 || index >= struct_or_array_fields.count) {
        return nullptr;
    }
    return &struct_or_array_fields[index];
}

// TODO: 重构
FuncValue Value::func_val() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Function);
    XP_ASSERT_DEFAULT(is_function_type(type));
    return func_value;
}

bool Value::is_unresolved_func_val() const {
    return actual_value_type == ActualValueType::Function && type == undefined_type();
}

FuncValue Value::unresolved_func_val() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Function);
    XP_ASSERT_DEFAULT(type == undefined_type());
    return func_value;
}


void Value::pointer_val(Pointer ptr) {
    actual_value_type = ActualValueType::Pointer;
    pointer_value = ptr;
}
Pointer Value::pointer_val() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Pointer);
    return pointer_value;
}

void Value::type_val(TypeRef type_ref) {
    ASSERT(is_type_type(type));

    actual_value_type = ActualValueType::Type;
    type_value = type_ref;
}
TypeRef Value::type_val() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Type);
    return type_value;
}

void Value::package_val(Ref<Package> pkg) {
    actual_value_type = ActualValueType::Package;
    package_value = pkg;
}
Ref<Package> Value::package_val() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Package);
    return package_value;
}






Value clone_value(const Value& v, xpAllocator allocator) {
    Value copy = make_value(v.type);
    copy.is_null = v.is_null;

    switch (v.actual_type()) {
        case ActualValueType::Integer:  copy.integer_val(v.integer_val()); break;
        case ActualValueType::Float:    copy.float_val(v.float_val()); break;
        case ActualValueType::Bool:     copy.bool_val(v.bool_val()); break;
        case ActualValueType::Pointer:  copy.pointer_val(v.pointer_val()); break;
        case ActualValueType::Type:     copy.type_val(v.type_val()); break;
        case ActualValueType::Package:  copy.package_val(v.package_val()); break;
        case ActualValueType::Function: {
            copy.actual_value_type = ActualValueType::Function;
            copy.func_value = v.func_value;
        } break;
        case ActualValueType::Struct: {
            Array<Value> fields = make_array<Value>(allocator);
            auto src = v.struct_fields_val();
            FOR(src) {
                fields.push_back(clone_value(it, allocator));
            }
            copy.struct_fields_val(fields);
        } break;
        case ActualValueType::Array: {
            Array<Value> elems = make_array<Value>(allocator);
            auto src = v.array_element_values();
            FOR(src) {
                elems.push_back(clone_value(it, allocator));
            }
            copy.array_element_values(elems);
        } break;
        case ActualValueType::Nothing: break;
    }
    return copy;
}




Value Value::zero(TypeRef type) {
    return zero(type, permanent_allocator());
}

Value Value::zero(TypeRef type, xpAllocator allocator) {
    Value v = make_value(type);

    if(is_integer_type(type)) {
        v.integer_val(0);
    } else if(is_float_type(type)) {
        v.float_val(0.0);
    } else if(type->kind == Type_bool) {
        v.bool_val(false);
    } else if(type->kind == Type_pointer) {
        v.pointer_val(Pointer::make_null());
        v.is_null = true;
    } else if(is_struct_type(type)) {
        Array<Value> fields = make_array<Value>(allocator);
        FOR(type->struct_info.struct_fields) {
            fields.push_back(zero(it.type, allocator));
        }
        v.struct_fields_val(fields);
    } else if(is_array_type(type)) {
        Array<Value> elems = make_array<Value>(allocator);
        FOR_0_TO(type->array_info.count) {
            elems.push_back(zero(type->array_info.element_type, allocator));
        }
        v.array_element_values(elems);
    }
    // 其余类型（void/type/function/package）没有数据载荷，留 Nothing

    return v;
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





//
// Pointer
//

Pointer Pointer::make_slot(ValueRef slot) {
    Pointer p{};
    p.slot = slot;
    return p;
}

Pointer Pointer::make_null() {
    return Pointer{};
}

bool Pointer::is_null() const {
    return slot == nullptr;
}


Value Pointer::load(TypeRef pointee_type) const {
    ASSERT_MSG(slot != nullptr, "Cannot load from a null pointer");

    // 指针声明指向单个 T，而槽实际握着 [N]T（连续元素）：指向的是首元素
    if(slot->actual_type() == ActualValueType::Array && !is_array_type(pointee_type)) {
        ValueRef first = slot->element_ref(0);
        ASSERT_MSG(first != nullptr, "指针目标是个空数组槽");
        return *first;
    }

    return *slot;
}

void Pointer::store(Value v) const {
    store(v, permanent_allocator());
}

void Pointer::store(Value v, xpAllocator allocator) const {
    ASSERT_MSG(slot != nullptr, "Cannot store to a null pointer");
    // 深拷：槽自己持有数据，不跟源值共享 Array
    *slot = clone_value(v, allocator);
}
