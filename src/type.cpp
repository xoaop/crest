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
        t.struct_members = make_array<Type>(permanent_allocator());
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
    
    // TODO 更多复杂类型比较
    default:
        return true;
    }
}

int size_of_type(Type type) {
    switch(type.kind) {
    case Type_i8:
    case Type_u8:
        return 8;
    case Type_i32:
    case Type_u32:
        return 32;
    case Type_i64:
    case Type_u64:
        return 64;
    case Type_bool:
        return 1;
    default:
        XP_ASSERT_DEFAULT(0);
        return 0;
    }
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
    return type.kind != Type_literal && type.kind != Type_literal_float;
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


void point_to(Type *type, Type *pointed_type) {
    XP_ASSERT_DEFAULT(type->kind == Type_pointer);
    type->pointed_type = pointed_type;
}

void struct_add_member(Type *type, Type member_type) {
    XP_ASSERT_DEFAULT(type->kind == Type_struct);
    type->struct_members.push_back(member_type);
}



void print_type(Type type) {
    switch (type.kind)
    {
    case Type_literal:
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
    case Type_Undefined:
        printf("undefined");
        break;
    default:
        printf("unknown_type");
        break;
    }
}