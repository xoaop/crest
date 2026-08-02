#include "value_ops.hpp"

#include "ast.hpp"

// 内联运算
ValueResult exec_binary(Value &v1, Value &v2, TokenType op_type) {
    if(!((is_integer_or_untyped_type(v1.type) && is_integer_or_untyped_type(v2.type)) ||
        (is_float_or_untyped_type(v1.type) && is_float_or_untyped_type(v2.type)) ||
        (v1.type == easy_type(Type_bool) && v2.type == easy_type(Type_bool)) ||
        (is_enum_type(v1.type) && is_enum_type(v2.type)))) {

        return ValueResult::err(ValueErrorKind::TypeError);
    }

    if(v1.type != v2.type) {
        return ValueResult::err(ValueErrorKind::TypeError);
    }

    Value result = make_value();

    bool is_int_or_untyped = is_integer_or_untyped_type(v1.type) && is_integer_or_untyped_type(v2.type);
    bool is_float_or_untyped = is_float_or_untyped_type(v1.type) && is_float_or_untyped_type(v2.type);
    bool is_bool = v1.type == easy_type(Type_bool) && v2.type == easy_type(Type_bool);
    bool is_enum = is_enum_type(v1.type) && is_enum_type(v2.type);

    bool is_int_overflow = false;
    switch(op_type) {

        case TokenType::Add: {
            if(is_int_or_untyped) {
                i128 res;
                is_int_overflow = xp_check_i128_add_overflow(v1.integer_val(), v2.integer_val(), &res);
                result.integer_val(res);
            } else if(is_float_or_untyped) {
                result.float_val(v1.float_val() + v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);
            }
        } break;

        case TokenType::Minus: {
            if(is_int_or_untyped) {
                i128 res;
                is_int_overflow = xp_check_i128_sub_overflow(v1.integer_val(), v2.integer_val(), &res);
                result.integer_val(res);
            } else if(is_float_or_untyped) {
                result.float_val(v1.float_val() - v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::Star: {
            if(is_int_or_untyped) {
                i128 res;
                is_int_overflow = xp_check_i128_mul_overflow(v1.integer_val(), v2.integer_val(), &res);
                result.integer_val(res);
            } else if(is_float_or_untyped) {
                result.float_val(v1.float_val() * v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::ForwardSlash: {
            if(is_int_or_untyped) {
                if(v2.integer_val() == 0) {
                    return ValueResult::err(ValueErrorKind::DivideByZero);
                }
                i128 res;
                is_int_overflow = xp_check_i128_div_overflow(v1.integer_val(), v2.integer_val(), &res);
                result.integer_val(res);
            } else if(is_float_or_untyped) {
                if(v2.float_val() == 0.0) {
                    return ValueResult::err(ValueErrorKind::DivideByZero);
                }
                result.float_val(v1.float_val() / v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::Percent: {
            if(is_int_or_untyped) {
                if(v2.integer_val() == 0) {
                    return ValueResult::err(ValueErrorKind::DivideByZero);
                }
                i128 res;
                is_int_overflow = xp_check_i128_mod_overflow(v1.integer_val(), v2.integer_val(), &res);
                result.integer_val(res);
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;


        case TokenType::GreaterThan: {
            if(is_int_or_untyped) {
                result.bool_val( v1.integer_val() > v2.integer_val());
            } else if(is_float_or_untyped) {
                result.bool_val( v1.float_val() > v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::LessThan: {
            if(is_int_or_untyped) {
                result.bool_val( v1.integer_val() < v2.integer_val());
            } else if(is_float_or_untyped) {
                result.bool_val( v1.float_val() < v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::GreaterEqual: {
            if(is_int_or_untyped) {
                result.bool_val( v1.integer_val() >= v2.integer_val());
            } else if(is_float_or_untyped) {
                result.bool_val( v1.float_val() >= v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::LessEqual: {
            if(is_int_or_untyped) {
                result.bool_val( v1.integer_val() <= v2.integer_val());
            } else if(is_float_or_untyped) {
                result.bool_val( v1.float_val() <= v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::DoubleEqual: {
            if(is_int_or_untyped) {
                result.bool_val( v1.integer_val() == v2.integer_val());
            } else if(is_float_or_untyped) {
                result.bool_val( v1.float_val() == v2.float_val());
            } else if(is_bool) {
                result.bool_val( v1.bool_val() == v2.bool_val());
            } else if(is_enum) {
                result.bool_val( v1.integer_val() == v2.integer_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::ExclamationEqual: {
            if(is_int_or_untyped) {
                result.bool_val( v1.integer_val() != v2.integer_val());
            } else if(is_float_or_untyped) {
                result.bool_val( v1.float_val() != v2.float_val());
            } else if(is_bool) {
                result.bool_val( v1.bool_val() != v2.bool_val());
            } else if(is_enum) {
                result.bool_val( v1.integer_val() != v2.integer_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::DoubleAnd: {
            if(is_bool) {
                result.bool_val( v1.bool_val() && v2.bool_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::DoubleOr: {
            if(is_bool) {
                result.bool_val( v1.bool_val() || v2.bool_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }

        } break;

        default: {
            return ValueResult::err(ValueErrorKind::OperatorError);
        } break;
    }


    // 检查溢出（仅算术运算，比较运算返回 bool 无需检查）
    if(!is_return_bool_operator(op_type)) {
        if(is_int_or_untyped) {
            if(is_int_overflow) {
                return ValueResult::err(ValueErrorKind::Overflow);
            }
        } else if(is_float_or_untyped) {
            if(xp_check_f64_is_inf(result.float_val())) {
                return ValueResult::err(ValueErrorKind::Overflow);
            }
        }

        bool overflowed = false;
        if(is_int_or_untyped) {
            if(is_certain_type(v1.type)) {
                overflowed = check_integer_overflow(result.integer_val(), v1.type);
            }
        } else if(is_float_or_untyped) {
            if(is_certain_type(v1.type)) {
                overflowed = check_float_overflow(result.float_val(), v1.type);
            }
        }
        if(overflowed) {
            return ValueResult::err(ValueErrorKind::Overflow);
        }
    }

    // 设置结果类型
    if(is_return_bool_operator(op_type) || is_bool) {
        result.set_type(easy_type(Type_bool));
    } else if(is_int_or_untyped) {
        result.set_type(v1.type);
    } else if(is_float_or_untyped) {
        result.set_type(v1.type);
    } else {
        std::unreachable();
    }
    

    return ValueResult::ok(result);

}




ValueResult exec_unary(Value &operand, TokenType op) {
    if(!is_integer_or_untyped_type(operand.type) &&
       !is_float_or_untyped_type(operand.type) &&
       operand.type != easy_type(Type_bool)) {
        return ValueResult::err(ValueErrorKind::TypeError);
    }

    Value result = make_value();

    bool is_int_or_untyped = is_integer_or_untyped_type(operand.type);
    bool is_float_or_untyped = is_float_or_untyped_type(operand.type);
    bool is_bool = operand.type == easy_type(Type_bool);

    bool is_int_overflow = false;
    switch(op) {
        case TokenType::Minus: {
            if(is_int_or_untyped) {
                i128 res;
                is_int_overflow = xp_check_i128_neg_overflow(operand.integer_val(), &res);
                result.integer_val(res);
            } else if(is_float_or_untyped) {
                result.float_val(-operand.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);
            }
        } break;

        case TokenType::Exclamation: {
            if(is_bool) {
                result.bool_val( !operand.bool_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);
            }
        } break;

        default: {
            return ValueResult::err(ValueErrorKind::OperatorError);
        } break;
    }

    if(is_int_or_untyped) {
        if(is_int_overflow) {
            return ValueResult::err(ValueErrorKind::Overflow);
        }
    } else if(is_float_or_untyped) {
        if(xp_check_f64_is_inf(result.float_val())) {
            return ValueResult::err(ValueErrorKind::Overflow);
        }
    }

    bool overflowed = false;
    if(is_int_or_untyped) {
        if(is_certain_type(operand.type)) {
            overflowed = check_integer_overflow(result.integer_val(), operand.type);
        }
    } else if(is_float_or_untyped) {
        if(is_certain_type(operand.type)) {
            overflowed = check_float_overflow(result.float_val(), operand.type);
        }
    }
    if(overflowed) {
        return ValueResult::err(ValueErrorKind::Overflow);
    }

    if(is_int_or_untyped) {
        result.set_type(operand.type);
    } else if(is_float_or_untyped) {
        result.set_type(operand.type);
    } else if(is_bool) {
        result.set_type(easy_type(Type_bool));
    } else {
        std::unreachable();
    }

    return ValueResult::ok(result);
}




ValueResult exec_cast(Value &val, TypeRef target_type) {
    if(val.type == target_type) {
        return ValueResult::ok(val);
    }

    Value result = val;
    result.set_type(target_type);

    bool target_needs_integer = is_integer_type(target_type) || is_enum_type(target_type);
    bool target_needs_float   = is_float_type(target_type);
    bool target_needs_bool    = (target_type->kind == Type_bool);

    if((target_needs_integer && val.actual_type() == ActualValueType::Integer) ||
       (target_needs_float   && val.actual_type() == ActualValueType::Float)   ||
       (target_needs_bool    && val.actual_type() == ActualValueType::Bool)) {
        if(target_needs_integer && val.actual_type() == ActualValueType::Integer) {
            if(check_integer_overflow(result.integer_val(), target_type)) {
                return ValueResult::err(ValueErrorKind::Overflow);
            }
        } else if(target_needs_float && val.actual_type() == ActualValueType::Float) {
            if(check_float_overflow(result.float_val(), target_type)) {
                return ValueResult::err(ValueErrorKind::Overflow);
            }
        }
        return ValueResult::ok(result);
    }

    if(val.actual_type() == ActualValueType::Integer && target_needs_float) {
        result.float_val(static_cast<double>(val.integer_val()));
        return ValueResult::ok(result);
    }

    if(val.actual_type() == ActualValueType::Float && target_needs_integer) {
        result.integer_val(static_cast<i128>(val.float_val()));
        return ValueResult::ok(result);
    }

    if(val.actual_type() == ActualValueType::Integer && target_needs_bool) {
        result.bool_val( (val.integer_val() != 0));
        return ValueResult::ok(result);
    }

    if(val.actual_type() == ActualValueType::Bool && target_needs_integer) {
        result.integer_val(val.bool_val() ? 1 : 0);
        return ValueResult::ok(result);
    }

    return ValueResult::err(ValueErrorKind::TypeError);
}

