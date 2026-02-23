#include "value_ops.hpp"



ValueResult eval_binary_expr(Value& v1, Value& v2, TokenType op_type) {
    if(v1.has_error()) {
        return ValueResult::err(ValueErrorKind::ErrorValue);
    }
    if(v2.state == ValueState::Error) {
        return ValueResult::err(ValueErrorKind::ErrorValue);
    }


    if(v1.is_runtime_value || v2.is_runtime_value) {
        return ValueResult::err(ValueErrorKind::UsingRuntimeValue);
    }

    // 目前只处理整数, 浮点数, bool类型的二元表达式, 其他类型的二元表达式如指针、结构体、数组等的二元表达式不在编译时求值的范围内
    if(!((is_integer_or_untyped_type(v1.type) && is_integer_or_untyped_type(v2.type)) ||
         (is_float_or_untyped_type(v1.type) && is_float_or_untyped_type(v2.type)) ||
         (v1.type == easy_type(Type_bool) && v2.type == easy_type(Type_bool)))) {
        return ValueResult::err(ValueErrorKind::TypeError);
    }

    // 在只有整数, 浮点数, bool类型的情况下, 只有当两个操作数类型完全相同时才进行计算, 否则报错, 例如untyped int和i32不进行计算, untyped float和f64不进行计算, i32和f32不进行计算等
    if(v1.type != v2.type) {
        return ValueResult::err(ValueErrorKind::TypeError);
    }


    Value result = make_comptime_sovled_val(undefined_type());

    bool is_int_or_untyped = is_integer_or_untyped_type(v1.type) && is_integer_or_untyped_type(v2.type);
    bool is_float_or_untyped = is_float_or_untyped_type(v1.type) && is_float_or_untyped_type(v2.type);
    bool is_bool = v1.type == easy_type(Type_bool) && v2.type == easy_type(Type_bool);


    bool is_int_overflow = false;
    switch(op_type) {

        case TokenType::Add: {
            if(is_int_or_untyped) {
                is_int_overflow = xp_check_i128_add_overflow(v1.integer_value, v2.integer_value, &result.integer_value);
            } else if(is_float_or_untyped) {
                result.float_value = v1.float_value + v2.float_value;
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);
            }
        } break;

        case TokenType::Minus: {
            if(is_int_or_untyped) {
                is_int_overflow = xp_check_i128_sub_overflow(v1.integer_value, v2.integer_value, &result.integer_value);
            } else if(is_float_or_untyped) {
                return ValueResult::err(ValueErrorKind::TypeError);;
            } else if(is_float_or_untyped) {
                result.float_value = v1.float_value - v2.float_value;
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::Star: {
            if(is_int_or_untyped) {
                is_int_overflow = xp_check_i128_mul_overflow(v1.integer_value, v2.integer_value, &result.integer_value);
            } else if(is_float_or_untyped) {
                result.float_value = v1.float_value * v2.float_value;
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::ForwardSlash: {
            if(is_int_or_untyped) {
                if(v2.integer_value == 0) {
                    return ValueResult::err(ValueErrorKind::DivideByZero);
                }
                is_int_overflow = xp_check_i128_div_overflow(v1.integer_value, v2.integer_value, &result.integer_value);
            } else if(is_float_or_untyped) {
                if(v2.float_value == 0.0) {
                    return ValueResult::err(ValueErrorKind::DivideByZero);
                }
                result.float_value = v1.float_value / v2.float_value;
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::Percent: {
            if(is_int_or_untyped) {
                if(v2.integer_value == 0) {
                    return ValueResult::err(ValueErrorKind::DivideByZero);
                }
                is_int_overflow = xp_check_i128_mod_overflow(v1.integer_value, v2.integer_value, &result.integer_value);
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;


        case TokenType::GreaterThan: {
            if(is_int_or_untyped) {
                result.bool_value = v1.integer_value > v2.integer_value;
            } else if(is_float_or_untyped) {
                result.bool_value = v1.float_value > v2.float_value;
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::LessThan: {
            if(is_int_or_untyped) {
                result.bool_value = v1.integer_value < v2.integer_value;
            } else if(is_float_or_untyped) {
                result.bool_value = v1.float_value < v2.float_value;
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::GreaterEqual: {
            if(is_int_or_untyped) {
                result.bool_value = v1.integer_value >= v2.integer_value;
            } else if(is_float_or_untyped) {
                result.bool_value = v1.float_value >= v2.float_value;
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::LessEqual: {
            if(is_int_or_untyped) {
                result.bool_value = v1.integer_value <= v2.integer_value;
            } else if(is_float_or_untyped) {
                result.bool_value = v1.float_value <= v2.float_value;
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::DoubleEqual: {
            if(is_int_or_untyped) {
                result.bool_value = v1.integer_value == v2.integer_value;
            } else if(is_float_or_untyped) {
                result.bool_value = v1.float_value == v2.float_value;
            } else if(is_bool) {
                result.bool_value = v1.bool_value == v2.bool_value;
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::ExclamationEqual: {
            if(is_int_or_untyped) {
                result.bool_value = v1.integer_value != v2.integer_value;
            } else if(is_float_or_untyped) {
                result.bool_value = v1.float_value != v2.float_value;
            } else if(is_bool) {
                result.bool_value = v1.bool_value != v2.bool_value;
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::DoubleAnd: {
            if(is_bool) {
                result.bool_value = v1.bool_value && v2.bool_value;
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::DoubleOr: {
            if(is_bool) {
                result.bool_value = v1.bool_value || v2.bool_value;
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
            
        } break;

        default: {
            return ValueResult::err(ValueErrorKind::OperatorError);
        } break;
    }


    // 检查内置溢出
    if(is_int_or_untyped) {
        if(is_int_overflow) {
            return ValueResult::err(ValueErrorKind::Overflow);
        }
    } else if(is_float_or_untyped) {
        if(xp_check_f64_is_inf(result.float_value)) {
            return ValueResult::err(ValueErrorKind::Overflow);
        }
    }

    // 检查类型溢出
    bool overflowed = false;
    if(is_int_or_untyped) {
        if(is_certain_type(v1.type)) {
            overflowed = check_integer_overflow(result.integer_value, v1.type);
        }
    } else if(is_float_or_untyped) {
        if(is_certain_type(v1.type)) {
            overflowed = check_float_overflow(result.float_value, v1.type);
        }
    }
    if(overflowed) {
        return ValueResult::err(ValueErrorKind::Overflow);
    }

    // 设置结果类型
    if(is_int_or_untyped) {
        result.set_type(v1.type);
    } else if(is_float_or_untyped) {
        result.set_type(v1.type);
    } else if(is_bool) {
        result.set_type(easy_type(Type_bool));
    } else {
        UNREACHABLE();
    }
    

    return ValueResult::ok(result);
}



ValueResult eval_unary_expr(Value& v, TokenType op_type) {
    if(v.has_error()) {
        return ValueResult::err(ValueErrorKind::ErrorValue);
    }


    if(v.is_runtime_value) {
        return ValueResult::err(ValueErrorKind::UsingRuntimeValue);
    }


    switch(op_type) {
        case TokenType::Minus:
            return unary_arithmetic_value(v, op_type);
        case TokenType::Exclamation:
            return unary_boolean_value(v, op_type);
        default:
            return ValueResult::err(ValueErrorKind::OperatorError);
    }
}


// 一元算数
ValueResult unary_arithmetic_value(Value& v, TokenType op_type) {
    if(v.is_runtime_value) {
        return ValueResult::err(ValueErrorKind::UsingRuntimeValue);
    }

    if(!is_integer_or_untyped_type(v.type) && !is_float_or_untyped_type(v.type)) {
        return ValueResult::err(ValueErrorKind::TypeError);
    }

    Value result = make_value();
    if(is_integer_or_untyped_type(v.type)) {
        switch(op_type) {
            case TokenType::Minus: {
                result.integer_value = -v.integer_value;
            } break;
            default: {
                return ValueResult::err(ValueErrorKind::OperatorError);
            } break;
        }

        result.set_type(v.type);

        // 溢出检查
        bool overflowed = false;
        if(is_certain_type(v.type)) {
            overflowed = check_integer_overflow(result.integer_value, v.type);
        }

    } else if(is_float_or_untyped_type(v.type)) {
        switch(op_type) {
            case TokenType::Minus: {
                result.float_value = -v.float_value;
            } break;
            default: {
                return ValueResult::err(ValueErrorKind::OperatorError);
            } break;
        }

        result.set_type(v.type);

        // 溢出检查
        bool overflowed = false;
        if(is_certain_type(v.type)) {
            overflowed = check_float_overflow(result.float_value, v.type);
        }
    } else {
        return ValueResult::err(ValueErrorKind::TypeError);;
    }

    return ValueResult::ok(result);

}




/// @brief 一元布尔运算, 目前只有逻辑非
/// @param v 
/// @param op_type 
/// @return 
ValueResult unary_boolean_value(Value& v, TokenType op_type) {
    if(v.is_runtime_value) {
        return ValueResult::err(ValueErrorKind::UsingRuntimeValue);
    }

    if(v.type != easy_type(Type_bool)) {
        return ValueResult::err(ValueErrorKind::TypeError);;
    }

    Value result = make_comptime_sovled_val(undefined_type());
    switch(op_type) {
        case TokenType::Exclamation: {
            result.bool_value = !v.bool_value;
        } break;
        default: {
            return ValueResult::err(ValueErrorKind::OperatorError);
        } break;
    }

    result.set_type(easy_type(Type_bool));

    return ValueResult::ok(result);
}