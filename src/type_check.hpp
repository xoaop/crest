#pragma once

#include "type.hpp"
#include "cir_instruction_ref.hpp"

struct CIRPackage;


bool check_explicit_type_cast(CIRPackage *pkg, CIRInstructionRef casted_inst_ref, TypeRef casted_expr_type, TypeRef target_type);


// 类型位置取值的限制：必须真的是类型值，且该类型有存储布局（变量、结构体/联合体字段）。
// 合法时返回 nullopt。is_param: 函数形参（含类型形参 $T，其类型就是 'type'）
std::optional<xpString> check_var_type(const Value& val, bool is_param = false);


bool check_untyped_int_to_type(i128 value, TypeRef target_type);
bool check_untyped_float_to_type(double value, TypeRef target_type);


TypeRef get_compliable_integer_type(i128 value);
TypeRef get_compliable_float_type(double value);



std::optional<TypeRef> default_certain_type_for_untyped_type_opt(TypeRef untyped_type);
TypeRef default_certain_type_for_untyped_type(TypeRef untyped_type);