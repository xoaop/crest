#pragma once

#include "type.hpp"
#include "cir_instruction_ref.hpp"

struct CIRPackage;


bool check_explicit_type_cast(CIRPackage *pkg, CIRInstructionRef casted_inst_ref, TypeRef casted_expr_type, TypeRef target_type);


bool check_untyped_int_to_type(i128 value, TypeRef target_type);
bool check_untyped_float_to_type(double value, TypeRef target_type);


TypeRef get_compliable_integer_type(i128 value);
TypeRef get_compliable_float_type(double value);



std::optional<TypeRef> default_certain_type_for_untyped_type_opt(TypeRef untyped_type);
TypeRef default_certain_type_for_untyped_type(TypeRef untyped_type);