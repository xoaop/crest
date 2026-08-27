#ifndef CREST_VALUE_OPS_HPP
#define CREST_VALUE_OPS_HPP

#include "value.hpp"
#include "tokenizer.hpp"


// 二元运算
ValueResult exec_binary(Value &v1, Value &v2, TokenType op_type);

// 一元运算
ValueResult exec_unary(Value &operand, TokenType op);

// cast运算
ValueResult exec_cast(Value &val, TypeRef target_type);




#endif // CREST_VALUE_OPS_HPP
