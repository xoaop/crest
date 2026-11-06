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




// comptime 指针：指向某个容器持有的 Value 槽
struct Pointer {
    ValueRef slot = nullptr;

    static Pointer make_slot(ValueRef slot);
    static Pointer make_null();
    bool is_null() const;


    // 取指针指向的值。pointee_type 是指针声明指向的类型：
    // 声明指向单个 T 而槽实际握着 [N]T（连续元素）时，取的是首元素
    Value load(TypeRef pointee_type) const;
    void store(Value v) const;
    void store(Value v, xpAllocator allocator) const;   // 先深拷再落，槽自己持有数据
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
    Pointer,   // comptime 指针：Pointer{Value* slot}，type 字段指向 *T
    Type,      // 类型值：TypeRef 存储在 union 中
    Package,   // 包值：Package* 存储在 union 中
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

    bool equal(const Value& other) const;

    void integer_val(i128 int_val);
    void float_val(double float_val);
    void bool_val(bool bool_val);
    void struct_fields_val(Array<Value> field_values);
    void array_element_values(Array<Value> elem_values);
    void func_val(Ref<CIRInstResult> func_key);
    void func_val_key(Ref<CIRInstResult> key);
    void pointer_val(Pointer ptr);          // comptime 指针
    void type_val(TypeRef type_ref);
    void package_val(Ref<Package> pkg);


    i128 integer_val() const;
    double float_val() const;
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


template<>
struct std::hash<Value> {
    usize operator()(const Value& v) const {
        const auto kind = v.actual_type();
        u64 h = static_cast<u64>(kind);
        switch(kind) {
        case ActualValueType::Nothing:
            break;
        case ActualValueType::Integer: {
            const auto n = v.integer_val();
            h = xp_hash_combine_u64(h, static_cast<u64>(n));
            h = xp_hash_combine_u64(h, static_cast<u64>(n >> 64));
        } break;
        case ActualValueType::Float:
            h = xp_hash_combine_u64(h, std::hash<double>{}(v.float_val()));
            break;
        case ActualValueType::Bool:
            h = xp_hash_combine_u64(h, std::hash<bool>{}(v.bool_val()));
            break;
        case ActualValueType::Type:
            h = xp_hash_combine_u64(h, std::hash<TypeRef>{}(v.type_val()));
            break;
        case ActualValueType::Pointer:
            h = xp_hash_combine_u64(h, std::hash<ValueRef>{}(v.pointer_val().slot));
            break;
        case ActualValueType::Function: {
            const auto fv = v.is_unresolved_func_val() ? v.unresolved_func_val() : v.func_val();
            h = xp_hash_combine_u64(h, std::hash<Ref<CIRInstResult>>{}(fv.func_key));
        } break;
        case ActualValueType::Package:
            h = xp_hash_combine_u64(h, std::hash<Ref<Package>>{}(v.package_val()));
            break;
        case ActualValueType::Struct: {
            const auto fields = v.struct_fields_val();
            h = xp_hash_combine_u64(h, static_cast<u64>(fields.count));
            FOR(fields) {
                h = xp_hash_combine_u64(h, std::hash<Value>{}(it));
            }
        } break;
        case ActualValueType::Array: {
            const auto elems = v.array_element_values();
            h = xp_hash_combine_u64(h, static_cast<u64>(elems.count));
            FOR(elems) {
                h = xp_hash_combine_u64(h, std::hash<Value>{}(it));
            }
        } break;
        }
        return static_cast<usize>(h);
    }
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

// 切片结构体 {data, count} 的 data 字段（值层布局：字段 0）
Pointer slice_data_val(const Value& slice_value);






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
