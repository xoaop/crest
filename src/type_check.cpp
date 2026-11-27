#include "type_check.hpp"

#include <cstdint>
#include <cfloat>

#include "error_msg.hpp"
#include "print.hpp"
#include "context.hpp"




bool check_explicit_type_cast(CIRPackageRef pkg, CIRInstructionRef casted_inst_ref, TypeRef casted_expr_type, TypeRef target_type) {

    if((is_integer_or_untyped_type(casted_expr_type) || is_float_or_untyped_type(casted_expr_type)) && (is_integer_or_untyped_type(target_type) || is_float_or_untyped_type(target_type))) {
        // 任意数字类型之间都可以转化

        return true;
    } else if(is_pointer_type(casted_expr_type) && is_pointer_type(target_type)) {
        //
        //  pointer -> pointer 允许任何指针类型之间的转化
        //
        return true;

    } else if(is_array_type(casted_expr_type) && is_slice_struct_type(target_type)) {
        //
        // 数组可以转化为slice结构体类型
        //

        // 前提: 元素类型相同, 且操作数是IdentRef(稳定栈槽), 因为slice的.ptr需要稳定地址
        if(pkg->result_of(casted_inst_ref).value_kind == CIRValueKind::LValue
            && target_type->struct_info.struct_fields[0].type->pointed_type == casted_expr_type->array_info.element_type) {
            return true;
        } else {
            return false;
        }
    } else if(is_enum_type(casted_expr_type)) {
        // 枚举类型可以转化为其底层整数类型
        if(casted_expr_type->enum_info.element_type == target_type) {
            return true;
        } else {
            return false;
        }
    }
    
    else {
        // 其他类型之间只能转化为完全相同的类型
        if(casted_expr_type == target_type) {
            return true;
        } else {
            return false;
        }
    }

}



bool check_extern_c_func(xpAllocator allocator, const Array<TypeRef>& param_types, TypeRef return_type, xpString& err_msg) {
    // 空聚合体在 C 里无法表达：零大小的参数/返回值没有 ABI 位置
    auto is_empty_agg = [](TypeRef t) {
        return (is_struct_type(t) || is_union_type(t)) && type_size_of(t) == 0;
    };

    for(TypeRef pt : param_types) {
        if(is_array_type(pt)) {
            std::string msg = std::format(
                "extern \"C\" 形参不能是数组：C 会把数组形参退化为指针，请改用 '*{}'",
                pt->array_info.element_type->name()
            );

            err_msg = xp_make_string(allocator, msg.c_str());
            return false;
        }

        if(is_empty_agg(pt)) {
            err_msg = xp_make_string(allocator, "extern \"C\" 形参不能是空聚合体：C 无法表达零大小参数");
            return false;
        }
    }

    if(is_array_type(return_type)) {
        err_msg = xp_make_string(allocator, "extern \"C\" 函数不能返回数组（C 不支持按值返回数组）");
        return false;
    }

    if(is_empty_agg(return_type)) {
        err_msg = xp_make_string(allocator, "extern \"C\" 函数不能返回空聚合体：C 无法表达零大小返回");
        return false;
    }

    return true;
}



bool check_untyped_int_to_type(i128 value, TypeRef target_type) {
    // 如连整数类型都不是, 那肯定无法转化
    if(!is_integer_or_untyped_type(target_type)) {
        return false;
    }


    // 在整数的基础上, 再检查溢出
    return !check_integer_overflow(value, target_type);
}

bool check_untyped_float_to_type(double value, TypeRef target_type) {
    // 如连浮点数类型都不是, 那肯定无法转化
    if(!is_float_or_untyped_type(target_type)) {
        return false;
    }

    // 在浮点数的基础上, 再检查溢出
    return !check_float_overflow(value, target_type);
}


bool check_untyped_to_type(Value& val, TypeRef target_type) {
    if(val.type == easy_type(Type_untyped_int)) {
        return check_untyped_int_to_type(val.integer_val(), target_type);
    } else if(val.type == easy_type(Type_untyped_float)) {
        return check_untyped_float_to_type(val.float_val(), target_type);
    } else {
        UNREACHABLE();
        return false;
    }
}



TypeRef get_compliable_integer_type(i128 value) {

    if(check_untyped_int_to_type(value, easy_type(Type_i32))) {
        return easy_type(Type_i32);
    } else if(check_untyped_int_to_type(value, easy_type(Type_i64))) {
        return easy_type(Type_i64);
    } else if(check_untyped_int_to_type(value, easy_type(Type_u64))) {
        return easy_type(Type_u64);
    } else {
        return error_type();
    }
}

TypeRef get_compliable_float_type(double value) {

    if(check_untyped_float_to_type(value, easy_type(Type_f32))) {
        return easy_type(Type_f32);
    } else if(check_untyped_float_to_type(value, easy_type(Type_f64))) {
        return easy_type(Type_f64);
    } else {
        return error_type();
    }

}

TypeRef get_compliable_const_type(Value& val) {
    ASSERT(is_untyped_type(val.type));


    if(val.type == easy_type(Type_untyped_int)) {
        return get_compliable_integer_type(val.integer_val());
    } else if(val.type == easy_type(Type_untyped_float)) {
        return get_compliable_float_type(val.float_val());
    } else {
        UNREACHABLE();
        return error_type();
    }
}

bool check_integer_overflow(i128 val, TypeRef type) {
    ASSERT(is_integer_type(type));

    switch(type->kind) {
        case Type_i8:
            return (val < INT8_MIN || val > INT8_MAX);
        case Type_i16:
            return (val < INT16_MIN || val > INT16_MAX);
        case Type_i32:
            return (val < INT32_MIN || val > INT32_MAX);
        case Type_i64:
            return (val < INT64_MIN || val > INT64_MAX);
        case Type_u8:
            return (val < 0 || val > UINT8_MAX);
        case Type_u16:
            return (val < 0 || val > UINT16_MAX);
        case Type_u32:
            return (val < 0 || val > UINT32_MAX);
        case Type_u64:
            return (val < 0 || val > UINT64_MAX);
        case Type_isize:
            return (val < INTPTR_MIN || val > INTPTR_MAX);
        case Type_usize:
            return (val < 0 || val > (i128)UINTPTR_MAX);
        default:
            UNREACHABLE();
            return false;
    }
}

bool check_float_overflow(double val, TypeRef type) {
    ASSERT(is_float_type(type));

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

bool is_val_overflow(const Value& val) {
    if(is_integer_type(val.type)) {
        return check_integer_overflow(val.integer_val(), val.type);
    } else if(is_float_type(val.type)) {
        return check_float_overflow(val.float_val(), val.type);
    } else {
        return false;
    }
}


std::optional<xpString> check_var_type(xpAllocator allocator, const Value& val, bool is_param) {
    // 类型位置拿到的是函数值：写的是函数声明当类型用，比如 field: some_func
    if(val.actual_type() == ActualValueType::Function) {
        return xp_make_string(allocator, "类型不能是函数（函数不是类型，也没有存储布局），请用函数指针 '*...' 或改用具名函数类型");
    }
    const TypeRef type = val.type_val();
    // $T 类型形参的类型就是 'type'，是唯一合法的例外
    if(type == type_type() && !is_param) {
        return xp_make_string(allocator, "类型不能是 'type'（类型类型没有存储空间）");
    }
    // void 无存储布局，只有函数返回类型位置合法
    if(type->kind == Type_void) {
        return xp_make_string(allocator, "类型不能是 'void'（void 没有存储空间）");
    }
    if(is_function_type(type)) {
        // C 会自动把函数类型降级成指针；这里要显式的 '*'，所以按错误报出去
        return xp_string_concat_mid(
            xp_make_string(allocator, "类型不能是函数类型（没有存储布局），请用函数指针 '*"),
            type->name(),
            xpOption<xpString>(xp_make_string(allocator, "'")),
            allocator
        );
    }
    return std::nullopt;
}

std::optional<xpString> check_const_type(xpAllocator allocator, TypeRef type) {
    // void 无存储布局，不能绑给常量（b :: <void 值>）
    if(type->kind == Type_void) {
        return xp_make_string(allocator, "常量不能是 'void' 类型（void 没有存储空间）");
    }

    if(is_pointer_type(type)) {
        return xp_make_string(allocator, "常量不能是 指针类型（指针没有存储空间）");
    }

    return std::nullopt;
}


std::optional<TypeRef> default_certain_type_for_untyped_type_opt(TypeRef untyped_type) {
    if(untyped_type == easy_type(Type_untyped_int)) {
        return easy_type(Type_i32);
    } else if(untyped_type == easy_type(Type_untyped_float)) {
        return easy_type(Type_f64);
    }

    return std::nullopt;
}


TypeRef default_certain_type_for_untyped_type(TypeRef untyped_type) {
    const auto type = default_certain_type_for_untyped_type_opt(untyped_type);
    if(type.has_value()) {
        return type.value();
    }
    
    DEBUG_PANIC("untyped_type {} is not untyped int or untyped float", untyped_type->name());
    return nullptr;
}
