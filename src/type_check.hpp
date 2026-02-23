#pragma once

#include "analyser.hpp"


bool fit_in_type(i128 value, TypeRef target_type);
bool check_untyped_to_type(double value, TypeRef target_type);
TypeRef get_compliable_integer_type(i128 value);

TypeRef infer_expr_type(Ast *expr, bool has_target, TypeRef target_type, Analyser analyser, bool allow_untyped);