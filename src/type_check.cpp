#include "type_check.hpp"


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


// // TODO TEST
// void infer_expr_type_without_target(Ast *expr, Analyser *analyser) {
//     switch(expr->type)
//     {
//     case AstType_Constant: {
//         if(is_equal_type(expr->v_type, make_type(Type_literal))) {
//             expr->v_type = get_compliable_integer_type(expr->Constant.value);
//         }
//     } break;
    
//     case AstType_BinaryExpr: {
//         Type old_left_type = expr->BinaryExpr.left->v_type;
//         Type old_right_type = expr->BinaryExpr.right->v_type;

//         infer_expr_type_without_target(expr->BinaryExpr.left, analyser);
//         infer_expr_type_without_target(expr->BinaryExpr.right, analyser);

//         if(old_left_type.kind != Type_literal && old_right_type.kind != Type_literal) {
//             // NOTE: 这个应该只要判断类型是否一样即可

//         } else if((old_left_type.kind == Type_literal && old_right_type.kind != Type_literal) || (old_left_type.kind != Type_literal && old_right_type.kind == Type_literal)) {
//             // 确定哪个是Type_literal, 哪个不是
//             Ast *literal_expr = expr->BinaryExpr.left;
//             Ast *not_literal_expr = expr->BinaryExpr.right;
//             if(old_right_type.kind == Type_literal) {
//                 Ast *t = literal_expr;
//                 literal_expr = not_literal_expr;
//                 not_literal_expr = t;
//             }

            
//             if(!fit_in_type(literal_expr->Constant.value, not_literal_expr->v_type)) {
//                 // TODO 常量溢出错误处理
//                 XP_ASSERT_MSG(0, "constant overflowed");
//             }
//             literal_expr->v_type = not_literal_expr->v_type;

//         } else if(old_left_type.kind == Type_literal && old_right_type.kind == Type_literal) {
//             // TODO 把两个转化为较大类型的
//         }


//         if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
//             XP_ASSERT_DEFAULT(0);
//         }

//         if(is_equal_type(expr->BinaryExpr.left->v_type, make_type(Type_bool)) || 
//            is_equal_type(expr->BinaryExpr.right->v_type, make_type(Type_bool))) {
//             XP_ASSERT_DEFAULT(is_operator_for_bool(expr->BinaryExpr.op));
//         }


//         if(is_return_bool_operator(expr->BinaryExpr.op)) {
//             expr->v_type = make_type(Type_bool);
//         } else {
//             expr->v_type = expr->BinaryExpr.left->v_type;
//         }

//     } break;
    
//     case AstType_UnaryExpr: {
//         Type old_type = expr->UnaryExpr.operand->v_type;

//         infer_expr_type_without_target(expr->UnaryExpr.operand, analyser);
//         expr->v_type = expr->UnaryExpr.operand->v_type;

//         if(expr->UnaryExpr.op == TokenType::Minus) {
//             if(is_equal_type(old_type, make_type(Type_literal))) {
                
//                 i128 val = expr->UnaryExpr.operand->Constant.value;
//                 i128 neg_val = -val;
                
//                 // 重新推导类型
//                 Type inferred_type = get_compliable_integer_type(neg_val);
                
//                 if(fit_in_type(neg_val, inferred_type)) {
//                     expr->v_type = inferred_type;
//                     expr->UnaryExpr.operand->v_type = inferred_type;
//                 } else {
//                     // TODO 常量溢出错误处理
//                     XP_ASSERT_MSG(0, "constant overflowed");
//                 }
    
//             }
//         } else if(expr->UnaryExpr.op == TokenType::Exclamation) {
//             // 逻辑非运算符, 输入和输出都是bool类型

//             Type bool_type = make_type(Type_bool);
//             infer_expr_type_without_target(expr->UnaryExpr.operand, analyser);
//             if(!is_equal_type(expr->UnaryExpr.operand->v_type, bool_type)) {
//                 // TODO 逻辑非运算符操作数类型错误处理
//                 XP_ASSERT_MSG(0, "logical not operator operand must be bool type");
//             }

//             expr->v_type = bool_type;
//         }

//     } break;


//     case AstType_CastExpr: {
//         expr->v_type = expr->CastExpr.target_type;

//         infer_expr_type_without_target(expr->CastExpr.expr, analyser);

//         if((!is_equal_type(expr->CastExpr.expr->v_type, make_type(Type_bool)) && is_equal_type(expr->CastExpr.target_type, make_type(Type_bool))) || (is_equal_type(expr->CastExpr.expr->v_type, make_type(Type_bool)) && !is_equal_type(expr->CastExpr.target_type, make_type(Type_bool)))) {
//             XP_ASSERT_DEFAULT(0);
//         }

//     } break;

//     // 别的expr类型确定了, 不用推导
//     case AstType_VarExpr: {
//         SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->VarExpr.name);
//         expr->v_type = info->type;
//     } break;
//     case AstType_FunctionCallExpr: {
//         SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->FunctionCallExpr.name);
//         expr->v_type = *info->type.function_info.return_type;
//     } break;
    

//     default:
//         XP_ASSERT_DEFAULT(0);
//         break;
//     }
// }

// // TODO 完成
// void check_type(Ast *expr, Type target_type, Analyser *analyser) {
//     switch(expr->type)
//     {
//     case AstType_Constant: {
//         if(expr->v_type.kind == Type_literal) {
//             // 防止Type_literal转化成Type_bool
//             if(is_equal_type(target_type, make_type(Type_bool))) {
//                 XP_ASSERT_MSG(0, "Type_literal can not be Type_bool");
//             }

//             // 尝试把Type_literal值放入target_type
//             if(!fit_in_type(expr->Constant.value, target_type)) {
//                 // TODO 常量溢出错误处理
//                 XP_ASSERT_MSG(0, "constant overflowed");
//             } 
//         }
//     } break;
    
//     case AstType_BinaryExpr: {

//         TokenType op = expr->BinaryExpr.op;
//         Ast *left_expr = expr->BinaryExpr.left;
//         Ast *right_expr = expr->BinaryExpr.right;

//         Type left_type = expr->BinaryExpr.left->v_type;
//         Type right_type = expr->BinaryExpr.right->v_type;


//         if((left_type.kind != Type_literal && right_type.kind == Type_literal) || 
//             (right_type.kind != Type_literal && left_type.kind == Type_literal)) {
//             // *DONE!

//             // 确定哪个是Type_literal, 哪个不是
//             Ast *literal_expr = left_expr;
//             Ast *not_literal_expr = right_expr;
//             if(not_literal_expr->v_type.kind == Type_literal) {
//                 Ast *t = literal_expr;
//                 literal_expr = not_literal_expr;
//                 not_literal_expr = t;
//             }


//             check_type(not_literal_expr, target_type, analyser);

//             if(!fit_in_type(literal_expr->Constant.value, not_literal_expr->v_type)) {
//                 // TODO 常量溢出错误处理
//                 XP_ASSERT_MSG(0, "constant overflowed");
//             }

//             literal_expr->v_type = not_literal_expr->v_type;
//             expr->v_type = not_literal_expr->v_type;

//         } else if(left_type.kind == Type_literal && right_type.kind == Type_literal) {
//             // *DONE!

//             if(is_equal_type(target_type, make_type(Type_bool))) {

//                 // TODO 换成自动转化为大的类型
//                 expr->BinaryExpr.left->v_type = get_compliable_integer_type(expr->BinaryExpr.left->Constant.value);
//                 expr->BinaryExpr.right->v_type = get_compliable_integer_type(expr->BinaryExpr.right->Constant.value);
//                 if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
//                     XP_ASSERT_MSG(0, "binary expr type mismatch in infer_expr_type");
//                 }
//             } else {
//                 check_type(expr->BinaryExpr.left, target_type, analyser);
//                 check_type(expr->BinaryExpr.right, target_type, analyser);

//                 // TODO 换成自动转化为大的类型
//                 if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
//                     XP_ASSERT_MSG(0, "binary expr type mismatch in infer_expr_type");
//                 }
//             }

//             expr->v_type = expr->BinaryExpr.left->v_type;

//         } else if(left_type.kind != Type_literal && right_type.kind != Type_literal) {



//             check_type(expr->BinaryExpr.left, target_type, analyser);
//             check_type(expr->BinaryExpr.right, target_type, analyser);

//             if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
//                 XP_ASSERT_MSG(0, "binary expr type mismatch in infer_expr_type");
//             }

//             expr->v_type = expr->BinaryExpr.left->v_type;
//         }

        
//         if(expr->BinaryExpr.left->v_type.kind == Type_bool) {
//             // TODO 逻辑运算符只能操作bool类型
//             XP_ASSERT_MSG(is_operator_for_bool(expr->BinaryExpr.op), "bool type operands can only be used with logical operators and double/! equal operator");
//         }


//         // TODO TEST
//         if(is_return_bool_operator(expr->BinaryExpr.op)) {
//             expr->v_type = make_type(Type_bool);
//         }
//     } break;

//     case AstType_UnaryExpr: {

//     } break;

//     // *无视target_type, 因为CastExpr的子表达式的类型无论如何都是要被转化为CastExpr.target_type
//     case AstType_CastExpr: {
//         infer_expr_type_without_target(expr->CastExpr.expr, analyser);

//         // 限制cast能力
//         if(!is_equal_type(expr->CastExpr.expr->v_type, make_type(Type_bool)) && is_equal_type(expr->CastExpr.target_type, make_type(Type_bool))) {
//             // TODO 错误处理 非bool类型转换为bool类型
//             XP_ASSERT_MSG(0, "only bool type can be casted to bool type");
//         }

//         expr->v_type = expr->CastExpr.target_type;
//     } break;

//     // *无视target_type
//     case AstType_VarExpr: {
//         SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->VarExpr.name);
//         expr->v_type = info->type;
//     } break;
//     case AstType_FunctionCallExpr: {
//         SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->FunctionCallExpr.name);
//         expr->v_type = *info->type.function_info.return_type;
//     } break;

//     default:
//         break;
//     }

// }







// // 这个函数用来尝试递归地让表达式迎合target_type
// // TODO 重构, 现在这个函数太乱
// void infer_expr_type(Ast *expr, Type *target_type, Analyser *analyser) {
//     static i32 recurisve_depth = 0;

//     recurisve_depth++;

//     switch(expr->type) {

//         /* AstType_Constant目前有两种类型
//             1. Type_literial: 未确定的整数， 可能是i8-64, u8-64 类型 它们需要推导
//             2. Type_bool: true、false 不需要推导
//             3. Type_i/u 8-64: 这里不应该出现
//         */ 
//         case AstType_Constant: {

//             /*
//                 1. 对于Type_literal, 如果有target_type, 尝试把其放入target_type
//                 Type_literial可以是i8-64, u8-64 类型, 不可以是bool
            
//             */
//             if(expr->v_type.kind == Type_literal) {
//                 if(target_type != NULL) {

//                     // 1. Type_literal不可以是bool
//                     if(is_equal_type(*target_type, make_type(Type_bool))) {
//                         XP_ASSERT_MSG(0, "Type_literal can not be Type_bool");
//                     }
//                     // 2. Type_literal要放的进target_type
//                     if(!fit_in_type(expr->Constant.value, *target_type)) {
//                         // TODO 常量溢出错误处理
//                         XP_ASSERT_MSG(0, "constant overflowed");
//                     } 

//                     expr->v_type = *target_type;

//                 } else {
//                     expr->v_type = get_compliable_integer_type(expr->Constant.value);
//                 }
//             } else if(expr->v_type.kind == Type_bool) {
//                 // 无需处理
//             } else {
//                 XP_ASSERT_MSG(0, "Constant must be Type_literal or Type_bool in infer_expr_type");
//             }
//         } break;


//         // 如果两个子表达式都是Type_literal 不能无视target_type
//         case AstType_BinaryExpr: {


//             Type left_type = expr->BinaryExpr.left->v_type;
//             Type right_type = expr->BinaryExpr.right->v_type;


//             if((left_type.kind != Type_literal && right_type.kind == Type_literal) || 
//                (right_type.kind != Type_literal && left_type.kind == Type_literal)) {

//                 // 确定哪个是Type_literal, 哪个不是
//                 Ast *literal_expr = expr->BinaryExpr.left;
//                 Ast *not_literal_expr = expr->BinaryExpr.right;
//                 if(not_literal_expr->v_type.kind == Type_literal) {
//                     Ast *t = literal_expr;
//                     literal_expr = not_literal_expr;
//                     not_literal_expr = t;
//                 }

//                 infer_expr_type(not_literal_expr, target_type, analyser);

//                 if(!fit_in_type(literal_expr->Constant.value, not_literal_expr->v_type)) {
//                     // TODO 常量溢出错误处理
//                     XP_ASSERT_MSG(0, "constant overflowed");
//                 }
//                 literal_expr->v_type = not_literal_expr->v_type;
//                 expr->v_type = not_literal_expr->v_type;

//             } else if(left_type.kind == Type_literal && right_type.kind == Type_literal) {
//                 if(target_type == NULL) {
//                     infer_expr_type(expr->BinaryExpr.left, target_type, analyser);
//                     infer_expr_type(expr->BinaryExpr.right, target_type, analyser);

//                     // TODO 换成自动转化为大的类型
//                     if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
//                         XP_ASSERT_MSG(0, "binary expr type mismatch in infer_expr_type");
//                     }
//                 } else if(target_type != NULL) {
//                     if(is_equal_type(*target_type, make_type(Type_bool))) {
                        
//                         // TODO 换成自动转化为大的类型
//                         expr->BinaryExpr.left->v_type = get_compliable_integer_type(expr->BinaryExpr.left->Constant.value);
//                         expr->BinaryExpr.right->v_type = get_compliable_integer_type(expr->BinaryExpr.right->Constant.value);
//                         if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
//                             XP_ASSERT_MSG(0, "binary expr type mismatch in infer_expr_type");
//                         }
//                     } else {
//                         infer_expr_type(expr->BinaryExpr.left, target_type, analyser);
//                         infer_expr_type(expr->BinaryExpr.right, target_type, analyser);

//                         // TODO 换成自动转化为大的类型
//                         if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
//                             XP_ASSERT_MSG(0, "binary expr type mismatch in infer_expr_type");
//                         }
//                     }

//                 }
//                 expr->v_type = expr->BinaryExpr.left->v_type;

//             } else if(left_type.kind != Type_literal && right_type.kind != Type_literal) {
//                 infer_expr_type(expr->BinaryExpr.left, target_type, analyser);
//                 infer_expr_type(expr->BinaryExpr.right, target_type, analyser);

//                 if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
//                     print_ast(expr);
//                     XP_ASSERT_MSG(0, "binary expr type mismatch in infer_expr_type");
//                 }

//                 expr->v_type = expr->BinaryExpr.left->v_type;
//             }

            
//             if(expr->BinaryExpr.left->v_type.kind == Type_bool) {
//                 // TODO 逻辑运算符只能操作bool类型
//                 XP_ASSERT_MSG(is_operator_for_bool(expr->BinaryExpr.op), "bool type operands can only be used with logical operators and double/! equal operator");
//             }


//             // TODO TEST
//             if(is_return_bool_operator(expr->BinaryExpr.op)) {
//                 expr->v_type = make_type(Type_bool);
//             }
            
//         } break;

//         case AstType_UnaryExpr: {
            
            
//             // NOTE: 注意单独处理如 -128(i8) 这种情况,  不能简单地把 128 作为常量处理, 不然会被推导为高一级别的类型
//             // TODO 特殊处理负号
//             if(expr->UnaryExpr.op == TokenType::Minus) {
//                 if(expr->UnaryExpr.operand->type == AstType_Constant) {
//                     if(expr->UnaryExpr.operand->v_type.kind == Type_literal) {

//                         i128 val = expr->UnaryExpr.operand->Constant.value;
//                         i128 neg_val = -val;
                        
//                         // 重新推导类型
//                         Type inferred_type;
//                         if(target_type != NULL) {
//                             inferred_type = *target_type;
//                         } else {
//                             inferred_type = get_compliable_integer_type(neg_val);
//                         }
                        
//                         if(fit_in_type(neg_val, inferred_type)) {
//                             expr->v_type = inferred_type;
//                             expr->UnaryExpr.operand->Constant.value = val;
//                             expr->UnaryExpr.operand->v_type = inferred_type;
//                         } else {
//                             // TODO 常量溢出错误处理
//                             XP_ASSERT_MSG(0, "constant overflowed");
//                         }
    
//                         return;
//                     } 

//                 }

//                 infer_expr_type(expr->UnaryExpr.operand, target_type, analyser);
//                 if(expr->UnaryExpr.operand->v_type.kind == Type_bool) {
//                     // TODO 负号不能操作bool类型错误处理
//                     XP_ASSERT_MSG(0, "minus operator can not operate on bool type");
//                 }
//                 expr->v_type = expr->UnaryExpr.operand->v_type;
//                 return;

//             } else if(expr->UnaryExpr.op == TokenType::Exclamation) {
//                 // 逻辑非运算符, 输入和输出都是bool类型

//                 Type bool_type = make_type(Type_bool);
//                 // infer_expr_type(expr->UnaryExpr.operand, NULL, analyser);
//                 infer_expr_type_without_target(expr->UnaryExpr.operand, analyser);
//                 if(!is_equal_type(expr->UnaryExpr.operand->v_type, bool_type)) {
//                     // TODO 逻辑非运算符操作数类型错误处理
//                     XP_ASSERT_MSG(0, "logical not operator operand must be bool type");
//                 }

//                 expr->v_type = bool_type;
//                 return;
//             }
            
//             // 普遍推导操作数类型
//             infer_expr_type(expr->UnaryExpr.operand, target_type, analyser);

//             expr->v_type = expr->UnaryExpr.operand->v_type;
//         } break;
        

//         // *无视target_type, 因为CastExpr的子表达式的类型无论如何都是要被转化为CastExpr.target_type
//         case AstType_CastExpr: {
//             // 强制转换类型
                        
//             // infer_expr_type(expr->CastExpr.expr, &expr->CastExpr.target_type, analyser);
//             infer_expr_type_without_target(expr->CastExpr.expr, analyser);

//             if(!is_equal_type(expr->CastExpr.expr->v_type, make_type(Type_bool)) && is_equal_type(expr->CastExpr.target_type, make_type(Type_bool))) {
//                 // TODO 错误处理 非bool类型转换为bool类型
//                 XP_ASSERT_MSG(0, "only bool type can be casted to bool type");
//             }

//             expr->v_type = expr->CastExpr.target_type;
//         } break;

//         // *无视target_type
//         // 别的expr类型确定了, 不用推导
//         case AstType_VarExpr: {
//             SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->VarExpr.name);
//             expr->v_type = info->type;
//         } break;
//         case AstType_FunctionCallExpr: {
//             SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->FunctionCallExpr.name);
//             expr->v_type = *info->type.function_info.return_type;
//         } break;


//         default: {
//             XP_ASSERT_DEFAULT(0);
//         } break;
//     }



//     if(recurisve_depth == 1 && target_type != NULL) {
//         // 顶层调用时, 尝试让expr迎合target_type
//         if(!is_equal_type(expr->v_type, *target_type)) {
//             // TODO 类型不匹配错误处理
//             XP_ASSERT_MSG(0, "expr type mismatch with target type in infer_expr_type");
//         }
//     }
    
//     recurisve_depth--;
// }




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

            // 普遍推导操作数类型
            infer_type_new(expr->UnaryExpr.operand, has_target, target_type, analyser);
            expr->v_type = expr->UnaryExpr.operand->v_type;


            // 如果操作数是bool类型, 那么操作符必须是逻辑非或等于/不等于
            if(is_equal_type(expr->UnaryExpr.operand->v_type, make_type(Type_bool))) {
                XP_ASSERT_MSG(is_operator_for_bool(expr->UnaryExpr.op), "bool type operand can only be used with logical not operator and double/! equal operator");
            }


            // 特殊处理负号
            if(expr->UnaryExpr.op == TokenType::Minus) {

                if(is_equal_type(expr->UnaryExpr.operand->v_type, make_type(Type_literal))) {
                    i128 val = expr->UnaryExpr.operand->Constant.value;
                    i128 neg_val = -val;
                    
                    // 重新推导类型
                    Type inferred_type;
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

                    return;
                }

                if(is_equal_type(expr->UnaryExpr.operand->v_type, make_type(Type_literal_float))) {
                    double val = expr->UnaryExpr.operand->Constant.float_value;
                    double neg_val = -val;

                    Type inferred_type;
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