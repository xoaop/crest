#pragma once


#include "xoaop.h"
#include "common.hpp"
#include "array.hpp"
#include "cir_instruction_ref.hpp"


struct Value;
struct Ast;
struct Type;
using TypeRef = Type *;
struct Package;
struct ValueArray;

// 指向某个容器持有的 Value 槽（ValueArray 或 Array<Value> 的元素），不是随便一个 Value*
using ValueRef = Value *;


// 闲置：ValueMemory 体系（暂无使用者，见 value.cpp 的同名一节）
//
// struct ValueMemory;
//
// enum class MemoryKind: u8 {
//     Heap,
//     Stack,
//     String,  // 在可执行文件中有对应地址的数据（如字符串字面量）
// };
//
// struct MemPointer {
//     static constexpr isize BYTE_SIZE = 17; // u8 kind + isize offset + isize mem_ptr
//
//     MemoryKind kind = MemoryKind::Heap;
//     ValueMemory *mem = nullptr;
//     isize offset = 0;
//
//     static MemPointer make(ValueMemory *mem, isize offset);
//     static MemPointer make_null();
//     static MemPointer add(MemPointer p, isize offset, isize elem_size);
//
//     bool is_null() const;
//
//     Value load(TypeRef type, xpAllocator allocator) const;
//     Value load(TypeRef type, isize offset, xpAllocator allocator) const;
//     void load_bytes(isize offset, void* dst, isize size) const;
//     void store(Value v) const;
//     void store_bytes(const void* src, isize size) const;
//
//     void to_bytes(Array<u8>& bytes, isize offset) const;
//     static MemPointer from_bytes(const Array<u8>& bytes, isize offset);
// };


// comptime 指针：指向某个容器持有的 Value 槽
struct Pointer {
    ValueRef slot = nullptr;

    static Pointer make_slot(ValueRef slot);
    static Pointer make_null();
    bool is_null() const;

    // 元素：往目标的第 index 个元素走
    Pointer add(isize index) const;

    Value load() const;
    void store(Value v) const;
    void store(Value v, xpAllocator allocator) const;   // 先深拷再落，槽自己持有数据
};


// 闲置：ValueMemory 体系的字节内存区
//
// struct ValueMemory {
//     MemoryKind kind;
//     Array<u8> bytes;
//
//     void init(MemoryKind kind, xpAllocator allocator);
//     void free();
//
//     // 分配 size 字节，按 align 对齐，返回起始偏移
//     MemPointer alloc_bytes(isize size, isize align);
//
//     // 底层字节读写
//     void write_bytes(isize offset, const void* src, isize size);
//     void read_bytes(isize offset, void* dst, isize size) const;
// };




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
    Pointer,   // comptime 指针：Pointer{Value* slot}，type 字段指向 *T
    Type,      // 类型值：TypeRef 存储在 union 中
    Package,   // 包值：Package* 存储在 union 中
    ValueRef,  // 预留：指向 Value 槽（现由 Pointer 承担）
};



struct FuncValue {
    Ref<CIRInstResult>                func_key;
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
    void func_val(Ref<CIRInstResult> func_key);
    void func_val_key(Ref<CIRInstResult> key);
    // 闲置：ValueMemory 体系的字节指针
    // void mem_pointer_val(MemPointer ptr);
    void pointer_val(Pointer ptr);          // comptime 指针
    void type_val(TypeRef type_ref);
    void package_val(Ref<Package> pkg);


    i128 integer_val() const;
    float float_val() const;
    bool bool_val() const;
    Array<Value> struct_fields_val() const;
    Value struct_field_val(isize index) const;
    Value struct_field_val(xpString field_name) const;
    Array<Value> array_element_values() const;
    Value array_element_val(isize index) const;
    ValueRef struct_field_ref(isize index);    // 字段槽的地址（Pointer 用）
    ValueRef array_element_ref(isize index);   // 元素槽的地址（Pointer 用）

    // 元素槽的地址：值没握元素缓冲或索引越界给 nullptr（按元素扫描时当终止信号用）
    ValueRef element_ref(isize index);
    Pointer pointer_val() const;           // comptime 指针
    // 闲置：ValueMemory 体系的字节指针
    // MemPointer mem_pointer_val() const;
    TypeRef type_val() const;
    Ref<Package> package_val() const;
    FuncValue func_val() const;

    // $T 泛型模板的函数值: payload 是函数, 但 type 未定(签名要等调用点推导出类型变量)。
    // func_val() 断言 is_function_type(type), 这种值过不了, 故另开一对明确语义的访问器。
    bool is_unresolved_func_val() const;
    FuncValue unresolved_func_val() const;




    bool is_null = false;
private:
    ActualValueType actual_value_type = ActualValueType::Nothing;
    union {
        i128 integer_value;

        double float_value;

        bool bool_value;

        Array<Value> struct_or_array_fields;

        FuncValue func_value;

        // MemPointer mem_pointer_value;  // 闲置：ValueMemory 体系的字节指针
        Pointer pointer_value;         // comptime 指针

        TypeRef type_value;        // 类型值

        Ref<Package> package_value;    // 包值（全局包表编号）
    };

public:
    friend Value clone_value(const Value& v, xpAllocator allocator);

    // 提供任意类型的 "零值"
    static Value zero(TypeRef type);
    static Value zero(TypeRef type, xpAllocator allocator);
};


//
// Value Makers
//
Value make_value();
Value make_value(TypeRef type);

//
// Value Utils
//

// 深拷一个值：Struct/Array 连同数据一起拷；Pointer 只抄地址（指针语义）
Value clone_value(const Value& v, xpAllocator allocator);




// 闲置：Value ↔ 字节序列化（comptime 字节级内存模型）
//
// void write_value_to_bytes(Array<u8>& bytes, isize offset, const Value& v);
// Value read_value_from_bytes(const Array<u8>& bytes, isize offset, TypeRef type, xpAllocator allocator);
//
// 类型序列化布局函数
//
// isize type_serialize_size(TypeRef type);
// isize type_serialize_align(TypeRef type);
// isize type_serialize_stride(TypeRef type);
// isize field_serialize_offset(TypeRef struct_type, isize index);
// isize serialize_align_up(isize value, isize alignment);


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
