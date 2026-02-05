#include "type_check.hpp"

#include "const_fold.hpp"

#include "error_msg.hpp"

bool fit_in_type(i128 value, TypeRef target_type) {
    bool fit = false;

    // i64 high = cast(i64)(value >> 64);
    // i64 low = cast(i64)(value & 0xFFFFFFFFFFFFFFFF);

    // printf("Checking if value fits in type: ");
    // print_i128(value);
    // print_type(target_type);
    // printf("\n");


    switch(target_type->kind) {
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
            // print_type(target_type);
            // printf("\n");
            // XP_ASSERT_DEFAULT(0);
            break;
    }

    return fit;
}

bool fit_in_type(double value, TypeRef target_type) {
    bool fit = false;
    switch(target_type->kind) {
        case Type_f32:
            fit = (value >= -3.4028235e+38 && value <= 3.4028235e+38);
            break;
        case Type_f64:
            fit = true;
            break;
        default:
            // XP_ASSERT_DEFAULT(0);
            break;
    }
    return fit;
}

bool fit_in_type(Ast *constant, TypeRef target_type) {
    XP_ASSERT_DEFAULT(constant->type == AstType_Constant);

    if(is_integer_type(constant->v_type) || constant->v_type == easy_type(Type_untyped_int)) {
        return fit_in_type(constant->Constant.value, target_type);
    } else if(is_float_type(constant->v_type) || constant->v_type == easy_type(Type_untyped_float)) {
        return fit_in_type(constant->Constant.float_value, target_type);
    } else {
        XP_ASSERT_DEFAULT(0);
        return false;
    }
}


TypeRef get_compliable_integer_type(i128 value) {

    if(fit_in_type(value, easy_type(Type_i32))) {
        return easy_type(Type_i32);
    } else if(fit_in_type(value, easy_type(Type_i64))) {
        return easy_type(Type_i64);
    } else if(fit_in_type(value, easy_type(Type_u64))) {
        return easy_type(Type_u64);
    }


    // TODO 超出范围错误处理
    XP_ASSERT_DEFAULT(0);
    return undefined_type();
}

TypeRef get_compliable_float_type(double value) {

    if(fit_in_type(value, easy_type(Type_f32))) {
        return easy_type(Type_f32);
    } else {
        return easy_type(Type_f64);
    }

}

TypeRef get_compliable_const_type(Ast *constant) {
    XP_ASSERT_DEFAULT(!is_certain_type(constant->v_type));

    if(constant->v_type == easy_type(Type_untyped_int)) {
        return get_compliable_integer_type(constant->Constant.value);
    } else if(constant->v_type == easy_type(Type_untyped_float)) {
        return get_compliable_float_type(constant->Constant.float_value);
    } else {
        XP_ASSERT_DEFAULT(0);
        return undefined_type();
    }
}

static bool check_is_lvalue(Ast *expr, TypeRef v_type) {
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

    // case AstType_VarExpr: {
    //     return true;
    // } break;

    case AstType_Ident: {
        if(v_type == undefined_type()) {
            return false;
        } else {
            return true;
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


// 检查某表达式是是否可以被当作target_type
static bool check_target_type(Ast *expr, TypeRef target_type) {


    if(expr->is_null) {
        // null可以被当作任意指针类型

        // 如target_type不是指针类型, 那就不行
        if(!is_pointer_type(target_type)) {
            return false;
        }
    } else if(is_pointer_type(expr->v_type) && target_type == pointer_type(easy_type(Type_void))) {
        // 任意类型指针都可以隐式转化为*void类型
        return true;
    }  else if(is_array_type(expr->v_type) && is_slice_struct_type(target_type)) {
        // 数组可以隐式转化为slice结构体类型
        
        // 前提元素类型相同
        if(target_type->struct_info.struct_fields[0].type->pointed_type == expr->v_type->array_info.element_type) {

            expr->implicit_conversion_tag = ImplicitConversionTag::ArrayToSliceStruct;
            return true;
        } else {

            return false;
        }
    }
    
    else if(expr->v_type != target_type) {
        return false;
    }

    return true;
}



void infer_expr_type(Ast *expr, bool has_target, TypeRef target_type, Analyser analyser) {
    static isize depth = 0;

    depth += 1;
    switch(expr->type) {

        case AstType_Constant: {
            if(is_certain_type(expr->v_type)) {
                break;
            }

            // example: x : i32 = true;  // target_type = i32, has_target = true, expr->v_type = bool
            if(expr->v_type == easy_type(Type_bool) && has_target && target_type != easy_type(Type_bool)) {
                error_msg(&expr->token, "cannot assign bool constant to non-bool type");
                XP_ASSERT_DEFAULT(0);
            }

            // example: x : bool = 1;  // target_type = bool, has_target = true, expr->v_type = literal
            if((expr->v_type != easy_type(Type_bool)) && has_target && target_type == easy_type(Type_bool)) {
                error_msg(&expr->token, "cannot assign non-bool constant to bool type");
                XP_ASSERT_DEFAULT(0);
            }


            if(has_target) {
                if(!fit_in_type(expr, target_type)) {
                    error_msg(&expr->token, "constant value not fit in target type");
                    print_type(target_type);
                    printf("\n");

                    XP_ASSERT_DEFAULT(0);
                }
                expr->v_type = target_type;
            } else {
                expr->v_type = get_compliable_const_type(expr);
            }

            
        } break;


        case AstType_BinaryExpr: {

            bool should_check_equal_type = true;


            TypeRef old_left_type = expr->BinaryExpr.left->v_type;
            TypeRef old_right_type = expr->BinaryExpr.right->v_type;



            if(is_certain_type(old_left_type) && is_certain_type(old_right_type)) {
                infer_expr_type(expr->BinaryExpr.left, has_target, target_type, analyser);
                infer_expr_type(expr->BinaryExpr.right, has_target, target_type, analyser);

                // 处理结构体类型的运算
                if(is_struct_type(expr->BinaryExpr.left->v_type) && is_struct_type(expr->BinaryExpr.right->v_type)) {
                    // 结构体类型之间只能比较是否相等/不等
                    if(is_equal_compare_operator(expr->BinaryExpr.op)) {
                        expr->v_type = easy_type(Type_bool);
                    } else {
                        error_msg(&expr->token, "only equality comparison is allowed between struct types");
                        XP_ASSERT_DEFAULT(0);
                    }
                }

            } else if(!is_certain_type(old_left_type) && !is_certain_type(old_right_type)) {
                // 两个都是不确定类型
                // 例如: 1 + 2.0   1 == 2

                
                // 一个是Type_literal, 另一个是Type_untyped_float
                if(old_left_type != old_right_type) {
                    error_msg(&expr->token, "cannot infer types for both sides of binary expression");
                    XP_ASSERT_DEFAULT(0);
                }

                // 如果是返回bool类型的操作符, 那么target_type没有意义
                if(is_return_bool_operator(expr->BinaryExpr.op)) {
                    infer_expr_type(expr->BinaryExpr.left, false, target_type, analyser);
                    infer_expr_type(expr->BinaryExpr.right, false, target_type, analyser);
                } else {
                    infer_expr_type(expr->BinaryExpr.left, has_target, target_type, analyser);
                    infer_expr_type(expr->BinaryExpr.right, has_target, target_type, analyser);
                }


                TypeRef common_type = get_common_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type);
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

                infer_expr_type(not_literal_expr, has_target, target_type, analyser);

                // 如果确定类型表达式是指针类型, 那不确定类型表达式的类型不需要检查, 避免尝试被当作指针类型转化
                // 但是还不能确定是整数类型, 因为可能是浮点类型
                bool should_check_untyped = !is_pointer_type(not_literal_expr->v_type);

                infer_expr_type(literal_expr, should_check_untyped, not_literal_expr->v_type, analyser);
            }



            // 处理指针和指针之间的比较
            if(is_pointer_type(expr->BinaryExpr.left->v_type) && is_pointer_type(expr->BinaryExpr.right->v_type)) {
                
                // 指针类型之间只能比较是否相同
                if(!is_equal_compare_operator(expr->BinaryExpr.op)) {
                    error_msg(&expr->token, "only equality comparison is allowed between pointer types");
                    XP_ASSERT_DEFAULT(0);
                }

                // null指针可以和任何指针类型比较
                if(expr->BinaryExpr.left->is_null || expr->BinaryExpr.right->is_null) {
                    should_check_equal_type = false;

                    // should_check_equal_type会绕开把v_type设为左边类型的逻辑
                    // 不过由于是比较操作, 结果类型是bool, 下面有v_type的设置
                    // 最终这里不用设置expr->v_type = 非null指针的类型
                }

            } else if(is_pointer_type(expr->BinaryExpr.left->v_type) || is_pointer_type(expr->BinaryExpr.right->v_type)) {
                
                // 处理指针和整数的加减法运算
                Ast *pointer_expr = is_pointer_type(expr->BinaryExpr.left->v_type) ? expr->BinaryExpr.left : expr->BinaryExpr.right;
                Ast *non_pointer_expr = is_pointer_type(expr->BinaryExpr.left->v_type) ? expr->BinaryExpr.right : expr->BinaryExpr.left;


                // 指针类型不能是void指针
                if(get_innermost_type_of_pointer(pointer_expr->v_type) == easy_type(Type_void)) {
                    error_msg(&pointer_expr->token, "void pointer type cannot be used in pointer arithmetic");
                    XP_ASSERT_DEFAULT(0);
                }

                // 确保另一个是整数类型, 而不是浮点类型
                if(!is_integer_type(non_pointer_expr->v_type)) {
                    error_msg(&non_pointer_expr->token, "only integer type can be used with pointer type in binary expressions");
                    XP_ASSERT_DEFAULT(0);
                }

                // 确保操作符是加法或减法
                if(!is_add_sub_operator(expr->BinaryExpr.op)) {
                    error_msg(&expr->token, "only addition and subtraction are allowed for pointer types");
                    XP_ASSERT_DEFAULT(0);
                }

                should_check_equal_type = false;
                expr->v_type = pointer_expr->v_type;
            }

            


            // 如果需要, 检查左右表达式类型是否相等
            if(should_check_equal_type && expr->BinaryExpr.left->v_type != expr->BinaryExpr.right->v_type) {
                error_msg(&expr->token, "binary expression left and right side types do not match");
                print_type(expr->BinaryExpr.left->v_type);
                printf(" vs ");
                print_type(expr->BinaryExpr.right->v_type);
                printf("\n");

                XP_ASSERT_DEFAULT(0);
            }

            // 如果要检查左右表达式类型相等, 那么结果类型就是左边的类型
            if(should_check_equal_type) {
                expr->v_type = expr->BinaryExpr.left->v_type;
            }




            // 如果有一个是bool类型, 那么操作符必须是逻辑操作符或等于/不等于
            if(expr->BinaryExpr.left->v_type == easy_type(Type_bool) || 
               expr->BinaryExpr.right->v_type == easy_type(Type_bool)) {
                if(!is_operator_for_bool(expr->BinaryExpr.op)) {
                    error_msg(&expr->token, "bool type can only be used with logical operators and double/! equal operator");
                    XP_ASSERT_DEFAULT(0);
                }
            }

            // 如果操作符是%且类型是浮点类型, 报错
            if(expr->BinaryExpr.op == TokenType::Percent) {
                if(is_float_type(expr->BinaryExpr.left->v_type)) {
                    // TODO 浮点类型不能使用取模操作符错误处理
                    error_msg(&expr->token, "modulus operator cannot be used with float type");
                    XP_ASSERT_MSG(0, "modulus operator can not be used with float type");
                }
            }

            // 如果是返回bool类型的操作符, 那么类型就是bool
            // 否则不变
            if(is_return_bool_operator(expr->BinaryExpr.op)) {
                expr->v_type = easy_type(Type_bool);
            } 
            
        } break;

        case AstType_UnaryExpr: {


            // 特殊处理如-128(i8)这种情况, 不能简单地把 128 作为常量处理, 不然会被推导为高一级别的类型
            if(expr->UnaryExpr.op == TokenType::Minus && !is_certain_type(expr->UnaryExpr.operand->v_type)) {
                TypeRef inferred_type;

                if(expr->UnaryExpr.operand->v_type == easy_type(Type_untyped_int)) {
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
                        error_msg(&expr->UnaryExpr.operand->token, "constant value overflowed for inferred type");
                        XP_ASSERT_MSG(0, "constant overflowed");
                    }
                    
                }

                if(expr->UnaryExpr.operand->v_type == easy_type(Type_untyped_float)) {
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
                        error_msg(&expr->UnaryExpr.operand->token, "constant value overflowed for inferred type");
                        XP_ASSERT_MSG(0, "constant overflowed");
                    }

                }
                
            } else {
                // 普遍推导操作数类型
                infer_expr_type(expr->UnaryExpr.operand, has_target, target_type, analyser);
                expr->v_type = expr->UnaryExpr.operand->v_type;

            }
            
            
            // 如果操作数是bool类型, 那么操作符必须是逻辑非或等于/不等于
            if(expr->UnaryExpr.operand->v_type == easy_type(Type_bool)) {
                if(!is_operator_for_bool(expr->UnaryExpr.op)) {
                    error_msg(&expr->token, "bool type operand can only be used with logical not operator and double/! equal operator");
                    XP_ASSERT_DEFAULT(0);
                }
            }
            



            if(is_return_bool_operator(expr->UnaryExpr.op)) {
                expr->v_type = easy_type(Type_bool);
            } else {
                expr->v_type = expr->UnaryExpr.operand->v_type;
            }

            if(expr->UnaryExpr.op == TokenType::And) {

                // 取地址只能用于左值
                if(!expr->UnaryExpr.operand->is_lvalue) {
                    error_msg(&expr->token, "address-of operator can only be applied to lvalue expressions");
                    XP_ASSERT_DEFAULT(0);
                }

                // 设置取地址后的类型
                expr->v_type = pointer_type(expr->v_type);
            }

            // 解引用只能用于指针类型
            if(expr->UnaryExpr.op == TokenType::Star) {
                if(!is_pointer_type(expr->UnaryExpr.operand->v_type)) {
                    error_msg(&expr->token, "dereference operator can only be applied to pointer types");
                    XP_ASSERT_DEFAULT(0);
                }

                // *void 类型不能解引用
                if(get_pointed_type(expr->UnaryExpr.operand->v_type) == easy_type(Type_void)) {
                    error_msg(&expr->token, "void pointer type cannot be dereferenced");
                    XP_ASSERT_DEFAULT(0);
                }

                // 设置解引用后的类型
                expr->v_type = get_pointed_type(expr->UnaryExpr.operand->v_type);

            }
            

        } break;
        


        case AstType_CastExpr: {
            infer_expr_type(expr->CastExpr.expr, false, target_type, analyser);

            // struct -> struct 必须是相同类型
            if(is_struct_type(expr->CastExpr.expr->v_type)) {
                if(expr->CastExpr.expr->v_type != expr->CastExpr.target_type) {
                    // TODO 结构体类型转换必须是相同类型错误处理

                    error_msg(&expr->token, "struct type can only be casted to the same struct type");
                    XP_ASSERT_DEFAULT(0);
                }
            }
            
            // array -> array 必须是相同类型
            if(is_array_type(expr->CastExpr.expr->v_type)) {
                if(expr->CastExpr.expr->v_type != expr->CastExpr.target_type) {
                    
                    // TODO 错误处理 数组类型转换必须是相同类型 

                    error_msg(&expr->token, "array type can only be casted to the same array type");
                    XP_ASSERT_DEFAULT(0);
                }
            }

            // pointer -> 非pointer 错误
            if(is_pointer_type(expr->CastExpr.expr->v_type) && !is_pointer_type(expr->CastExpr.target_type)) {
                // TODO 错误处理 指针类型转换为非指针类型

                error_msg(&expr->token, "pointer type cannot be casted to non-pointer type");
                XP_ASSERT_MSG(0, "pointer type cannot be casted to non-pointer type");
            }

            // 非pointer -> pointer
            if(!is_pointer_type(expr->CastExpr.expr->v_type) && is_pointer_type(expr->CastExpr.target_type)) {
                
                // TODO 错误处理 非指针类型转换为指针类型

                error_msg(&expr->token, "non-pointer type cannot be casted to pointer type");
                XP_ASSERT_MSG(0, "non-pointer type cannot be casted to pointer type");
            }


            // 非bool -> bool 错误
            if(expr->CastExpr.expr->v_type != easy_type(Type_bool) && expr->CastExpr.target_type == easy_type(Type_bool)) {
                // TODO 错误处理 非bool类型转换为bool类型

                error_msg(&expr->token, "only bool type can be casted to bool type");
                XP_ASSERT_MSG(0, "only bool type can be casted to bool type");
            }



            expr->v_type = expr->CastExpr.target_type;

        } break;

        // TODO FIRST 实现
        case AstType_FieldAccess: {

            Ast *parent = expr->FieldAccess.parent;
            xpString field_name = expr->FieldAccess.field_name;

            infer_expr_type(parent, has_target, target_type, analyser);

            bool is_struct_field_access = false;
            TypeRef parent_struct_type = NULL;

            if(parent->v_type == undefined_type()) {
                // 表示parent是个包名

                is_struct_field_access = false;
            } else if(is_struct_type(parent->v_type)) {
                // 结构体变量名
                is_struct_field_access = true;

                parent_struct_type = parent->v_type;

            } else if(is_pointer_type(parent->v_type) && is_struct_type(get_pointed_type(parent->v_type))) {
                // 结构体指针变量名
                is_struct_field_access = true;

                // 获取指针指向的结构体类型, 方便后面检查字段
                parent_struct_type = get_pointed_type(parent->v_type);
            } else {
                error_msg(&parent->token, "field access parent expression must be a struct type or package name");
                XP_ASSERT_DEFAULT(0);
            }

            if(is_struct_field_access) {
                
                // 检查a.b中, a有没有b这个字段
                StructField field;
                bool found = false;
                for(isize i = 0; i < parent_struct_type->struct_info.struct_fields.count; i++) {
                    field = parent_struct_type->struct_info.struct_fields[i];
                    if(xp_string_cmp(field.name, expr->FieldAccess.field_name) == 0) {
                        found = true;
                        break;
                    }
                }
                if(!found) {
                    error_msg(&expr->token, "struct type '%s' has no field named '%s'", parent_struct_type->type_name.c_str, expr->FieldAccess.field_name.c_str);
                    XP_ASSERT_DEFAULT(0);
                }
    
    
                // 如果不是结构体类型, 直接从字段类型赋值
                // 如果是结构体类型, 从符号表中找到字段类型信息再赋值
                // expr->v_type = get_type_detail_if_have(symbol_table(), field.type);
                expr->v_type = field.type;
            } else {
                // 包名.符号名

                // 符号存在性和符号类别已在resolve阶段检查过了

                SymbolInfo *package_info = find_symbol_by_ident_or_fieldaccess_in_other_packages(parent, analyser);
            
                SymbolInfo *field_info = find_symbol_in_curr_scope(&package_info->imported_package->package_scope, field_name);

                expr->v_type = field_info->type;
            }


        } break;

        case AstType_StructInitExpr: {
            // 检查是否是结构体类型

            SymbolInfo *struct_type_info = find_symbol_by_ident_or_fieldaccess_in_other_packages(expr->StructInitExpr.struct_type_ident, analyser);

            // 检查字段数量是否匹配
            if(struct_type_info->type->struct_info.struct_fields.count != expr->StructInitExpr.field_inits.count) {
                error_msg(&expr->token, "struct init field count does not match struct type field count");
                XP_ASSERT_DEFAULT(0);
            }

            // 检查字段初始化表达式类型是否和字段类型匹配
            for(isize i = 0; i < expr->StructInitExpr.field_inits.count; i++) {
                infer_expr_type(expr->StructInitExpr.field_inits[i], true, struct_type_info->type->struct_info.struct_fields[i].type, analyser);
                
                if(!check_target_type(expr->StructInitExpr.field_inits[i], struct_type_info->type->struct_info.struct_fields[i].type)) {
                    error_msg(&expr->StructInitExpr.field_inits[i]->token, "struct field init expression type does not match struct field type");
                    XP_ASSERT_DEFAULT(0);
                }
            }

            expr->v_type = struct_type_info->type;


        } break;

        // TODO 数组字面量类型检查+推导
        case AstType_ArrayInitExpr: {

            // 数组字面量的元素个数不能为0
            if(expr->ArrayInitExpr.elements.count == 0) {
                error_msg(&expr->token, "array literal cannot be empty");
                XP_ASSERT_DEFAULT(0);
            }

            if(has_target) {
                // 如果有目标类型

                // target_type必须是数组类型
                if(!is_array_type(target_type)) {
                    error_msg(&expr->token, "target type for array literal must be an array type");
                    XP_ASSERT_DEFAULT(0);
                }

                TypeRef target_element_type = target_type->array_info.element_type;
                usize target_count = target_type->array_info.count;

                // 检查元素数量是否匹配
                if(target_count != cast(usize)(expr->ArrayInitExpr.elements.count)) {
                    error_msg(&expr->token, "array literal element count does not match target array type count");
                    XP_ASSERT_DEFAULT(0);
                }

                // 推导每个元素类型, 检查是否匹配目标元素类型
                for(isize i = 0; i < expr->ArrayInitExpr.elements.count; i++) {
                    infer_expr_type(expr->ArrayInitExpr.elements[i], true, target_element_type, analyser);

                    if(target_element_type != expr->ArrayInitExpr.elements[i]->v_type) {
                        error_msg(&expr->ArrayInitExpr.elements[i]->token, "array literal element type does not match target array element type");
                        XP_ASSERT_DEFAULT(0);
                    }
                }

                expr->v_type = target_type;
            } else {
                // 如果没有目标类型
                // 先找有没有确定类型的元素

                TypeRef element_type = undefined_type();
                bool found_certain_type = false;
                for(isize i = 0; i < expr->ArrayInitExpr.elements.count; i++) {
                    if(is_certain_type(expr->ArrayInitExpr.elements[i]->v_type)) {
                        found_certain_type = true;
                        element_type = expr->ArrayInitExpr.elements[i]->v_type;
                        break;
                    }
                }
                if(!found_certain_type) {
                    // 如果没有确定类型的元素, 那就用第一个元素推导类型
                    infer_expr_type(expr->ArrayInitExpr.elements[0], false, target_type, analyser);
                    element_type = expr->ArrayInitExpr.elements[0]->v_type;
                }
                // TODO 这里如果没有确定类型的元素, 会多执行一次推导(因为要获取第一个元素类型, 提前推导), 可以优化

                // 推导其他元素类型
                for(isize i = 0; i < expr->ArrayInitExpr.elements.count; i++) {
                    infer_expr_type(expr->ArrayInitExpr.elements[i], true, element_type, analyser);

                    if(element_type != expr->ArrayInitExpr.elements[i]->v_type) {
                        error_msg(&expr->ArrayInitExpr.elements[i]->token, "array literal element type does not match first element type");
                        XP_ASSERT_DEFAULT(0);
                    }
                }

                expr->v_type = array_type(element_type, cast(usize)(expr->ArrayInitExpr.elements.count));
            }
            

        } break;

        case AstType_IndexExpr: {
            infer_expr_type(expr->IndexExpr.array_var_expr, false, target_type, analyser);
            infer_expr_type(expr->IndexExpr.index_expr, false, target_type, analyser);

            // TODO 添加对切片类型的支持

            // 检查被下标访问表达式是不是: 
            // 1.数组类型 
            // 2.切片类型
            // 3.字符串类型
            TypeRef as_type = expr->IndexExpr.array_var_expr->v_type;
            if(!is_array_type(as_type) && !is_slice_struct_type(as_type) && !is_string_struct_type(as_type)) {
                error_msg(&expr->token, "only array or slice struct types can be indexed");
                XP_ASSERT_DEFAULT(0);
            }

            // 检查下标表达式是不是整数类型
            TypeRef index_type = expr->IndexExpr.index_expr->v_type;
            if(!is_integer_type(index_type)) {
                error_msg(&expr->IndexExpr.index_expr->token, "index expression must be of integer type");
                XP_ASSERT_DEFAULT(0);
            }

            // 检查常量表达式下标的范围是否越界
            // 首先判断下标表达式是不是常量表达式
            // 如果是, 就做常量折叠, 判断是否在范围内
            // 如果不是, 就不管

            if(is_array_type(as_type)) {

                if(expr->IndexExpr.index_expr->is_const_expr) {
                    try_constant_expr_folding(expr->IndexExpr.index_expr);
                    XP_ASSERT_DEFAULT(expr->IndexExpr.index_expr->type == AstType_Constant && expr->IndexExpr.index_expr->is_const_expr);
    
                    if(expr->IndexExpr.index_expr->Constant.value < 0 || 
                       expr->IndexExpr.index_expr->Constant.value >= cast(i128)as_type->array_info.count) {
                        error_msg(&expr->IndexExpr.index_expr->token, "index expression out of bounds");
                        XP_ASSERT_DEFAULT(0);
                    }
                }

            }
            
            // 设置下标表达式的类型
            if(is_array_type(as_type)) {
                
                    
                expr->v_type = as_type->array_info.element_type;
            } else if(is_slice_struct_type(as_type)) {


                expr->v_type = as_type->struct_info.struct_fields[0].type->pointed_type;
            } else if(is_string_struct_type(as_type)) {

                
                expr->v_type = as_type->struct_info.struct_fields[0].type->pointed_type;
            } else {
                XP_ASSERT_DEFAULT(0);
            }
        } break;

        case AstType_StringLiteralExpr: {
            

        } break;


        case AstType_Ident: {
            SymbolInfo *info = find_symbol_by_ident_or_fieldaccess_in_other_packages(expr, analyser);
            expr->v_type = info->type;

        } break;
        case AstType_FunctionCallExpr: {
            SymbolInfo *info = find_symbol_by_ident_or_fieldaccess_in_other_packages(expr->FunctionCallExpr.func_ident, analyser);
            expr->v_type = info->type->function_info.return_type;
        } break;


        default: {
            XP_ASSERT_DEFAULT(0);
        } break;
    }


    // 设置是否是左值
    expr->is_lvalue = check_is_lvalue(expr, expr->v_type);


    if(depth == 1) {
        if(has_target) {
            if(!check_target_type(expr, target_type)) {
                error_msg(&expr->token, "expression type does not match target type");
                XP_ASSERT_DEFAULT(0);
            }
        }
    }

    depth -= 1;
}



