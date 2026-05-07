#ifndef CREST_VALUE_OPS_HPP
#define CREST_VALUE_OPS_HPP

#include "value.hpp"
#include "tokenizer.hpp"

// 二元运算, 包括 + - * / % > < >= <= == != && ||
ValueResult eval_binary_expr(Value& v1, Value& v2, TokenType op_type);

// 一元运算, 包括 - !
ValueResult eval_unary_expr(Value& v, TokenType op_type);

// 一元算数运算, 目前只有一元负号
ValueResult unary_arithmetic_value(Value& v, TokenType op_type);

// 一元布尔运算, 目前只有逻辑非
ValueResult unary_boolean_value(Value& v, TokenType op_type);

// 处理类型转化
ValueResult eval_cast_expr(Value& v, TypeRef target_type);


#endif // CREST_VALUE_OPS_HPP
