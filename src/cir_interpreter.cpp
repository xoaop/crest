#include "cir_interpreter.hpp"
#include "scope.hpp"
#include "type.hpp"
#include "context.hpp"
#include "symbol.hpp"
#include "package.hpp"

#include <intrin.h>

// 内联运算
static ValueResult exec_binary(Value &v1, Value &v2, TokenType op_type) {
    if(!((is_integer_or_untyped_type(v1.type) && is_integer_or_untyped_type(v2.type)) ||
        (is_float_or_untyped_type(v1.type) && is_float_or_untyped_type(v2.type)) ||
        (v1.type == easy_type(Type_bool) && v2.type == easy_type(Type_bool)) ||
        (is_enum_type(v1.type) && is_enum_type(v2.type)))) {

        return ValueResult::err(ValueErrorKind::TypeError);
    }

    if(v1.type != v2.type) {
        return ValueResult::err(ValueErrorKind::TypeError);
    }

    Value result = make_value();

    bool is_int_or_untyped = is_integer_or_untyped_type(v1.type) && is_integer_or_untyped_type(v2.type);
    bool is_float_or_untyped = is_float_or_untyped_type(v1.type) && is_float_or_untyped_type(v2.type);
    bool is_bool = v1.type == easy_type(Type_bool) && v2.type == easy_type(Type_bool);
    bool is_enum = is_enum_type(v1.type) && is_enum_type(v2.type);

    bool is_int_overflow = false;
    switch(op_type) {

        case TokenType::Add: {
            if(is_int_or_untyped) {
                i128 res;
                is_int_overflow = xp_check_i128_add_overflow(v1.integer_val(), v2.integer_val(), &res);
                result.integer_val(res);
            } else if(is_float_or_untyped) {
                result.float_val(v1.float_val() + v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);
            }
        } break;

        case TokenType::Minus: {
            if(is_int_or_untyped) {
                i128 res;
                is_int_overflow = xp_check_i128_sub_overflow(v1.integer_val(), v2.integer_val(), &res);
                result.integer_val(res);
            } else if(is_float_or_untyped) {
                result.float_val(v1.float_val() - v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::Star: {
            if(is_int_or_untyped) {
                i128 res;
                is_int_overflow = xp_check_i128_mul_overflow(v1.integer_val(), v2.integer_val(), &res);
                result.integer_val(res);
            } else if(is_float_or_untyped) {
                result.float_val(v1.float_val() * v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::ForwardSlash: {
            if(is_int_or_untyped) {
                if(v2.integer_val() == 0) {
                    return ValueResult::err(ValueErrorKind::DivideByZero);
                }
                i128 res;
                is_int_overflow = xp_check_i128_div_overflow(v1.integer_val(), v2.integer_val(), &res);
                result.integer_val(res);
            } else if(is_float_or_untyped) {
                if(v2.float_val() == 0.0) {
                    return ValueResult::err(ValueErrorKind::DivideByZero);
                }
                result.float_val(v1.float_val() / v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::Percent: {
            if(is_int_or_untyped) {
                if(v2.integer_val() == 0) {
                    return ValueResult::err(ValueErrorKind::DivideByZero);
                }
                i128 res;
                is_int_overflow = xp_check_i128_mod_overflow(v1.integer_val(), v2.integer_val(), &res);
                result.integer_val(res);
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;


        case TokenType::GreaterThan: {
            if(is_int_or_untyped) {
                result.bool_val( v1.integer_val() > v2.integer_val());
            } else if(is_float_or_untyped) {
                result.bool_val( v1.float_val() > v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::LessThan: {
            if(is_int_or_untyped) {
                result.bool_val( v1.integer_val() < v2.integer_val());
            } else if(is_float_or_untyped) {
                result.bool_val( v1.float_val() < v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::GreaterEqual: {
            if(is_int_or_untyped) {
                result.bool_val( v1.integer_val() >= v2.integer_val());
            } else if(is_float_or_untyped) {
                result.bool_val( v1.float_val() >= v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::LessEqual: {
            if(is_int_or_untyped) {
                result.bool_val( v1.integer_val() <= v2.integer_val());
            } else if(is_float_or_untyped) {
                result.bool_val( v1.float_val() <= v2.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::DoubleEqual: {
            if(is_int_or_untyped) {
                result.bool_val( v1.integer_val() == v2.integer_val());
            } else if(is_float_or_untyped) {
                result.bool_val( v1.float_val() == v2.float_val());
            } else if(is_bool) {
                result.bool_val( v1.bool_val() == v2.bool_val());
            } else if(is_enum) {
                result.bool_val( v1.integer_val() == v2.integer_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::ExclamationEqual: {
            if(is_int_or_untyped) {
                result.bool_val( v1.integer_val() != v2.integer_val());
            } else if(is_float_or_untyped) {
                result.bool_val( v1.float_val() != v2.float_val());
            } else if(is_bool) {
                result.bool_val( v1.bool_val() != v2.bool_val());
            } else if(is_enum) {
                result.bool_val( v1.integer_val() != v2.integer_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::DoubleAnd: {
            if(is_bool) {
                result.bool_val( v1.bool_val() && v2.bool_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }
        } break;

        case TokenType::DoubleOr: {
            if(is_bool) {
                result.bool_val( v1.bool_val() || v2.bool_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);;
            }

        } break;

        default: {
            return ValueResult::err(ValueErrorKind::OperatorError);
        } break;
    }


    // 检查内置溢出
    if(is_int_or_untyped) {
        if(is_int_overflow) {
            return ValueResult::err(ValueErrorKind::Overflow);
        }
    } else if(is_float_or_untyped) {
        if(xp_check_f64_is_inf(result.float_val())) {
            return ValueResult::err(ValueErrorKind::Overflow);
        }
    }

    // 检查类型溢出
    bool overflowed = false;
    if(is_int_or_untyped) {
        if(is_certain_type(v1.type)) {
            overflowed = check_integer_overflow(result.integer_val(), v1.type);
        }
    } else if(is_float_or_untyped) {
        if(is_certain_type(v1.type)) {
            overflowed = check_float_overflow(result.float_val(), v1.type);
        }
    }
    if(overflowed) {
        return ValueResult::err(ValueErrorKind::Overflow);
    }

    // 设置结果类型
    if(is_return_bool_operator(op_type) || is_bool) {
        result.set_type(easy_type(Type_bool));
    } else if(is_int_or_untyped) {
        result.set_type(v1.type);
    } else if(is_float_or_untyped) {
        result.set_type(v1.type);
    } else {
        UNREACHABLE();
    }
    

    return ValueResult::ok(result);

}




static ValueResult exec_unary(Value &operand, TokenType op) {
    if(!is_integer_or_untyped_type(operand.type) &&
       !is_float_or_untyped_type(operand.type) &&
       operand.type != easy_type(Type_bool)) {
        return ValueResult::err(ValueErrorKind::TypeError);
    }

    Value result = make_value();

    bool is_int_or_untyped = is_integer_or_untyped_type(operand.type);
    bool is_float_or_untyped = is_float_or_untyped_type(operand.type);
    bool is_bool = operand.type == easy_type(Type_bool);

    bool is_int_overflow = false;
    switch(op) {
        case TokenType::Minus: {
            if(is_int_or_untyped) {
                i128 res;
                is_int_overflow = xp_check_i128_neg_overflow(operand.integer_val(), &res);
                result.integer_val(res);
            } else if(is_float_or_untyped) {
                result.float_val(-operand.float_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);
            }
        } break;

        case TokenType::Exclamation: {
            if(is_bool) {
                result.bool_val( !operand.bool_val());
            } else {
                return ValueResult::err(ValueErrorKind::TypeError);
            }
        } break;

        default: {
            return ValueResult::err(ValueErrorKind::OperatorError);
        } break;
    }

    if(is_int_or_untyped) {
        if(is_int_overflow) {
            return ValueResult::err(ValueErrorKind::Overflow);
        }
    } else if(is_float_or_untyped) {
        if(xp_check_f64_is_inf(result.float_val())) {
            return ValueResult::err(ValueErrorKind::Overflow);
        }
    }

    bool overflowed = false;
    if(is_int_or_untyped) {
        if(is_certain_type(operand.type)) {
            overflowed = check_integer_overflow(result.integer_val(), operand.type);
        }
    } else if(is_float_or_untyped) {
        if(is_certain_type(operand.type)) {
            overflowed = check_float_overflow(result.float_val(), operand.type);
        }
    }
    if(overflowed) {
        return ValueResult::err(ValueErrorKind::Overflow);
    }

    if(is_int_or_untyped) {
        result.set_type(operand.type);
    } else if(is_float_or_untyped) {
        result.set_type(operand.type);
    } else if(is_bool) {
        result.set_type(easy_type(Type_bool));
    } else {
        UNREACHABLE();
    }

    return ValueResult::ok(result);
}




static ValueResult exec_cast(Value &val, TypeRef target_type) {
    if(val.type == target_type) {
        return ValueResult::ok(val);
    }

    Value result = val;
    result.set_type(target_type);

    bool target_needs_integer = is_integer_type(target_type) || is_enum_type(target_type);
    bool target_needs_float   = is_float_type(target_type);
    bool target_needs_bool    = (target_type->kind == Type_bool);

    if((target_needs_integer && val.actual_type() == ActualValueType::Integer) ||
       (target_needs_float   && val.actual_type() == ActualValueType::Float)   ||
       (target_needs_bool    && val.actual_type() == ActualValueType::Bool)) {
        if(target_needs_integer && val.actual_type() == ActualValueType::Integer) {
            if(check_integer_overflow(result.integer_val(), target_type)) {
                return ValueResult::err(ValueErrorKind::Overflow);
            }
        } else if(target_needs_float && val.actual_type() == ActualValueType::Float) {
            if(check_float_overflow(result.float_val(), target_type)) {
                return ValueResult::err(ValueErrorKind::Overflow);
            }
        }
        return ValueResult::ok(result);
    }

    if(val.actual_type() == ActualValueType::Integer && target_needs_float) {
        result.float_val(static_cast<double>(val.integer_val()));
        return ValueResult::ok(result);
    }

    if(val.actual_type() == ActualValueType::Float && target_needs_integer) {
        result.integer_val(static_cast<i128>(val.float_val()));
        return ValueResult::ok(result);
    }

    if(val.actual_type() == ActualValueType::Integer && target_needs_bool) {
        result.bool_val( (val.integer_val() != 0));
        return ValueResult::ok(result);
    }

    if(val.actual_type() == ActualValueType::Bool && target_needs_integer) {
        result.integer_val(val.bool_val() ? 1 : 0);
        return ValueResult::ok(result);
    }

    return ValueResult::err(ValueErrorKind::TypeError);
}



bool check_explicit_type_cast(CIRInstruction *casted_inst, TypeRef casted_expr_type, TypeRef target_type) {

    if((is_integer_or_untyped_type(casted_expr_type) || is_float_or_untyped_type(casted_expr_type)) && (is_integer_or_untyped_type(target_type) || is_float_or_untyped_type(target_type))) {
        // 任意数字类型之间都可以转化

        return true;
    } else if(is_pointer_type(casted_expr_type) && is_pointer_type(target_type)) {
        //
        //  pointer -> pointer 允许任何指针类型之间的转化
        //
        return true;

    } else if(is_array_type(casted_expr_type) && is_slice_struct_type(target_type)) {
        //
        // 数组可以转化为slice结构体类型
        //

        // 前提: 元素类型相同, 且操作数是IdentRef(稳定栈槽), 因为slice的.ptr需要稳定地址
        if(casted_inst->result.value_kind == CIRValueKind::LValue
            && target_type->struct_info.struct_fields[0].type->pointed_type == casted_expr_type->array_info.element_type) {
            return true;
        } else {
            return false;
        }
    } else if(is_enum_type(casted_expr_type)) {
        // 枚举类型可以转化为其底层整数类型
        if(casted_expr_type->enum_info.element_type == target_type) {
            return true;
        } else {
            return false;
        }
    }
    
    else {
        // 其他类型之间只能转化为完全相同的类型
        if(casted_expr_type == target_type) {
            return true;
        } else {
            return false;
        }
    }

}



bool check_untyped_int_to_type(i128 value, TypeRef target_type) {
    // 如连整数类型都不是, 那肯定无法转化
    if(!is_integer_or_untyped_type(target_type)) {
        return false;
    }


    // 在整数的基础上, 再检查溢出
    return !check_integer_overflow(value, target_type);
}

bool check_untyped_float_to_type(double value, TypeRef target_type) {
    // 如连浮点数类型都不是, 那肯定无法转化
    if(!is_float_or_untyped_type(target_type)) {
        return false;
    }

    // 在浮点数的基础上, 再检查溢出
    return !check_float_overflow(value, target_type);
}

bool is_val_overflow(const Value& val) {
    if(is_integer_type(val.type)) {
        return check_integer_overflow(val.integer_val(), val.type);
    } else if(is_float_type(val.type)) {
        return check_float_overflow(val.float_val(), val.type);
    } else {
        return false;
    }
}

bool check_untyped_to_type(Value& val, TypeRef target_type) {
    if(val.type == easy_type(Type_untyped_int)) {
        return check_untyped_int_to_type(val.integer_val(), target_type);
    } else if(val.type == easy_type(Type_untyped_float)) {
        return check_untyped_float_to_type(val.float_val(), target_type);
    } else {
        UNREACHABLE();
        return false;
    }
}

TypeRef get_compliable_integer_type(i128 value) {

    if(check_untyped_int_to_type(value, easy_type(Type_i32))) {
        return easy_type(Type_i32);
    } else if(check_untyped_int_to_type(value, easy_type(Type_i64))) {
        return easy_type(Type_i64);
    } else if(check_untyped_int_to_type(value, easy_type(Type_u64))) {
        return easy_type(Type_u64);
    } else {
        return error_type();
    }
}

TypeRef get_compliable_float_type(double value) {

    if(check_untyped_float_to_type(value, easy_type(Type_f32))) {
        return easy_type(Type_f32);
    } else if(check_untyped_float_to_type(value, easy_type(Type_f64))) {
        return easy_type(Type_f64);
    } else {
        return error_type();
    }

}



TypeRef get_compliable_const_type(Value& val) {
    XP_ASSERT_DEFAULT(is_untyped_type(val.type));


    if(val.type == easy_type(Type_untyped_int)) {
        return get_compliable_integer_type(val.integer_val());
    } else if(val.type == easy_type(Type_untyped_float)) {
        return get_compliable_float_type(val.float_val());
    } else {
        UNREACHABLE();
        return error_type();
    }
}




//
//
//




Interpreter::Interpreter(xpAllocator allocator) {
    pc_stack = make_array<AnalyzeFlowState>(allocator);
    eval_mode_stack = make_array<EvalMode>(allocator);
    loop_stack = make_array<CIRInstructionRef>(allocator);
}

Interpreter::~Interpreter() {
    array_free(&pc_stack);
    array_free(&eval_mode_stack);
}

Interpreter::AnalyzeFlowState& Interpreter::curr_state() { return pc_stack.back(); }
CIRInstructionRef&               Interpreter::pc()        { return curr_state().pc; }
Scope*&                           Interpreter::scope()    { return curr_state().scope; }




//
// 分析入口
//
void analyze_package(Package *pkg) {
    defer(xp_arena_allocator_clear(stage_allocator()));

    DEBUG_TRACE("start analyzing package {}", pkg->path);

    Interpreter interpreter(stage_allocator());
    interpreter.analyze_cir_package(&pkg->cir_package);
}


//
// interp 入口
//
void Interpreter::analyze_cir_package(CIRPackage* cir_package) {
    cir_package->all_func_inst_sym_scopes = make_array<std::tuple<CIRInstructionRef, SymbolInfo*, Scope*>>(permanent_allocator());

    pkg = cir_package;

    if(pkg->file_ranges.count == 0) {
        return;
    }

    for(isize fi = 0; fi < pkg->file_ranges.count; fi++) {
        auto& range = pkg->file_ranges[fi];
        pc_stack.clear();
        pc_stack.push_back({range.file_scope, range.start});

        CIRInstructionRef end = (fi + 1 < pkg->file_ranges.count)
            ? pkg->file_ranges[fi + 1].start
            : pkg->instructions.count;

        while(pc() < end) {
            analyze_instruction();
        }
    }
}



void Interpreter::analyze_instruction(std::optional<CIROperator> expected_op, AnalyzeParams params) {
    CIRInstruction *inst = pkg->inst(pc());

    DEBUG_TRACE("curr pc(): {}, inst: {}, src_loc: {}", pc(), inst->to_string(), inst->src_loc);


    if(expected_op.has_value()) {
        if(inst->op != expected_op.value()) {
            DEBUG_PANIC("Expected instruction {} at pc() {}, but got {}", string(expected_op.value()), pc(), string(inst->op));
        }
    }

    switch(inst->op) {
        case CIROperator::Block:         analyze_block(params.block_eval_mode); break;
        case CIROperator::Break:         analyze_break(); break;
        case CIROperator::CondBr:        analyze_condbr(); break;
        case CIROperator::Loop:          analyze_loop(); break;

        case CIROperator::ConstDecl:     analyze_const_decl(); break;
        case CIROperator::FunctionDecl:  analyze_function_decl(); break;

        case CIROperator::Binary:        analyze_binary(); break;
        case CIROperator::Unary:         analyze_unary(); break;
        case CIROperator::Call:          analyze_call(); break;
        case CIROperator::Cast:          analyze_cast(); break;
        case CIROperator::FieldAccess:   analyze_field_access(); break;
        case CIROperator::FieldPtr:      analyze_field_ptr(); break;
        case CIROperator::StructInit:    analyze_struct_init(); break;
        case CIROperator::ArrayInit:     analyze_array_init(); break;
        case CIROperator::Index:         analyze_index(); break;
        case CIROperator::IndexPtr:      analyze_index_ptr(); break;

        case CIROperator::VariableDecl:  analyze_variable_decl(); break;
        case CIROperator::IdentRef:      analyze_ident_ref(); break;
        case CIROperator::IdentVal:      analyze_ident_val(); break;
        case CIROperator::Load:          analyze_load(); break;
        case CIROperator::Store:         analyze_store(); break;
        case CIROperator::TypeAscribe:   analyze_type_ascribe(); break;

        case CIROperator::ConstantValue: analyze_constant_value(); break;

        case CIROperator::PointerType:   analyze_pointer_type(); break;
        case CIROperator::ArrayType:     analyze_array_type(); break;
        case CIROperator::SliceType:     analyze_slice_type(); break;
        case CIROperator::FuncType:      analyze_func_type(); break;

        case CIROperator::GetOrInitStruct: analyze_get_or_init_struct(); break;
        case CIROperator::StructField:   analyze_struct_field(); break;
        case CIROperator::FinishStruct:  analyze_finish_struct(); break;
        case CIROperator::EnumDeclInit: analyze_enum_decl_init(); break;
        case CIROperator::UnionDecl:     analyze_union_decl(); break;

        case CIROperator::EnterScope:    analyze_enter_scope(); break;
        case CIROperator::ExitScope:     analyze_exit_scope(); break;

        case CIROperator::DetermineType: analyze_determine_type(); break;

        case CIROperator::AddrOf:        analyze_addr_of(); break;
        case CIROperator::TypeOfInstResult: analyze_type_of_inst_result(); break;
        case CIROperator::FieldTypeOfStruct: analyze_field_type_of_struct(); break;
        case CIROperator::FuncParamType: analyze_func_param_type(); break;


        default:

            DEBUG_PANIC("Unsupported CIR instruction: {}", inst->to_string());
            XP_ASSERT_MSG(false, "Unsupported CIR instruction");
            break;
    }

    pc() += 1;
}


// 控制流
void Interpreter::analyze_block(std::optional<EvalMode> force_eval_mode) {
    XP_ASSERT_DEFAULT(pkg->inst(pc())->op == CIROperator::Block);

    auto block_info = pkg->inst(pc())->block_info;

    bool pushed_eval_mode = false;

    if(force_eval_mode.has_value()) {
        eval_mode_stack.push_back(*force_eval_mode);
        pushed_eval_mode = true;
    } else if(block_info.is_comptime) {
        eval_mode_stack.push_back(EvalMode::FullEval);
        pushed_eval_mode = true;
    }

    CIRInstructionRef block_inst_ref = pc();

    isize end_pc = pc() + 1 + block_info.body_len;
    pc() += 1; // 跳过 Block 指令本身，进入 Block 内部指令分析
    while(pc() < end_pc && !has_result_val(block_inst_ref)) {
        analyze_instruction();
    }
    pc() = end_pc - 1; // FullEval 提前退出时跳过剩余指令


    if(pushed_eval_mode) {
        eval_mode_stack.pop_back();
    }

}

void Interpreter::analyze_break() {
    auto info = pkg->inst(pc())->break_info;

    CIRInstructionRef target_block = info.break_block;
    XP_ASSERT_DEFAULT(pkg->inst(target_block)->op == CIROperator::Block || pkg->inst(target_block)->op == CIROperator::Loop);


    if(info.break_value_inst != INVALID_INST) {

        if(propagate_error({info.break_value_inst})) { Set_ResultError(target_block); return; }

        // 如果已经设置类型了, 判断类型是否匹配
        if(result_state(target_block) >= CIRResultType::OnlyType) {
            TypeRef existing_type = ResultType(target_block);
            TypeRef new_type = ResultType(info.break_value_inst);
            if(existing_type != new_type) {
                context()->reporter.report_error(pkg->inst(pc())->src_loc, "type mismatch in break value");
                Set_ResultError(pc());
                Set_ResultError(target_block);
                return;
            }
        } else {
            // 否则设置类型
            Set_ResultType(target_block, ResultType(info.break_value_inst));
        }


        if(curr_eval_mode() == EvalMode::FullEval) {
            Set_ResultValue(target_block, ResultValue(info.break_value_inst));
        }
    } else {
        set_result_state(target_block, CIRResultType::DontHave);
    }



}

void Interpreter::analyze_condbr() {
    XP_ASSERT_DEFAULT(pkg->inst(pc())->op == CIROperator::CondBr);

    auto if_info = pkg->inst(pc())->condbr_info;
    auto cond_inst = if_info.condition_inst;
    auto true_blk = if_info.true_block_inst;
    auto false_blk = if_info.false_block_inst;

    XP_ASSERT_DEFAULT(cond_inst != INVALID_INST);
    XP_ASSERT_DEFAULT(true_blk != INVALID_INST);
    XP_ASSERT_DEFAULT(false_blk != INVALID_INST);

    auto cond_info = pkg->inst(cond_inst);
    auto true_info = pkg->inst(true_blk);
    auto false_info = pkg->inst(false_blk);
    XP_ASSERT_DEFAULT(cond_info->op != CIROperator::Block);
    XP_ASSERT_DEFAULT(true_info->op == CIROperator::Block);
    XP_ASSERT_DEFAULT(false_info->op == CIROperator::Block);

    

    if(propagate_error({cond_inst})) {
        return;
    }

    TypeRef cond_type = ResultType(cond_inst);
    if(cond_type != easy_type(Type_bool)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "condition expression of if statement must be of type bool");
        return;
    }

    if(curr_eval_mode() == EvalMode::FullEval || has_result_val(cond_inst)) {
        Value cond_val = ResultValue(cond_inst);
        bool cond = cond_val.bool_val();

        if(cond) {
            new_pc_flow(true_blk);
        } else if(false_blk != INVALID_INST) {
            new_pc_flow(false_blk);
        }

        analyze_instruction(CIROperator::Block);
        recover_pc_flow();

        propagate_error({true_blk, false_blk});
    }
}

void Interpreter::analyze_loop() {
    auto& loop_info = pkg->inst(pc())->loop_info;


    CIRInstructionRef loop_inst_ref = pc();

    isize end_pc = pc() + 1 + loop_info.body_len;
    pc() += 1; // 跳过 Block 指令本身，进入 Block 内部指令分析
    while(pc() < end_pc && !has_result_val(loop_inst_ref)) {
        analyze_instruction();
    }
    pc() = end_pc - 1; // FullEval 提前退出时跳过剩余指令

    for(isize i = loop_inst_ref + 1; i < end_pc; i++) {
        if(has_error(i)) {
            Set_ResultError(loop_inst_ref);
            break;
        }
    }

    if(curr_eval_mode() == EvalMode::FullEval) {
        XP_TODO(); // TODO: 目前不支持编译时循环
    }

}



void Interpreter::analyze_const_decl() {
    if(has_result_val(pc())) {
        return;
    }

    CIRConstDecl info = pkg->inst(pc())->const_decl;
    SymbolInfo* sym = find_symbol_until_global(scope(), info.ident);
    XP_ASSERT_DEFAULT(sym != nullptr);



    CIRInstructionRef value_inst = info.value_inst;
    XP_ASSERT_DEFAULT(value_inst == pc() + 1);

    CIRInstructionRef const_decl_inst = pc();
    pc() += 1;

    if(sym->state == SymbolState::Solved) {
        XP_ASSERT_DEFAULT(pkg->inst(value_inst)->op == CIROperator::Block);
        isize next_inst_count = pkg->inst(value_inst)->block_info.body_len + 1;
        pc() += next_inst_count - 1; // 跳过整个 const 声明和初始化过程
        return;
    }


    analyze_instruction(CIROperator::Block); // const 的值在一个 block 里计算出来
    pc() -= 1;

    Value result = ResultValue(info.value_inst);
    sym->val(result);
    sym->state = SymbolState::Solved;

    Set_ResultTypeAndValue(const_decl_inst, result);
}



void Interpreter::analyze_function_decl() {
    auto& func = pkg->inst(pc())->func_decl;

    // 解析参数类型
    Array<TypeRef> param_types = make_array<TypeRef>(stage_allocator());
    defer(array_free(&param_types));

    for(isize i = 0; i < func.arg_type_insts.count; i++) {
        if(func.arg_type_insts[i] != INVALID_INST) {
            param_types.push_back(extract_type_from_val_as_type(ResultValue(func.arg_type_insts[i])));
        } else {
            // var_arg
            param_types.push_back(easy_type(Type_var_arg_c));
        }
    }

    // 解析返回类型
    TypeRef return_type = easy_type(Type_void);
    if(func.return_type_inst != INVALID_INST) {
        return_type = extract_type_from_val_as_type(ResultValue(func.return_type_inst));
    }

    TypeRef func_type = function_type(param_types, return_type);

    Value v = make_value(func_type);
    v.func_val(func.name);

    // TODO: check
    SymbolInfo* func_sym = find_symbol_until_global(scope(), func.name);
    v.func_val_key({pkg, func_sym->package, pc()});

    DEBUG_TRACE("to set type and result for func {}", func.name);

    Set_ResultTypeAndValue(pc(), v);

    // 保存函数实例和符号，供llvmcodegen使用
    pkg->all_func_inst_sym_scopes.push_back({pc(), func_sym, scope()});


    if(func.is_extern_c) {
        // extern "C" 函数没有函数体，不分析了
    } else {
        ASSERT(func.body_inst != INVALID_INST, "non-extern function must have body");

        pc() = func.body_inst;
        analyze_instruction(CIROperator::Block, {.block_eval_mode = EvalMode::TypeOnly});
        pc() -= 1;
    }



}

void Interpreter::analyze_call() {
    auto& call_info = pkg->inst(pc())->call_info;
    CIRInstructionRef called_inst = call_info.called_thing;

    if(propagate_error({called_inst})) return;
    if(propagate_error(call_info.arg_insts)) return;

    TypeRef called_type = ResultType(called_inst);

    if(is_pointer_type(called_type) && is_function_type(called_type->pointed_type)) {
        called_type = called_type->pointed_type;
    }

    if(!is_function_type(called_type)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "calling non-function value");
        Set_ResultError(pc());
        return;
    }

    auto& param_types = called_type->function_info.param_types;

    // 参数数量检查
    isize non_var_arg_count = 0;
    bool has_var_arg = false;
    for(isize i = 0; i < param_types.count; i++) {
        if(param_types[i] == easy_type(Type_var_arg_c)) {
            has_var_arg = true;
        } else {
            non_var_arg_count++;
        }
    }

    isize arg_count = call_info.arg_insts.count;
    if(has_var_arg) {
        if(arg_count < non_var_arg_count) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc,
                "too few arguments: expected at least {} but got {}",
                non_var_arg_count, arg_count);
            Set_ResultError(pc());
            return;
        }
    } else {
        if(arg_count != param_types.count) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc,
                "argument count mismatch: expected {} but got {}",
                param_types.count, arg_count);
            Set_ResultError(pc());
            return;
        }
    }

    TypeRef return_type = called_type->function_info.return_type;
    Set_ResultType(pc(), return_type);

    if(curr_eval_mode() == EvalMode::FullEval) {
        // TODO: 支持编译期函数调用，目前先不支持
        XP_TODO();
    }

}

void Interpreter::analyze_get_or_init_struct() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->get_or_init_struct_info;

    std::optional<xpString> struct_name = std::nullopt;
    if(info.symbol != nullptr) {
        struct_name = info.symbol->name;
    }

    // 创建未完成的结构体类型，防止递归字段求值时无限展开
    TypeRef st = nullptr;
    if(struct_name.has_value()) {
        st = unfinished_struct_type(info.decl_ast, struct_name.value());
    } else {
        st = unfinished_anonymous_struct_type(info.decl_ast);
    }

    TypeRef tt = type_type(st);
    Value v = make_value(tt);
    v.set_type(tt);

    Set_ResultType(pc(), v.type);

    Set_ResultValue(pc(), v);

    // 壳子创建后立即绑定符号，使字段内的 *Self 能查到
    if(info.symbol != nullptr) {
        info.symbol->val(v);
        info.symbol->state = SymbolState::Solved;
    }

}

void Interpreter::analyze_struct_field() {
    // 纯数据载体，不干活。所有工作在 analyze_finish_struct 中完成
}


void Interpreter::analyze_finish_struct() {
    auto& info = pkg->inst(pc())->finish_struct_info;
    auto struct_decl_inst = info.struct_decl_inst;

    // 获取 GetOrInitStruct 创建的结构体类型
    TypeRef st = extract_type_from_val_as_type(ResultValue(struct_decl_inst));

    // 收集字段名和类型
    Array<StructField> fields = make_array<StructField>(type_allocator());
    for(isize i = 0; i < info.field_insts.count; i++) {
        auto field_inst = info.field_insts[i];
        auto& field_info = pkg->inst(field_inst)->struct_field_info;

        TypeRef field_type = extract_type_from_val_as_type(ResultValue(field_info.type_block_inst));

        StructField sf;
        sf.name = field_info.name;
        sf.type = field_type;
        fields.push_back(sf);

        if(sf.type == st) {
            context()->reporter.report_error(pkg->inst(field_inst)->src_loc, "struct '{}' contains itself without indirection, which would cause infinite size", sf.name);
            Set_ResultError(pc());
            return;
        }
    }

    // 填满字段，完成结构体类型
    finish_unfinish_struct_type(st, fields);

    // FinishStruct 结果 = GetOrInitStruct 结果（同一个 type_type）
    Set_ResultTypeAndValue(pc(), ResultValue(struct_decl_inst));

}

void Interpreter::analyze_enum_decl_init() {
    auto& info = pkg->inst(pc())->enum_decl_init_info;

    if(propagate_error({info.tag_type_inst})) return;

    TypeRef elem_type = extract_type_from_val_as_type(ResultValue(info.tag_type_inst));
    if(!is_integer_type(elem_type)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "underlying type of enum must be an integer type");
        Set_ResultError(pc());
        return;
    }


    std::optional<xpString> enum_name = std::nullopt;
    if(info.symbol != nullptr) {
        enum_name = info.symbol->name;
    }

    TypeRef enum_type = enum_type_impl(info.decl_ast, enum_name, elem_type, info.scope);

    TypeRef meta = type_type(enum_type);
    Value v = make_value(meta);
    Set_ResultTypeAndValue(pc(), v);

    if(info.symbol != nullptr) {
        info.symbol->val(v);
        info.symbol->state = SymbolState::Solved;
    }

    // 注册枚举字段到 enum_scope
    Scope *enum_scope = enum_type->enum_info.enum_scope;
    i128 next_auto_value = 0;
    for(isize i = 0; i < info.fields.count; i++) {
        auto& ef = info.fields[i];

        Value field_val;
        if(ef.value_inst != INVALID_INST) {
            if(propagate_error({ef.value_inst})) return;
            field_val = ResultValue(ef.value_inst);
            XP_ASSERT_DEFAULT(field_val.type == elem_type);

            next_auto_value = field_val.integer_val() + 1;
        } else {
            field_val = make_value(elem_type);
            field_val.integer_val(next_auto_value);

            if(check_integer_overflow(field_val.integer_val(), elem_type)) {
                context()->reporter.report_error(pkg->inst(pc())->src_loc, "enum discriminant value overflow");
                Set_ResultError(pc());
                return;
            }

            next_auto_value += 1;
        }
        field_val.set_type(enum_type);

        SymbolInfo *field_sym = find_symbol_curr(enum_scope, ef.name);
        XP_ASSERT_DEFAULT(field_sym != nullptr);
        field_sym->val(field_val);
        field_sym->state = SymbolState::Solved;
    }

}

void Interpreter::analyze_union_decl() {
    // 联合体声明：目前只做类型创建
    // 联合体在 xoaop 语言中尚未完全定义语义，保留占位
    Type union_t = make_type(Type_union);
    TypeRef union_type = get_or_add_type(union_t);
    TypeRef meta = type_type(union_type);

    Value v = make_value(meta);
    Set_ResultTypeAndValue(pc(), v);

}


// 表达式
void Interpreter::analyze_binary() {
    if(has_result_val(pc())) {
        return;
    }

    auto& binary_info = pkg->inst(pc())->binary_info;

    auto op = binary_info.op;
    auto left_inst = binary_info.left_inst;
    auto right_inst = binary_info.right_inst;

    if(propagate_error({left_inst, right_inst})) return;

    TypeRef left_type = ResultType(left_inst);
    TypeRef right_type = ResultType(right_inst);

    //
    // untyped 被数值类型传染
    //
    CIRInstructionRef contagion_target = INVALID_INST;
    if(is_number_type(left_type) && is_untyped_type(right_type)) {
        Set_ResultType(right_inst, left_type);
        contagion_target = right_inst;
    } else if(is_untyped_type(left_type) && is_number_type(right_type)) {
        Set_ResultType(left_inst, right_type);
        contagion_target = left_inst;
    }
    if(contagion_target != INVALID_INST && has_result_val(contagion_target) && is_val_overflow(ResultValue(contagion_target))) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "value overflow in type contagion");
        Set_ResultError(pc());
        return;
    }
    left_type  = ResultType(left_inst);
    right_type = ResultType(right_inst);


    //
    // 确定结果类型
    //
    TypeRef result_type;
    if(is_return_bool_operator(op)) {
        result_type = easy_type(Type_bool);
    } else if (is_pointer_type(left_type) || is_pointer_type(right_type)) {
        result_type = is_pointer_type(left_type) ? left_type : right_type;
    } else {
        result_type = left_type;
    }


    //
    // 类型检查
    //
    if(is_pointer_type(left_type) && is_pointer_type(right_type)) {
        if(!is_equal_compare_operator(op)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "only equality comparison is allowed between pointer types");
            Set_ResultError(pc());
            return;
        }

        if(left_type != right_type) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "pointer types in comparison must be identical");
            Set_ResultError(pc());
            return;
        }

    } else if(is_pointer_type(left_type) || is_pointer_type(right_type)) {
        TypeRef ptr   = is_pointer_type(left_type) ? left_type : right_type;
        TypeRef other = is_pointer_type(left_type) ? right_type : left_type;

        if(!is_add_sub_operator(op)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "only + and - operators are allowed for pointer arithmetic");
            Set_ResultError(pc());
            return;
        }

        if(get_innermost_type_of_pointer(ptr) == easy_type(Type_void)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "pointer arithmetic is not allowed for void pointers");
            Set_ResultError(pc());
            return;
        }

        if(!is_integer_or_untyped_type(other)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "pointer arithmetic only allowed between pointer and integer types");
            Set_ResultError(pc());
            return;
        }

    } else {
        if(left_type != right_type) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "binary operator requires both operands to have the same type");
            Set_ResultError(pc());
            return;
        }

        if(is_struct_type(left_type) || is_array_type(left_type)) {
            if(!is_equal_compare_operator(op)) {
                context()->reporter.report_error(pkg->inst(pc())->src_loc, "only equality comparison is allowed for struct and array types");
                Set_ResultError(pc());
                return;
            }
        }

        if(is_enum_type(left_type)) {
            if(!is_equal_compare_operator(op)) {
                context()->reporter.report_error(pkg->inst(pc())->src_loc, "only equality comparison and boolean operators are allowed for enum types");
                Set_ResultError(pc());
                return;
            }
        }

        if(left_type == easy_type(Type_bool)) {
            if(!is_operator_for_bool(op)) {
                context()->reporter.report_error(pkg->inst(pc())->src_loc, "only boolean operators and equality comparison are allowed for bool types");
                Set_ResultError(pc());
                return;
            }
        }

        if(op == TokenType::Percent && is_float_type(left_type)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, 
            "modulo operator is not allowed for float types");
            Set_ResultError(pc());
            return;
        }
    }

    XP_ASSERT_DEFAULT(result_type != nullptr);
    Set_ResultType(pc(), result_type);


    if(has_result_val(left_inst) && has_result_val(right_inst)) {
        Value left_val = ResultValue(left_inst);
        Value right_val = ResultValue(right_inst);

        ValueResult exec_res = exec_binary(left_val, right_val, binary_info.op);
        if(exec_res.is_err()) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "error evaluating binary expression");
            Set_ResultError(pc());
            return;
        }

        Value result_val = exec_res.as_ok();
        Set_ResultValue(pc(), result_val);
    }

}

void Interpreter::analyze_unary() {
    if(has_result_val(pc())) {
        return;
    }

    auto& unary_info = pkg->inst(pc())->unary_info;

    auto op = unary_info.op;
    auto operand_inst = unary_info.operand_inst;

    if(propagate_error({operand_inst})) return;

    TypeRef operand_type = ResultType(operand_inst);

    //
    // 确定结果类型
    //
    TypeRef result_type;
    if(op == TokenType::Exclamation) {
        result_type = easy_type(Type_bool);
    } else if(op == TokenType::Minus) {
        result_type = operand_type;
    } else {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "unknown unary operator");
        Set_ResultError(pc());
        return;
    }

    //
    // 类型检查
    //
    if(op == TokenType::Minus) {
        if(!is_number_type(operand_type)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "unary minus requires a numeric operand");
            Set_ResultError(pc());
            return;
        }
    } else if(op == TokenType::Exclamation) {
        if(operand_type != easy_type(Type_bool)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "logical not requires a boolean operand");
            Set_ResultError(pc());
            return;
        }
    }

    XP_ASSERT_DEFAULT(result_type != nullptr);
    Set_ResultType(pc(), result_type);

    if(curr_eval_mode() == EvalMode::FullEval || has_result_val(operand_inst)) {
        Value operand_val = ResultValue(operand_inst);

        ValueResult exec_res = exec_unary(operand_val, unary_info.op);
        if(exec_res.is_err()) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "error evaluating unary expression");
            Set_ResultError(pc());
            return;
        }

        Value result_val = exec_res.as_ok();
        Set_ResultValue(pc(), result_val);
    }

}

void Interpreter::analyze_cast() {
    if(has_result_val(pc())) {
        return;
    }

    auto& cast_info = pkg->inst(pc())->cast_info;

    auto expr_inst = cast_info.expr_inst;
    auto target_type_inst = cast_info.target_type_inst;

    if(propagate_error({expr_inst, target_type_inst})) return;

    TypeRef expr_type = ResultType(expr_inst);
    TypeRef target_type = extract_type_from_val_as_type(ResultValue(target_type_inst));

    CIRInstruction *expr_ci = pkg->inst(expr_inst);
    if(!check_explicit_type_cast(expr_ci, expr_type, target_type)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "cannot cast between these types");
        Set_ResultError(pc());
        return;
    }

    TypeRef result_type = target_type;
    XP_ASSERT_DEFAULT(result_type != nullptr);
    Set_ResultType(pc(), result_type);

    if(curr_eval_mode() == EvalMode::FullEval || has_result_val(expr_inst)) {
        Value expr_val = ResultValue(expr_inst);

        ValueResult exec_res = exec_cast(expr_val, target_type);
        if(exec_res.is_err()) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "error evaluating cast expression");
            Set_ResultError(pc());
            return;
        }

        Value result_val = exec_res.as_ok();
        Set_ResultValue(pc(), result_val);
    }

}



void Interpreter::analyze_field_access() {

    if(has_result_val(pc())) {
        // 已经分析过了，避免重复分析
        return;
    }

    auto& info = pkg->inst(pc())->field_access_info;
    CIRInstructionRef parent_inst = info.parent_inst;

    if(propagate_error({parent_inst})) return;

    TypeRef parent_type = ResultType(parent_inst);

    // 解析结构体类型（支持指针自动解引用）
    auto get_struct_type = [](TypeRef t) -> TypeRef {
        if(is_struct_type(t)) return t;
        if(is_pointer_type(t) && is_struct_type(t->pointed_type)) return t->pointed_type;
        return nullptr;
    };

    if(is_package_type(parent_type)) {
        // 包成员访问: pkg.symbol
        Package *pkg_val = parent_type->package_info;
        SymbolInfo *field_sym = find_symbol_curr(&pkg_val->package_scope, info.field_name);
        if(field_sym == nullptr) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "package member '{}' not found", info.field_name);
            Set_ResultError(pc());
            return;
        }

        Set_ResultTypeAndValue(pc(), field_sym->val());

    } else if(TypeRef struct_type = get_struct_type(parent_type)) {
        bool field_found = false;
        for(isize i = 0; i < struct_type->struct_info.struct_fields.count; i++) {
            if(xp_string_equal(struct_type->struct_info.struct_fields[i].name, info.field_name)) {
                field_found = true;

                Set_ResultType(pc(), struct_type->struct_info.struct_fields[i].type);

                if(is_string_struct_type(struct_type)) {
                    break;
                }

                if(curr_eval_mode() == EvalMode::FullEval || has_result_val(parent_inst)) {
                    auto parent_val = ResultValue(parent_inst);
                    Value field_val = parent_val.struct_field_val(i);
                    Set_ResultValue(pc(), field_val);
                }

                break;
            }
        }

        if(!field_found) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "struct field '{}' not found", info.field_name);
            Set_ResultError(pc());
            return;
        }

    } else if(is_type_type(parent_type) && is_enum_type(parent_type->self_type_info)) {
        // 枚举成员访问: EnumType.Variant
        TypeRef enum_type = parent_type->self_type_info;

        Set_ResultType(pc(), enum_type);

        SymbolInfo *field_sym = find_symbol_curr(enum_type->enum_info.enum_scope, info.field_name);

        if(field_sym == nullptr) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "enum variant '{}' not found", info.field_name);
            Set_ResultError(pc());
            return;
        }

        Value field_val = field_sym->val();
        Set_ResultValue(pc(), field_val);

    } else {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "field access on unsupported type");
        Set_ResultError(pc());
        return;
    }

}

void Interpreter::analyze_field_ptr() {
    auto& info = pkg->inst(pc())->field_access_info;
    CIRInstructionRef parent_inst = info.parent_inst;

    if(propagate_error({parent_inst})) return;

    if(!is_lvalue(parent_inst)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "cannot access field pointer of non-lvalue expression");
        Set_ResultError(pc());
        return;
    }

    TypeRef parent_type = ResultType(parent_inst);
    TypeRef struct_type = nullptr;

    if(is_struct_type(parent_type)) {
        struct_type = parent_type;
    } else if(is_pointer_type(parent_type) && is_struct_type(parent_type->pointed_type)) {
        struct_type = parent_type->pointed_type;
    } else {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "field pointer access only supported on struct types or pointers to struct");
        Set_ResultError(pc());
        return;
    }

    for(isize i = 0; i < struct_type->struct_info.struct_fields.count; i++) {
        if(xp_string_equal(struct_type->struct_info.struct_fields[i].name, info.field_name)) {
            Set_ResultType(pc(), struct_type->struct_info.struct_fields[i].type);
            set_lvalue(pc());

            if(curr_eval_mode() == EvalMode::FullEval) {
                XP_TODO();
            }
            return;
        }
    }
    context()->reporter.report_error(pkg->inst(pc())->src_loc, "struct field '{}' not found for FieldPtr", info.field_name);
    Set_ResultError(pc());
}

void Interpreter::analyze_struct_init() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->struct_init_info;

    if(propagate_error({info.struct_type_inst})) return;
    if(propagate_error(info.field_init_insts)) return;

    TypeRef st = extract_type_from_val_as_type(ResultValue(info.struct_type_inst));
    XP_ASSERT_DEFAULT(is_struct_type(st));
    Set_ResultType(pc(), st);

    if(info.field_init_insts.count != st->struct_info.struct_fields.count) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "struct initializer field count does not match struct definition");
        Set_ResultError(pc());
        return;
    }

    // 字段类型检查
    for(isize i = 0; i < info.field_init_insts.count; i++) {
        TypeRef field_type = ResultType(info.field_init_insts[i]);
        TypeRef expected = st->struct_info.struct_fields[i].type;
        std::optional<TypeRef> implicit_type = pkg->result_of(info.field_init_insts[i]).implicit_type;
        if(field_type != expected && ((!implicit_type.has_value()) || implicit_type != expected)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "struct initializer field type does not match struct definition");
            Set_ResultError(pc());
            return;
        }
    }

    bool all_fields_have_val = true;
    for(isize i = 0; i < info.field_init_insts.count; i++) {
        if(!has_result_val(info.field_init_insts[i])) { all_fields_have_val = false; break; }
    }
    if(curr_eval_mode() == EvalMode::FullEval || all_fields_have_val) {
        Value v = make_value(st);
        v.set_type(st);
        Array<Value> field_values = make_array<Value>(permanent_allocator());
        for(isize i = 0; i < info.field_init_insts.count; i++) {
            field_values.push_back(ResultValue(info.field_init_insts[i]));
        }
        v.struct_fields_val(field_values);
        Set_ResultValue(pc(), v);
    }

}

void Interpreter::analyze_array_init() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->array_init_info;

    if(propagate_error(info.element_insts)) return;

    // 从第一个元素推断数组元素类型，或从上下文获取
    TypeRef elem_type = nullptr;
    if(info.element_insts.count > 0) {
        elem_type = ResultType(info.element_insts[0]);
    }
    usize count = info.element_insts.count;

    TypeRef arr_type = array_type(elem_type ? elem_type : undefined_type(), count);
    Set_ResultType(pc(), arr_type);

    bool all_elems_have_val = true;
    for(isize i = 0; i < info.element_insts.count; i++) {
        if(!has_result_val(info.element_insts[i])) { all_elems_have_val = false; break; }
    }
    if(curr_eval_mode() == EvalMode::FullEval || all_elems_have_val) {
        Value v = make_value(arr_type);
        v.set_type(arr_type);
        Array<Value> elem_values = make_array<Value>(stage_allocator());
        for(isize i = 0; i < info.element_insts.count; i++) {
            elem_values.push_back(ResultValue(info.element_insts[i]));
        }
        v.array_element_values(elem_values);
        Set_ResultValue(pc(), v);
    }

}

void Interpreter::analyze_index() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->index_info;
    CIRInstructionRef array_inst = info.array_inst;
    CIRInstructionRef index_inst = info.index_inst;

    if(propagate_error({array_inst, index_inst})) return;

    TypeRef array_type_ref = ResultType(array_inst);

    TypeRef elem_type = nullptr;
    if(is_array_type(array_type_ref)) {

        elem_type = array_type_ref->array_info.element_type;

    } else if(is_string_struct_type(array_type_ref)) {

        elem_type = easy_type(Type_u8);

    } else if(is_slice_struct_type(array_type_ref)) {

        elem_type = array_type_ref->struct_info.struct_fields[0].type->pointed_type;

    } else {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "indexing on unsupported type");
        Set_ResultError(pc());
        return;
    }


    Set_ResultType(pc(), elem_type);

    if(curr_eval_mode() == EvalMode::FullEval || (has_result_val(array_inst) && has_result_val(index_inst))) {
        i128 idx = ResultValue(index_inst).integer_val();

        if(is_array_type(array_type_ref)) {
            if(idx < 0 || idx >= array_type_ref->array_info.count) {
                context()->reporter.report_error(pkg->inst(pc())->src_loc, "array index out of bounds");
                Set_ResultError(pc());
                return;
            }
            XP_TODO(); // compile-time array element extraction
        } else if(is_string_struct_type(array_type_ref)) {
            xpString str = ResultValue(array_inst).string_val();
            if(idx < 0 || idx >= str.length) {
                context()->reporter.report_error(pkg->inst(pc())->src_loc, "string index out of bounds");
                Set_ResultError(pc());
                return;
            }
            Value char_val = make_value(easy_type(Type_u8));
            char_val.integer_val(static_cast<u8>(str.c_str[idx]));
            Set_ResultValue(pc(), char_val);
        } else if(is_slice_struct_type(array_type_ref)) {
            XP_TODO(); // compile-time slice element extraction
        }
    }

}

void Interpreter::analyze_index_ptr() {
    auto& info = pkg->inst(pc())->index_info;
    CIRInstructionRef array_inst = info.array_inst;

    if(propagate_error({array_inst})) return;

    if(!is_lvalue(array_inst)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "cannot index pointer of non-lvalue expression");
        Set_ResultError(pc());
        return;
    }

    TypeRef array_type_ref = ResultType(array_inst);

    TypeRef elem_type = nullptr;
    if(is_array_type(array_type_ref)) {
        elem_type = array_type_ref->array_info.element_type;
    } else if(is_slice_struct_type(array_type_ref)) {
        elem_type = array_type_ref->struct_info.struct_fields[0].type->pointed_type;
    } else {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "index pointer on unsupported type");
        Set_ResultError(pc());
        return;
    }

    Set_ResultType(pc(), elem_type);
    set_lvalue(pc());
}

void Interpreter::analyze_addr_of() {
    auto& info = pkg->inst(pc())->addr_of_info;
    CIRInstructionRef lval_inst = info.lval_inst;

    if(propagate_error({lval_inst})) return;

    TypeRef lval_type = ResultType(lval_inst);

    if(!is_lvalue(lval_inst) && !is_function_type(lval_type)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "cannot take address of non-lvalue expression");
        Set_ResultError(pc());
        return;
    }

    TypeRef ptr_type = pointer_type(lval_type);
    // TODO: 编译期取地址同样需要专门的变量存储空间
    Set_ResultType(pc(), ptr_type);

}


// 变量 / 存储 / 类型标注
void Interpreter::analyze_load() {
    auto& load_info = pkg->inst(pc())->load_info;
    CIRInstructionRef ptr_inst = load_info.ptr_inst;

    if(propagate_error({ptr_inst})) return;

    if(is_lvalue(ptr_inst)) {
        TypeRef loaded_type = ResultType(ptr_inst);
        Set_ResultType(pc(), loaded_type);
    } else {
        TypeRef ptr_type = ResultType(ptr_inst);
        if(!is_pointer_type(ptr_type)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "cannot load from a non-pointer value");
            Set_ResultError(pc());
            return;
        }
        Set_ResultType(pc(), ptr_type->pointed_type);
    }

}

void Interpreter::analyze_variable_decl() {
    auto& vd = pkg->inst(pc())->var_decl;
    SymbolInfo *sym = find_symbol_curr(scope(), vd.name);
    XP_ASSERT_DEFAULT(sym != nullptr);

    Set_ResultType(pc(), undefined_type());
    set_lvalue(pc());
    sym->val(CIRInstUniqueKey{pkg, sym->package, pc()});
    sym->state = SymbolState::Solved;

}


void Interpreter::analyze_ident_ref() {
    xpString ident = pkg->inst(pc())->ident;
    SymbolInfo *info = find_symbol_until_global(scope(), ident);

    if(info == nullptr) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "undefined identifier '{}'", ident);
        Set_ResultError(pc());
        return;
    }

    if(!info->is_var_decl() && !info->is_const_decl_and_func()) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "identifier '{}' is not an addressable entity", ident);
        Set_ResultError(pc());
        return;
    }

    if(info->state == SymbolState::Unsolved) {
        new_pc_flow(info->val_as_inst_key().defining_inst);
        defer(recover_pc_flow());

        analyze_instruction();
    } else if(info->state == SymbolState::Solving) {
        context()->reporter.report_error(SourceLocation({}, pkg->inst(pc())->src_loc.span), "circular dependency detected when evaluating '{}'", ident);
        Set_ResultError(pc());
        return;
    }

    Set_ResultType(pc(), info->val().type);
    set_lvalue(pc());

    if(curr_eval_mode() == EvalMode::FullEval) {
        // TODO: 构造指针值
        XP_TODO();
    }

}

void Interpreter::analyze_ident_val() {
    
    if(has_result_val(pc())) {
        return;
    }
    
    xpString ident = pkg->inst(pc())->ident;
    SymbolInfo *info = find_symbol_until_global(scope(), ident);

    if(info == nullptr) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "undefined identifier '{}'", ident);
        Set_ResultError(pc());
        return;
    }

    if(info->is_var_decl()) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "IdentVal should only be used for consts, '{}' is a variable", ident);
        Set_ResultError(pc());
        return;
    }

    // 以支持顶层constDecl的顺序无关声明
    if(info->state == SymbolState::Unsolved) {
        new_pc_flow(info->val_as_inst_key().defining_inst);
        defer(recover_pc_flow());

        analyze_instruction();
    } else if(info->state == SymbolState::Solving) {
        context()->reporter.report_error(SourceLocation({}, pkg->inst(pc())->src_loc.span), "circular dependency detected when evaluating '{}'", ident);
        Set_ResultError(pc());
        return;
    }

    Set_ResultTypeAndValue(pc(), info->val());

}

void Interpreter::analyze_store() {
    auto& store_info = pkg->inst(pc())->store_info;
    CIRInstructionRef var_inst = store_info.var_inst;
    CIRInstructionRef value_inst = store_info.value_inst;

    if(propagate_error({var_inst, value_inst})) return;

    TypeRef target_type;
    if(is_lvalue(var_inst)) {
        target_type = ResultType(var_inst);
    } else {
        TypeRef ptr_type = ResultType(var_inst);
        if(!is_pointer_type(ptr_type)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "you can only assign to variables, fields, array elements, or dereferenced pointers");
            Set_ResultError(pc());
            return;
        }
        target_type = ptr_type->pointed_type;
    }

    if(target_type == undefined_type()) {
        TypeRef inferred = ResultType(store_info.value_inst);
        Set_ResultType(var_inst, inferred);
    } else {
        TypeRef value_type = ResultType(store_info.value_inst);
        std::optional<TypeRef> implicit_type = pkg->result_of(store_info.value_inst).implicit_type;

        if(value_type != target_type && ((!implicit_type.has_value()) || implicit_type.value() != target_type)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc,
            "try to store value with type '{}', but expected '{}'",
            value_type->t_name(), target_type->t_name());
            Set_ResultError(pc());
            return;
        }
    }

    // TODO: 编译期 Store 需要专门的变量存储空间，不能直接写 SymbolInfo
    if(curr_eval_mode() == EvalMode::FullEval) {
        XP_TODO(); // TODO: 目前先不支持编译期变量赋值
    }

    set_result_state(pc(), CIRResultType::DontHave);
}

void Interpreter::analyze_type_ascribe() {
    auto& info = pkg->inst(pc())->type_ascribe_info;

    if(propagate_error({info.var_inst, info.type_inst})) return;

    // 限制:
    // var_inst == VariableDecl
    // type_inst.result is type_type
    XP_ASSERT_DEFAULT(pkg->inst(info.var_inst)->op == CIROperator::VariableDecl);



    TypeRef declared_type = extract_type_from_val_as_type(ResultValue(info.type_inst));


    TypeRef existing = ResultType(info.var_inst);
    if(existing == undefined_type()) {
        Set_ResultType(info.var_inst, declared_type);
    } else {
        if(existing != declared_type) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "type annotation conflicts with deduced variable type");
            Set_ResultError(pc());
            return;
        }
    }

    set_result_state(pc(), CIRResultType::DontHave);
}


// 叶子值
void Interpreter::analyze_constant_value() {


    // TODO: EXPAND
    // 去除重复求值
    if(has_result_val(pc())) {
        return;
    }

    Value& val = pkg->inst(pc())->imm_val;

    Set_ResultTypeAndValue(pc(), val);
}



// 类型构造
void Interpreter::analyze_pointer_type() {
    if(has_result_val(pc())) {
        return;
    }

    auto pointed_inst = pkg->inst(pc())->pointer_type_info.pointed_type_inst;

    if(propagate_error({pointed_inst})) return;


    if(!has_result_val(pointed_inst)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "pointer type operand has no value");
        Set_ResultError(pc());
        return;
    }

    TypeRef pointed = extract_type_from_val_as_type(ResultValue(pointed_inst));
    TypeRef ptr_meta = type_type(pointer_type(pointed));

    Value result = make_value(ptr_meta);
    Set_ResultTypeAndValue(pc(), result);

}

void Interpreter::analyze_array_type() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->array_type_info;

    if(propagate_error({info.element_type_inst, info.count_inst})) return;

    TypeRef elem_tt = ResultType(info.element_type_inst);
    XP_ASSERT_DEFAULT(is_type_type(elem_tt));
    TypeRef elem_type = elem_tt->self_type_info;

    Value count_val = ResultValue(info.count_inst);
    i128 count = count_val.integer_val();

    TypeRef arr_type = array_type(elem_type, count);
    TypeRef meta = type_type(arr_type);

    Value result = make_value(meta);
    Set_ResultTypeAndValue(pc(), result);

}

void Interpreter::analyze_slice_type() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->slice_type_info;

    if(propagate_error({info.element_type_inst})) return;

    TypeRef elem_tt = ResultType(info.element_type_inst);
    XP_ASSERT_DEFAULT(is_type_type(elem_tt));
    TypeRef elem_type = elem_tt->self_type_info;

    TypeRef slice_type = slice_type_as_struct(elem_type);
    TypeRef meta = type_type(slice_type);

    Value result = make_value(meta);
    Set_ResultTypeAndValue(pc(), result);

}

void Interpreter::analyze_func_type() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->func_type_info;

    if(propagate_error(info.param_type_insts)) return;
    if(propagate_error({info.return_type_inst})) return;

    Array<TypeRef> param_types = make_array<TypeRef>(stage_allocator());
    defer(array_free(&param_types));

    for(isize i = 0; i < info.param_type_insts.count; i++) {
        TypeRef pt = extract_type_from_val_as_type(ResultValue(info.param_type_insts[i]));
        param_types.push_back(pt);
    }

    TypeRef return_type = extract_type_from_val_as_type(ResultValue(info.return_type_inst));

    TypeRef func_type = function_type(param_types, return_type);
    TypeRef meta = type_type(func_type);

    Value result = make_value(meta);
    Set_ResultTypeAndValue(pc(), result);
}




void Interpreter::analyze_type_of_inst_result() {
    if(has_result_val(pc())) {
        return;
    }

    auto target_inst = pkg->inst(pc())->type_of_inst_result_info.target_inst;

    if(propagate_error({target_inst})) return;

    TypeRef target_type = ResultType(target_inst);
    TypeRef meta = type_type(target_type);

    Value result = make_value(meta);
    Set_ResultTypeAndValue(pc(), result);

}

void Interpreter::analyze_field_type_of_struct() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->field_type_of_struct_info;

    if(propagate_error({info.struct_type_inst})) return;

    TypeRef st = extract_type_from_val_as_type(ResultValue(info.struct_type_inst));

    if(!is_struct_type(st)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "can't get field type from a non-struct type");
        Set_ResultError(pc());
        return;
    }

    TypeRef field_type = st->struct_info.struct_fields[info.field_index].type;
    TypeRef meta = type_type(field_type);

    Value result = make_value(meta);
    Set_ResultTypeAndValue(pc(), result);
}


void Interpreter::analyze_func_param_type() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->func_param_type_info;

    if(propagate_error({info.type_of_func_type_inst})) return;

    TypeRef func_type = extract_type_from_val_as_type(ResultValue(info.type_of_func_type_inst));

    if(is_pointer_type(func_type) && is_function_type(func_type->pointed_type)) {
        func_type = func_type->pointed_type;
    }

    if(!is_function_type(func_type)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc,
            "cannot get parameter type from a non-function type");
        Set_ResultError(pc());
        return;
    }

    auto& param_types = func_type->function_info.param_types;

    TypeRef param_type;
    if(info.param_index < param_types.count) {
        param_type = param_types[info.param_index];
    } else {
        // 越界: 只有变参函数才允许，视为 var_arg
        bool any_var_arg = false;
        for(isize i = 0; i < param_types.count; i++) {
            if(param_types[i] == easy_type(Type_var_arg_c)) {
                any_var_arg = true;
                break;
            }
        }
        if(!any_var_arg) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc,
                "parameter index {} out of bounds (function has {} parameters)",
                info.param_index, param_types.count);
            Set_ResultError(pc());
            return;
        }
        param_type = easy_type(Type_var_arg_c);
    }
    TypeRef meta = type_type(param_type);
    Value result = make_value(meta);
    Set_ResultTypeAndValue(pc(), result);
}


void Interpreter::analyze_enter_scope() {
    Scope *scope = pkg->inst(pc())->scope_info.scope;
    set_scope(scope);

}

void Interpreter::analyze_exit_scope() {
    Scope *scope = pkg->inst(pc())->scope_info.scope;
    set_scope(scope);

}

void Interpreter::analyze_determine_type() {
    auto& info = pkg->inst(pc())->determine_type_info;
    auto determined_inst = info.determining_inst;
    auto expected_type_inst = info.type_inst;

    if(propagate_error({determined_inst})) return;
    if(expected_type_inst != INVALID_INST && propagate_error({expected_type_inst})) return;

    TypeRef determined_type = ResultType(determined_inst);
    bool has_val = has_result_val(determined_inst);
    Value result_val;
    if(has_val) {
        result_val = ResultValue(determined_inst);
    }


    bool check_overflow = false;
    TypeRef expected_type = determined_type;

    if(has_val) {
        if(is_untyped_type(determined_type)) {
            expected_type = get_compliable_const_type(result_val);
        }
    } else {
        if(determined_type == easy_type(Type_untyped_int)) {
            expected_type = easy_type(Type_i32);
        } else if(determined_type == easy_type(Type_untyped_float)) {
            expected_type = easy_type(Type_f64);
        }
    }

    // TODO: 统一路径
    Set_ResultType(determined_inst, expected_type);

    if(expected_type_inst != INVALID_INST) {
        // @EXPLAIN: 如果有参数, 表示有目标类型
        expected_type = extract_type_from_val_as_type(ResultValue(info.type_inst));

        XP_ASSERT_DEFAULT(!is_untyped_type(expected_type)); // 目标类型不应该是 untyped

        // var_arg: 不约束类型，任何实参都接受
        if(expected_type == easy_type(Type_var_arg_c)) {
            set_result_state(pc(), CIRResultType::DontHave);
            return;
        }

        bool ok = false;
        bool is_implicit_cast = false;

        if(has_val) {
            if(result_val.is_null) {
                if(is_pointer_type(expected_type)) {
                    ok = true;
                }
            }
        }

        if(is_pointer_type(determined_type) && expected_type == pointer_type(easy_type(Type_void))) {
            ok = true;
        } else if(is_array_type(determined_type) && is_slice_struct_type(expected_type)) {
            if(expected_type->struct_info.struct_fields[0].type->pointed_type == determined_type->array_info.element_type) {
                // TODO: 标记隐式转化: 数组转为切片
                is_implicit_cast = true;
                ok = true;
            }
        } else if(is_untyped_type(determined_type) && is_certain_type(expected_type)) {
            if((determined_type == easy_type(Type_untyped_int) && is_integer_type(expected_type)) ||
                (determined_type == easy_type(Type_untyped_float) && is_float_type(expected_type))) {
                ok = true;
            }
        } else if(is_function_type(determined_type)) {
            if(is_pointer_type(expected_type) && expected_type->pointed_type == determined_type) {
                // 函数类型可以隐式转为指向函数的指针类型
                
                // TODO: 因为是指针, 目前还不支持编译期指针, 所以如果该值是求值了的
                // 要强制改为TypeOnly
                // 不知道有没有bug
                if(pkg->result_of(determined_inst).type == CIRResultType::WholeValue) {
                    pkg->result_of(determined_inst).type = CIRResultType::OnlyType;

                    has_val = false;
                }

                is_implicit_cast = true;
                ok = true;
            }
        } else if(determined_type == expected_type) {
            ok = true;
        }


        if(!ok) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "cannot determine type: expected {}, got {}", expected_type->t_name(), determined_type->t_name());
            Set_ResultError(pc());
            return;
        }

        if(!is_implicit_cast) {
            Set_ResultType(determined_inst, expected_type);
        } else {
            pkg->result_of(determined_inst).implicit_type = expected_type;
        }

        if(has_val) {
            result_val = ResultValue(determined_inst);
            if(is_val_overflow(result_val)) {
                context()->reporter.report_error(pkg->inst(pc())->src_loc, "value overflow in type determination");
                Set_ResultError(pc());
                return;
            }
        }


    } else {
        // 无参数 = 强制消除 untyped，把源指令的类型具体化
        expected_type = determined_type;

       
    }



    set_result_state(pc(), CIRResultType::DontHave);
}














//
// utils
//

// Value Interpreter::get_result_val_or_eval(CIRInstructionRef ref) {
//     auto& res = pkg->Inst(ref)->result;

//     if(res.state == CIRResultType::
// }

bool Interpreter::has_result_val(CIRInstructionRef ref) {
    auto& res = pkg->inst(ref)->result;
    return res.type == CIRResultType::WholeValue;
}

bool Interpreter::has_result_type(CIRInstructionRef ref) {
    auto& res = pkg->inst(ref)->result;
    return res.type == CIRResultType::OnlyType || res.type == CIRResultType::WholeValue;
}

bool Interpreter::has_error(CIRInstructionRef ref) {
    return pkg->inst(ref)->result.type == CIRResultType::Error;
}

void Interpreter::Set_ResultError(CIRInstructionRef ref) {
    pkg->inst(ref)->result.type = CIRResultType::Error;
}

bool Interpreter::propagate_error(std::initializer_list<CIRInstructionRef> refs) {
    for(auto ref : refs) {
        if(has_error(ref)) { 
            Set_ResultError(pc()); return true; 
        }
    }
    return false;
}

bool Interpreter::propagate_error(Array<CIRInstructionRef>& refs) {
    for (isize i = 0; i < refs.count; i++)
        if (has_error(refs[i])) { Set_ResultError(pc()); return true; }
    return false;
}

CIRResultType Interpreter::result_state(CIRInstructionRef ref) {
    return pkg->inst(ref)->result.type;
}

void Interpreter::set_result_state(CIRInstructionRef ref, CIRResultType state) {
    auto& res = pkg->inst(ref)->result;
    res.type = state;
}


TypeRef Interpreter::ResultType(CIRInstructionRef ref) {
    auto res = pkg->inst(ref)->result;

    XP_ASSERT_DEFAULT(res.type == CIRResultType::OnlyType || res.type == CIRResultType::WholeValue);
    return res.val.type;
}

void Interpreter::Set_ResultType(CIRInstructionRef ref, TypeRef type) {
    ASSERT(type != nullptr, "cannot set result type to null");

    auto& res = pkg->inst(ref)->result;
    XP_ASSERT_DEFAULT(res.type == CIRResultType::NothingYet || res.type == CIRResultType::OnlyType || res.type == CIRResultType::WholeValue);

    if(res.type != CIRResultType::WholeValue) {
        res.type = CIRResultType::OnlyType;
    }

    res.val.type = type;
}

Value Interpreter::ResultValue(CIRInstructionRef ref) {
    auto res = pkg->inst(ref)->result;
    
    if(res.type != CIRResultType::WholeValue) {
        DEBUG_PANIC("trying to get value of instruction that doesn't have a value yet: {}", pkg->inst(ref)->src_loc);
    }

    return pkg->inst(ref)->result.val;
}


void Interpreter::Set_ResultValue(CIRInstructionRef ref, Value val) {
    auto& res = pkg->inst(ref)->result;
    XP_ASSERT_DEFAULT(res.type == CIRResultType::OnlyType);
    XP_ASSERT_DEFAULT(res.val.type == val.type);

    DEBUG_TRACE("Set_ResultValue: ref: {}, op: {}", ref, pkg->inst(ref)->to_string());

    res.type = CIRResultType::WholeValue;
    res.val = val;
}


void Interpreter::Set_ResultTypeAndValue(CIRInstructionRef ref, Value val) {
    Set_ResultType(ref, val.type);
    Set_ResultValue(ref, val);
}



void Interpreter::set_scope(Scope *sc) {
    scope() = sc;
}




void Interpreter::new_pc_flow(CIRInstructionRef new_pc) {
    pc_stack.push_back(curr_state());
    if(pkg->file_ranges.count > 0) {
        Scope *target_file_scope = pkg->scope_for_pc(new_pc);
        Scope *current_file_scope = pkg->scope_for_pc(pc());
        if(target_file_scope != current_file_scope) {
            scope() = target_file_scope;
        }
    }
    pc() = new_pc;
}

void Interpreter::recover_pc_flow() {
    XP_ASSERT(pc_stack.count > 1);
    pc_stack.pop_back();
}


EvalMode Interpreter::curr_eval_mode() const {
    if(eval_mode_stack.count == 0) {
        return EvalMode::TypeOnly; // 默认是类型推导模式
    } else {
        return eval_mode_stack.back();
    }
}

bool Interpreter::is_lvalue(CIRInstructionRef ref) {
    return pkg->inst(ref)->result.value_kind == CIRValueKind::LValue;
}

void Interpreter::set_lvalue(CIRInstructionRef ref) {
    pkg->inst(ref)->result.value_kind = CIRValueKind::LValue;
}
