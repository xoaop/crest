#include "type_check.hpp"

#include "const_fold.hpp"

#include "error_msg.hpp"

#include "context.hpp"

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

// bool check_untyped_to_type(Ast *constant, TypeRef target_type) {
//     XP_ASSERT_DEFAULT(constant->type == AstType_Constant);

//     if(constant->v_type == easy_type(Type_untyped_int)) {
//         return check_untyped_int_to_type(constant->Constant.value, target_type);
//     } else if(constant->v_type == easy_type(Type_untyped_float)) {
//         return check_untyped_float_to_type(constant->Constant.float_value, target_type);
//     } else {
//         UNREACHABLE();
//         return false;
//     }
// }

bool check_untyped_to_type(Value& val, TypeRef target_type) {
    if(val.type == easy_type(Type_untyped_int)) {
        return check_untyped_int_to_type(val.integer_value, target_type);
    } else if(val.type == easy_type(Type_untyped_float)) {
        return check_untyped_float_to_type(val.float_value, target_type);
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
    XP_ASSERT_DEFAULT(is_untyped_type(val.type));


    if(val.type == easy_type(Type_untyped_int)) {
        return get_compliable_integer_type(val.integer_value);
    } else if(val.type == easy_type(Type_untyped_float)) {
        return get_compliable_float_type(val.float_value);
    } else {
        UNREACHABLE();
        return error_type();
    }
}
   





static bool check_is_lvalue(Ast *expr, TypeRef v_type, Analyser analyser) {
    switch(expr->type) {
    case AstType_UnaryExpr: {

        if(expr->UnaryExpr.op == TokenType::Star) {

            if(expr->UnaryExpr.operand->is_lvalue) {
                return true;
            } else {
                return false;
            }    

        } else {

            return false;
        }    
    } break;    


    case AstType_Ident: {
        xpString ident_name = expr->Ident.name;
        SymbolInfo *info = find_symbol_until_global(analyser.current_scope, ident_name);
        if(info == NULL) {
            return false;
        }

        Value val = info->value;
        if(val.is_runtime_value) {
            // 说明只是变量

            return true;
        } else {
            // 说明是常量, 常量不是lvalue

            return false;
        }

    } break;    

    case AstType_IndexExpr: {
        if(expr->IndexExpr.array_var_expr->is_lvalue) {
            return true;
        } else {
            return false;
        }    
    } break;    

    case AstType_FieldAccess: {
        if(expr->FieldAccess.parent->is_lvalue) {
            return true;
        } else {
            return false;
        }    

    } break;    

    default: {
        return false;
    } break;    

    }

    return false;
}

// 如果是untyped类型, 尝试转化为target_type, 转化成功返回true, 转化失败返回false 
// 如果不是untyped类型, 那么直接返回true, 表示无事发生, 一切正常
bool if_untyped_try_to_typed_or_do_nothing(Value& val, TypeRef target_type) {
    if(is_untyped_type(val.type)) {
        if(check_untyped_to_type(val, target_type)) {
            return true;
        } else {
            return false;
        }
    }

    return true;
}




// TODO: 逻辑待检查
bool check_explicit_type_cast(TypeRef casted_expr_type, TypeRef target_type) {

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

        if(target_type->struct_info.struct_fields[0].type->pointed_type == casted_expr_type->array_info.element_type) {
            // 前提元素类型相同
            return true;
        } else {
            return false;
        }
    } else {
        // 其他类型之间只能转化为完全相同的类型

        if(casted_expr_type == target_type) {
            return true;
        } else {
            return false;
        }
    } 
        
}

// NOTE: 隐式转化目前只代表赋值时的隐式转化
bool check_implicit_convension(Ast *expr, TypeRef checked_type, TypeRef target_type) {
    bool implicit_spec_ok = false; // 特定类型的隐式转换是否合法
    if(expr->is_null) {
        if(is_pointer_type(target_type)) {
            implicit_spec_ok = true;
        }
    } else if(is_pointer_type(checked_type) && target_type == pointer_type(easy_type(Type_void))) {
        implicit_spec_ok = true;
    } else if(is_array_type(checked_type) && is_slice_struct_type(target_type)) {
        if(target_type->struct_info.struct_fields[0].type->pointed_type == checked_type->array_info.element_type) {
            expr->implicit_conversion_tag = ImplicitConversionTag::ArrayToSliceStruct;
            implicit_spec_ok = true;
        }
    } else if(is_untyped_type(checked_type) && is_certain_type(target_type)) {
        // 无类型整数或浮点数可以隐式转换为任意确定类型的整数或浮点数, 但不能隐式转换为非数字类型
        // NOTE: 没有检查数值是否在目标类型的范围内, 这个检查在常量折叠阶段进行

        if((checked_type == easy_type(Type_untyped_int) && is_integer_type(target_type)) ||
           (checked_type == easy_type(Type_untyped_float) && is_float_type(target_type))) {
            implicit_spec_ok = true;
        }
    } else if(checked_type == target_type) {
        implicit_spec_ok = true;
    }

    return implicit_spec_ok;
}    




TypeRef infer_expr_type(Ast *expr, bool has_target, TypeRef target_type, Analyser analyser, bool allow_untyped) {
    static isize depth = 0;

    depth += 1;

    TypeRef result_type = error_type();
    switch(expr->type) {

        case AstType_Constant: {
            if(expr->v_type == error_type()) {
                break;
            }

            
            if(is_certain_type(expr->v_type)) {
                result_type = expr->v_type;
                break;
            }

            // 还没有类型推导
            if(expr->v_type == undefined_type()) {
                Token token = expr->token;
                if(token.type == TokenType::KW_true || token.type == TokenType::KW_false) {
                    result_type = easy_type(Type_bool);
                    expr->Constant.value.bool_value = (token.type == TokenType::KW_true) ? 1 : 0;
                    expr->Constant.value.set_type(result_type);
                    break;
                }
    
                if(token.type == TokenType::KW_null) {
                    result_type = pointer_type(easy_type(Type_void));
                    expr->Constant.value.set_type(result_type);
                    // expr->Constant.value = 0;
                    expr->is_null = true;
    
                    break;
                }
    
                // 无类型后缀的数字字面量, 先标记为untyped类型, 等待类型推导
                if(token.type == TokenType::Integer) {
                    result_type = easy_type(Type_untyped_int);
                    expr->Constant.value.set_type(result_type);
                } else if(token.type == TokenType::Float) {
                    result_type = easy_type(Type_untyped_float);
                    expr->Constant.value.set_type(result_type);
                }

                if(token.number_info.type_kind_of_number != Type_Undefined) {
                    TypeRef postfix_type = easy_type(token.number_info.type_kind_of_number);

                    if(!check_untyped_to_type(expr->Constant.value, postfix_type)) {
                        context()->reporter.report_error(
                            expr->span, analyser.curr_ast_file->source_code,
                            "constant value can't convert to type specified by suffix"
                        );
                        result_type = error_type();
                        expr->Constant.value.set_type(result_type);

                        break;
                    } else {
                        result_type = postfix_type;
                        expr->Constant.value.set_type(result_type);
                    }

                }
            } else {
                result_type = expr->v_type;
                expr->Constant.value.set_type(result_type);
            }


            // TODO: ABSTRACT
            if(is_untyped_type(result_type)) {
                if(has_target) {
                    if(!check_untyped_to_type(expr->Constant.value, target_type)) {
                        context()->reporter.report_error(
                            expr->span, analyser.curr_ast_file->source_code,
                            "constant value can't convert to target type"
                        );
                        result_type = error_type();
                        expr->Constant.value.set_type(result_type);
                    } else {
                        result_type = target_type;
                        expr->Constant.value.set_type(result_type);
                    }
                } else if(!allow_untyped) {
                    result_type = get_compliable_const_type(expr->Constant.value);
                    expr->Constant.value.set_type(result_type);
                    if(result_type == error_type()) {
                        context()->reporter.report_error(
                            expr->span, analyser.curr_ast_file->source_code,
                            "constant value is too large to fit in any integer or float type"
                        );
                    } else {
                        result_type = result_type;
                        expr->Constant.value.set_type(result_type);
                    }
                } else {
                    // 允许无类型常量存在, 先标记为untyped类型, 等待后续使用时推导
                    result_type = result_type;
                    expr->Constant.value.set_type(result_type);
                }
            }
            
        } break;


        case AstType_BinaryExpr: {

            bool should_check_equal_type = true;

            TypeRef infer_left_type = NULL;
            TypeRef infer_right_type = NULL;


            if(is_return_bool_operator(expr->BinaryExpr.op)) {
                infer_left_type = infer_expr_type(expr->BinaryExpr.left, false, NULL, analyser, true);
                infer_right_type = infer_expr_type(expr->BinaryExpr.right, false, NULL, analyser, true);
            } else {
                // 如果该运算的结果类型不是操作数类型, 那么就不传递target type, 让子表达式自己推导

                // ! TODO allow_untyped 不确定

                infer_left_type = infer_expr_type(expr->BinaryExpr.left, has_target, target_type, analyser, allow_untyped);
                infer_right_type = infer_expr_type(expr->BinaryExpr.right, has_target, target_type, analyser, allow_untyped);
            }

            expr->BinaryExpr.left->v_type = infer_left_type;
            expr->BinaryExpr.right->v_type = infer_right_type;
            
            
            if(infer_left_type == error_type() || infer_right_type == error_type()) {
                break;
            }
            
            // 处理untyped被兄弟表达式传染成确定类型的情况
            if(is_untyped_type(infer_left_type) && !is_untyped_type(infer_right_type)) {
                infer_left_type = infer_expr_type(expr->BinaryExpr.left, true, infer_right_type, analyser, false);
                expr->BinaryExpr.left->v_type = infer_left_type;
            } else if(!is_untyped_type(infer_left_type) && is_untyped_type(infer_right_type)) {
                infer_right_type = infer_expr_type(expr->BinaryExpr.right, true, infer_left_type, analyser, false);
                expr->BinaryExpr.right->v_type = infer_right_type;
            }
            
            if(is_return_bool_operator(expr->BinaryExpr.op) && (is_untyped_type(infer_left_type) && is_untyped_type(infer_right_type))) {
                infer_left_type = infer_expr_type(expr->BinaryExpr.left, false, NULL, analyser, false);
                infer_right_type = infer_expr_type(expr->BinaryExpr.right, false, NULL, analyser, false);

                TypeRef common_type = get_common_type(infer_left_type, infer_right_type);
                if(common_type == error_type()) {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "两个untyped为操作数的bool二元表达式无法推导出一个共同的类型"
                    );
                    break;
                }
                
                infer_left_type = common_type;
                infer_right_type = common_type;

                expr->BinaryExpr.left->v_type = infer_left_type;
                expr->BinaryExpr.right->v_type = infer_right_type;
            }

            if(infer_left_type == error_type() || infer_right_type == error_type()) {
                break;
            }


            
            TypeRef binary_expr_type = NULL;

            // 处理指针和指针之间的比较
            if(is_pointer_type(infer_left_type) && is_pointer_type(infer_right_type)) {
                
                // 指针类型之间只能比较是否相同
                if(!is_equal_compare_operator(expr->BinaryExpr.op)) {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "only equality comparison is allowed between pointer types"
                    );

                    break;
                }

                // null指针可以和任何指针类型比较
                if(expr->BinaryExpr.left->is_null || expr->BinaryExpr.right->is_null) {
                    should_check_equal_type = false;

                    // should_check_equal_type会绕开把v_type设为左边类型的逻辑
                    // 不过由于是比较操作, 结果类型是bool, 下面有v_type的设置
                    // 最终这里不用设置expr->v_type = 非null指针的类型
                }

            } else if(is_pointer_type(infer_left_type) || is_pointer_type(infer_right_type)) {
                
                // 处理指针和整数的加减法运算
                Ast *pointer_expr = is_pointer_type(infer_left_type) ? expr->BinaryExpr.left : expr->BinaryExpr.right;
                Ast *non_pointer_expr = is_pointer_type(infer_left_type) ? expr->BinaryExpr.right : expr->BinaryExpr.left;
                TypeRef pointer_type = is_pointer_type(infer_left_type) ? infer_left_type : infer_right_type;
                TypeRef non_pointer_type = is_pointer_type(infer_left_type) ? infer_right_type : infer_left_type;

                // 指针类型不能是void指针
                if(get_innermost_type_of_pointer(pointer_type) == easy_type(Type_void)) {
                    context()->reporter.report_error(
                        pointer_expr->span, analyser.curr_ast_file->source_code,
                        "pointer arithmetic is not allowed on void pointer type"
                    );

                    break;
                }

                // 确保另一个是整数类型
                if(!(is_integer_or_untyped_type(non_pointer_type))) {
                    context()->reporter.report_error(
                        non_pointer_expr->span, analyser.curr_ast_file->source_code,
                        "pointer arithmetic requires integer type"
                    );

                    break;
                }

                // 确保操作符是加法或减法
                if(!is_add_sub_operator(expr->BinaryExpr.op)) {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "only addition and subtraction operators are allowed for pointer arithmetic"
                    );

                    break;
                }

                should_check_equal_type = false;
                binary_expr_type = pointer_type;
            }

            

            
            // 如果需要, 检查左右表达式类型是否相等
            if(should_check_equal_type && infer_left_type != infer_right_type) {
                context()->reporter.report_error(
                    expr->span, analyser.curr_ast_file->source_code,
                    "type mismatch between left and right operands of binary expression"
                );

                break;
            }

            // 如果要检查左右表达式类型相等, 那么结果类型就是左边的类型
            if(should_check_equal_type) {
                binary_expr_type = infer_left_type;
            }


            // 检查结构体, 数组类型的运算符
            if((is_struct_type(infer_left_type) && is_struct_type(infer_right_type)) ||
                (is_array_type(infer_left_type) && is_array_type(infer_right_type))) {
                    
                // 结构体、数组类型之间只能比较是否相同
                if(!is_equal_compare_operator(expr->BinaryExpr.op)) {

                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "only equality comparison is allowed between struct/array types"
                    );

                    break;
                }
            }



            // 如果有一个是bool类型, 那么操作符必须是逻辑操作符或等于/不等于
            if(infer_left_type == easy_type(Type_bool) || 
               infer_right_type == easy_type(Type_bool)) {
                if(!is_operator_for_bool(expr->BinaryExpr.op)) {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "bool type operand can only be used with logical not operator and double/! equal operator"
                    );
                    break;
                }
            }

            // 如果操作符是%且类型是浮点类型, 报错
            if(expr->BinaryExpr.op == TokenType::Percent) {
                if(is_float_type(infer_left_type) || is_float_type(infer_right_type)) {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "modulo operator is not allowed for float types"
                    );
                    break;
                }
            }

            // 如果是返回bool类型的操作符, 那么类型就是bool
            // 否则不变
            if(is_return_bool_operator(expr->BinaryExpr.op)) {
                binary_expr_type = easy_type(Type_bool);
            }
            XP_ASSERT_DEFAULT(binary_expr_type != NULL);
            
            result_type = binary_expr_type;
        } break;

        case AstType_UnaryExpr: {

            TokenType op = expr->UnaryExpr.op;

            TypeRef operand_type = infer_expr_type(expr->UnaryExpr.operand, has_target, target_type, analyser, allow_untyped);
            expr->UnaryExpr.operand->v_type = operand_type;


            if(operand_type == error_type()) {
                break;
            }
            

            switch(op) {
            case TokenType::Minus: {
                // 一元负号操作符只能用于整数或浮点类型, 结果类型为操作数类型

                bool is_integer_or_float_type = is_integer_or_untyped_type(operand_type) || is_float_or_untyped_type(operand_type);
                if(is_integer_or_float_type) {
                    result_type = operand_type;
                } else {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "unary minus operator can only be applied to integer or float types"
                    );
                }
            } break;
            case TokenType::Exclamation: {
                // 逻辑非操作符只能用于bool类型, 结果类型为bool类型

                bool is_bool_type = operand_type == easy_type(Type_bool);
                if(is_bool_type) {
                    result_type = easy_type(Type_bool);
                } else {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "logical not operator can only be applied to bool type"
                    );
                }
            } break;
            case TokenType::And: {
                // 取地址操作符只能用于左值, 结果类型为指向操作数类型的指针类型

                if(expr->UnaryExpr.operand->is_lvalue) {
                    result_type = pointer_type(operand_type);
                } else {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "address-of operator can only be applied to lvalue expressions"
                    );
                }
            } break;
            case TokenType::Star: {
                // 解引用操作符只能用于非void*指针类型, 结果类型为操作数指向的类型

                if(is_pointer_type(operand_type) == false) {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "dereference operator can only be applied to pointer types"
                    );
                    break;
                }

                // *void 类型不能解引用
                if(get_pointed_type(operand_type) == easy_type(Type_void)) {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "void pointer type cannot be dereferenced"
                    );
                    break;
                }

                // 设置解引用后的类型
                result_type = get_pointed_type(operand_type);
            } break;

            default:
                UNREACHABLE();
            }

        } break;
        


        case AstType_CastExpr: {
            TypeRef casted_expr_type = infer_expr_type(expr->CastExpr.expr, false, target_type, analyser, false);


            TypeRef casted_type = casted_expr_type;
            TypeRef target_type = expr->CastExpr.target_type;

            if(casted_type == error_type()) {
                result_type = error_type();
                break;
            }

            if(!check_explicit_type_cast(casted_type, target_type)) {
                context()->reporter.report_error(
                    expr->span, analyser.curr_ast_file->source_code,
                    "invalid explicit type cast"
                );
                break;
            }

            
            result_type = target_type;
        } break;

        // TODO FIRST 实现
        case AstType_FieldAccess: {
            expr->FieldAccess.parent->v_type = infer_expr_type(expr->FieldAccess.parent, has_target, target_type, analyser, false);

            
            Ast *parent_ast = expr->FieldAccess.parent;
            TypeRef parent_type = parent_ast->v_type;
            xpString field_name = expr->FieldAccess.field_name;

            if(parent_type == error_type()) {
                break;
            }

            bool is_struct_field_access = false;
            TypeRef parent_struct_type = NULL;

            if(is_package_type(parent_type)) {
                // 表示parent是个包名

                is_struct_field_access = false;
            } else if(is_struct_type(parent_type)) {
                // 结构体变量名
                is_struct_field_access = true;

                parent_struct_type = parent_type;

            } else if(is_pointer_type(parent_type) && is_struct_type(get_pointed_type(parent_type))) {
                // 结构体指针变量名
                is_struct_field_access = true;

                // 获取指针指向的结构体类型, 方便后面检查字段
                parent_struct_type = get_pointed_type(parent_type);
            } else {
                context()->reporter.report_error(
                    parent_ast->span, analyser.curr_ast_file->source_code,
                    "only struct types and package names can be accessed with field access operator"
                );

                break;
            }

            if(is_struct_field_access) {
                
                // 检查a.b中, a有没有b这个字段
                StructField field;
                bool found = false;
                for(isize i = 0; i < parent_struct_type->struct_info.struct_fields.count; i++) {
                    field = parent_struct_type->struct_info.struct_fields[i];
                    if(xp_string_equal(field.name, expr->FieldAccess.field_name)) {
                        found = true;
                        break;
                    }
                }
                if(!found) {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "struct type '%s' does not have field '%s'",
                        parent_struct_type->type_name.c_str, field_name.c_str
                    );
                    break;
                }
    
    
                // 如果是结构体类型, 直接从字段类型赋值
                result_type = field.type;
            } else {
                // 包名.符号名
                
                // TODO: 检查是否有空指针异常风险

                SymbolInfo *field_info = find_symbol_curr(&parent_type->package_info->package_scope, field_name);
                
                if(field_info == NULL) {
                    break;
                }

                if(has_target && is_untyped_type(field_info->value.type)) {
                    // 如果有目标类型, 且field_info的类型是untyped类型, 那就检查能不能从untyped类型转化为目标类型, 不能的话报错
                    
                    if(!check_untyped_to_type(field_info->value, target_type)) {
                        context()->reporter.report_error(
                            expr->span, analyser.curr_ast_file->source_code,
                            "package member '%s' value can't convert to target type",
                            field_name.c_str
                        );
                        break;
                    } else {
                        result_type = target_type;
                    }
                } else {
                    // 否则直接用原始类型

                    result_type = field_info->value.type;
                }


            }


        } break;

        case AstType_StructInitExpr: {
            // 检查是否是结构体类型

            SymbolInfo *struct_type_info = expr->StructInitExpr.struct_type_ident->ast_symbol;

            
            if(struct_type_info == NULL || !(is_type_type(struct_type_info->value.type) && is_struct_type(get_type_value(struct_type_info->value)))) {
                result_type = error_type();
                break;
            }

            Value struct_type_value = struct_type_info->value;
            TypeRef struct_type = get_type_value(struct_type_value);

            // 检查字段数量是否匹配
            isize field_count = struct_type->struct_info.struct_fields.count;
            isize init_sub_expr_count = expr->StructInitExpr.field_inits.count;
            if(field_count != init_sub_expr_count) {
                context()->reporter.report_error(
                    expr->span, analyser.curr_ast_file->source_code,
                    "struct literal field count does not match struct type field count"
                );
                break;
            }

            // 检查字段初始化表达式类型是否和字段类型匹配
            bool has_error = false;
            for(isize i = 0; i < init_sub_expr_count; i++) {
                TypeRef target_field_type = struct_type->struct_info.struct_fields[i].type;

                expr->StructInitExpr.field_inits[i]->v_type = infer_expr_type(expr->StructInitExpr.field_inits[i], true, target_field_type, analyser, false);
                
                TypeRef field_init_type = expr->StructInitExpr.field_inits[i]->v_type;

                if(field_init_type == error_type()) {
                    result_type = error_type();
                    has_error = true;
                }

                if(!check_implicit_convension(expr->StructInitExpr.field_inits[i], field_init_type, target_field_type)) {
                    context()->reporter.report_error(
                        expr->StructInitExpr.field_inits[i]->span, analyser.curr_ast_file->source_code,
                        "struct literal field type does not match struct type field type"
                    );
                    has_error = true;
                }
            }
            if(has_error) {
                break;
            }

            result_type = struct_type;
        } break;

        // TODO 数组字面量类型检查+推导
        case AstType_ArrayInitExpr: {

            isize init_element_count = expr->ArrayInitExpr.elements.count;

            // 数组字面量的元素个数不能为0
            if(init_element_count == 0) {
                context()->reporter.report_error(
                    expr->span, analyser.curr_ast_file->source_code,
                    "array literal must have at least one element"
                );
                break;
            }
            
            TypeRef target_elem_type = NULL;
            if(has_target) {
                // 如果有目标类型

                // target_type必须是数组类型
                if(!is_array_type(target_type)) {
                    break;
                }
                
                target_elem_type = target_type->array_info.element_type;
                usize target_count = target_type->array_info.count;

                // 检查元素数量是否匹配
                if(target_count != cast(usize)(init_element_count)) {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "array literal element count does not match target array type element count"
                    );
                    break;
                }
            } else {
                // 如果没有目标类型
                // 先找有没有确定类型的元素
                TypeRef element_type = undefined_type();
                bool found_certain_type = false;
                for(isize i = 0; i < init_element_count; i++) {
                    TypeRef elem_type = expr->ArrayInitExpr.elements[i]->v_type;

                    if(is_certain_type(elem_type)) {
                        found_certain_type = true;
                        element_type = expr->ArrayInitExpr.elements[i]->v_type;
                        break;
                    }
                }
                if(!found_certain_type) {
                    // 如果没有确定类型的元素, 那就用第一个元素推导类型
                    expr->ArrayInitExpr.elements[0]->v_type = infer_expr_type(expr->ArrayInitExpr.elements[0], false, target_type, analyser, false);
                    element_type = expr->ArrayInitExpr.elements[0]->v_type;
                    if(element_type == error_type()) {
                        break;
                    }
                    
                }
                target_elem_type = element_type;
                // TODO 这里如果没有确定类型的元素, 会多执行一次推导(因为要获取第一个元素类型, 提前推导), 可以优化
            }

            // 推导其他元素类型
            bool has_error = false;
            for(isize i = 0; i < init_element_count; i++) {
                expr->ArrayInitExpr.elements[i]->v_type = infer_expr_type(expr->ArrayInitExpr.elements[i], true, target_elem_type, analyser, false);

                TypeRef elem_expr_type = expr->ArrayInitExpr.elements[i]->v_type;
                if(elem_expr_type == error_type()) {
                    has_error = true;
                    continue;
                }

                if(!check_implicit_convension(expr->ArrayInitExpr.elements[i], elem_expr_type, target_elem_type)) {
                    context()->reporter.report_error(
                        expr->ArrayInitExpr.elements[i]->span, analyser.curr_ast_file->source_code,
                        "array literal element type does not match other element type"
                    );
                    has_error = true;
                }
            }
            if(has_error) {
                break;
            }

            result_type = array_type(target_elem_type, init_element_count);
        } break;

        case AstType_IndexExpr: {
            expr->IndexExpr.array_var_expr->v_type = infer_expr_type(expr->IndexExpr.array_var_expr, false, NULL, analyser, false);
            expr->IndexExpr.index_expr->v_type = infer_expr_type(expr->IndexExpr.index_expr, false, NULL, analyser, false);


            // 检查被下标访问表达式是不是: 
            // 1.数组类型 
            // 2.切片类型
            // 3.字符串类型
            TypeRef as_type = expr->IndexExpr.array_var_expr->v_type;
            if(!is_array_type(as_type) && !is_slice_struct_type(as_type) && !is_string_struct_type(as_type)) {
                context()->reporter.report_error(
                    expr->IndexExpr.array_var_expr->span, analyser.curr_ast_file->source_code,
                    "expression cannot be indexed because it is not an array, slice struct or string struct type"
                );
                break;
            }

            // 检查下标表达式是不是整数类型
            TypeRef index_type = expr->IndexExpr.index_expr->v_type;
            if(!is_integer_type(index_type)) {
                context()->reporter.report_error(
                    expr->IndexExpr.index_expr->span, analyser.curr_ast_file->source_code,
                    "index expression must be of integer type"
                );
                break;
            }

            // 检查常量表达式下标的范围是否越界
            // 首先判断下标表达式是不是常量表达式
            // 如果是, 就做常量折叠, 判断是否在范围内
            // 如果不是, 就不管

            // if(is_array_type(as_type)) {

            //     if(expr->IndexExpr.index_expr->is_const_expr) {
            //         if(!try_constant_expr_folding(expr->IndexExpr.index_expr)) {
            //             context()->reporter.report_error(
            //                 expr->IndexExpr.index_expr->span, analyser.curr_ast_file->source_code,
            //                 "溢出的常量表达式无法作为数组下标"
            //             );
            //             break;
            //         }
    
            //         if(expr->IndexExpr.index_expr->Constant.value < 0 || 
            //            expr->IndexExpr.index_expr->Constant.value >= cast(i128)as_type->array_info.count) {
            //             context()->reporter.report_error(
            //                 expr->IndexExpr.index_expr->span, analyser.curr_ast_file->source_code,
            //                 "index expression is out of bounds for array type"
            //             );
            //             break;
            //         }
            //     }

            // }
            
            // 设置下标表达式的类型
            if(is_array_type(as_type)) {
                
                    
                result_type = as_type->array_info.element_type;
            } else if(is_slice_struct_type(as_type)) {


                result_type = as_type->struct_info.struct_fields[0].type->pointed_type;
            } else if(is_string_struct_type(as_type)) {

                
                result_type = as_type->struct_info.struct_fields[0].type->pointed_type;
            } else {
                UNREACHABLE();
            }
        } break;


        case AstType_StringLiteralExpr: {
            result_type = string_type_as_struct();

        } break;


        case AstType_Ident: {
            SymbolInfo *info = expr->ast_symbol;
            if(info == NULL) {
                result_type = error_type();
                break;
            }

            /*
             * 如果是无类型的标识符, 需要根据上下文推导类型
             * 如果有目标类型, 就用目标类型推导
             * 如果没有目标类型, 就用标识符的值推导
            */
            if(is_untyped_type(info->value.type)) {


                if(has_target) {
                    if(!check_untyped_to_type(info->value, target_type)) {
                        context()->reporter.report_error(
                            expr->span, analyser.curr_ast_file->source_code,
                            "identifier '%s' of untyped type cannot be used as target type '%s'",
                            info->name.c_str, target_type->type_name.c_str
                        );
                        result_type = error_type();
                    } else {
                        result_type = target_type;
                    }
                } else if(!allow_untyped) {
                    result_type = get_compliable_const_type(info->value);
                    if(result_type == error_type()) {
                        context()->reporter.report_error(
                            expr->span, analyser.curr_ast_file->source_code,
                            "unable to infer type of identifier '%s' with untyped value",
                            info->name.c_str
                        );
                    }
                } else {
                    result_type = info->value.type;
                }

            } else {
                result_type = info->value.type;
            }


        } break;
        case AstType_FunctionCallExpr: {
            SymbolInfo *info = expr->FunctionCallExpr.func_ident->ast_symbol;
            if(info == NULL) {
                context()->reporter.report_error(
                    expr->span, analyser.curr_ast_file->source_code,
                    "function not found"
                );
                break;
            }

            TypeRef val_type = info->value.type;

            if(!is_function_type(val_type)) {
                context()->reporter.report_error(
                    expr->span, analyser.curr_ast_file->source_code,
                    "called symbol '%s' is not a function",
                    info->name.c_str
                );
                break;
            }

            if(val_type->function_info.param_types.count != expr->FunctionCallExpr.args.count) {
                context()->reporter.report_error(
                    expr->span, analyser.curr_ast_file->source_code,
                    "function '%s' expects %lld arguments but got %lld",
                    info->name.c_str, val_type->function_info.param_types.count, expr->FunctionCallExpr.args.count
                );
                break;
            }

            for(isize i = 0; i < expr->FunctionCallExpr.args.count; i++) {
                infer_expr_type(expr->FunctionCallExpr.args[i], true, val_type->function_info.param_types[i], analyser, false);
            }
            

            
            result_type = info->value.type->function_info.return_type;
        } break;



        // 错误表达式类型, 直接把类型设为error_type
        case AstType_BadExpr: {
            result_type = error_type();
        } break;

        default: {
            UNREACHABLE();
        } break;
    }


    // 设置是否是左值
    expr->is_lvalue = check_is_lvalue(expr, result_type, analyser);



    if(depth == 1) {
        if(result_type == error_type()) {
            // context()->reporter.report_error(
            //     expr->span, analyser.curr_ast_file->source_code,
            //     "expression has type error"
            // );


        } else if(result_type == undefined_type()) {
            // 只有包名才会是undefined_type

            context()->reporter.report_error(
                expr->span, analyser.curr_ast_file->source_code,
                "package name cannot be used as a value"
            );
            result_type = error_type();

        } else if(has_target) {
            if(!check_implicit_convension(expr, result_type, target_type)) {
                context()->reporter.report_error(
                    expr->span, analyser.curr_ast_file->source_code,
                    "expression type does not match target type"
                );
            }
        }
    }
    
    depth -= 1;
    
    expr->v_type = result_type;
    return result_type;
}



