#pragma once

#include "analyser.hpp"


TypeRef infer_expr_type(Ast *expr, bool has_target, TypeRef target_type, Analyser analyser, bool allow_untyped);