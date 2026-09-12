#include "type_check.hpp"
#include "error_msg.hpp"
#include "print.hpp"
#include "context.hpp"




bool check_explicit_type_cast(CIRPackage *pkg, CIRInstructionRef casted_inst_ref, TypeRef casted_expr_type, TypeRef target_type) {

    if((is_integer_or_untyped_type(casted_expr_type) || is_float_or_untyped_type(casted_expr_type) || casted_expr_type->kind == Type_bool) && (is_integer_or_untyped_type(target_type) || is_float_or_untyped_type(target_type) || target_type->kind == Type_bool)) {
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

std::optional<xpString> check_var_type(const Value& val, bool is_param) {
    // 类型位置拿到的是函数值：写的是函数声明当类型用，比如 field: some_func
    if(val.actual_type() == ActualValueType::Function) {
        return xp_make_string(temp_allocator(), "类型不能是函数（函数不是类型，也没有存储布局），请用函数指针 '*...' 或改用具名函数类型");
    }
    const TypeRef type = val.type_val();
    // $T 类型形参的类型就是 'type'，是唯一合法的例外
    if(type == type_type() && !is_param) {
        return xp_make_string(temp_allocator(), "类型不能是 'type'（类型类型没有存储空间）");
    }
    if(is_function_type(type)) {
        // C 会自动把函数类型降级成指针；这里要显式的 '*'，所以按错误报出去
        return xp_string_concat_mid(
            xp_make_string(temp_allocator(), "类型不能是函数类型（没有存储布局），请用函数指针 '*"),
            type->name(),
            xpOption<xpString>(xp_string_c("'")),
            temp_allocator()
        );
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
