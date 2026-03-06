#include "type.hpp"

#include "symbol.hpp"


#include "context.hpp"


#include "package.hpp"








static const char *type_strings[] = {

#define TYPE_KIND(name, str) str,
    TYPE_KINDS
#undef TYPE_KIND

};





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
        XP_ASSERT_DEFAULT(0);
    }
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
    Type t;
    t.kind = src->kind;
    if(src->type_name != xp_make_string_zero()) {
        t.type_name = xp_string_copy(type_allocator(), src->type_name);
    }


    switch(src->kind) {
    case Type_function: {
        t.function_info.param_types = array_copy(&src->function_info.param_types, type_allocator());
        t.function_info.return_type = src->function_info.return_type;
    } break;
    case Type_pointer: {
        t.pointed_type = src->pointed_type;
    } break;
    case Type_struct: {
        t.struct_info.pkg = src->struct_info.pkg;
        t.struct_info.struct_fields = array_copy(&src->struct_info.struct_fields, type_allocator());
        t.struct_info.decl_ast = src->struct_info.decl_ast;
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
        return xp_string_equal(a.type_name, b.type_name) && xp_string_equal(a.struct_info.pkg->path, b.struct_info.pkg->path);

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
        case Type_u8:
        case Type_u32:
        case Type_u64:
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

    if(type->type_name.c_str[0] != '[') {
        return false;
    }

    return true;
}

bool is_string_struct_type(TypeRef type) {
    if(!is_struct_type(type)) {
        return false;
    }

    if(xp_string_cmp(type->type_name, xp_string_c("string")) == 0) {
        return true;
    }

    return false;
}


bool is_value_type(TypeRef type) {
    return is_integer_or_untyped_type(type) || 
           is_float_or_untyped_type(type) || 
           type->kind == Type_bool || 
           is_pointer_type(type) ||
           is_struct_type(type) ||
           is_array_type(type);
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
        default: 
            UNREACHABLE();
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
        default:
            UNREACHABLE();
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
            UNREACHABLE();
            return false;
    }
}



void print_type(TypeRef type) {
    if(type == NULL) {
        printf("null_type");
        return;
    }

    switch (type->kind)
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
        for(isize i = 0; i < type->function_info.param_types.count; i++) {
            print_type(type->function_info.param_types[i]);
            if(i != type->function_info.param_types.count - 1) {
                printf(", ");
            }
        }
        printf(") -> ");
        print_type(type->function_info.return_type);
        break;
    case Type_pointer: {
        Type *curr = type;
        for(;;) {
            printf("*");
            if(curr->pointed_type->kind == Type_pointer) {
                curr = curr->pointed_type;
            } else {
                print_type(curr->pointed_type);
                break;
            }
        }
    } break;
    case Type_struct:
        printf("struct %s", type->type_name.c_str);
        break;

    case Type_Undefined:
        printf("undefined");
        break;
    default:
        printf("unknown_type");
        break;
    }
}



//
//
//



static TypeTable global_type_table;


void init_type_table(Context *ctx) {
    xp_interning_table_init(&global_type_table.type_interning_table);
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

TypeRef struct_type(Package *pkg, xpString ident, Array<StructField> fields) {
    Type t = {};
    t.kind = Type_struct;
    t.type_name = ident;
    t.struct_info.pkg = pkg;
    t.struct_info.struct_fields = fields;

    TypeRef type_ref = get_or_add_type(t);

    return type_ref;
}

TypeRef unfinished_struct_type(Package *pkg, xpString ident) {
    Type t = {};
    t.kind = Type_struct;
    t.type_name = ident;
    t.struct_info.pkg = pkg;

    TypeRef type_ref = get_or_add_type(t);

    return type_ref;
}

TypeRef type_type(TypeRef self_type_info) {
    Type t = {};
    t.kind = Type_type;
    t.self_type_info = self_type_info;

    return get_or_add_type(t);
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

void finish_unfinish_struct_type(TypeRef unfinish, Array<StructField> fields) {
    XP_ASSERT_DEFAULT(is_struct_type(unfinish));
    XP_ASSERT_DEFAULT(unfinish->struct_info.struct_fields.count == 0);

    unfinish->struct_info.struct_fields = fields;
}


TypeRef error_type() {
    Type t = {};
    t.kind = Type_error;

    return get_or_add_type(t);
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
    return type_ref;
}




// NOTE: 实际上就是结构体
TypeRef slice_type_as_struct(TypeRef elem_type) {
    
    xpString slice_struct_name = xp_make_string(temp_allocator(), "[]");
    xpString elem_str = get_or_make_type_str(elem_type, temp_allocator(), false);

    xp_string_append(&slice_struct_name, elem_str);


    Type t = {};
    t.kind = Type_struct;
    t.type_name = slice_struct_name;


    Array<StructField> fields = make_array<StructField>(temp_allocator());



    array_push_back(&fields, StructField { 
        xp_make_string(type_allocator(), "data"), 
        pointer_type(elem_type) 
    });
    array_push_back(&fields, StructField { 
        xp_make_string(type_allocator(), "count"), 
        easy_type(Type_i64) 
    });



    return struct_type(&context()->global_blank_package, slice_struct_name, fields);

    xp_arena_allocator_clear(temp_allocator());
}

// NOTE: 实际上就是结构体
TypeRef string_type_as_struct() {
    xpString string_struct_name = xp_string_c("string");

    Type t = {};
    t.kind = Type_struct;
    t.type_name = string_struct_name;
    t.struct_info.pkg = &context()->global_blank_package;

    TypeRef type_ref = get_type(t);
    
    if(type_ref != NULL) {
        return type_ref;
    } else {
        XP_ASSERT_MSG(0, "string type should be pre-defined");
    }
    
}





xpString get_type_kind_str(TypeKind kind) {
    return xp_string_c(type_strings[kind]);
}

xpString get_type_name(TypeRef type, bool need_free_temp_allocator) {

    // 如果已经有名字就直接返回
    if(type->type_name.capacity != 0) {
        return type->type_name;
    }

    type->type_name = get_or_make_type_str(type, permanent_allocator(), need_free_temp_allocator);

    return type->type_name;
}


xpString get_or_make_type_str(TypeRef type, xpAllocator allocator, bool need_free_temp_allocator) {

    static i32 depth = 0;
    depth++;
    defer(depth--);

    // 如果用temp_allocator作为结果的allocator, 那结果就没法返回了, 因为释放了
    XP_ASSERT_DEFAULT(!(depth == 1 && allocator.data == temp_allocator().data && need_free_temp_allocator == true)); // TODO 这里比较太tricky了
        

    // 使用temp_allocator避免内存泄漏
    defer({
        if(depth == 1) {
            xp_arena_allocator_clear(temp_allocator());
        }
    });


    

    
    if(is_easy_type_kind(type->kind)) {

        return get_type_kind_str(type->kind);
    } else if(is_pointer_type(type)) {
        xpString base_str = xp_make_string(allocator, "*");

        // 因为可能有多级指针，所以递归获取, 而且用temp_allocator避免内存泄漏
        xpString pointed_type_str = get_or_make_type_str(type->pointed_type, temp_allocator(), need_free_temp_allocator);

        xp_string_append(&base_str, pointed_type_str);

        return base_str;

    } else if(is_struct_type(type)) {
        // 直接用类型名

        return type->type_name;

    } else if(is_array_type(type)) {
        xpString base_str = xp_make_string(allocator, "[");
            
        xpString count_str = xp_isize_to_string(type->array_info.count, temp_allocator());
        xp_string_append(&base_str, count_str);

        xp_string_append(&base_str, xp_string_c("]"));

        // 因为可能是复杂类型，所以递归获取, 而且用temp_allocator避免内存泄漏
        xpString elem_type_str = get_or_make_type_str(type->array_info.element_type, temp_allocator(), need_free_temp_allocator);

        xp_string_append(&base_str, elem_type_str);

        return base_str;

    } else if(is_function_type(type)) {
        xpString func_str = xp_make_string(allocator, "(");

        for(isize i = 0; i < type->function_info.param_types.count; i++) {

            xpString param_type_str = get_or_make_type_str(type->function_info.param_types[i], temp_allocator(), need_free_temp_allocator);
            xp_string_append(&func_str, param_type_str);

            if(i != type->function_info.param_types.count - 1) {
                xpString comma_str = xp_string_c(", ");
                xp_string_append(&func_str, comma_str);
            }

            xpString rb_arrow_str = xp_string_c(") -> ");
            xp_string_append(&func_str, rb_arrow_str);

            xpString return_type_str = get_or_make_type_str(type->function_info.return_type, temp_allocator(), need_free_temp_allocator);
            xp_string_append(&func_str, return_type_str);

            return func_str;
        }

    } else if(is_type_type(type)) {
        xpString type_str = xp_make_string(allocator, "type(");
        xpString self_type_str = get_or_make_type_str(type->self_type_info, temp_allocator(), need_free_temp_allocator);
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
    UNREACHABLE();
}
















template<>
usize xp_hash_func(Type *type) {
    switch(type->kind) {

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
        case Type_f32:
        case Type_f64:
        case Type_bool:
        case Type_void:
        case Type_untyped_int:
        case Type_untyped_float: {
            return cast(usize)(type->kind);
        } break;

        case Type_function: {
            u64 hash_value = Type_function;
            for(isize i = 0; i < type->function_info.param_types.count; i++) {
                u64 param_type_hash = cast(u64)(xp_hash_func(type->function_info.param_types[i]));
                hash_value = xp_hash_combine_u64(hash_value, param_type_hash);
            }
            u64 return_type_hash = cast(u64)(xp_hash_func(type->function_info.return_type));
            hash_value = xp_hash_combine_u64(hash_value, return_type_hash);

            return cast(usize)(hash_value);
        } break;

        case Type_pointer: {
            u64 hash_pointer = Type_pointer;
            u64 hash_pointed_type = cast(u64)(xp_hash_func(type->pointed_type));
            return cast(usize)(xp_hash_combine_u64(hash_pointer, hash_pointed_type));
        } break;


        case Type_struct: {
            u64 hash_struct = Type_struct;
            u64 hash_type_name = cast(u64)(xp_hash_func(&type->type_name));

            // package用path作为哈希值
            u64 hash_package = cast(u64)(xp_hash_func(&type->struct_info.pkg->path));


            u64 hash_value = xp_hash_combine_u64(hash_struct, hash_type_name);
            hash_value = xp_hash_combine_u64(hash_value, hash_package);


            return cast(usize)(hash_value);
        } break;

        case Type_array: {
            u64 hash_array = Type_array;
            u64 hash_element_type = cast(u64)(xp_hash_func(type->array_info.element_type));
            u64 hash_count = cast(u64)(type->array_info.count);
            u64 hash_value = xp_hash_combine_u64(hash_array, hash_element_type);
            hash_value = xp_hash_combine_u64(hash_value, hash_count);
            return cast(usize)(hash_value);

        } break;

        case Type_type: {
            u64 hash_type = Type_type;
            u64 hash_self_type_info = cast(u64)(xp_hash_func(type->self_type_info));
            u64 hash_value = xp_hash_combine_u64(hash_type, hash_self_type_info);
            return cast(usize)(hash_value);
        } break;


        case Type_package: {
            u64 hash_package = Type_package;
            u64 hash_package_info = cast(u64)(xp_hash_func(&type->package_info->path));
            u64 hash_value = xp_hash_combine_u64(hash_package, hash_package_info);
            return cast(usize)(hash_value);
        } break;


        default: {
            XP_ASSERT_DEFAULT(0);
        } break;
    
    }
}








xpAllocator type_allocator() {
    return permanent_allocator();
}