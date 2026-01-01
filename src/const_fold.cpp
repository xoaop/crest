#include "const_fold.hpp"


i128 operation_integer_ast(TokenType op_type, Ast *left_c, Ast *right_c) {
    XP_ASSERT_DEFAULT(left_c->type == AstType_Constant);
    XP_ASSERT_DEFAULT(right_c == NULL || right_c->type == AstType_Constant);

    i128 left = left_c->Constant.value;
    i128 right;
    
    i128 result = 0;


    if(right_c == NULL) {
        goto unary_operation;
    } else {
        goto binary_operation;
    }

binary_operation:
    right = right_c->Constant.value;
    switch(op_type) {
        case TokenType::Add: {
            result = left + right;
        } break;
        case TokenType::Minus: {
            result = left - right;
        } break;
        case TokenType::Star: {
            result = left * right;
        } break;
        case TokenType::ForwardSlash: {
            XP_ASSERT_DEFAULT(right != 0);
            result = left / right;
        } break;
        case TokenType::Percent: {
            XP_ASSERT_DEFAULT(right != 0);
            result = left % right;
        } break;

        case TokenType::DoubleEqual: {
            result = left == right;
        } break;
        case TokenType::ExclamationEqual: {
            result = left != right;
        } break;
        case TokenType::GreaterThan: {
            result = left > right;
        } break;
        case TokenType::GreaterEqual: {
            result = left >= right;
        } break;
        case TokenType::LessThan: {
            result = left < right;
        } break;
        case TokenType::LessEqual: {
            result = left <= right;
        } break;
        case TokenType::DoubleAnd: {
            result = left && right;
        } break;
        case TokenType::DoubleOr: {
            result = left || right;
        } break;

        default: {
            XP_ASSERT_DEFAULT(0);
        } break;
    }
    goto unsigned_cast;


unary_operation:

    switch(op_type)
    {
    case TokenType::Minus: {
        result = -left;
    } break;
    case TokenType::Exclamation: {
        result = !left;
    } break;
    
    default:
        break;
    }
    goto unsigned_cast;


unsigned_cast:

    switch (left_c->v_type.kind)
    {
    case Type_u8:
        result = cast(u8) result;
        break;
    case Type_u32:
        result = cast(u32) result;
        break;
    case Type_u64:
        result = cast(u64) result;
        break;
    default:
        break;
    }

    return result;
}

double operation_float_ast(TokenType op_type, Ast *left_c, Ast *right_c) {
    XP_ASSERT_DEFAULT(left_c->type == AstType_Constant);
    XP_ASSERT_DEFAULT(right_c == NULL || right_c->type == AstType_Constant);

    double left = left_c->Constant.float_value;
    double right;
    
    double result = 0.0;
    if(right_c == NULL) {
        goto unary_operation;
    } else {
        goto binary_operation;
    }


binary_operation:
    right = right_c->Constant.float_value;
    switch(op_type) {
        case TokenType::Add: {
            result = left + right;
        } break;
        case TokenType::Minus: {
            result = left - right;
        } break;
        case TokenType::Star: {
            result = left * right;
        } break;
        case TokenType::ForwardSlash: {
            result = left / right;
        } break;

        case TokenType::DoubleEqual: {
            result = left == right;
        } break;
        case TokenType::ExclamationEqual: {
            result = left != right;
        } break;
        case TokenType::GreaterThan: {
            result = left > right;
        } break;
        case TokenType::GreaterEqual: {
            result = left >= right;
        } break;
        case TokenType::LessThan: {
            result = left < right;
        } break;
        case TokenType::LessEqual: {
            result = left <= right;
        } break;

        default: {
            XP_ASSERT_DEFAULT(0);
        } break;
    }
    goto end;

unary_operation:
    switch(op_type)
    {
    case TokenType::Minus: {
        result = -left;
    } break;
    
    default:
        break;
    }
    goto end;
end:
    return result;


}






void try_constant_expr_folding(Ast *const_expr) {
    if(const_expr->is_const_expr == false) {
        return;
    }


    switch(const_expr->type)
    {
    case AstType_BinaryExpr: 
    case AstType_UnaryExpr:
    {
        TokenType op_type;
        Ast *left;
        Ast *right;
        if(const_expr->type == AstType_BinaryExpr) {
            try_constant_expr_folding(const_expr->BinaryExpr.left);
            try_constant_expr_folding(const_expr->BinaryExpr.right);

            op_type = const_expr->BinaryExpr.op;
            left = const_expr->BinaryExpr.left;
            right = const_expr->BinaryExpr.right;
        } else {
            try_constant_expr_folding(const_expr->UnaryExpr.operand);

            op_type = const_expr->UnaryExpr.op;
            left = const_expr->UnaryExpr.operand;
            right = NULL;
        }



        i128 result;
        double dresult;

        if(is_integer_type(const_expr->v_type)) {
            result = operation_integer_ast(op_type, left, right);
        } else if(is_float_type(const_expr->v_type)) {
            dresult = operation_float_ast(op_type, left, right);
        } 

        if(check_literal_overflow(const_expr->v_type.kind, result, 0.0)) {
            // TODO 常量溢出错误处理
            XP_ASSERT_MSG(0, "constant overflowed");
        }

        Ast constant = ast_make(AstType_Constant);
        constant.is_const_expr = true;
        constant.v_type = const_expr->v_type;

        if(is_integer_type(const_expr->v_type)) {
            constant.Constant.value = result;
        } else if(is_float_type(const_expr->v_type)) {
            constant.Constant.float_value = dresult;
        }

        *const_expr = constant;
    } break;

    case AstType_CastExpr: {
        try_constant_expr_folding(const_expr->CastExpr.expr);

        i128 result = const_expr->CastExpr.expr->Constant.value;
        double dresult = const_expr->CastExpr.expr->Constant.float_value;
        
        Type target_type = const_expr->CastExpr.target_type;




        // 类型转换
        // TODO 改掉这种写法, 太丑陋了
        if(is_integer_type(target_type)) {
            if(is_integer_type(const_expr->CastExpr.expr->v_type)) {
                switch (target_type.kind)
                {
                case Type_i8:
                    result = cast(i8) result;
                    break;
                case Type_i32:
                    result = cast(i32) result;
                    break;
                case Type_i64:
                    result = cast(i64) result;
                    break;
                case Type_u8:
                    result = cast(u8) result;
                    break;
                case Type_u32:
                    result = cast(u32) result;
                    break;
                case Type_u64:
                    result = cast(u64) result;
                    break;
                default:
                    break;
                }    

            } else if(is_float_type(const_expr->CastExpr.expr->v_type)) {
                switch(target_type.kind)
                {
                case Type_i8:
                    result = cast(i8) dresult;
                    break;
                case Type_i32:
                    result = cast(i32) dresult;
                    break;
                case Type_i64:
                    result = cast(i64) dresult;
                    break;
                case Type_u8:
                    result = cast(u8) dresult;
                    break;
                case Type_u32:
                    result = cast(u32) dresult;
                    break;
                case Type_u64:
                    result = cast(u64) dresult;
                    break;
                default:
                    break;
                }
            }
        } else if(is_float_type(target_type)) {
            if(is_integer_type(const_expr->CastExpr.expr->v_type)) {
                switch(target_type.kind)
                {
                case Type_f32:
                    dresult = cast(float) result;
                    break;
                case Type_f64:
                    dresult = cast(double) result;
                    break;
                default:
                    break;
                }
            } else if(is_float_type(const_expr->CastExpr.expr->v_type)) {
                switch(target_type.kind)
                {
                case Type_f32:
                    dresult = cast(float) dresult;
                    break;
                case Type_f64:
                    dresult = cast(double) dresult;
                    break;
                default:
                    break;
                }
            }
        }



        if(check_literal_overflow(target_type.kind, result, dresult)) {
            // TODO 常量溢出错误处理
            XP_ASSERT_MSG(0, "constant overflowed");
        }

        Ast constant = ast_make(AstType_Constant);
        constant.is_const_expr = true;
        
        if(is_integer_type(target_type)) {
            constant.Constant.value = result;
        } else if(is_float_type(target_type)) {
            constant.Constant.float_value = dresult;
        }

        constant.v_type = target_type;

        *const_expr = constant;
    } break;

    
    default:
        break;
    }
}





