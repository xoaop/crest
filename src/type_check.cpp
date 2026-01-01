#include "type_check.hpp"

#include "error_msg.hpp"

bool fit_in_type(i128 value, Type target_type) {
    bool fit = false;

    // i64 high = cast(i64)(value >> 64);
    // i64 low = cast(i64)(value & 0xFFFFFFFFFFFFFFFF);

    // printf("Checking if value fits in type: ");
    // print_i128(value);
    // print_type(target_type);
    // printf("\n");


    switch(target_type.kind) {
        case Type_i8:
            fit = (value >= INT8_MIN && value <= INT8_MAX);
            break;
        case Type_i32:
            fit = (value >= INT32_MIN && value <= INT32_MAX);
            break;
        case Type_i64:
            fit = (value >= INT64_MIN && value <= INT64_MAX);
            break;
        case Type_u8:
            fit = (value >= 0 && value <= UINT8_MAX);
            break;
        case Type_u32:
            fit = (value >= 0 && value <= UINT32_MAX);
            break;
        case Type_u64:
            fit = (value >= 0 && value <= UINT64_MAX);
            break;
        default:
            // TODO 
            print_type(target_type);
            printf("\n");
            XP_ASSERT_DEFAULT(0);
            break;
    }

    return fit;
}

bool fit_in_type(double value, Type target_type) {
    bool fit = false;
    switch(target_type.kind) {
        case Type_f32:
            fit = (value >= -3.4028235e+38 && value <= 3.4028235e+38);
            break;
        case Type_f64:
            fit = true;
            break;
        default:
            XP_ASSERT_DEFAULT(0);
            break;
    }
    return fit;
}

bool fit_in_type(Ast *constant, Type target_type) {
    XP_ASSERT_DEFAULT(constant->type == AstType_Constant);

    if(is_integer_type(constant->v_type)) {
        return fit_in_type(constant->Constant.value, target_type);
    } else if(is_float_type(constant->v_type)) {
        return fit_in_type(constant->Constant.float_value, target_type);
    } else {
        XP_ASSERT_DEFAULT(0);
        return false;
    }
}


Type get_compliable_integer_type(i128 value) {

    if(fit_in_type(value, make_type(Type_i32))) {
        return make_type(Type_i32);
    } else if(fit_in_type(value, make_type(Type_i64))) {
        return make_type(Type_i64);
    } else if(fit_in_type(value, make_type(Type_u64))) {
        return make_type(Type_u64);
    }


    // TODO 超出范围错误处理
    XP_ASSERT_DEFAULT(0);
    return make_type(Type_Undefined);
}

Type get_compliable_float_type(double value) {

    if(fit_in_type(value, make_type(Type_f32))) {
        return make_type(Type_f32);
    } else {
        return make_type(Type_f64);
    }

}

Type get_compliable_const_type(Ast *constant) {
    XP_ASSERT_DEFAULT(constant->type == AstType_Constant);

    if(is_equal_type(constant->v_type, make_type(Type_literal))) {
        return get_compliable_integer_type(constant->Constant.value);
    } else if(is_equal_type(constant->v_type, make_type(Type_literal_float))) {
        return get_compliable_float_type(constant->Constant.float_value);
    } else {
        XP_ASSERT_DEFAULT(0);
        return make_type(Type_Undefined);
    }
}




// TODO 用来替代infer_expr_type
void infer_type_new(Ast *expr, bool has_target, Type target_type, Analyser *analyser) {
    switch(expr->type) {

        case AstType_Constant: {
            if(is_certain_type(expr->v_type)) {
                break;
            }

            // example: x : i32 = true;  // target_type = i32, has_target = true, expr->v_type = bool
            if(is_equal_type(expr->v_type, make_type(Type_bool)) && has_target && !is_equal_type(target_type, make_type(Type_bool))) {
                XP_ASSERT_DEFAULT(0);
            }

            // example: x : bool = 1;  // target_type = bool, has_target = true, expr->v_type = literal
            if(!is_equal_type(expr->v_type, make_type(Type_bool)) && has_target && is_equal_type(target_type, make_type(Type_bool))) {
                XP_ASSERT_DEFAULT(0);
            }


            if(has_target) {
                if(is_equal_type(expr->v_type, make_type(Type_literal))) {
                    if(!fit_in_type(expr->Constant.value, target_type)) {
                        error_msg(&expr->token, "constant value overflowed for target type");
                        XP_ASSERT_DEFAULT(0);
                    }
                } else if(is_equal_type(expr->v_type, make_type(Type_literal_float))) {
                    if(!fit_in_type(expr->Constant.float_value, target_type)) {
                        XP_ASSERT_DEFAULT(0);
                    }
                }

                expr->v_type = target_type;
            } else {
                if(is_equal_type(expr->v_type, make_type(Type_literal))) {
                    expr->v_type = get_compliable_integer_type(expr->Constant.value);
                } else if(is_equal_type(expr->v_type, make_type(Type_literal_float))) {
                    expr->v_type = get_compliable_float_type(expr->Constant.float_value);
                }
            }

            
        } break;


        case AstType_BinaryExpr: {
            Type old_left_type = expr->BinaryExpr.left->v_type;
            Type old_right_type = expr->BinaryExpr.right->v_type;

            if(is_certain_type(old_left_type) && is_certain_type(old_right_type)) {
                infer_type_new(expr->BinaryExpr.left, has_target, target_type, analyser);
                infer_type_new(expr->BinaryExpr.right, has_target, target_type, analyser);

                
            } else if(!is_certain_type(old_left_type) && !is_certain_type(old_right_type)) {
                // 一个是Type_literal, 另一个是Type_literal_float
                if(!is_equal_type(old_left_type, old_right_type)) {
                    XP_ASSERT_DEFAULT(0);
                }

                infer_type_new(expr->BinaryExpr.left, false, target_type, analyser);
                infer_type_new(expr->BinaryExpr.right, false, target_type, analyser);

                Type common_type = get_common_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type);
                expr->BinaryExpr.left->v_type = common_type;
                expr->BinaryExpr.right->v_type = common_type;
            
            } else if(!is_certain_type(old_left_type) || !is_certain_type(old_right_type)) {
                // 一个是确定类型, 另一个不是

                // 确定哪个是Type_literal, 哪个不是
                Ast *literal_expr = expr->BinaryExpr.left;
                Ast *not_literal_expr = expr->BinaryExpr.right;
                if(is_certain_type(literal_expr->v_type)) {
                    Ast *t = literal_expr;
                    literal_expr = not_literal_expr;
                    not_literal_expr = t;
                }

                infer_type_new(not_literal_expr, has_target, target_type, analyser);
                infer_type_new(literal_expr, false, {}, analyser);


                if(!fit_in_type(literal_expr, not_literal_expr->v_type)) {
                    // TODO 常量溢出错误处理
                    XP_ASSERT_MSG(0, "constant overflowed");
                }
                literal_expr->v_type = not_literal_expr->v_type;
            }

            // 左右表达式类型得相同
            if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
                XP_ASSERT_DEFAULT(0);
            }

            // 如果有一个是bool类型, 那么操作符必须是逻辑操作符或等于/不等于
            if(is_equal_type(expr->BinaryExpr.left->v_type, make_type(Type_bool)) || 
               is_equal_type(expr->BinaryExpr.right->v_type, make_type(Type_bool))) {
                XP_ASSERT_DEFAULT(is_operator_for_bool(expr->BinaryExpr.op));
            }

            // 如果操作符是%且类型是浮点类型, 报错
            if(expr->BinaryExpr.op == TokenType::Percent) {
                if(is_float_type(expr->BinaryExpr.left->v_type)) {
                    // TODO 浮点类型不能使用取模操作符错误处理
                    XP_ASSERT_MSG(0, "modulus operator can not be used with float type");
                }
            }

            // 确定该BinaryExpr的类型
            // 如果是返回bool类型的操作符, 那么类型就是bool
            // 否则就是左右表达式的类型
            if(is_return_bool_operator(expr->BinaryExpr.op)) {
                expr->v_type = make_type(Type_bool);
            } else {
                expr->v_type = expr->BinaryExpr.left->v_type;
            }

            
        } break;

        case AstType_UnaryExpr: {

            // TODO 考虑是否需要
            // 这个提前返回
            // if(expr->UnaryExpr.op == TokenType::Exclamation) {
            //     // 逻辑非运算符, 输入和输出都是bool类型
            
            //     Type bool_type = make_type(Type_bool);
            //     infer_type_new(expr->UnaryExpr.operand, true, bool_type, analyser);
            //     if(!is_equal_type(expr->UnaryExpr.operand->v_type, bool_type)) {
                //         // TODO 逻辑非运算符操作数类型错误处理
                //         XP_ASSERT_MSG(0, "logical not operator operand must be bool type");
                //     }
                
                //     expr->v_type = bool_type;
                //     break;
                // }
                
                // 特殊处如-128(i8)这种情况, 不能简单地把 128 作为常量处理, 不然会被推导为高一级别的类型
                if(expr->UnaryExpr.op == TokenType::Minus && !is_certain_type(expr->UnaryExpr.operand->v_type)) {
                    Type inferred_type;
    
                    if(is_equal_type(expr->UnaryExpr.operand->v_type, make_type(Type_literal))) {
                        i128 val = expr->UnaryExpr.operand->Constant.value;
                        i128 neg_val = -val;
                        
                        // 重新推导类型
                        if(has_target) {
                            inferred_type = target_type;
                        } else {
                            inferred_type = get_compliable_integer_type(neg_val);
                        }
                        
                        if(fit_in_type(neg_val, inferred_type)) {
                            expr->UnaryExpr.operand->Constant.value = val;
                            expr->UnaryExpr.operand->v_type = inferred_type;
                        } else {
                            // TODO 常量溢出错误处理
                            XP_ASSERT_MSG(0, "constant overflowed");
                        }
                        
                    }
    
                    if(is_equal_type(expr->UnaryExpr.operand->v_type, make_type(Type_literal_float))) {
                        double val = expr->UnaryExpr.operand->Constant.float_value;
                        double neg_val = -val;
    
                        if(has_target) {
                            inferred_type = target_type;
                        } else {
                            inferred_type = get_compliable_float_type(neg_val);
                        }
                        if(fit_in_type(neg_val, inferred_type)) {
                            expr->UnaryExpr.operand->Constant.float_value = val;
                            expr->UnaryExpr.operand->v_type = inferred_type;
                        } else {
                            // TODO 常量溢出错误处理
                            XP_ASSERT_MSG(0, "constant overflowed");
                        }

                    }
                    
                } else {
                    // 普遍推导操作数类型
                    infer_type_new(expr->UnaryExpr.operand, has_target, target_type, analyser);
                    expr->v_type = expr->UnaryExpr.operand->v_type;

                }
            
            
            // 如果操作数是bool类型, 那么操作符必须是逻辑非或等于/不等于
            if(is_equal_type(expr->UnaryExpr.operand->v_type, make_type(Type_bool))) {
                XP_ASSERT_MSG(is_operator_for_bool(expr->UnaryExpr.op), "bool type operand can only be used with logical not operator and double/! equal operator");
            }
            



            if(is_return_bool_operator(expr->UnaryExpr.op)) {
                expr->v_type = make_type(Type_bool);
            } else {
                expr->v_type = expr->UnaryExpr.operand->v_type;
            }
           
        } break;
        


        case AstType_CastExpr: {
            infer_type_new(expr->CastExpr.expr, false, target_type, analyser);

            if(!is_equal_type(expr->CastExpr.expr->v_type, make_type(Type_bool)) && is_equal_type(expr->CastExpr.target_type, make_type(Type_bool))) {
                // TODO 错误处理 非bool类型转换为bool类型
                XP_ASSERT_MSG(0, "only bool type can be casted to bool type");
            }

            expr->v_type = expr->CastExpr.target_type;
        } break;

        case AstType_VarExpr: {
            SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->VarExpr.name);
            expr->v_type = info->type;
        } break;
        case AstType_FunctionCallExpr: {
            SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->FunctionCallExpr.name);
            expr->v_type = *info->type.function_info.return_type;
        } break;


        default: {
            XP_ASSERT_DEFAULT(0);
        } break;
    }
}