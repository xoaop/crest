#include "type.hpp"








TypeKind string_to_type_kind(xpString str) {
    if(!xp_string_cmp(str, xp_string_c("i8"))) {
        return Type_i8;
    } else if(!xp_string_cmp(str, xp_string_c("i32"))) {
        return Type_i32;
    } else if(!xp_string_cmp(str, xp_string_c("i64"))) {
        return Type_i64;
    } else if(!xp_string_cmp(str, xp_string_c("u8"))) {
        return Type_u8;
    } else if(!xp_string_cmp(str, xp_string_c("u32"))) {
        return Type_u32;
    } else if(!xp_string_cmp(str, xp_string_c("u64"))) {
        return Type_u64;
    } else if(!xp_string_cmp(str, xp_string_c("f32"))) {
        return Type_f32;
    } else if(!xp_string_cmp(str, xp_string_c("f64"))) {
        return Type_f64;
    } else if(!xp_string_cmp(str, xp_string_c("bool"))) {
        return Type_bool;
    } else if(!xp_string_cmp(str, xp_string_c("void"))) {
        return Type_void;
    } else {
        return Type_Undefined;
    }
}


Type make_type(TypeKind kind) {
    Type t = {};
    t.kind = kind;

    switch(kind) {
    case Type_function:
        t.function_info.param_types = make_array<Type>(permanent_allocator());
        t.function_info.return_type = alloc_type(permanent_allocator(), Type_Undefined);
        break;
    case Type_pointer:
        t.pointed_type = NULL;
        break;
    case Type_struct:
        t = make_struct_type();
        break;
    case Type_array:
        t.array_info.element_type = NULL;
        t.array_info.count = 0;
        break;
    default: 
        break;
    }


    return t;
}

Type copy_type(Type *src) {
    Type t;
    t.kind = src->kind;
    t.type_name = src->type_name;


    switch(src->kind) {
    case Type_function:
        t.function_info.param_types = make_array<Type>(permanent_allocator());
        for(isize i = 0; i < src->function_info.param_types.count; i++) {
            array_push_back(&t.function_info.param_types, copy_type(&src->function_info.param_types[i]));
        }
        t.function_info.return_type = alloc_type(permanent_allocator(), src->function_info.return_type->kind);
        *(t.function_info.return_type) = copy_type(src->function_info.return_type);
        break;
    case Type_pointer:
        t.pointed_type = alloc_type(permanent_allocator(), src->pointed_type->kind);
        *(t.pointed_type) = copy_type(src->pointed_type);
        break;
    case Type_struct: {
        t.struct_fields = array_copy(&src->struct_fields, permanent_allocator());
    } break;
    
    // TODO: 更多复杂类型

    default:
        break;
    }

    return t;
}


Type make_pointer_type(TypeKind base_type_kind, isize level_of_pointer) {
    XP_ASSERT_DEFAULT(level_of_pointer > 0);
    XP_ASSERT_DEFAULT(base_type_kind != Type_pointer);

    
    Type pointer_type = make_type(Type_pointer);

    Type* *curr_pointed_type = &pointer_type.pointed_type;
    for(isize i = 0; i < level_of_pointer - 1; i++) {
        Type *next = alloc_type(permanent_allocator(), Type_pointer);
        *curr_pointed_type = next;

        curr_pointed_type = &(*curr_pointed_type)->pointed_type;
    }

    *curr_pointed_type = alloc_type(permanent_allocator(), base_type_kind);

    return pointer_type;
}

Type make_pointer_type(Type base_type, isize level_of_pointer) {
    XP_ASSERT_DEFAULT(level_of_pointer > 0);
    XP_ASSERT_DEFAULT(base_type.kind != Type_pointer);

    
    Type pointer_type = make_type(Type_pointer);

    Type* *curr_pointed_type = &pointer_type.pointed_type;
    for(isize i = 0; i < level_of_pointer - 1; i++) {
        Type *next = alloc_type(permanent_allocator(), Type_pointer);
        *curr_pointed_type = next;

        curr_pointed_type = &(*curr_pointed_type)->pointed_type;
    }

    *curr_pointed_type = alloc_type(permanent_allocator(), base_type.kind);
    *( *curr_pointed_type ) = base_type;

    return pointer_type;
}

Type make_pointer_type(Type pointed_type) {
    Type pointer_type = make_type(Type_pointer);
    pointer_type.pointed_type = alloc_type(permanent_allocator(), pointed_type.kind);

    *(pointer_type.pointed_type) = pointed_type;

    return pointer_type;
}

Type get_pointed_type(Type pointer_type) {
    XP_ASSERT_DEFAULT(pointer_type.kind == Type_pointer);
    XP_ASSERT_DEFAULT(pointer_type.pointed_type != NULL);

    return *(pointer_type.pointed_type);
}

void point_to(Type *type, Type *pointed_type) {
    XP_ASSERT_DEFAULT(type->kind == Type_pointer);
    type->pointed_type = pointed_type;
}

Type get_innermost_type_of_pointer(Type pointer_type) {
    XP_ASSERT_DEFAULT(pointer_type.kind == Type_pointer);

    for(;;) {
        if(!is_pointer_type(pointer_type)) {
            return pointer_type;
        }

        pointer_type = *(pointer_type.pointed_type);
    }
}



Type make_struct_type() {
    Type struct_type;
    struct_type.kind = Type_struct;
    struct_type.struct_fields = make_array<StructField>(permanent_allocator());
    return struct_type;
}


Type make_struct_type(xpString name) {
    Type struct_type = make_struct_type();
    struct_type.type_name = name;
    return struct_type;
}



void struct_add_member(Type *type, xpString name, Type member_type) {
    XP_ASSERT_DEFAULT(type->kind == Type_struct);
    array_push_back(&type->struct_fields, StructField{name, member_type});
}





Type *alloc_type(xpAllocator allocator, TypeKind kind) {
    Type *t = cast(Type *) xp_alloc(allocator, sizeof(Type));
    *t = make_type(kind);
    return t;
}


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
            if(!is_equal_type(a.function_info.param_types[i], b.function_info.param_types[i])) {
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
        return xp_string_cmp(a.type_name, b.type_name) == 0;

    // TODO 更多复杂类型比较
    default:
        return true;
    }
}



bool is_basic_type_kind(TypeKind kind) {
    switch(kind) {
    case Type_i8:
    case Type_i32:
    case Type_i64:
    case Type_u8:
    case Type_u32:
    case Type_u64:
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
    return is_basic_type_kind(kind) || kind == Type_untyped_int || kind == Type_untyped_float;
}

// 包括复杂类型和不确定类型
bool is_hard_type_kind(TypeKind kind) {
    return is_complex_type_kind(kind) || kind == Type_uncertain;
}



bool is_integer_type(Type type) {
    switch(type.kind)
    {
    case Type_i8:
    case Type_i32:
    case Type_i64:
    case Type_u8:
    case Type_u32:
    case Type_u64:
    // case Type_bool: // NOTE: bool 也作为整数类型处理, i1
        return true;
    default:
        return false;
    }
}

bool is_integer_or_bool_type(Type type) {
    switch(type.kind)
    {
    case Type_i8:
    case Type_i32:
    case Type_i64:
    case Type_u8:
    case Type_u32:
    case Type_u64:
    case Type_bool: // NOTE: bool 也作为整数类型处理, i1
        return true;
    default:
        return false;
    }
}

bool is_signed_type(Type type) {
    XP_ASSERT_DEFAULT(is_integer_type(type));
    switch(type.kind) {
    case Type_i8:
    case Type_i32:
    case Type_i64:
        return true;
    default:
        return false;
    }
}

bool is_signed_or_bool_type(Type type) {
    XP_ASSERT_DEFAULT(is_integer_or_bool_type(type));
    switch(type.kind) {
    case Type_i8:
    case Type_i32:
    case Type_i64:
    case Type_bool:
        return true;
    default:
        return false;
    }
}

bool is_unsigned_type(Type type) {
    XP_ASSERT_DEFAULT(is_integer_type(type));
    switch(type.kind) {
    case Type_u8:
    case Type_u32:
    case Type_u64:

    // case Type_bool: // NOTE: bool 也作为无符号整数类型处理, i1
        return true;
    default:
        return false;
    }
}

bool is_float_type(Type type) {
    return type.kind == Type_f32 || type.kind == Type_f64;
}

bool is_certain_type(Type type) {
    return type.kind != Type_untyped_int && type.kind != Type_untyped_float;
}

bool is_pointer_type(Type type) {
    return type.kind == Type_pointer;
}

bool is_struct_type(Type type) {
    return type.kind == Type_struct;
}


int get_type_rank(Type t) {
    switch (t.kind) {
        case Type_i8: case Type_u8:  return 8;
        case Type_i32: case Type_u32: return 32;
        case Type_i64: case Type_u64: return 64;
        case Type_f32: return 32;
        case Type_f64: return 64;
        default: 
            XP_ASSERT_DEFAULT(0);
            return 0;
    }
}


Type get_common_type(Type a, Type b) {
    if(is_equal_type(a, b)) {
        return a;
    }

    if(is_float_type(a) && is_float_type(b)) {
        return (get_type_rank(a) >= get_type_rank(b)) ? a : b;
    }


    if(is_integer_type(a) && is_integer_type(b)) {
        if (is_signed_type(a) != is_signed_type(b)) {
            XP_ASSERT_DEFAULT(0);
        }

        return (get_type_rank(a) >= get_type_rank(b)) ? a : b;
    }


    XP_ASSERT_DEFAULT(0);
    
    return make_type(Type_Undefined);
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






void print_type(Type type) {
    switch (type.kind)
    {
    case Type_untyped_int:
        printf("literal");
        break;
    case Type_i8:
        printf("i8");
        break;
    case Type_i32:
        printf("i32");
        break;
    case Type_i64:
        printf("i64");
        break;
    case Type_u8:
        printf("u8");
        break;
    case Type_u32:
        printf("u32");
        break;
    case Type_u64:
        printf("u64");
        break;
    case Type_f32:
        printf("f32");
        break;
    case Type_f64:
        printf("f64");
        break;
    case Type_bool:
        printf("bool");
        break;
    case Type_void:
        printf("void");
        break;
    case Type_function:
        printf("func(");
        for(isize i = 0; i < type.function_info.param_types.count; i++) {
            print_type(type.function_info.param_types[i]);
            if(i != type.function_info.param_types.count - 1) {
                printf(", ");
            }
        }
        printf(") -> ");
        print_type(*type.function_info.return_type);
        break;
    case Type_pointer: {
        Type *curr = &type;
        for(;;) {
            printf("*");
            if(curr->pointed_type->kind == Type_pointer) {
                curr = curr->pointed_type;
            } else {
                print_type(*curr->pointed_type);
                break;
            }
        }
    } break;
    case Type_struct:
        printf("struct %s", type.type_name.c_str);
        break;
    case Type_uncertain:
        printf("uncertain");
        break;

    case Type_Undefined:
        printf("undefined");
        break;
    default:
        printf("unknown_type");
        break;
    }
}



static TypeTable global_type_table;


void init_type_table() {
    global_type_table.type_set = xp_hash_set_make<Type>(permanent_allocator());
}


TypeRef easy_type(TypeKind kind) {
    XP_ASSERT_DEFAULT(is_easy_type_kind(kind));
    Type t = make_type(kind);
    return get_type(t);
}


TypeRef pointer_type(TypeRef pointed_type) {
    XP_TODO;    
}

TypeRef function_type(Array<TypeRef> param_types, TypeRef return_type) {
    XP_TODO;
}

TypeRef struct_type(xpString name) {
    XP_TODO;
}


TypeRef get_type(Type type) {
    TypeRef type_ref = xp_hash_set_get(&global_type_table.type_set, type);
    if(type_ref != NULL) {
        return type_ref;
    }

    return add_type(type);
}


TypeRef add_type(Type type) {
    TypeRef type_ref = xp_hash_set_insert(&global_type_table.type_set, type);
    return type_ref;
}





template<>
usize xp_hash_func(Type *type) {
    switch(type->kind) {

        // 基本类型
        // 由于没有额外类型信息, 直接用kind作为hash值
        case Type_i8:
        case Type_i32:
        case Type_i64:
        case Type_u8:
        case Type_u32:
        case Type_u64:
        case Type_f32:
        case Type_f64:
        case Type_bool:
        case Type_void:
        case Type_untyped_int:
        case Type_untyped_float: {
            return cast(usize)(type->kind);
        } break;

        case Type_function: {
            XP_TODO;
        } break;

        case Type_pointer: {
            XP_TODO;
        } break;

        // 目前的uncertain只可能表示不确定的结构体类型, 所以哈希逻辑和struct一样
        case Type_uncertain: 
        case Type_struct: {
            XP_TODO;
        } break;

        case Type_array: {
            XP_TODO;
        } break;

        default: {
            XP_ASSERT_DEFAULT(0);
        } break;
    
    }
}