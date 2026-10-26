#pragma once

#include "type.hpp"
#include "cir_instruction_ref.hpp"

struct CIRPackage;


bool check_explicit_type_cast(CIRPackage *pkg, CIRInstructionRef casted_inst_ref, TypeRef casted_expr_type, TypeRef target_type);

// extern_C 签名约束（数组形参/返回值）：返回 true 表示合法；否则把原因写入 err_msg
bool check_extern_c_func(xpAllocator allocator, const Array<TypeRef>& param_types, TypeRef return_type, xpString& err_msg);


std::optional<xpString> check_var_type(xpAllocator allocator, const Value& val, bool is_param = false);
std::optional<xpString> check_const_type(xpAllocator allocator, TypeRef type);


bool check_untyped_int_to_type(i128 value, TypeRef target_type);
bool check_untyped_float_to_type(double value, TypeRef target_type);


TypeRef get_compliable_integer_type(i128 value);
TypeRef get_compliable_float_type(double value);



std::optional<TypeRef> default_certain_type_for_untyped_type_opt(TypeRef untyped_type);
TypeRef default_certain_type_for_untyped_type(TypeRef untyped_type);