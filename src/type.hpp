#pragma once

#include <optional>

#include "xoaop.h"
#include "array.hpp"
#include "common.hpp"
#include "scope.hpp"

struct CIRResultInstance;   // 前向声明（union_info.creation_instance）

#define TYPE_KINDS                  \
    TYPE_KIND(Undefined)            \
    TYPE_KIND(i8)                   \
    TYPE_KIND(i16)                  \
    TYPE_KIND(i32)                  \
    TYPE_KIND(i64)                  \
    TYPE_KIND(isize)                \
    TYPE_KIND(u8)                   \
    TYPE_KIND(u16)                  \
    TYPE_KIND(u32)                  \
    TYPE_KIND(u64)                  \
    TYPE_KIND(usize)                \
    TYPE_KIND(f32)                  \
    TYPE_KIND(f64)                  \
    TYPE_KIND(bool)                 \
    TYPE_KIND(void)                 \
    TYPE_KIND(function)             \
    TYPE_KIND(pointer)              \
    TYPE_KIND(struct)               \
    TYPE_KIND(array)                \
    TYPE_KIND(enum)                 \
    TYPE_KIND(union)                \
    TYPE_KIND(untyped_int)          \
    TYPE_KIND(untyped_float)        \
                                    \
    TYPE_KIND(type)                 \
    TYPE_KIND(package)              \
                                    \
    TYPE_KIND(var_arg_c)            \
                                    \
                                    \
    TYPE_KIND(error)                \
/**/


enum TypeKind: int {

#define TYPE_KIND(name) Type_##name,
    TYPE_KINDS
#undef TYPE_KIND

};

TypeKind string_to_type_kind(xpString str);

template<>
struct std::formatter <TypeKind> : std::formatter<std::string_view> {
    auto format(TypeKind kind, std::format_context& ctx) const {
        #define TYPE_KIND(name) case Type_##name: return std::formatter<std::string_view>::format(#name, ctx);
        switch(kind) {
            TYPE_KINDS
            default:
                return std::formatter<std::string_view>::format("UnknownTypeKind", ctx);
        }
        #undef TYPE_KIND
    }
};



// 前向声明
struct Type;
typedef Type *TypeRef;


struct StructField;

struct Package;

struct Ast;


struct TypeHashKey {

    TypeHashKey(Ast *decl_ast) : decl_ast(decl_ast) {}
    TypeHashKey(Ast *decl_ast, xpString name) : decl_ast(decl_ast), name(name) {}

    u64 hash() const;
    bool operator==(const TypeHashKey& other) const;

    TypeHashKey clone(xpAllocator allocator) const;

    Ast *decl_ast; // 结构体/枚举/联合体的声明AST节点
    std::optional<xpString> name;
    u64 unique_id = 0; // 用于让 add_type 总能插入成功，0 表示由 get_or_add_type 去重
};

template<>
struct std::hash<TypeHashKey> {
    usize operator()(const TypeHashKey& key) const {
        return key.hash();
    }
};


bool is_equal_type(Type a, Type b);

struct Type {
    TypeKind kind;
    xpString type_name;

    union {
        
        // 函数
        struct {
            Array<TypeRef> param_types;
            TypeRef return_type;
        } function_info;
        
        // 指针
        TypeRef pointed_type;
        
        // 结构体
        struct {
            Array<StructField> struct_fields;
            TypeHashKey hash_key;
        } struct_info;

        
        
        // 数组
        struct {
            TypeRef element_type;
            usize count;
        } array_info;


        // 枚举
        struct {
            TypeRef element_type; // 枚举成员的类型
            RefN<Scope> enum_scope; // 枚举的作用域, 包含枚举成员的符号表信息
            TypeHashKey hash_key;
        } enum_info;


        // 联合体
        struct {
            RefN<Scope> union_scope;   // resolve 建的共享字段符号表（字段符号绑 InCIRInstruction）
            Ref<CIRResultInstance> creation_instance;   // 类型创建时的调用实例（泛型逐实例解析字段类型；INVALID = 包级）
            TypeHashKey hash_key;
        } union_info;


        // 元类型
        TypeRef self_type_info;

        // package类型
        RefN<Package> package_info;
    };

    // *重要: 用于hash set比较Type, 不然会出问题
    bool operator== (const Type &other) const {
        return is_equal_type(*this, other);
    }

    xpString name();   // 符号名, 可能是结构体的名字, 包的名字, 基本类型的名字等
    xpString t_name(); // 纯类型名, 可能是type(struct)
};


struct StructField {
    xpString name;
    TypeRef type;
};




enum class ImplicitConversionTag {
    None,
    ArrayToSliceStruct,
};






Type make_type(TypeKind kind);
Type *alloc_type(xpAllocator allocator, TypeKind kind);

Type copy_type(Type *src);


TypeRef get_pointed_type(TypeRef pointer_type);
TypeRef get_innermost_type_of_pointer(TypeRef pointer_type);



// bool is_equal_type(Type a, Type b);
bool is_integer_type(TypeRef type);
bool is_integer_or_untyped_type(TypeRef type);
bool is_integer_or_bool_type(TypeRef type);
bool is_signed_type(TypeRef type);
bool is_signed_or_bool_type(TypeRef type);
bool is_unsigned_type(TypeRef type);
bool is_float_type(TypeRef type);
bool is_float_or_untyped_type(TypeRef type);
bool is_certain_type(TypeRef type);
bool is_number_type(TypeRef type);
bool is_untyped_type(TypeRef type);
bool is_function_type(TypeRef type);
bool is_pointer_type(TypeRef type);
bool is_struct_type(TypeRef type);
bool is_union_type(TypeRef type);
bool is_enum_type(TypeRef type);
bool is_array_type(TypeRef type);
bool is_type_type(TypeRef type);
bool is_package_type(TypeRef type);
bool is_slice_struct_type(TypeRef type);
// bool is_string_struct_type(TypeRef type);
bool is_value_type(TypeRef type);
bool is_var_arg_function(TypeRef type);
bool is_named_type(TypeRef type);

bool is_basic_type_kind(TypeKind kind);
bool is_complex_type_kind(TypeKind kind);
bool is_easy_type_kind(TypeKind kind);
bool is_hard_type_kind(TypeKind kind);




TypeRef get_common_type(TypeRef a, TypeRef b);
u32 get_fixed_param_count(TypeRef func_type);


bool check_literal_overflow(TypeKind type_kind, i128 result, double dresult);
bool check_integer_overflow(i128 val, TypeRef type);
bool check_float_overflow(double val, TypeRef type);



xpString get_type_name(TypeRef type, bool is_pure_type_name = false);
xpString get_type_kind_str(TypeKind kind);
xpString get_or_make_type_str(TypeRef type, xpAllocator allocator, bool is_pure_type_name = false);


const char *to_string(TypeKind kind);

void print_type(TypeRef type);


// Type 的 formatter：基本类型经 formatter<TypeKind>（TYPE_KINDS 宏）自动覆盖，
// 新增类型无需改这里；复杂类型单独格式化。
template<>
struct std::formatter<Type> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const Type& type, std::format_context& ctx) const {
        auto out = ctx.out();
        switch(type.kind) {
        case Type_function: {
            out = std::format_to(out, "func(");
            for(isize i = 0; i < type.function_info.param_types.count; i++) {
                out = std::format_to(out, "{}", *type.function_info.param_types[i]);
                if(i != type.function_info.param_types.count - 1) {
                    out = std::format_to(out, ", ");
                }
            }
            out = std::format_to(out, ") -> ");
            return std::format_to(out, "{}", *type.function_info.return_type);
        }
        case Type_pointer: {
            const Type *curr = &type;
            for(;;) {
                out = std::format_to(out, "*");
                if(curr->pointed_type->kind == Type_pointer) {
                    curr = curr->pointed_type;
                } else {
                    return std::format_to(out, "{}", *curr->pointed_type);
                }
            }
        } break;
        case Type_struct:
            return std::format_to(out, "struct {}", type.type_name);
        case Type_untyped_int:
            return std::format_to(out, "literal");
        case Type_Undefined:
            return std::format_to(out, "undefined");
        default:
            // 基本类型等：直接用 kind 名
            return std::format_to(out, "{}", type.kind);
        }
    }
};

// TypeRef（Type*）：转发到 Type 的 formatter
template<>
struct std::formatter<TypeRef> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const TypeRef& type, std::format_context& ctx) const {
        auto out = ctx.out();
        if(type == nullptr) {
            return std::format_to(out, "null_type");
        }
        return std::format_to(out, "{}", *type);
    }
};







static constexpr size_t TYPE_TABLE_CAPACITY = 16384;

struct TypeTable {
    xpInterningTable<Type, TYPE_TABLE_CAPACITY> type_interning_table;
};

// TODO 移除
struct Context;
void init_type_table(Context *ctx);
void free_type_table(Context *ctx);



TypeRef easy_type(TypeKind kind);
TypeRef pointer_type(TypeRef pointed_type);
TypeRef function_type(Array<TypeRef> param_types, TypeRef return_type);
TypeRef array_type(TypeRef element_type, usize count);
TypeRef anonymous_struct_type(Ast *decl, Array<StructField> fields);
TypeRef struct_type(Ast *decl, xpString name, Array<StructField> fields);
TypeRef type_type();
TypeRef package_type(RefN<Package> package_info);
TypeRef undefined_type();
TypeRef error_type();

TypeRef add_type_unique(Type type);

TypeRef unfinished_anonymous_struct_type(Ast *decl);
TypeRef unfinished_struct_type(Ast *decl, xpString ident);
void finish_unfinish_struct_type(TypeRef unfinish, Array<StructField> fields);

TypeRef enum_type_impl(Ast *decl_ast, std::optional<xpString> ident, TypeRef elem_type, RefN<Scope> scope);

TypeRef union_type_impl(Ast *decl_ast, std::optional<xpString> ident, RefN<Scope> scope);
TypeRef union_field_type(TypeRef union_type, xpString field_name);   // 按创建实例解析字段类型
bool type_contains_by_value(TypeRef outer, TypeRef inner);   // 判断 outer 是否按值包含 inner（指针处截断）

isize type_size_of(TypeRef type);
isize type_align_of(TypeRef type);
isize field_offset_in_struct(TypeRef struct_type, isize index);
TypeRef slice_type_as_struct(TypeRef elem_type);

TypeRef get_or_add_type(Type type);
TypeRef get_type(Type type);
TypeRef add_type(Type type);

xpAllocator type_allocator();