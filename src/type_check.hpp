#pragma once

#include "analyser.hpp"


bool fit_in_type(i128 value, TypeRef target_type);
bool fit_in_type(double value, TypeRef target_type);
TypeRef get_compliable_integer_type(i128 value);

// void infer_expr_type_without_target(Ast *expr, Analyser *analyser);
// void check_type(Ast *expr, Type target_type, Analyser *analyser);
// void infer_expr_type(Ast *expr, Type *target_type, Analyser *analyser);
void infer_expr_type(Ast *expr, bool has_target, TypeRef target_type, Analyser analyser);