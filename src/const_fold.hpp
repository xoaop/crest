#include "analyser.hpp"

i128 operation_integer_ast(TokenType op_type, Ast *left_c, Ast *right_c);
double operation_float_ast(TokenType op_type, Ast *left_c, Ast *right_c);

void try_constant_expr_folding(Ast *const_expr);
