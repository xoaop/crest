#include "const_fold.hpp"

#include "error_msg.hpp"

struct Valuee {
    enum class Type {
        Integer,
        Float,
    } type;

    static Valuee make(i128 val);
    static Valuee make(double val);

    Valuee() = default;

    Type get_type() const;
    i128 get_as_integer() const;
    double get_as_float() const;
    void set(i128 val);
    void set(double val);

    // TODO: 实现运算符重载, 直接用表达式的方式计算, 这样就不需要写operation_integer_ast和operation_float_ast了

private:
    union {
        i128 int_value;
        double float_value;
    };

    bool has_error = false;
};



Valuee Valuee::make(i128 val) {
    Valuee v = {};
    v.type = Type::Integer;
    v.int_value = val;
    return v;
}
Valuee Valuee::make(double val) {
    Valuee v = {};
    v.type = Type::Float;
    v.float_value = val;
    return v;
}

Valuee::Type Valuee::get_type() const {
    return type;
}


i128 Valuee::get_as_integer() const {
    XP_ASSERT_DEFAULT(type == Type::Integer);
    return int_value;
}

double Valuee::get_as_float() const {
    XP_ASSERT_DEFAULT(type == Type::Float);
    return float_value;
}

void Valuee::set(i128 val) {
    type = Type::Integer;
    int_value = val;
}

void Valuee::set(double val) {
    type = Type::Float;
    float_value = val;
}








i128 operation_integer_ast(TokenType op_type, Ast *left_c, Ast *right_c) {
    XP_ASSERT_DEFAULT(left_c->type == AstType_Constant);
    XP_ASSERT_DEFAULT(right_c == NULL || right_c->type == AstType_Constant);

    i128 left;
    i128 right;

    if(left_c->v_type == easy_type(Type_bool)) {
        left = left_c->Constant.value.bool_value;
    } else {
        left = left_c->Constant.value.integer_value;
    }

    
    i128 result = 0;


    if(right_c == NULL) {
        goto unary_operation;
    } else {
        goto binary_operation;
    }

binary_operation:
    if(right_c->v_type == easy_type(Type_bool)) {
        right = right_c->Constant.value.bool_value;
    } else {
        right = right_c->Constant.value.integer_value;
    }

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

    switch (left_c->v_type->kind)
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

    double left = left_c->Constant.value.float_value;
    double right;
    
    double result = 0.0;
    if(right_c == NULL) {
        goto unary_operation;
    } else {
        goto binary_operation;
    }


binary_operation:
    right = right_c->Constant.value.float_value;
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






bool try_constant_expr_folding(Ast *const_expr) {
    if(const_expr->is_const_expr == false) {
        return false;
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
            if(!try_constant_expr_folding(const_expr->BinaryExpr.left) || !try_constant_expr_folding(const_expr->BinaryExpr.right)) {
                return false;
            }

            op_type = const_expr->BinaryExpr.op;
            left = const_expr->BinaryExpr.left;
            right = const_expr->BinaryExpr.right;
        } else {
            if(!try_constant_expr_folding(const_expr->UnaryExpr.operand)) {
                return false;
            }

            op_type = const_expr->UnaryExpr.op;
            left = const_expr->UnaryExpr.operand;
            right = NULL;
        }



        i128 result;
        double dresult;

        if(is_integer_or_bool_type(const_expr->v_type)) {
            result = operation_integer_ast(op_type, left, right);
        } else if(is_float_type(const_expr->v_type)) {
            dresult = operation_float_ast(op_type, left, right);
        } else {
            XP_ASSERT_DEFAULT(0);
        }

        if(check_literal_overflow(const_expr->v_type->kind, result, dresult)) {
            // TODO 常量溢出错误处理
            // error_msg(&const_expr->token, "constant overflowed");
            // XP_ASSERT_MSG(0, "constant overflowed");

            return false;
        }

        Ast constant = ast_make(AstType_Constant);
        constant.is_const_expr = true;
        constant.v_type = const_expr->v_type;
        constant.span = const_expr->span;

        Value result_val = make_comptime_sovled_val(const_expr->v_type);

        if(is_integer_type(const_expr->v_type)) {
            result_val.integer_value = result;
        } else if(is_float_type(const_expr->v_type)) {
            result_val.float_value = dresult;
        } else if(const_expr->v_type == easy_type(Type_bool)) {
            result_val.bool_value = (result != 0);
        }

        constant.Constant.value = result_val;


        *const_expr = constant;
    } break;

    case AstType_CastExpr: {
        if(!try_constant_expr_folding(const_expr->CastExpr.expr)) {
            return false;
        }

        i128 result;
        if(is_integer_type(const_expr->CastExpr.expr->v_type)) {
            result = const_expr->CastExpr.expr->Constant.value.integer_value;
        } else if(const_expr->CastExpr.expr->v_type == easy_type(Type_bool)) {
            result = const_expr->CastExpr.expr->Constant.value.bool_value;
        }

        double dresult = const_expr->CastExpr.expr->Constant.value.float_value;
        
        TypeRef target_type = const_expr->CastExpr.target_type;




        // 类型转换
        // TODO 改掉这种写法, 太丑陋了
        if(is_integer_or_bool_type(target_type)) {
            if(is_integer_or_bool_type(const_expr->CastExpr.expr->v_type)) {
                switch (target_type->kind)
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
                case Type_bool:
                    result = (result != 0) ? 1 : 0;
                    break;
                default:
                    break;
                }    

            } else if(is_float_type(const_expr->CastExpr.expr->v_type)) {
                switch(target_type->kind)
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
                case Type_bool:
                    result = (dresult != 0.0) ? 1 : 0;
                    break;
                default:
                    break;
                }
            }
        } else if(is_float_type(target_type)) {
            if(is_integer_or_bool_type(const_expr->CastExpr.expr->v_type)) {
                switch(target_type->kind)
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
                switch(target_type->kind)
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



        if(check_literal_overflow(target_type->kind, result, dresult)) {
            // TODO 常量溢出错误处理
            // XP_ASSERT_MSG(0, "constant overflowed");

            return false;
        }

        Ast constant = ast_make(AstType_Constant);
        constant.is_const_expr = true;
        constant.span = const_expr->span;

        Value result_val = make_comptime_sovled_val(target_type);
        
        if(is_integer_or_bool_type(target_type)) {
            result_val.integer_value = result;
        } else if(is_float_type(target_type)) {
            result_val.float_value = dresult;
        }

        constant.v_type = target_type;

        constant.Constant.value = result_val;

        *const_expr = constant;
    } break;

    
    default:
        break;
    }

    return true;
}





