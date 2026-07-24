#include "value.hpp"
#include "type.hpp"

#include "error_msg.hpp"

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

Value make_value_string(Pointer data, isize count, xpAllocator allocator) {
    Value v = make_value(string_type_as_struct());
    Array<Value> field_values = make_array<Value>(allocator);

    // data
    Value data_field = make_value(v.type->struct_info.struct_fields[0].type);
    data_field.pointer_val(data);
    field_values.push_back(data_field);

    // count
    Value count_field = make_value(v.type->struct_info.struct_fields[1].type);
    count_field.integer_val(count);
    field_values.push_back(count_field);

    v.struct_fields_val(field_values);

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

void Value::struct_fields_val(Array<Value> field_values) {
    actual_value_type = ActualValueType::Struct;
    struct_or_array_fields = field_values;
}

void Value::array_element_values(Array<Value> elem_values) {
    actual_value_type = ActualValueType::Array;
    struct_or_array_fields = elem_values;
}

void Value::func_val(CIRInstResultRef func_key) {
    actual_value_type = ActualValueType::Function;
    this->func_value.func_key = func_key;
}

void Value::func_val_key(CIRInstResultRef key) {
    this->func_value.func_key = key;
}

Value* Value::ref_val() const {
    ASSERT(actual_value_type == ActualValueType::Reference);
    return ref_value;
}

void Value::ref_val(Value* ref) {
    actual_value_type = ActualValueType::Reference;
    ref_value = ref;
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

// TODO: 重构
FuncValue Value::func_val() const {
    XP_ASSERT_DEFAULT(actual_value_type == ActualValueType::Function);
    XP_ASSERT_DEFAULT(is_function_type(type));
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

void Value::package_val(Package* pkg) {
    actual_value_type = ActualValueType::Package;
    package_value = pkg;
}
Package* Value::package_val() const {
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
            // copy.func_val(v.func_val().name, v.func_val().func_key);
            copy.func_val(v.func_val().func_key);
        } break;
        case ActualValueType::Struct: {
            Array<Value> fields = make_array<Value>(allocator);
            auto src = v.struct_fields_val();
            for (isize i = 0; i < src.count; i++) {
                fields.push_back(clone_value(src[i], allocator));
            }
            copy.struct_fields_val(fields);
        } break;
        case ActualValueType::Array: {
            Array<Value> elems = make_array<Value>(allocator);
            auto src = v.array_element_values();
            for (isize i = 0; i < src.count; i++) {
                elems.push_back(clone_value(src[i], allocator));
            }
            copy.array_element_values(elems);
        } break;
        case ActualValueType::Nothing: break;
        case ActualValueType::Reference: ASSERT(false && "Reference not implemented"); break;
    }
    return copy;
}


Value Value::zero(TypeRef type) {
    Value v = make_value(type);


    // TODO: 配合ValueMemory来实现任意类型的零值, 因为涉及到内存
    if(is_integer_type(type)) {
        v.integer_val(0);
    } else if(is_float_type(type)) {
        v.float_val(0.0);
    } else if(type->kind == Type_bool) {
        v.bool_val(false);
    } else {
        DEBUG_PANIC("unsupported type for zero value");
    }

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


// ============================================================
// [NEW] Value ↔ 字节序列化（comptime 字节级内存模型）
// ============================================================

void write_value_to_bytes(Array<u8>& bytes, isize offset, const Value& v) {
    TypeRef t = v.type;
    isize size = type_size_of(t);

    switch (v.actual_type()) {
        case ActualValueType::Type: {
            isize ptr = (isize)(v.type_val());
            memcpy(&bytes[offset], &ptr, (size_t)size);
            break;
        }
        case ActualValueType::Package: {
            isize ptr = (isize)(v.package_val());
            memcpy(&bytes[offset], &ptr, (size_t)size);
            break;
        }
        case ActualValueType::Integer: {
            i128 val = v.integer_val();
            memcpy(&bytes[offset], &val, (size_t)size);
            break;
        }
        case ActualValueType::Float: {
            double val = v.float_val();
            if (t->kind == Type_f32) {
                float f = (float)val;
                memcpy(&bytes[offset], &f, (size_t)size);
            } else {
                memcpy(&bytes[offset], &val, (size_t)size);
            }
            break;
        }
        case ActualValueType::Bool: {
            u8 b = v.bool_val() ? 1 : 0;
            memcpy(&bytes[offset], &b, (size_t)size);
            break;
        }
        case ActualValueType::Pointer: {
            v.pointer_val().to_bytes(bytes, offset);
            break;
        }
        case ActualValueType::Struct: {
            auto fields = v.struct_fields_val();
            ASSERT(is_struct_type(t));
            for (isize i = 0; i < fields.count; i++) {
                isize field_off = field_offset_in_struct(t, i);
                write_value_to_bytes(bytes, offset + field_off, fields[i]);
            }
            break;
        }
        case ActualValueType::Array: {
            auto elems = v.array_element_values();
            ASSERT(is_array_type(t));
            TypeRef et = t->array_info.element_type;
            isize stride = type_stride_of(et);
            for (isize i = 0; i < elems.count; i++) {
                write_value_to_bytes(bytes, offset + i * stride, elems[i]);
            }
            break;
        }
        case ActualValueType::Function: {
            CIRInstResultRef key = v.func_val().func_key;
            memcpy(&bytes[offset], &key.inst_ref, (size_t)size);
            break;
        }
        case ActualValueType::Nothing: {
            if (size > 0) memset(&bytes[offset], 0, (size_t)size);
            break;
        }
        case ActualValueType::Reference: {
            ASSERT(false && "Reference not implemented");
        } break;
    }
}

Value read_value_from_bytes(const Array<u8>& bytes, isize offset, TypeRef type, xpAllocator allocator) {
    isize size = type_size_of(type);

    switch (type->kind) {
        case Type_i8: case Type_u8: case Type_i32: case Type_u32:
        case Type_i64: case Type_u64: case Type_untyped_int: {
            i128 val = 0;
            memcpy(&val, &bytes[offset], (size_t)size);
            if (is_signed_type(type) && size < 16) {
                isize bits = size * 8;
                val = (val << (128 - bits)) >> (128 - bits);
            }
            Value v = make_value(type);
            v.integer_val(val);
            return v;
        }
        case Type_f32: {
            float val; memcpy(&val, &bytes[offset], (size_t)size);
            Value v = make_value(type); v.float_val((double)val); return v;
        }
        case Type_f64: case Type_untyped_float: {
            double val; memcpy(&val, &bytes[offset], (size_t)size);
            Value v = make_value(type); v.float_val(val); return v;
        }
        case Type_bool: {
            u8 val; memcpy(&val, &bytes[offset], (size_t)size);
            Value v = make_value(type); v.bool_val(val != 0); return v;
        }
        case Type_pointer: {
            Pointer ptr = Pointer::from_bytes(bytes, offset);
            Value v = make_value(type);
            v.pointer_val(ptr);
            return v;
        }
        case Type_type: {
            isize ptr;
            memcpy(&ptr, &bytes[offset], (size_t)size);
            Value v = make_value(type);
            v.type_val((TypeRef)ptr);
            return v;
        }
        case Type_package: {
            isize ptr;
            memcpy(&ptr, &bytes[offset], (size_t)size);
            Value v = make_value(type);
            v.package_val((Package*)ptr);
            return v;
        }
        case Type_function: {
            CIRInstructionRef inst_ref;
            memcpy(&inst_ref, &bytes[offset], (size_t)size);
            Value v = make_value(type);
            CIRInstResultRef key = {};
            key.inst_ref = inst_ref;
            v.func_val_key(key);
            return v;
        }
        case Type_struct: {
            Value v = make_value(type);
            Array<Value> fields = make_array_count<Value>(allocator, type->struct_info.struct_fields.count);
            for (isize i = 0; i < type->struct_info.struct_fields.count; i++) {
                TypeRef ft = type->struct_info.struct_fields[i].type;
                isize field_off = field_offset_in_struct(type, i);
                fields[i] = read_value_from_bytes(bytes, offset + field_off, ft, allocator);
            }
            v.struct_fields_val(fields);
            return v;
        }
        case Type_array: {
            TypeRef et = type->array_info.element_type;
            isize count = type->array_info.count;
            isize stride = type_stride_of(et);

            Value v = make_value(type);
            Array<Value> elems = make_array_count<Value>(allocator, count);
            for (isize i = 0; i < count; i++) {
                elems[i] = read_value_from_bytes(bytes, offset + i * stride, et, allocator);
            }
            v.array_element_values(elems);
            return v;
        }
        default:
            return make_value(type);
    }
}

//
// ValueMemory
//

void ValueMemory::init(MemoryKind kind, xpAllocator allocator) {
    this->kind = kind;
    bytes = make_array<u8>(allocator);
}

void ValueMemory::free() {
    array_free(&bytes);
}

Pointer ValueMemory::alloc_bytes(isize size, isize align) {
    isize addr = align_up(bytes.count, align);
    isize new_count = addr + size;
    bytes.resize(new_count);

    Pointer p = Pointer::make(this, addr);

    return p;
}

void ValueMemory::write_bytes(isize offset, const void* src, isize size) {
    XP_ASSERT(offset >= 0 && offset + size <= bytes.count);
    memcpy(&bytes[offset], src, size);
}

void ValueMemory::read_bytes(isize offset, void* dst, isize size) const {
    XP_ASSERT(offset >= 0 && offset + size <= bytes.count);
    memcpy(dst, &bytes[offset], size);
}




//
// Pointer
//

Pointer Pointer::make(ValueMemory *mem, isize offset) {
    Pointer p{};
    p.mem = mem;
    p.kind = mem->kind;
    p.offset = offset;
    return p;
}

Pointer Pointer::make_null() {
    Pointer p{};
    p.mem = nullptr;
    p.kind = MemoryKind::Heap;
    p.offset = -1;
    return p;
}

bool Pointer::is_null() const {
    return mem == nullptr || offset < 0;
}


Pointer Pointer::add(Pointer p, isize offset, isize elem_size) {
    ASSERT(!p.is_null());

    Pointer new_p = p;
    new_p.offset += offset * elem_size;
    return new_p;
}


Value Pointer::load(TypeRef type, xpAllocator allocator) const {
    ASSERT_MSG(!is_null(), "Cannot load from a null pointer");
    return read_value_from_bytes(mem->bytes, offset, type, allocator);
}

Value Pointer::load(TypeRef type, isize offset, xpAllocator allocator) const {
    ASSERT_MSG(!is_null(), "Cannot load from a null pointer");
    return read_value_from_bytes(mem->bytes, this->offset + offset, type, allocator);
}

void Pointer::load_bytes(isize offset, void* dst, isize size) const {
    ASSERT_MSG(!is_null(), "Cannot load from a null pointer");
    mem->read_bytes(this->offset + offset, dst, size);
}

void Pointer::store(Value v) const {
    ASSERT_MSG(!is_null(), "Cannot store to a null pointer");
    write_value_to_bytes(mem->bytes, offset, v);
}

void Pointer::store_bytes(const void* src, isize size) const {
    ASSERT_MSG(!is_null(), "Cannot store to a null pointer");
    mem->write_bytes(offset, src, size);
}


void Pointer::to_bytes(Array<u8>& bytes, isize offset) const {
    // 前8字节：offset低63位 + kind(1 bit)打包在offset的最高位
    isize packed = (this->offset & ~(1LL << 63)) | ((isize)kind << 63);
    memcpy(&bytes[offset], &packed, sizeof(packed));
    // 后8字节：mem原始指针地址
    isize mem_addr = (isize)mem;
    memcpy(&bytes[offset + sizeof(packed)], &mem_addr, sizeof(mem_addr));
}


Pointer Pointer::from_bytes(const Array<u8>& bytes, isize offset) {
    isize packed;
    memcpy(&packed, &bytes[offset], sizeof(packed));
    isize mem_addr;
    memcpy(&mem_addr, &bytes[offset + sizeof(packed)], sizeof(mem_addr));

    Pointer ptr{};
    ptr.kind = (MemoryKind)(packed >> 63);
    ptr.offset = packed & ~(1LL << 63);
    ptr.mem = (ValueMemory*)mem_addr;
    return ptr;
}