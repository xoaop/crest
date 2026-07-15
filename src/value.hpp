#ifndef CREST_VALUE_HPP
#define CREST_VALUE_HPP


#include "xoaop.h"
#include "array.hpp"
#include "cir_key.hpp"


struct Value;
struct Ast;
struct Type;
using TypeRef = Type *;

struct ValueMemory;  // 前置声明，定义在 cir_builder.hpp


enum class MemoryKind: u8 {
    Heap,
    Stack,
};

struct Pointer {
    MemoryKind kind = MemoryKind::Heap;
    ValueMemory *mem = nullptr;
    isize offset = 0;

    static Pointer make(ValueMemory *mem, isize offset);
    static Pointer make_null();
    static Pointer add(Pointer p, isize offset, isize elem_size);

    bool is_null() const;

    Value load(TypeRef type, xpAllocator allocator) const;
    Value load(TypeRef type, isize offset, xpAllocator allocator) const;
    void load_bytes(isize offset, void* dst, isize size) const;
    void store(Value v) const;
    void store_bytes(const void* src, isize size) const;


    void to_bytes(Array<u8>& bytes, isize offset) const;
    static Pointer from_bytes(const Array<u8>& bytes, isize offset);

};


struct ValueMemory {
    MemoryKind kind;
    Array<u8> bytes;

    void init(MemoryKind kind, xpAllocator allocator);
    void free();

    // 分配 size 字节，按 align 对齐，返回起始偏移
    Pointer alloc_bytes(isize size, isize align);

    // 底层字节读写
    void write_bytes(isize offset, const void* src, isize size);
    void read_bytes(isize offset, void* dst, isize size) const;
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


enum class ActualValueType {
    Nothing,
    Integer,
    Float,
    Bool,
    Struct,
    Array,
    Function,
    Pointer,   // comptime 指针：Pointer{mem, offset}，type 字段指向 *T
    Type,      // 类型值：TypeRef 存储在 union 中
    Package,   // 包值：Package* 存储在 union 中
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
    void struct_fields_val(Array<Value> field_values);
    void array_element_values(Array<Value> elem_values);
    void func_val(xpString func_name);

    i128 integer_val() const;
    float float_val() const;
    bool bool_val() const;
    Array<Value> struct_fields_val() const;
    Value struct_field_val(isize index) const;
    Value struct_field_val(xpString field_name) const;
    Array<Value> array_element_values() const;
    Value array_element_val(isize index) const;
    void func_val_key(CIRInstUniqueKey key);
    void pointer_val(Pointer ptr);
    Pointer pointer_val() const;
    void type_val(TypeRef type_ref);
    TypeRef type_val() const;
    void package_val(Package* pkg);
    Package* package_val() const;




    bool is_null = false;
private:
    ActualValueType actual_value_type = ActualValueType::Nothing;
    union {
        i128 integer_value;

        double float_value;

        bool bool_value;

        Array<Value> struct_or_array_fields;

        FuncValue func_value;

        Pointer pointer_value;        // comptime 指针：{mem, offset}

        TypeRef type_value;        // 类型值
        Package* package_value;    // 包值
    };

public:
    FuncValue func_val() const;


    friend Value clone_value(const Value& v, xpAllocator allocator);

    // 提供任意类型的 "零值"
    static Value zero(TypeRef type);
};


//
// Value Makers
//
Value make_value();
Value make_value(TypeRef type);

Value make_value_string(Pointer data, isize count, xpAllocator allocator);

//
// Value Utils
//

Value clone_value(const Value& v, xpAllocator allocator);




//
// [NEW] Value ↔ 字节序列化（comptime 字节级内存模型）
//
void write_value_to_bytes(Array<u8>& bytes, isize offset, const Value& v);
Value read_value_from_bytes(const Array<u8>& bytes, isize offset, TypeRef type, xpAllocator allocator);


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