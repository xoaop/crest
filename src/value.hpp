#ifndef CREST_VALUE_HPP
#define CREST_VALUE_HPP


#include "xoaop.h"
#include "array.hpp"
#include "cir_instruction_ref.hpp"


struct Value;
struct Ast;
struct Type;
using TypeRef = Type *;
struct Package;
struct ValueMemory;  // 前置声明，定义在 cir_builder.hpp

using PackageRef = isize;


enum class MemoryKind: u8 {
    Heap,
    Stack,
    String,  // 在可执行文件中有对应地址的数据（如字符串字面量）
};

struct Pointer {
    static constexpr isize BYTE_SIZE = 17; // u8 kind + isize offset + isize mem_ptr

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
    Reference, // 引用值：Value* 存储在 union 中
};



enum class BuiltinKind : u8 {
    None,
    SizeOf,
};


struct FuncValue {
    CIRInstResultRef                func_key;
    BuiltinKind                     builtin_kind = BuiltinKind::None;
};

struct Value {
public:

    Value();
    // 拷贝构造/赋值用编译器隐式生成的 trivial 拷贝（bitwise，同 memcpy）
    // 不再手写——否则 Value 被当作非平凡拷贝，连累含它的 union 无法 = default

    Value set_type(TypeRef new_type);

    TypeRef type;


    ActualValueType actual_type() const;

    void integer_val(i128 int_val);
    void float_val(double float_val);
    void bool_val(bool bool_val);
    void struct_fields_val(Array<Value> field_values);
    void array_element_values(Array<Value> elem_values);
    void func_val(CIRInstResultRef func_key);
    void func_val(CIRInstResultRef func_key, BuiltinKind builtin_kind);
    void func_val_key(CIRInstResultRef key);
    void pointer_val(Pointer ptr);
    void type_val(TypeRef type_ref);
    void package_val(PackageRef pkg);
    void ref_val(Value* ref);


    i128 integer_val() const;
    float float_val() const;
    bool bool_val() const;
    Array<Value> struct_fields_val() const;
    Value struct_field_val(isize index) const;
    Value struct_field_val(xpString field_name) const;
    Array<Value> array_element_values() const;
    Value array_element_val(isize index) const;
    Pointer pointer_val() const;
    TypeRef type_val() const;
    PackageRef package_val() const;
    FuncValue func_val() const;
    Value* ref_val() const;
    



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

        PackageRef package_value;    // 包值（全局包表编号）

        Value* ref_value;
    };

public:
    friend Value clone_value(const Value& v, xpAllocator allocator);

    // 提供任意类型的 "零值"
    static Value zero(TypeRef type);
};


//
// Value Makers
//
Value make_value();
Value make_value(TypeRef type);

//
// Value Utils
//

Value clone_value(const Value& v, xpAllocator allocator);




//
// [NEW] Value ↔ 字节序列化（comptime 字节级内存模型）
//
void write_value_to_bytes(Array<u8>& bytes, isize offset, const Value& v);
Value read_value_from_bytes(const Array<u8>& bytes, isize offset, TypeRef type, xpAllocator allocator);

//
// 类型序列化布局函数
//
isize type_serialize_size(TypeRef type);
isize type_serialize_align(TypeRef type);
isize type_serialize_stride(TypeRef type);
isize field_serialize_offset(TypeRef struct_type, isize index);
isize serialize_align_up(isize value, isize alignment);


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