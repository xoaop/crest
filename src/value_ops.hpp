#ifndef CREST_VALUE_OPS_HPP
#define CREST_VALUE_OPS_HPP

#include "value.hpp"
#include "tokenizer.hpp"

// 二元运算, 包括 + - * / % > < >= <= == != && ||
Value eval_binary_expr(Value& v1, Value& v2, TokenType op_type);

// 一元运算, 包括 - !
Value eval_unary_expr(Value& v, TokenType op_type);

// 一元算数运算, 目前只有一元负号
Value unary_arithmetic_value(Value& v, TokenType op_type);

// 一元布尔运算, 目前只有逻辑非
Value unary_boolean_value(Value& v, TokenType op_type);



#endif // CREST_VALUE_OPS_HPP
