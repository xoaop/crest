// #include "value_ops.hpp"



// ValueResult eval_binary_expr(Value& v1, Value& v2, TokenType op_type) {
//     if(v1.has_error()) {
//         return ValueResult::err(ValueErrorKind::ErrorValue);
//     }
//     if(v2.state == ValueState::Error) {
//         return ValueResult::err(ValueErrorKind::ErrorValue);
//     }


//     if(v1.is_runtime_value || v2.is_runtime_value) {
//         return ValueResult::err(ValueErrorKind::UsingRuntimeValue);
//     }

//     // 目前只处理整数, 浮点数, bool类型的二元表达式, 其他类型的二元表达式如指针、结构体、数组等的二元表达式不在编译时求值的范围内
//     // 再加上enum
//     if(!((is_integer_or_untyped_type(v1.type) && is_integer_or_untyped_type(v2.type)) ||
//          (is_float_or_untyped_type(v1.type) && is_float_or_untyped_type(v2.type)) ||
//          (v1.type == easy_type(Type_bool) && v2.type == easy_type(Type_bool)) ||
//          (is_enum_type(v1.type) && is_enum_type(v2.type)))) {
//         return ValueResult::err(ValueErrorKind::TypeError);
//     }

//     // 在只有整数, 浮点数, bool类型的情况下, 只有当两个操作数类型完全相同时才进行计算, 否则报错, 例如untyped int和i32不进行计算, untyped float和f64不进行计算, i32和f32不进行计算等
//     if(v1.type != v2.type) {
//         return ValueResult::err(ValueErrorKind::TypeError);
//     }


//     Value result = make_comptime_sovled_val(undefined_type());

//     bool is_int_or_untyped = is_integer_or_untyped_type(v1.type) && is_integer_or_untyped_type(v2.type);
//     bool is_float_or_untyped = is_float_or_untyped_type(v1.type) && is_float_or_untyped_type(v2.type);
//     bool is_bool = v1.type == easy_type(Type_bool) && v2.type == easy_type(Type_bool);
//     bool is_enum = is_enum_type(v1.type) && is_enum_type(v2.type);

//     bool is_int_overflow = false;
//     switch(op_type) {

//         case TokenType::Add: {
//             if(is_int_or_untyped) {
//                 i128 res;
//                 is_int_overflow = xp_check_i128_add_overflow(v1.get_integer(), v2.get_integer(), &res);
//                 result.set_integer_value(res);
//             } else if(is_float_or_untyped) {
//                 result.set_float_value(v1.get_float() + v2.get_float());
//             } else {
//                 return ValueResult::err(ValueErrorKind::TypeError);
//             }
//         } break;

//         case TokenType::Minus: {
//             if(is_int_or_untyped) {
//                 i128 res;
//                 is_int_overflow = xp_check_i128_sub_overflow(v1.get_integer(), v2.get_integer(), &res);
//                 result.set_integer_value(res);
//             } else if(is_float_or_untyped) {
//                 result.set_float_value(v1.get_float() - v2.get_float());
//             } else {
//                 return ValueResult::err(ValueErrorKind::TypeError);;
//             }
//         } break;

//         case TokenType::Star: {
//             if(is_int_or_untyped) {
//                 i128 res;
//                 is_int_overflow = xp_check_i128_mul_overflow(v1.get_integer(), v2.get_integer(), &res);
//                 result.set_integer_value(res);
//             } else if(is_float_or_untyped) {
//                 result.set_float_value(v1.get_float() * v2.get_float());
//             } else {
//                 return ValueResult::err(ValueErrorKind::TypeError);;
//             }
//         } break;

//         case TokenType::ForwardSlash: {
//             if(is_int_or_untyped) {
//                 if(v2.get_integer() == 0) {
//                     return ValueResult::err(ValueErrorKind::DivideByZero);
//                 }
//                 i128 res;
//                 is_int_overflow = xp_check_i128_div_overflow(v1.get_integer(), v2.get_integer(), &res);
//                 result.set_integer_value(res);
//             } else if(is_float_or_untyped) {
//                 if(v2.get_float() == 0.0) {
//                     return ValueResult::err(ValueErrorKind::DivideByZero);
//                 }
//                 result.set_float_value(v1.get_float() / v2.get_float());
//             } else {
//                 return ValueResult::err(ValueErrorKind::TypeError);;
//             }
//         } break;

//         case TokenType::Percent: {
//             if(is_int_or_untyped) {
//                 if(v2.get_integer() == 0) {
//                     return ValueResult::err(ValueErrorKind::DivideByZero);
//                 }
//                 i128 res;
//                 is_int_overflow = xp_check_i128_mod_overflow(v1.get_integer(), v2.get_integer(), &res);
//                 result.set_integer_value(res);
//             } else {
//                 return ValueResult::err(ValueErrorKind::TypeError);;
//             }
//         } break;


//         case TokenType::GreaterThan: {
//             if(is_int_or_untyped) {
//                 result.set_bool_value(v1.get_integer() > v2.get_integer());
//             } else if(is_float_or_untyped) {
//                 result.set_bool_value(v1.get_float() > v2.get_float());
//             } else {
//                 return ValueResult::err(ValueErrorKind::TypeError);;
//             }
//         } break;

//         case TokenType::LessThan: {
//             if(is_int_or_untyped) {
//                 result.set_bool_value(v1.get_integer() < v2.get_integer());
//             } else if(is_float_or_untyped) {
//                 result.set_bool_value(v1.get_float() < v2.get_float());
//             } else {
//                 return ValueResult::err(ValueErrorKind::TypeError);;
//             }
//         } break;

//         case TokenType::GreaterEqual: {
//             if(is_int_or_untyped) {
//                 result.set_bool_value(v1.get_integer() >= v2.get_integer());
//             } else if(is_float_or_untyped) {
//                 result.set_bool_value(v1.get_float() >= v2.get_float());
//             } else {
//                 return ValueResult::err(ValueErrorKind::TypeError);;
//             }
//         } break;

//         case TokenType::LessEqual: {
//             if(is_int_or_untyped) {
//                 result.set_bool_value(v1.get_integer() <= v2.get_integer());
//             } else if(is_float_or_untyped) {
//                 result.set_bool_value(v1.get_float() <= v2.get_float());
//             } else {
//                 return ValueResult::err(ValueErrorKind::TypeError);;
//             }
//         } break;

//         case TokenType::DoubleEqual: {
//             if(is_int_or_untyped) {
//                 result.set_bool_value(v1.get_integer() == v2.get_integer());
//             } else if(is_float_or_untyped) {
//                 result.set_bool_value(v1.get_float() == v2.get_float());
//             } else if(is_bool) {
//                 result.set_bool_value(v1.get_bool() == v2.get_bool());
//             } else if(is_enum) {
//                 result.set_bool_value(v1.get_integer() == v2.get_integer());
//             } else {
//                 return ValueResult::err(ValueErrorKind::TypeError);;
//             }
//         } break;

//         case TokenType::ExclamationEqual: {
//             if(is_int_or_untyped) {
//                 result.set_bool_value(v1.get_integer() != v2.get_integer());
//             } else if(is_float_or_untyped) {
//                 result.set_bool_value(v1.get_float() != v2.get_float());
//             } else if(is_bool) {
//                 result.set_bool_value(v1.get_bool() != v2.get_bool());
//             } else if(is_enum) {
//                 result.set_bool_value(v1.get_integer() != v2.get_integer());
//             } else {
//                 return ValueResult::err(ValueErrorKind::TypeError);;
//             }
//         } break;

//         case TokenType::DoubleAnd: {
//             if(is_bool) {
//                 result.set_bool_value(v1.get_bool() && v2.get_bool());
//             } else {
//                 return ValueResult::err(ValueErrorKind::TypeError);;
//             }
//         } break;

//         case TokenType::DoubleOr: {
//             if(is_bool) {
//                 result.set_bool_value(v1.get_bool() || v2.get_bool());
//             } else {
//                 return ValueResult::err(ValueErrorKind::TypeError);;
//             }

//         } break;

//         default: {
//             return ValueResult::err(ValueErrorKind::OperatorError);
//         } break;
//     }


//     // 检查内置溢出
//     if(is_int_or_untyped) {
//         if(is_int_overflow) {
//             return ValueResult::err(ValueErrorKind::Overflow);
//         }
//     } else if(is_float_or_untyped) {
//         if(xp_check_f64_is_inf(result.get_float())) {
//             return ValueResult::err(ValueErrorKind::Overflow);
//         }
//     }

//     // 检查类型溢出
//     bool overflowed = false;
//     if(is_int_or_untyped) {
//         if(is_certain_type(v1.type)) {
//             overflowed = check_integer_overflow(result.get_integer(), v1.type);
//         }
//     } else if(is_float_or_untyped) {
//         if(is_certain_type(v1.type)) {
//             overflowed = check_float_overflow(result.get_float(), v1.type);
//         }
//     }
//     if(overflowed) {
//         return ValueResult::err(ValueErrorKind::Overflow);
//     }

//     // 设置结果类型
//     if(is_int_or_untyped) {
//         result.set_type(v1.type);
//     } else if(is_float_or_untyped) {
//         result.set_type(v1.type);
//     } else if(is_bool) {
//         result.set_type(easy_type(Type_bool));
//     } else {
//         UNREACHABLE();
//     }
    

//     return ValueResult::ok(result);
// }



// ValueResult eval_unary_expr(Value& v, TokenType op_type) {
//     if(v.has_error()) {
//         return ValueResult::err(ValueErrorKind::ErrorValue);
//     }


//     if(v.is_runtime_value) {
//         return ValueResult::err(ValueErrorKind::UsingRuntimeValue);
//     }


//     switch(op_type) {
//         case TokenType::Minus:
//             return unary_arithmetic_value(v, op_type);
//         case TokenType::Exclamation:
//             return unary_boolean_value(v, op_type);
//         default:
//             return ValueResult::err(ValueErrorKind::OperatorError);
//     }
// }


// // 一元算数
// ValueResult unary_arithmetic_value(Value& v, TokenType op_type) {
//     if(v.is_runtime_value) {
//         return ValueResult::err(ValueErrorKind::UsingRuntimeValue);
//     }

//     if(!is_integer_or_untyped_type(v.type) && !is_float_or_untyped_type(v.type)) {
//         return ValueResult::err(ValueErrorKind::TypeError);
//     }

//     Value result = make_value();
//     if(is_integer_or_untyped_type(v.type)) {
//         switch(op_type) {
//             case TokenType::Minus: {
//                 result.set_integer_value(-v.get_integer());
//             } break;
//             default: {
//                 return ValueResult::err(ValueErrorKind::OperatorError);
//             } break;
//         }

//         result.set_type(v.type);

//         // 溢出检查
//         bool overflowed = false;
//         if(is_certain_type(v.type)) {
//             overflowed = check_integer_overflow(result.get_integer(), v.type);
//         }

//     } else if(is_float_or_untyped_type(v.type)) {
//         switch(op_type) {
//             case TokenType::Minus: {
//                 result.set_float_value(-v.get_float());
//             } break;
//             default: {
//                 return ValueResult::err(ValueErrorKind::OperatorError);
//             } break;
//         }

//         result.set_type(v.type);

//         // 溢出检查
//         bool overflowed = false;
//         if(is_certain_type(v.type)) {
//             overflowed = check_float_overflow(result.get_float(), v.type);
//         }
//     } else {
//         return ValueResult::err(ValueErrorKind::TypeError);;
//     }

//     return ValueResult::ok(result);

// }




// /// @brief 一元布尔运算, 目前只有逻辑非
// /// @param v 
// /// @param op_type 
// /// @return 
// ValueResult unary_boolean_value(Value& v, TokenType op_type) {
//     if(v.is_runtime_value) {
//         return ValueResult::err(ValueErrorKind::UsingRuntimeValue);
//     }

//     if(v.type != easy_type(Type_bool)) {
//         return ValueResult::err(ValueErrorKind::TypeError);;
//     }

//     Value result = make_comptime_sovled_val(undefined_type());
//     switch(op_type) {
//         case TokenType::Exclamation: {
//             result.set_bool_value(!v.get_bool());
//         } break;
//         default: {
//             return ValueResult::err(ValueErrorKind::OperatorError);
//         } break;
//     }

//     result.set_type(easy_type(Type_bool));

//     return ValueResult::ok(result);
// }




// ValueResult eval_cast_expr(Value& v, TypeRef target_type) {
//     if(v.has_error()) {
//         return ValueResult::err(ValueErrorKind::ErrorValue);
//     }

//     if(v.is_runtime_value) {
//         return ValueResult::err(ValueErrorKind::UsingRuntimeValue);
//     }

//     // 如果类型相同，直接返回
//     if(v.type == target_type) {
//         return ValueResult::ok(v);
//     }

//     // 先完整复制源值的所有内容（包括actual_kind和实际值）
//     Value result = v;
//     // 仅更新类型标签
//     result.set_type(target_type);

//     // 设计说明：类型合法性和溢出检查已经由typecheck阶段处理
//     // 本函数只负责执行实际的数值转换和actual_kind迁移
//     // 只有当源actual_kind与目标类型要求的存储类型不匹配时，才需要转换值

//     // 判断目标类型需要的存储类型
//     bool target_needs_integer = is_integer_type(target_type) || is_enum_type(target_type);
//     bool target_needs_float = is_float_type(target_type);
//     bool target_needs_bool = (target_type->kind == Type_bool);
//     bool target_needs_pointer = is_pointer_type(target_type);

//     // 情况1：存储类型匹配，不需要转换值，直接返回
//     // 包括：整数↔整数、浮点↔浮点、枚举↔整数、指针↔指针 等场景
//     if( (target_needs_integer && v.is_integer_stored()) ||
//         (target_needs_float && v.is_float_stored()) ||
//         (target_needs_bool && v.is_bool_stored()) ||
//         (target_needs_pointer && v.is_pointer_stored()) ) {
//         return ValueResult::ok(result);
//     }

//     // 情况2：存储类型不匹配，需要转换值

//     // 整数 → 浮点
//     if(v.is_integer_stored() && target_needs_float) {
//         result.set_float_value(static_cast<double>(v.get_integer()));
//         return ValueResult::ok(result);
//     }

//     // 浮点 → 整数
//     if(v.is_float_stored() && target_needs_integer) {
//         result.set_integer_value(static_cast<i128>(v.get_float()));
//         return ValueResult::ok(result);
//     }

//     // 整数 → bool
//     if(v.is_integer_stored() && target_needs_bool) {
//         result.set_bool_value(v.get_integer() != 0);
//         return ValueResult::ok(result);
//     }

//     // bool → 整数
//     if(v.is_bool_stored() && target_needs_integer) {
//         result.set_integer_value(v.get_bool() ? 1 : 0);
//         return ValueResult::ok(result);
//     }

//     // 不支持的转换类型（typecheck阶段应该已经过滤，这里是安全兜底）
//     return ValueResult::err(ValueErrorKind::TypeError);
// }
