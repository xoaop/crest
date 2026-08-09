#include <cfloat>
#include <atomic>

#include "print.hpp"

#include "type.hpp"
#include "value.hpp"

#include "symbol.hpp"


#include "context.hpp"


#include "package.hpp"


#include "scope.hpp"





static const char *type_strings[] = {

#define TYPE_KIND(name) #name,
    TYPE_KINDS
#undef TYPE_KIND

};


u64 TypeHashKey::hash() const {

    auto hash = reinterpret_cast<u64>(decl_ast);

    if(name.has_value()) {
        hash = xp_hash_combine_u64(hash, std::hash<xpString>{}(name.value()));
    }

    hash = xp_hash_combine_u64(hash, unique_id);

    return hash;
}


bool TypeHashKey::operator==(const TypeHashKey& other) const {
    if(unique_id != other.unique_id) {
        return false;
    }
    bool same_decl = decl_ast == other.decl_ast;
    bool same_name_or_no_name = true;

    if(name.has_value() && other.name.has_value()) {
        xpString n1 = name.value();
        xpString n2 = other.name.value();

        same_name_or_no_name = xp_string_equal(n1, n2);
    }

    return same_decl && same_name_or_no_name;
}


TypeHashKey TypeHashKey::clone(xpAllocator allocator) const {
    TypeHashKey copy = *this;
    if(name.has_value()) {
        copy.name = xp_string_copy(allocator, name.value());
    }
    return copy;
}



TypeKind string_to_type_kind(xpString str) {
    if(sizeof(type_strings) <= 0) {
        return Type_Undefined;
    }

    for(size_t i = 0; i < sizeof(type_strings) / sizeof(type_strings[0]); i++) {
        if(xp_string_equal(str, xp_string_c(type_strings[i]))) {
            return static_cast<TypeKind>(i);
        }
    }

    return Type_Undefined;
}


Type make_type(TypeKind kind) {
    Type t{};  // 零初始化
    t.kind = kind;
    t.type_name = xp_make_string_zero();

    return t;
}

Type *alloc_type(xpAllocator allocator, TypeKind kind) {
    Type *t = cast(Type *) xp_alloc(allocator, sizeof(Type));
    *t = make_type(kind);
    return t;
}




Type copy_type(Type *src) {
    Type t{};
    t.kind = src->kind;
    t.kind = src->kind;
    if(src->type_name != xp_make_string_zero()) {
        t.type_name = xp_string_copy(type_allocator(), src->type_name);
    }


    switch(src->kind) {
    case Type_function: {
        t.function_info.param_types = src->function_info.param_types.copy(type_allocator());
        t.function_info.return_type = src->function_info.return_type;
    } break;
    case Type_pointer: {
        t.pointed_type = src->pointed_type;
    } break;
    case Type_struct: {
        t.struct_info.struct_fields = src->struct_info.struct_fields.copy(type_allocator());
        t.struct_info.hash_key = src->struct_info.hash_key.clone(type_allocator());
    } break;

    case Type_enum: {
        t.enum_info = src->enum_info;
        t.enum_info.enum_scope = src->enum_info.enum_scope;
        t.enum_info.hash_key = src->enum_info.hash_key.clone(type_allocator());
    } break;

    case Type_array: {
        t.array_info.element_type = src->array_info.element_type;
        t.array_info.count = src->array_info.count;
    } break;
    case Type_type: {
        t.self_type_info = src->self_type_info;
    } break;
    case Type_package: {
        t.package_info = src->package_info;
    } break;

    // TODO: 更多复杂类型

    default:
        break;
    }

    return t;
}


xpString Type::name() {
    return get_type_name(this);
}

xpString Type::t_name() {
    return get_type_name(this, true);
}



//
// 
//







TypeRef get_pointed_type(TypeRef pointer_type) {
    XP_ASSERT_DEFAULT(pointer_type->kind == Type_pointer);
    XP_ASSERT_DEFAULT(pointer_type->pointed_type != NULL);

    return pointer_type->pointed_type;
}


TypeRef get_innermost_type_of_pointer(TypeRef pointer_type) {
    XP_ASSERT_DEFAULT(pointer_type->kind == Type_pointer);

    for(;;) {
        if(!is_pointer_type(pointer_type)) {
            return pointer_type;
        }

        pointer_type = (pointer_type->pointed_type);
    }
}




// Type make_struct_type(xpString name, Array<StructField> fields) {
//     Type struct_type = {};
//     struct_type.kind = Type_struct;
//     struct_type.type_name = name;
//     struct_type.struct_fields = fields;
//     return struct_type;
// }





// 这个函数很重要, 用于比较类型是否相等, 包括复杂类型, 需要递归比较
bool is_equal_type(Type a, Type b) {
    if(a.kind != b.kind) {
        return false;
    }

    switch(a.kind) {
    case Type_function:
        if(a.function_info.param_types.count != b.function_info.param_types.count) {
            return false;
        }
        for(isize i = 0; i < a.function_info.param_types.count; i++) {
            if(!is_equal_type(*(a.function_info.param_types[i]), *(b.function_info.param_types[i]))) {
                return false;
            }
        }
        return is_equal_type(*a.function_info.return_type, *b.function_info.return_type);
    
    case Type_pointer:
        if((a.pointed_type == NULL && b.pointed_type != NULL) || (a.pointed_type != NULL && b.pointed_type == NULL)) {
            return false;
        }

        return is_equal_type(*a.pointed_type, *b.pointed_type);
    
    case Type_struct: 
        return a.struct_info.hash_key == b.struct_info.hash_key;

    // 1. type_name
    // 2. decl_ast
    case Type_enum:
        return a.enum_info.hash_key == b.enum_info.hash_key;
    
    // TODO 更多复杂类型比较
    default:
        return true;
    }
}


//
// IS-TYPE functions
//



bool is_basic_type_kind(TypeKind kind) {
    switch(kind) {
    case Type_i8:
    case Type_i32:
    case Type_i64:
    case Type_u8:
    case Type_u32:
    case Type_u64:
    case Type_isize:
    case Type_usize:
    case Type_f32:
    case Type_f64:
    case Type_bool:
    case Type_void:
        return true;
    default:
        return false;
    }
}

bool is_complex_type_kind(TypeKind kind) {
    switch(kind) {
    case Type_function:
    case Type_pointer:
    case Type_struct:
    case Type_array:
        return true;
    default:
        return false;
    }
}

// 包括基本类型和字面量类型
bool is_easy_type_kind(TypeKind kind) {
    return is_basic_type_kind(kind) || kind == Type_untyped_int || kind == Type_untyped_float || kind == Type_var_arg_c;
}

// 包括复杂类型和不确定类型
bool is_hard_type_kind(TypeKind kind) {
    return is_complex_type_kind(kind);
}



bool is_integer_type(TypeRef type) {
    switch(type->kind)
    {
    case Type_i8:
    case Type_i32:
    case Type_i64:
    case Type_u8:
    case Type_u32:
    case Type_u64:
    case Type_isize:
    case Type_usize:
        return true;
    default:
        return false;
    }
}

bool is_integer_or_untyped_type(TypeRef type) {
    return is_integer_type(type) || type->kind == Type_untyped_int;
}


bool is_integer_or_bool_type(TypeRef type) {
    return is_integer_type(type) || type->kind == Type_bool;
}

bool is_signed_type(TypeRef type) {
    XP_ASSERT_DEFAULT(is_integer_type(type));
    switch(type->kind) {
    case Type_i8:
    case Type_i32:
    case Type_i64:
    case Type_isize:
        return true;
    default:
        return false;
    }
}

bool is_signed_or_bool_type(TypeRef type) {
    XP_ASSERT_DEFAULT(is_integer_or_bool_type(type));
    switch(type->kind) {
    case Type_i8:
    case Type_i32:
    case Type_i64:
    case Type_isize:
    case Type_bool:
        return true;
    default:
        return false;    
    }
}

bool is_unsigned_type(TypeRef type) {
    XP_ASSERT_DEFAULT(is_integer_type(type));
    switch(type->kind) {
    case Type_u8:
    case Type_u32:
    case Type_u64:
    case Type_usize:

        return true;
    default:
        return false;
    }
}

bool is_float_type(TypeRef type) {
    return type->kind == Type_f32 || type->kind == Type_f64;
}

bool is_float_or_untyped_type(TypeRef type) {
    return is_float_type(type) || type->kind == Type_untyped_float;
}

bool is_certain_type(TypeRef type) {
    switch(type->kind) {
        case Type_void:
        case Type_i8: 
        case Type_i32:
        case Type_i64:
        case Type_isize:
        case Type_u8:
        case Type_u32:
        case Type_u64:
        case Type_usize:
        case Type_f32:
        case Type_f64:
        case Type_bool:
        case Type_pointer:
        case Type_struct: 
        case Type_array:
            return true;
        default:
            return false;
    }
}

bool is_number_type(TypeRef type) {
    return is_integer_type(type) || is_float_type(type);
}

bool is_untyped_type(TypeRef type) {
    return type->kind == Type_untyped_int || type->kind == Type_untyped_float;
}

bool is_function_type(TypeRef type) {
    return type->kind == Type_function;
}

bool is_pointer_type(TypeRef type) {
    return type->kind == Type_pointer;
}

bool is_struct_type(TypeRef type) {
    return type->kind == Type_struct;
}

bool is_enum_type(TypeRef type) {
    return type->kind == Type_enum;
}

bool is_array_type(TypeRef type) {
    return type->kind == Type_array;
}

bool is_type_type(TypeRef type) {
    return type->kind == Type_type;
}

bool is_package_type(TypeRef type) {
    return type->kind == Type_package;
}

bool is_slice_struct_type(TypeRef type) {
    if(!is_struct_type(type)) {
        return false;
    }

    if(type->type_name.length <= 0) {
        return false;
    }
    
    if(type->type_name[0] != '[') {
        return false;
    }

    return true;
}

// @deprecated
// bool is_string_struct_type(TypeRef type) {
//     if(!is_struct_type(type)) return false;
//
//     auto& fields = type->struct_info.struct_fields;
//     if(fields.count != 2) return false;
//
//     TypeRef field0_type = fields[0].type;
//     if(!is_pointer_type(field0_type)) return false;
//
//     if(field0_type->pointed_type->kind != Type_u8) return false;
//
//     return true;
// }


bool is_value_type(TypeRef type) {
    return is_integer_or_untyped_type(type) || 
           is_float_or_untyped_type(type) || 
           type->kind == Type_bool || 
           is_pointer_type(type) ||
           is_struct_type(type) ||
           is_array_type(type) || 
           is_enum_type(type);
}

bool is_var_arg_function(TypeRef type) {
    if(!is_function_type(type)) {
        return false;
    }

    if(type->function_info.param_types.count == 0) {
        return false;
    }

    TypeRef last_param_type = type->function_info.param_types[type->function_info.param_types.count - 1];
    return last_param_type->kind == Type_var_arg_c;
}

u32 get_fixed_param_count(TypeRef func_type) {
    if(is_var_arg_function(func_type)) {
        return func_type->function_info.param_types.count - 1;
    } else {
        return func_type->function_info.param_types.count;
    }
}


bool is_named_type(TypeRef type) {
    return is_struct_type(type) || 
           is_enum_type(type);
}

//
//
//


int get_type_rank(TypeRef t) {
    switch (t->kind) {
        case Type_i8: case Type_u8:  return 8;
        case Type_i32: case Type_u32: return 32;
        case Type_i64: case Type_u64: return 64;
        case Type_f32: return 32;
        case Type_f64: return 64;

        case Type_isize:
        case Type_usize: return (int)(sizeof(void*) * 8);
        default: 
            std::unreachable();
            return 0;
    }
}


TypeRef get_common_type(TypeRef a, TypeRef b) {
    if(a == b) {
        return a;
    }

    if(is_float_type(a) && is_float_type(b)) {
        return (get_type_rank(a) >= get_type_rank(b)) ? a : b;
    }


    if(is_integer_type(a) && is_integer_type(b)) {
        if (is_signed_type(a) != is_signed_type(b)) {
            return error_type();
        }

        return (get_type_rank(a) >= get_type_rank(b)) ? a : b;
    }

    
    return error_type();
}


bool check_literal_overflow(TypeKind type_kind, i128 result, double dresult) {
    bool overflowed = false;
    switch(type_kind) {
        case Type_i8: 
            overflowed = (result < INT8_MIN || result > INT8_MAX);
            break;
        case Type_i32: 
            overflowed = (result < INT32_MIN || result > INT32_MAX);
            break;
        case Type_i64:
            overflowed = (result < INT64_MIN || result > INT64_MAX);
            break;
        case Type_u8:
            overflowed = (result < 0 || result > UINT8_MAX);
            break;
        case Type_u32:
            overflowed = (result < 0 || result > UINT32_MAX);
            break;
        case Type_u64:
            overflowed = (result < 0 || result > UINT64_MAX);
            break;
        case Type_isize:
            overflowed = (result < INTPTR_MIN || result > INTPTR_MAX);
            break;
        case Type_usize:
            overflowed = (result < 0 || result > (i128)UINTPTR_MAX);
            break;
        case Type_f32:
            overflowed = (dresult < -FLT_MAX || dresult > FLT_MAX);
            break;
        case Type_f64:
            overflowed = (dresult < -DBL_MAX || dresult > DBL_MAX);
            break;
        
        // 注意类型检查
        case Type_bool: 
            overflowed = (result != 0 && result != 1);
            break;
        default:
            XP_ASSERT_DEFAULT(0);
            break;
    }

    return overflowed;
}

bool check_integer_overflow(i128 val, TypeRef type) {
    XP_ASSERT_DEFAULT(is_integer_type(type));

    switch(type->kind) {
        case Type_i8:
            return (val < INT8_MIN || val > INT8_MAX);
        case Type_i32:
            return (val < INT32_MIN || val > INT32_MAX);
        case Type_i64:
            return (val < INT64_MIN || val > INT64_MAX);
        case Type_u8:
            return (val < 0 || val > UINT8_MAX);
        case Type_u32:
            return (val < 0 || val > UINT32_MAX);
        case Type_u64:
            return (val < 0 || val > UINT64_MAX);
        case Type_isize:
            return (val < INTPTR_MIN || val > INTPTR_MAX);
        case Type_usize:
            return (val < 0 || val > (i128)UINTPTR_MAX);
        default:
            std::unreachable();
            return false;
    }
}

bool check_float_overflow(double val, TypeRef type) {
    XP_ASSERT_DEFAULT(is_float_type(type));

    switch(type->kind) {
        case Type_f32:
            return (val < -FLT_MAX || val > FLT_MAX);
        case Type_f64:
            return (val < -DBL_MAX || val > DBL_MAX);
        default:
            std::unreachable();
            return false;
    }
}



#if defined(CREST_DEBUG)
void print_type(TypeRef type) {
    // 转发到 formatter<TypeRef>，打印语义与旧 printf 版一致
    print_out("{}", type);
}
#endif // CREST_DEBUG



//
//
//



static TypeTable global_type_table;


void init_type_table(Context *ctx) {
    xp_interning_table_init(&global_type_table.type_interning_table);
}

void free_type_table(Context *ctx) {
    xp_interning_table_free(&global_type_table.type_interning_table);
}


TypeRef easy_type(TypeKind kind) {
    XP_ASSERT_DEFAULT(is_easy_type_kind(kind));
    Type t = make_type(kind);
    return get_or_add_type(t);
}


TypeRef pointer_type(TypeRef pointed_type) {
    Type t = make_type(Type_pointer);
    t.pointed_type = pointed_type;
    return get_or_add_type(t);  
}





TypeRef function_type(Array<TypeRef> param_types, TypeRef return_type) {
    Type t = {};
    t.kind = Type_function;
    t.function_info.param_types = param_types;
    t.function_info.return_type = return_type;

    return get_or_add_type(t);
}

TypeRef array_type(TypeRef element_type, usize count) {
    Type t = {};
    t.kind = Type_array;
    t.array_info.element_type = element_type;
    t.array_info.count = count;

    return get_or_add_type(t);
}

TypeRef anonymous_struct_type(Ast *decl, Array<StructField> fields) {
    Type t = {};
    t.kind = Type_struct;

    t.struct_info.struct_fields = fields;
    t.struct_info.hash_key = TypeHashKey{ decl };

    TypeRef type_ref = get_or_add_type(t);

    return type_ref;
}

TypeRef struct_type(Ast *decl, xpString name, Array<StructField> fields) {
    Type t = {};
    t.kind = Type_struct;
    t.type_name = name;

    t.struct_info.struct_fields = fields;
    t.struct_info.hash_key = TypeHashKey{ decl, name };

    TypeRef type_ref = get_or_add_type(t);

    return type_ref;
}

TypeRef unfinished_anonymous_struct_type(Ast *decl) {
    Type t = {};
    t.kind = Type_struct;
    t.struct_info.hash_key = TypeHashKey{ decl };

    return add_type_unique(t);
}

TypeRef unfinished_struct_type(Ast *decl, xpString ident) {
    Type t = {};
    t.kind = Type_struct;
    t.struct_info.hash_key = TypeHashKey{ decl, ident };

    return add_type_unique(t);
}

void finish_unfinish_struct_type(TypeRef unfinish, Array<StructField> fields) {
    XP_ASSERT_DEFAULT(is_struct_type(unfinish));
    XP_ASSERT_DEFAULT(unfinish->struct_info.struct_fields.count == 0);

    unfinish->struct_info.struct_fields = fields;
}


TypeRef enum_type_impl(Ast *decl_ast, std::optional<xpString> ident, TypeRef elem_type, Scope *scope) {
    XP_ASSERT_DEFAULT(is_integer_type(elem_type));

    Type t{Type_enum};

    xpAllocator alloc = type_allocator();
    t.enum_info.hash_key = ident.has_value()
        ? TypeHashKey{ decl_ast, ident.value() }
        : TypeHashKey{ decl_ast };
    t.enum_info.element_type = elem_type;
    t.enum_info.enum_scope = scope;

    return add_type_unique(t);
}


TypeRef type_type(TypeRef self_type_info) {
    Type t = {};
    t.kind = Type_type;
    t.self_type_info = self_type_info;

    return get_or_add_type(t);
}

TypeRef type_type() {
    return type_type(undefined_type());
}

TypeRef package_type(Package *package_info) {
    Type t = {};
    t.kind = Type_package;
    t.package_info = package_info;

    return get_or_add_type(t);
}



TypeRef undefined_type() {
    Type t = {};
    t.kind = Type_Undefined;

    return get_or_add_type(t);
}




TypeRef error_type() {
    Type t = {};
    t.kind = Type_error;

    return get_or_add_type(t);
}


static std::atomic<u64> g_type_unique_id{1};

// TODO: 吧这个unqueid实现到任意类型, 而不是
TypeRef add_type_unique(Type type) {
    switch(type.kind) {
    case Type_struct:
        type.struct_info.hash_key.unique_id = g_type_unique_id.fetch_add(1, std::memory_order_relaxed);
        break;
    case Type_enum:
        type.enum_info.hash_key.unique_id = g_type_unique_id.fetch_add(1, std::memory_order_relaxed);
        break;
    default:
        break;
    }
    Type copy = copy_type(&type);
    TypeRef type_ref = xp_interning_table_insert(&global_type_table.type_interning_table, copy);
    ASSERT_MSG(type_ref != nullptr, "add_type_unique: insert failed");
    return type_ref;
}


TypeRef get_or_add_type(Type type) {
    TypeRef type_ref = get_type(type);
    if(type_ref != NULL) {
        return type_ref;
    }

    return add_type(type);
}

TypeRef get_type(Type type) {
    TypeRef type_ref = xp_interning_table_get(&global_type_table.type_interning_table, type);
    return type_ref;
}


TypeRef add_type(Type type) {
    Type copy = copy_type(&type);
    TypeRef type_ref = xp_interning_table_insert(&global_type_table.type_interning_table, copy);

    ASSERT_MSG(type_ref != nullptr, "Failed to add type to interning table");

    return type_ref;
}




// NOTE: 实际上就是结构体
TypeRef slice_type_as_struct(TypeRef elem_type) {
    
    xpAutoArenaRestore temp_arena_restore{ temp_allocator() };

    

    xpString slice_struct_name = xp_make_string(temp_allocator(), "[]");
    xpString elem_str = get_or_make_type_str(elem_type, temp_allocator());

    xp_string_append(&slice_struct_name, elem_str);


    Type t = {};
    t.kind = Type_struct;
    t.type_name = slice_struct_name;


    Array<StructField> fields = make_array<StructField>(temp_allocator());



    fields.push_back(StructField { 
        xp_make_string(type_allocator(), "data"), 
        pointer_type(elem_type) 
    });
    fields.push_back(StructField { 
        xp_make_string(type_allocator(), "count"), 
        easy_type(Type_isize) 
    });


    /*
     * 假如不给结构体加上decl, 那要是不同作用域有同名struct的slice
     * 那它们的slice类型就会被认为是同一个类型, 这显然不对, 因此必须加上decl来区分
    */
    Ast *decl = is_struct_type(elem_type) ? elem_type->struct_info.hash_key.decl_ast : nullptr;
    return struct_type(decl, slice_struct_name, fields);
}





xpString get_type_kind_str(TypeKind kind) {
    return xp_string_c(type_strings[kind]);
}

xpString get_type_name(TypeRef type, bool is_pure_type_name) {

    // 如果已经有名字就直接返回
    if(type->type_name.capacity != 0) {
        return type->type_name;
    }

    xpAutoArenaRestore temp_arena_restore{
        temp_allocator(),
        xpAutoArenaRestore::Mode::RestoreOnly
    };

    type->type_name = get_or_make_type_str(type, permanent_allocator(), is_pure_type_name);

    return type->type_name;
}


xpString get_or_make_type_str(TypeRef type, xpAllocator allocator, bool is_pure_type_name) {

    if(is_easy_type_kind(type->kind)) {

        return get_type_kind_str(type->kind);
    } else if(is_pointer_type(type)) {
        xpString base_str = xp_make_string(allocator, "*");

        // 因为可能有多级指针，所以递归获取, 而且用temp_allocator避免内存泄漏
        xpString pointed_type_str = get_or_make_type_str(type->pointed_type, temp_allocator(), is_pure_type_name);

        xp_string_append(&base_str, pointed_type_str);

        return base_str;

    } else if(is_struct_type(type) || type->kind == Type_enum) {

        // 如果是纯类型名, 就直接返回类型的kind字符串, 否则返回类型名
        if(is_pure_type_name) {
            return get_type_kind_str(type->kind);
        }
        
        // 直接用类型名
        return type->type_name;

    } else if(is_array_type(type)) {
        xpString base_str = xp_make_string(allocator, "[");
            
        xpString count_str = xp_isize_to_string(type->array_info.count, temp_allocator());
        xp_string_append(&base_str, count_str);

        xp_string_append(&base_str, xp_string_c("]"));

        // 因为可能是复杂类型，所以递归获取, 而且用temp_allocator避免内存泄漏
        xpString elem_type_str = get_or_make_type_str(type->array_info.element_type, temp_allocator(), is_pure_type_name);

        xp_string_append(&base_str, elem_type_str);

        return base_str;

    } else if(is_function_type(type)) {
        xpString func_str = xp_make_string(allocator, "(");

        for(isize i = 0; i < type->function_info.param_types.count; i++) {

            xpString param_type_str = get_or_make_type_str(type->function_info.param_types[i], temp_allocator(), is_pure_type_name);
            xp_string_append(&func_str, param_type_str);

            if(i != type->function_info.param_types.count - 1) {
                xpString comma_str = xp_string_c(", ");
                xp_string_append(&func_str, comma_str);
            }

            
        }
        
        xpString rb_arrow_str = xp_string_c(") -> ");
        xp_string_append(&func_str, rb_arrow_str);

        xpString return_type_str = get_or_make_type_str(type->function_info.return_type, temp_allocator(), is_pure_type_name);
        xp_string_append(&func_str, return_type_str);
        return func_str;
    } else if(is_type_type(type)) {

        xpString type_str = xp_make_string(allocator, "type(");
        xpString self_type_str = get_or_make_type_str(type->self_type_info, temp_allocator(), is_pure_type_name);
        xp_string_append(&type_str, self_type_str);
        xp_string_append(&type_str, xp_string_c(")"));
        return type_str;

    } else if(is_package_type(type)) {
        return type->package_info->path;
    } else if(type->kind == Type_error) {
        return xp_string_c("error");
    } else if(type->kind == Type_Undefined) {
        return xp_string_c("undefined");
    }
    
    // Unreachable
    DEBUG_PANIC("unhandled type kind in get_or_make_type_str: {}", type->kind);

    return {};
}




const char *to_string(TypeKind kind) {
    #define TYPE_KIND(name) case Type_##name: return #name;
    switch(kind) {
        TYPE_KINDS
        default:
            return "UnknownTypeKind";
    }
}











template<>
struct std::hash<Type> {
    usize operator()(const Type& type) const {
        switch(type.kind) {

        // 基本类型
        // 由于没有额外类型信息, 直接用kind作为hash值
        case Type_Undefined:
        case Type_error:

        case Type_i8:
        case Type_i32:
        case Type_i64:
        case Type_u8:
        case Type_u32:
        case Type_u64:
        case Type_isize:
        case Type_usize:
        case Type_f32:
        case Type_f64:
        case Type_bool:
        case Type_void:
        case Type_untyped_int:
        case Type_untyped_float:
        case Type_var_arg_c: {
            return cast(usize)(type.kind);
        } break;

        case Type_function: {
            u64 hash_value = Type_function;
            for(isize i = 0; i < type.function_info.param_types.count; i++) {
                u64 param_type_hash = cast(u64)(std::hash<Type>{}(*type.function_info.param_types[i]));
                hash_value = xp_hash_combine_u64(hash_value, param_type_hash);
            }
            u64 return_type_hash = cast(u64)(std::hash<Type>{}(*type.function_info.return_type));
            hash_value = xp_hash_combine_u64(hash_value, return_type_hash);

            return cast(usize)(hash_value);
        } break;

        case Type_pointer: {
            u64 hash_pointer = Type_pointer;
            u64 hash_pointed_type = cast(u64)(std::hash<Type>{}(*type.pointed_type));
            return cast(usize)(xp_hash_combine_u64(hash_pointer, hash_pointed_type));
        } break;


        case Type_struct: {
            u64 hash_struct = Type_struct;

            // decl_ast要参与hash, 为了区分不同作用域的同名结构体
            u64 hash_decl_ast = type.struct_info.hash_key.hash();
            u64 hash_value = xp_hash_combine_u64(hash_struct, hash_decl_ast);

            return cast(usize)(hash_value);
        } break;

        case Type_array: {
            u64 hash_array = Type_array;
            u64 hash_element_type = cast(u64)(std::hash<Type>{}(*type.array_info.element_type));
            u64 hash_count = cast(u64)(type.array_info.count);
            u64 hash_value = xp_hash_combine_u64(hash_array, hash_element_type);
            hash_value = xp_hash_combine_u64(hash_value, hash_count);
            return cast(usize)(hash_value);

        } break;

        case Type_type: {
            u64 hash_type = Type_type;
            u64 hash_self_type_info = cast(u64)(std::hash<Type>{}(*type.self_type_info));
            u64 hash_value = xp_hash_combine_u64(hash_type, hash_self_type_info);
            return cast(usize)(hash_value);
        } break;


        case Type_package: {
            u64 hash_package = Type_package;
            u64 hash_package_info = cast(u64)(std::hash<xpString>{}(type.package_info->path));
            u64 hash_value = xp_hash_combine_u64(hash_package, hash_package_info);
            return cast(usize)(hash_value);
        } break;

        case Type_enum: {
            u64 hash_enum = Type_enum;

            // decl_ast要参与hash, 为了区分不同作用域的同名枚举
            u64 hash_decl_ast = type.enum_info.hash_key.hash();

            u64 hash_value = xp_hash_combine_u64(hash_enum, hash_decl_ast);

            return cast(usize)(hash_value);
        } break;


        default: {
            XP_ASSERT_MSG(0, "hashing unhandled type kind");
        } break;

    }

    return 0;
    }
};





xpAllocator type_allocator() {
    return permanent_allocator();
}

// ============================================================
// 类型系统布局函数（独立于序列化）
// ============================================================

static isize basic_type_size(TypeKind kind) {
    switch(kind) {
        case Type_i8:  case Type_u8:  case Type_bool:  return 1;
        case Type_i32: case Type_u32: case Type_f32:    return 4;
        case Type_i64: case Type_u64: case Type_f64:    return 8;
        case Type_untyped_int:  case Type_untyped_float: return 8;
        case Type_void:                                  return 0;
        case Type_pointer:                               return sizeof(void*);
        case Type_isize:  case Type_usize:              return sizeof(void*);
        default:                                         return 8;
    }
}

static isize basic_type_align(TypeKind kind) {
    if (kind == Type_pointer) return sizeof(void*);
    if (kind == Type_isize || kind == Type_usize) return sizeof(void*);
    return basic_type_size(kind);
}

isize type_size_of(TypeRef type) {
    switch(type->kind) {
        case Type_array:
            return (isize)type->array_info.count * type_size_of(type->array_info.element_type);
        case Type_struct: {
            if(type->struct_info.struct_fields.count == 0) return 0;
            auto& last = type->struct_info.struct_fields[type->struct_info.struct_fields.count - 1];
            return field_offset_in_struct(type, type->struct_info.struct_fields.count - 1)
                 + type_size_of(last.type);
        }
        default:
            return basic_type_size(type->kind);
    }
}

isize type_align_of(TypeRef type) {
    switch(type->kind) {
        case Type_struct: {
            isize max_align = 1;
            for(isize i = 0; i < type->struct_info.struct_fields.count; i++) {
                isize a = type_align_of(type->struct_info.struct_fields[i].type);
                if(a > max_align) max_align = a;
            }
            return max_align;
        }
        case Type_array:
            return type_align_of(type->array_info.element_type);
        default:
            return basic_type_align(type->kind);
    }
}

isize field_offset_in_struct(TypeRef struct_type, isize index) {
    XP_ASSERT_DEFAULT(is_struct_type(struct_type));
    isize offset = 0;
    for(isize i = 0; i < index; i++) {
        TypeRef ft = struct_type->struct_info.struct_fields[i].type;
        isize align = type_align_of(ft);
        offset = xp_align_up_isize(offset, align);
        offset += type_size_of(ft);
    }
    if(index < struct_type->struct_info.struct_fields.count) {
        TypeRef ft = struct_type->struct_info.struct_fields[index].type;
        offset = xp_align_up_isize(offset, type_align_of(ft));
    }
    return offset;
}

