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



bool check_explicit_type_cast(CIRPackage *pkg, CIRInstructionRef casted_inst_ref, TypeRef casted_expr_type, TypeRef target_type) {

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
        if(pkg->result_of(casted_inst_ref).value_kind == CIRValueKind::LValue
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

TypeRef default_certain_type_for_untyped_type(TypeRef untyped_type) {
    if(untyped_type == easy_type(Type_untyped_int)) {
        return easy_type(Type_i32);
    } else if(untyped_type == easy_type(Type_untyped_float)) {
        return easy_type(Type_f64);
    }
    
    DEBUG_PANIC("untyped_type {} is not untyped int or untyped float", untyped_type->name());
    return nullptr;
}


//
//
//




Interpreter::Interpreter(xpAllocator allocator) {
    pc_stack = make_array<AnalyzeFlowState>(allocator);
    eval_mode_stack = make_array<EvalMode>(allocator);
    loop_stack = make_array<CIRInstructionRef>(allocator);
    instance_stack = make_array<EvalInstance>(allocator);

    stack_mem.init(MemoryKind::Stack, allocator);
}

Interpreter::~Interpreter() {
    array_free(&pc_stack);
    array_free(&eval_mode_stack);
    array_free(&instance_stack);

    stack_mem.free();
}

Interpreter::AnalyzeFlowState& Interpreter::curr_state() { return pc_stack.back(); }
CIRInstructionRef&               Interpreter::pc()        { return curr_state().pc; }
Scope*&                           Interpreter::scope()    { return curr_state().scope; }




//
// 分析入口
//
void analyze_package(Package *pkg) {
    DEBUG_TRACE("start analyzing package {}", pkg->path);

    Interpreter interpreter(stage_allocator());
    interpreter.analyze_cir_package(&pkg->cir_package);
}


//
// interp 入口
//
void Interpreter::analyze_cir_package(CIRPackage* cir_package) {
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



// 数据流声明（实现在文件末尾）
static Array<CIRInstructionRef> deps_of(CIRInstruction* inst, xpAllocator alloc);
static Array<CIRInstructionRef> targets_of(CIRInstruction* inst, xpAllocator alloc);


void Interpreter::analyze_instruction(std::optional<CIROperator> expected_op, AnalyzeParams params) {
    CIRInstruction *inst = pkg->inst(pc());

    DEBUG_TRACE("curr pc(): {}, inst: {}, src_loc: {}, evalmode: {}", pc(), inst->to_string(), inst->src_loc, (int)curr_eval_mode());


    if(expected_op.has_value()) {
        if(inst->op != expected_op.value()) {
            DEBUG_PANIC("Expected instruction {} at pc() {}, but got {}", string(expected_op.value()), pc(), string(inst->op));
        }
    }

    // 数据流声明——用于自动错误传播
    auto dep_arr = deps_of(inst, stage_allocator());

    // 已迁移 handler 的结果（留空 = 旧 dispatch）
    std::optional<AnalyzeResult> result_opt = std::nullopt;

    // 自动 deps 错误传播：依赖有 error → 自动 Set_ResultError(pc()) + targets
    for(isize i = 0; i < dep_arr.count; i++) {
        if(dep_arr[i] == INVALID_INST) continue;
        if(has_error(dep_arr[i])) {
            Set_ResultError(pc());
            goto end;
        }
    }

    // ★ 已迁移 handler — 提前拦截
    if(inst->op == CIROperator::ConstantValue) {
        result_opt = handler_ConstantValue(inst, pc());
        goto end;
    }
    if(inst->op == CIROperator::PointerType) {
        result_opt = handler_PointerType(inst, pc());
        goto end;
    }
    if(inst->op == CIROperator::Load) {
        result_opt = handler_Load(inst, pc());
        goto end;
    }
    if(inst->op == CIROperator::Deref) {
        result_opt = handler_Deref(inst, pc());
        goto end;
    }

old:
    // ★ 未迁移 handler（旧 dispatch）
    if(inst->op == CIROperator::Block) {
        analyze_Block(params.block_eval_mode);
    } else {
        switch(inst->op) {
#define X(name) case CIROperator::name: analyze_##name(); break;
        CIR_OPERATORS
#undef X
        }
    }

end:
    if(result_opt.has_value()) {
        auto& [result, target] = *result_opt;
        apply_result(target, result);
    }
    pc() += 1;
}


// 控制流
void Interpreter::analyze_Block(std::optional<EvalMode> force_eval_mode) {
    XP_ASSERT_DEFAULT(pkg->inst(pc())->op == CIROperator::Block);

    auto block_info = pkg->inst(pc())->block_info;

    bool pushed_eval_mode = false;

    // 如果有强制的 eval_mode, 则优先选择force_eval_mode
    if(force_eval_mode.has_value()) {
        DEBUG_TRACE("analyze_block push force_eval_mode={} at pc={}", (int)*force_eval_mode, pc());
        
        
        eval_mode_stack.push_back(*force_eval_mode);
        pushed_eval_mode = true;
    } else if(block_info.immediate_eval) {
        // 如果需要立即求值或编译期块, 则强制 FullEval

        eval_mode_stack.push_back(EvalMode::FullEval);
        pushed_eval_mode = true;
    }

    CIRInstructionRef block_inst_ref = pc();

    isize end_pc = pc() + 1 + block_info.body_len;

    DEBUG_TRACE("analyze_block at pc={}: end_pc={}, body_len={}", pc(), end_pc, block_info.body_len);
    pc() += 1; // 跳过 Block 指令本身，进入 Block 内部指令分析

    while(pc() < end_pc && !has_result_val(block_inst_ref)) {
        analyze_instruction();
    }


    DEBUG_TRACE("analyze_block at pc={}: while loop done, pc={}, end_pc={}", block_inst_ref, pc(), end_pc);
    pc() = end_pc - 1; // FullEval 提前退出时跳过剩余指令




    if(pushed_eval_mode) {
        DEBUG_TRACE("analyze_block pop at pc={}, was mode={}", pc(), (int)eval_mode_stack.back());
        eval_mode_stack.pop_back();
    }

}

void Interpreter::analyze_Break() {
    auto info = pkg->inst(pc())->break_info;

    CIRInstructionRef target_block = info.break_block;
    // DEBUG_TRACE("Break: pc={}, target_block={} (op={}), break_value_inst={}, curr_eval_mode={}",
    //     pc(), target_block, pkg->inst(target_block)->to_string(), info.break_value_inst, (int)curr_eval_mode());
    XP_ASSERT_DEFAULT(pkg->inst(target_block)->op == CIROperator::Block || pkg->inst(target_block)->op == CIROperator::Loop);


    if(info.break_value_inst != INVALID_INST) {

        if(propagate_error({info.break_value_inst})) { Set_ResultError(target_block); return; }

        DEBUG_TRACE("Break type check: target_block {} state={}, break_value_inst {} state={}",
            target_block, (int)result_state(target_block), info.break_value_inst, (int)result_state(info.break_value_inst));


        // 如果已经设置类型了, 判断类型是否匹配
        if(result_state(target_block) >= CIRResultState::OnlyType) {
            TypeRef existing_type = ResultType(target_block);
            TypeRef new_type = ResultType(info.break_value_inst);
            DEBUG_TRACE("Break type compare: existing={}, new={}", (void*)existing_type, (void*)new_type);
            if(existing_type != new_type) {
                context()->reporter.report_error(pkg->inst(pc())->src_loc, "type mismatch in break value");
                Set_ResultError(pc());
                Set_ResultError(target_block);
                return;
            }
        } else {

            // TODO: HACK
            // 否则设置类型
            if(has_result_type(info.break_value_inst)) {
                TypeRef new_type = ResultType(info.break_value_inst);
                DEBUG_TRACE("Break set type: target_block {} gets type {}", target_block, (void*)new_type);
                Set_ResultType(target_block, ResultType(info.break_value_inst));
            }
        }


        DEBUG_TRACE("Break eval check: curr_eval_mode={}, break_val_has_val={}", (int)curr_eval_mode(), has_result_val(info.break_value_inst));

        if(curr_eval_mode() == EvalMode::FullEval && has_result_val(info.break_value_inst)) {

            DEBUG_TRACE("Break setting result of target_block {} from break_value_inst {} (has_val=true)",
                target_block, info.break_value_inst);

            Set_ResultValue(target_block, ResultValue(info.break_value_inst));
            DEBUG_TRACE("Break after Set_ResultValue: target_block={}, state={}, outstanding_type={}", target_block, (int)result_state(target_block), (void*)ResultType(target_block));
        }

    }

}

void Interpreter::analyze_CondBr() {
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

    if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval({cond_inst})) {
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

void Interpreter::analyze_Loop() {
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



void Interpreter::analyze_ConstDecl() {
    if(has_result_val(pc())) {
        return;
    }

    CIRConstDecl info = pkg->inst(pc())->const_decl;
    SymbolInfo* sym = (info.symbol)();
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


    sym->state = SymbolState::Solving;
    analyze_instruction(CIROperator::Block, {.block_eval_mode = EvalMode::FullEval}); // const 的值在 block 里完整求值
    pc() -= 1;

    if(has_error(info.value_inst)) {
        Set_ResultError(const_decl_inst);
        return;
    }

    if(!has_result_val(info.value_inst)) {
        if(has_result_type(info.value_inst)) {
            Set_ResultType(const_decl_inst, ResultType(info.value_inst));
        }
        sym->val(CIRInstUniqueKey{pkg, sym->package, const_decl_inst});
        sym->state = SymbolState::Solved;
        return;
    }

    Value result = ResultValue(info.value_inst);
    result = clone_value(result, permanent_allocator());
    sym->val(CIRInstUniqueKey{pkg, sym->package, const_decl_inst});
    sym->state = SymbolState::Solved;

    Set_ResultTypeAndValue(const_decl_inst, result);
}



void Interpreter::analyze_FunctionDecl(bool try_to_instantiate) {
    auto& func = pkg->inst(pc())->func_decl;

    // 解析参数类型
    Array<TypeRef> param_types = make_array<TypeRef>(stage_allocator());
    defer(array_free(&param_types));

    for(isize i = 0; i < func.arg_type_insts.count; i++) {
        if(func.arg_type_insts[i] != INVALID_INST) {
            param_types.push_back(ResultValue(func.arg_type_insts[i]).type_val());
        } else {
            // var_arg
            param_types.push_back(easy_type(Type_var_arg_c));
        }
    }

    for(isize i = 0; i < func.arg_decl_insts.count; i++) {
        if(!pkg->inst(func.arg_decl_insts[i])->var_decl.is_comptime && func.arg_type_insts[i] != INVALID_INST) {
            if(is_type_type(ResultValue(func.arg_type_insts[i]).type_val())) {
                context()->reporter.report_error(
                    pkg->inst(func.arg_decl_insts[i])->src_loc,
                    "parameter of type 'type' must be a compile-time parameter (prefix with '$')"
                );
                Set_ResultError(pc());
            }
        }
    }

    // 预先分析 comptime 参数的 VariableDecl，让符号在 return_type 和调用端可见
    if(is_generic_func(pkg, func)) {
        for(isize i = 0; i < func.arg_decl_insts.count; i++) {
            auto decl_inst = func.arg_decl_insts[i];
            auto& vd = pkg->inst(decl_inst)->var_decl;

            if(vd.is_comptime) {
                new_pc_flow(decl_inst);
                analyze_VariableDecl();
                recover_pc_flow();

                TypeRef param_type = undefined_type();
                if(func.arg_type_insts[i] != INVALID_INST) {
                    auto& arg_type_res = result_for(func.arg_type_insts[i]);
                    if(arg_type_res.state >= CIRResultState::OnlyType) {
                        param_type = arg_type_res.type();
                    }
                }
                result_for(decl_inst).set_type(param_type);
            }
        }
    }

    TypeRef return_type = undefined_type();
    if(!(is_generic_func(pkg, func) && !try_to_instantiate)) {
        new_pc_flow(func.return_type_inst);
        analyze_instruction(CIROperator::Block, {.block_eval_mode = EvalMode::FullEval});
        recover_pc_flow();

        if(has_result_val(func.return_type_inst)) {
            return_type = ResultValue(func.return_type_inst).type_val();
        }
    }

    {

        // TODO: NOTE: 如果泛型函数, return_type 是 undefined_type, 这里先占位, 但是很不优雅, 很HACK, 需要改进
        TypeRef func_type_type = function_type(param_types, return_type);

        SymbolInfo* func_sym = (func.symbol)();
        Value v = make_value(func_type_type);
        
        // TODO: check
        v.func_val(func.name, {pkg, func_sym->package, pc()});

        DEBUG_TRACE("to set type and result for func {}", func.name);

        if(result_state(pc()) != CIRResultState::Error) {

            // TODO: HACK: 这里直接设置了函数类型和函数值, 但是如果是泛型函数, 这个值是没有意义的, 因为泛型函数的值需要在实例化时才有意义
            // 但是目前为了在IdentVal能获取到它, 先设置了一个占位的函数值, 但是这个值是没有意义的, 因为泛型函数的值需要在实例化时才有意义
            // 这里应该改进, 让泛型函数的值在实例化时才有意义, 而不是在定义时就有意义
            Set_ResultTypeAndValue(pc(), v);
        }
    }

    if(func.is_extern_c) {
        // extern "C" 函数没有函数体，不分析了
    } else {
        ASSERT_MSG(func.body_inst != INVALID_INST, "non-extern function must have body");

        if(is_generic_func(pkg, func) && !try_to_instantiate) {
            // 泛型模板：跳过 body（编译期参数无值，body 无法分析）
            pc() = func.body_inst + pkg->inst(func.body_inst)->len();
            pc() -= 1;
        } else if(is_pure_comptime_func(func, *pkg)) {
            // 纯编译期函数：跳过 body
            pc() = func.body_inst + pkg->inst(func.body_inst)->len();
            pc() -= 1;
        } else {
            // 普通运行时函数：TypeOnly body
            pc() = func.body_inst;
            analyze_instruction(CIROperator::Block, {.block_eval_mode = EvalMode::TypeOnly});
            pc() -= 1;
        }
    }



}

void Interpreter::analyze_Call() {
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

    // ── 泛型实例化 ──────────────────────────────────────────
    bool callee_is_generic = false;
    CIRPackage *generic_callee_pkg = nullptr;
    CIRInstructionRef generic_func_decl_pc = {};
    CIRFunction* generic_func = nullptr;
    CIRResultInstance *generic_result_instance = nullptr;
    CIRPackage *generic_caller_pkg = pkg;
    CIRInstructionRef generic_caller_pc = pc();
    Scope *generic_saved_scope = scope();

    if(has_result_val(called_inst)) {
        Value called_val = ResultValue(called_inst);
        FuncValue fv = called_val.func_val();
        generic_callee_pkg = fv.func_key.cir_package;
        generic_func_decl_pc = fv.func_key.defining_inst;
        generic_func = &generic_callee_pkg->inst(generic_func_decl_pc)->func_decl;

        if(is_generic_func(generic_callee_pkg, *generic_func)) {
            callee_is_generic = true;
            auto& func = *generic_func;
            isize arg_count = func.arg_decl_insts.count;

            if(!has_result_val(call_info.arg_insts)) {
                if(curr_eval_mode() == EvalMode::FullEval) {
                    context()->reporter.report_error(pkg->inst(pc())->src_loc, "cannot call function with non-constant arguments at compile time");
                    Set_ResultError(pc());
                }
                return;
            }

            Array<Value> comptime_args = make_array<Value>(permanent_allocator());
            for(isize i = 0; i < arg_count; i++) {
                if(generic_callee_pkg->inst(func.arg_decl_insts[i])->var_decl.is_comptime) {
                    comptime_args.push_back(clone_value(ResultValue(call_info.arg_insts[i]), permanent_allocator()));
                }
            }

            FuncCallKey cache_key = {};
            cache_key.func_decl_pc = generic_func_decl_pc;
            cache_key.comptime_arg_refs = make_array<CIRInstResultRef>(permanent_allocator());
            {
                std::optional<CIRResultInstance*> ri;
                if(auto* inst = curr_instance()) ri = inst->result_instance;
                for(isize i = 0; i < arg_count; i++) {
                    Value arg_val = ResultValue(call_info.arg_insts[i]);
                    if(is_type_type(arg_val.type)) {
                        cache_key.comptime_arg_refs.push_back(CIRInstResultRef::make(pkg, call_info.arg_insts[i], ri));
                    }
                }
            }

            generic_result_instance = generic_callee_pkg->get_result_instance(cache_key);
            {
                CIRInstResult *cached_body = xp_hash_map_get(generic_result_instance->results, func.body_inst);
                if(cached_body && cached_body->state == CIRResultState::WholeValue) {
                    Set_ResultTypeAndValue(pc(), cached_body->actual_val());
                    return;
                }
            }
            {
                auto* ret_res = xp_hash_map_get(generic_result_instance->results, func.return_type_inst);
                if(ret_res && ret_res->state >= CIRResultState::WholeValue) {
                    TypeRef concrete = ret_res->actual_val().type_val();
                    if(concrete != type_type()) {
                        Set_ResultType(pc(), concrete);
                        return;
                    }
                }
            }

            generic_saved_scope = scope();

            auto inst = EvalInstance::make(generic_callee_pkg, generic_func_decl_pc, func.slot_count, stage_allocator());
            inst.result_instance = generic_result_instance;
            isize frame_base = stack_mem.bytes.count;
            inst.frame_base = frame_base;

            for(isize i = 0; i < arg_count; i++) {
                TypeRef arg_type = ResultType(call_info.arg_insts[i]);
                auto ptr = stack_mem.alloc_bytes(type_size_of(arg_type), type_align_of(arg_type));
                ptr.store(ResultValue(call_info.arg_insts[i]));
                inst.var_ptrs[i] = ptr;
            }

            inst.cache_key = cache_key;
            instance_stack.push_back(std::move(inst));
            pkg = generic_callee_pkg;
            scope() = nullptr;

            isize ct_idx = 0;
            for(isize i = 0; i < arg_count; i++) {
                auto decl_inst = func.arg_decl_insts[i];
                if(!pkg->inst(decl_inst)->var_decl.is_comptime) continue;

                result_for(decl_inst).set_val(comptime_args[ct_idx++]);
            }

            TypeRef concrete_ret = nullptr;
            new_pc_flow(func.return_type_inst);
            analyze_instruction(CIROperator::Block, {.block_eval_mode = EvalMode::FullEval});
            recover_pc_flow();
            if(has_result_val(func.return_type_inst))
                concrete_ret = ResultValue(func.return_type_inst).type_val();

            if(concrete_ret == type_type()) {
                // pure comptime
                if(curr_eval_mode() == EvalMode::TypeOnly) {
                    stack_mem.bytes.count = frame_base;
                    scope() = generic_saved_scope;
                    pkg = generic_caller_pkg;
                    EvalInstance::free(curr_instance());
                    instance_stack.pop_back();
                    pc() = generic_caller_pc;
                    Set_ResultType(generic_caller_pc, type_type());
                    return;
                }
                // FullEval: keep stack allocated, instance stays, fall through to execution block
                return_type = type_type();
            } else {
                // runtime generic
                TypeRef concrete = concrete_ret ? concrete_ret : return_type;

                {
                    new_pc_flow(func.body_inst);
                    analyze_instruction(CIROperator::Block, {.block_eval_mode = EvalMode::TypeOnly});
                    recover_pc_flow();
                }

                if(curr_eval_mode() != EvalMode::FullEval) {
                    generic_callee_pkg->generic_instance_keys.push_back(cache_key);
                }
                stack_mem.bytes.count = frame_base;
                scope() = generic_saved_scope;
                pkg = generic_caller_pkg;
                EvalInstance::free(curr_instance());
                instance_stack.pop_back();
                pc() = generic_caller_pc;
                Set_ResultType(generic_caller_pc, concrete);
                return;
            }
        }
    }

    // ── 编译期执行（FullEval only）────────────────────────────
    if(curr_eval_mode() == EvalMode::FullEval) {
        if(callee_is_generic) {
            // 纯 comptime 泛型：实例已就绪，只跑 body
            auto& func = *generic_func;
            if(func.is_extern_c) {
                context()->reporter.report_error(pkg->inst(pc())->src_loc, "cannot call extern \"C\" function at compile time");
                Set_ResultError(pc());
                return;
            }

            pc() = func.body_inst;
            analyze_instruction(CIROperator::Block, {.block_eval_mode = EvalMode::FullEval});

            bool has_return = has_result_val(func.body_inst);
            Value return_val = {};
            if(has_return) {
                return_val = clone_value(ResultValue(func.body_inst), permanent_allocator());
                CIRInstResult cached;
                cached.set_val(return_val);
                xp_hash_map_insert(&generic_result_instance->results, func.body_inst, cached);
            }

            scope() = generic_saved_scope;
            pkg = generic_caller_pkg;
            stack_mem.bytes.count = curr_instance()->frame_base;
            EvalInstance::free(curr_instance());
            instance_stack.pop_back();
            pc() = generic_caller_pc;

            if(has_return) {
                Set_ResultValue(generic_caller_pc, return_val);
            }
            return;
        }

        // 非泛型：完整设置 + 执行
        constexpr auto MAX_CALL_DEPTH = 100;
        if(instance_stack.count > MAX_CALL_DEPTH) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc,
                "circular dependency or recursion too deep (max {} call depth)", MAX_CALL_DEPTH);
            Set_ResultError(pc());
            return;
        }

        ASSERT(has_result_val(called_inst));

        Value called_val = ResultValue(called_inst);
        FuncValue fv = called_val.func_val();
        CIRPackage *callee_pkg = fv.func_key.cir_package;
        CIRInstructionRef func_decl_pc = fv.func_key.defining_inst;
        CIRFunction& func = callee_pkg->inst(func_decl_pc)->func_decl;

        isize var_count = func.slot_count;
        isize arg_count = func.arg_decl_insts.count;

        if(!has_result_val(call_info.arg_insts)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "cannot call function with non-constant arguments at compile time");
            Set_ResultError(pc());
            return;
        }

        FuncCallKey cache_key = {};
        cache_key.func_decl_pc = func_decl_pc;
        cache_key.comptime_arg_refs = make_array<CIRInstResultRef>(permanent_allocator());

        CIRResultInstance *callee_result_instance = callee_pkg->get_result_instance(cache_key);
        {
            CIRInstResult *cached_body = xp_hash_map_get(callee_result_instance->results, func.body_inst);
            if(cached_body && cached_body->state == CIRResultState::WholeValue) {
                Set_ResultTypeAndValue(pc(), cached_body->actual_val());
                return;
            }
        }

        if(func.is_extern_c) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "cannot call extern \"C\" function at compile time");
            Set_ResultError(pc());
            return;
        }

        CIRPackage *caller_pkg = pkg;
        CIRInstructionRef caller_pc = pc();
        Scope *saved_scope = scope();

        auto inst = EvalInstance::make(callee_pkg, func_decl_pc, var_count, stage_allocator());
        inst.result_instance = callee_result_instance;
        inst.frame_base = stack_mem.bytes.count;
        defer(stack_mem.bytes.count = inst.frame_base);

        for(isize i = 0; i < arg_count; i++) {
            TypeRef arg_type = ResultType(call_info.arg_insts[i]);
            isize size  = type_size_of(arg_type);
            isize align = type_align_of(arg_type);

            auto ptr = stack_mem.alloc_bytes(size, align);
            ptr.store(ResultValue(call_info.arg_insts[i]));

            inst.var_ptrs[i] = ptr;
        }

        inst.cache_key = cache_key;
        instance_stack.push_back(std::move(inst));
        pkg = callee_pkg;
        scope() = nullptr;

        pc() = func.body_inst;
        analyze_instruction(CIROperator::Block, {.block_eval_mode = EvalMode::FullEval});

        bool has_return = has_result_val(func.body_inst);
        Value return_val = {};
        if(has_return){
            return_val = ResultValue(func.body_inst);
        }

        scope() = saved_scope;
        pkg = caller_pkg;
        EvalInstance::free(curr_instance());
        instance_stack.pop_back();
        pc() = caller_pc;

        if(has_return) {
            return_val = clone_value(return_val, permanent_allocator());
            CIRInstResult cached;
            cached.set_val(return_val);
            xp_hash_map_insert(&callee_result_instance->results, func.body_inst, cached);
            Set_ResultValue(pc(), return_val);
        }
        return;
    }

    Set_ResultType(pc(), return_type);
}



void Interpreter::analyze_GetOrInitStruct() {
    if(!has_result_type(pc())) {

        // TODO: type_type(undefined_type())得换成更规范的表示
        Set_ResultType(pc(), type_type());
    }



    auto res = eval_GetOrInitStruct(pc());
    Set_ResultTypeAndValue(pc(), res);

}

void Interpreter::analyze_StructField() {
    auto type_block_inst = pkg->inst(pc())->struct_field_info.type_block_inst;
    if (propagate_error({type_block_inst})) {
        Set_ResultError(pc());
        return;
    }
    
    auto& res = result_for(type_block_inst);
    
    DEBUG_TRACE("StructField pc={}: type_block_inst={}, state={}, type={}", pc(), type_block_inst, (int)res.state, (void*)res.type());
    if(has_result_type(type_block_inst)) {
        Set_ResultType(pc(), ResultType(type_block_inst));
    }
    if(has_result_val(type_block_inst)) {
        Set_ResultValue(pc(), ResultValue(type_block_inst));
    }
}


void Interpreter::analyze_FinishStruct() {
    auto& info = pkg->inst(pc())->finish_struct_info;
    auto struct_decl_inst = info.struct_decl_inst;

    if(propagate_error({struct_decl_inst}) || propagate_error(info.field_insts)) {
        Set_ResultError(pc());
        return;
    }


    Set_ResultType(pc(), type_type());

    // 有具体字段值时，完成结构体类型（填字段）
    if(curr_eval_mode() == EvalMode::FullEval || (should_eval_for_lazy_eval({struct_decl_inst}) && should_eval_for_lazy_eval(info.field_insts))) {

        TypeRef st = ResultValue(struct_decl_inst).type_val();

        bool already_finished = st->struct_info.struct_fields.count > 0;
        if(!already_finished) {
            Array<StructField> fields = make_array<StructField>(type_allocator());
            for(isize i = 0; i < info.field_insts.count; i++) {
                auto field_inst = info.field_insts[i];
                auto& field_info = pkg->inst(field_inst)->struct_field_info;

                TypeRef field_type = ResultValue(field_info.type_block_inst).type_val();

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

            finish_unfinish_struct_type(st, fields);
        }

        Set_ResultValue(pc(), ResultValue(struct_decl_inst));
    }

}

void Interpreter::analyze_EnumDeclInit() {
    auto& info = pkg->inst(pc())->enum_decl_init_info;

    if(propagate_error({info.tag_type_inst})) return;

    Set_ResultType(pc(), type_type());

    if(!should_eval_for_lazy_eval({info.tag_type_inst})) {
        return;
    }

    TypeRef elem_type = ResultValue(info.tag_type_inst).type_val();
    if(!is_integer_type(elem_type)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "underlying type of enum must be an integer type");
        Set_ResultError(pc());
        return;
    }


    std::optional<xpString> enum_name = std::nullopt;
    SymbolInfo *enum_sym = info.symbol();
    if(enum_sym != nullptr) {
        enum_name = enum_sym->name;
    }

    TypeRef enum_type = enum_type_impl(info.decl_ast, enum_name, elem_type, info.scope);

    TypeRef meta = type_type();
    Value v = make_value(meta);
    v.type_val(enum_type);

    Set_ResultTypeAndValue(pc(), v);

    {
        SymbolInfo *enum_sym2 = info.symbol();
        if(enum_sym2 != nullptr) {
            enum_sym2->val(v);
            enum_sym2->state = SymbolState::Solved;
        }
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
            field_val.set_type(enum_type);
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
        if(ef.value_inst != INVALID_INST) {
            field_sym->val(CIRInstUniqueKey{pkg, field_sym->package, ef.value_inst});
        } else {
            field_sym->val(field_val);
        }
        field_sym->state = SymbolState::Solved;
    }

}

void Interpreter::analyze_UnionDecl() {
    // 联合体声明：目前只做类型创建
    // 联合体在 xoaop 语言中尚未完全定义语义，保留占位
    Type union_t = make_type(Type_union);
    TypeRef union_type = get_or_add_type(union_t);
    TypeRef meta = type_type();

    Value v = make_value(meta);
    v.type_val(union_type);
    Set_ResultTypeAndValue(pc(), v);

}




// 表达式
void Interpreter::analyze_Binary() {
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
    } else if(is_pointer_type(left_type) && is_untyped_type(right_type)) {
        Set_ResultType(right_inst, easy_type(Type_i64));
        contagion_target = right_inst;
    } else if(is_untyped_type(left_type) && is_pointer_type(right_type)) {
        Set_ResultType(left_inst, easy_type(Type_i64));
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


    if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval({left_inst, right_inst})) {
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

void Interpreter::analyze_Unary() {
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

    if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval({operand_inst})) {
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



void Interpreter::analyze_Cast() {
    if(has_result_val(pc())) {
        return;
    }

    auto& cast_info = pkg->inst(pc())->cast_info;

    auto expr_inst = cast_info.expr_inst;
    auto target_type_inst = cast_info.target_type_inst;

    if(propagate_error({expr_inst, target_type_inst})) return;

    if(!has_result_val(target_type_inst)) {
        return;
    }

    TypeRef expr_type = ResultType(expr_inst);
    TypeRef target_type = ResultValue(target_type_inst).type_val();

    if(!check_explicit_type_cast(pkg, expr_inst, expr_type, target_type)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "cannot cast between these types");
        Set_ResultError(pc());
        return;
    }

    TypeRef result_type = target_type;
    XP_ASSERT_DEFAULT(result_type != nullptr);
    Set_ResultType(pc(), result_type);

    if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval({expr_inst})) {
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






void Interpreter::analyze_FieldAccess() {

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

        auto r = field_sym->result(curr_cache_key());
        Set_ResultType(pc(), r.type());
        if(r.state == CIRResultState::WholeValue) {
            Set_ResultTypeAndValue(pc(), r.actual_val());
        }

    } else if(TypeRef struct_type = get_struct_type(parent_type)) {
        bool field_found = false;
        for(isize i = 0; i < struct_type->struct_info.struct_fields.count; i++) {
            if(xp_string_equal(struct_type->struct_info.struct_fields[i].name, info.field_name)) {
                field_found = true;

                Set_ResultType(pc(), struct_type->struct_info.struct_fields[i].type);

                if(is_string_struct_type(struct_type)) {
                    break;
                }

                if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval({parent_inst})) {
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

    } else if(has_result_val(parent_inst) && is_enum_type(ResultValue(parent_inst).type_val())) {
        // 枚举成员访问: EnumType.Variant
        TypeRef enum_type = ResultValue(parent_inst).type_val();

        Set_ResultType(pc(), enum_type);

        SymbolInfo *field_sym = find_symbol_curr(enum_type->enum_info.enum_scope, info.field_name);

        if(field_sym == nullptr) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "enum variant '{}' not found", info.field_name);
            Set_ResultError(pc());
            return;
        }

        auto r = field_sym->result(curr_cache_key());
        if(r.state == CIRResultState::WholeValue) {
            Set_ResultValue(pc(), r.actual_val());
        }

    } else {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "field access on unsupported type");
        Set_ResultError(pc());
        return;
    }

}



void Interpreter::analyze_FieldPtr() {
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
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "field pointer access only supported on struct types or pointers to struct, but got '{}'", parent_type->name());
        Set_ResultError(pc());
        return;
    }

    for(isize i = 0; i < struct_type->struct_info.struct_fields.count; i++) {
        if(xp_string_equal(struct_type->struct_info.struct_fields[i].name, info.field_name)) {
            Set_ResultType(pc(), struct_type->struct_info.struct_fields[i].type);
            set_lvalue(pc());


            if(curr_eval_mode() == EvalMode::FullEval && has_instance()) {

                auto ptr = ResultValue(parent_inst).pointer_val();

                TypeRef field_type = struct_type->struct_info.struct_fields[i].type;
                isize field_off = field_offset_in_struct(struct_type, i);
                isize total_off = ptr.offset + field_off;

                Value addr = make_value(pointer_type(field_type));

                // TODO: ABSTRACT ptr.mem
                addr.pointer_val(Pointer::make(ptr.mem, total_off));

                Set_ResultValue(pc(), addr);
            }

            return;
        }
    }

    context()->reporter.report_error(pkg->inst(pc())->src_loc, "struct field '{}' not found for FieldPtr", info.field_name);
    Set_ResultError(pc());
}



void Interpreter::analyze_StructInit() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->struct_init_info;

    if(propagate_error({info.struct_type_inst})) return;
    if(propagate_error(info.field_init_insts)) return;

    if(!has_result_val(info.struct_type_inst)) {
        return;
    }

    TypeRef st = ResultValue(info.struct_type_inst).type_val();
    // XP_ASSERT_DEFAULT(is_struct_type(st));
    if(!is_struct_type(st)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "你不能把非结构体类型 '{}' 当作结构体来初始化", st->name());
        Set_ResultError(pc());
        return;
    }

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
        std::optional<TypeRef> implicit_type = result_for(info.field_init_insts[i]).implicit_type;
        if(field_type != expected && ((!implicit_type.has_value()) || implicit_type != expected)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "struct initializer field type does not match struct definition");
            Set_ResultError(pc());
            return;
        }
    }

    if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval(info.field_init_insts)) {
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



void Interpreter::analyze_ArrayInit() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->array_init_info;

    if(propagate_error(info.element_insts)) return;

    // 找第一个有具体类型的元素作为数组元素类型
    // 都没有则用第一个元素的 untyped 类型，留给 DetermineType 决定
    TypeRef elem_type = nullptr;

    if(info.element_insts.count > 0) {
        elem_type = ResultType(info.element_insts[0]);

        for(isize i = 0; i < info.element_insts.count; i++) {
            TypeRef t = ResultType(info.element_insts[i]);
            if(!is_untyped_type(t)) {
                elem_type = t;
                break;
            }
        }
    }

    // 有具体类型则将其他 untyped 元素传染为该类型
    if(elem_type != nullptr && !is_untyped_type(elem_type)) {
        for(isize i = 0; i < info.element_insts.count; i++) {
            auto ei = info.element_insts[i];
            TypeRef t = ResultType(ei);

            if(is_untyped_type(t)) {
                Set_ResultType(ei, elem_type);
                if(has_result_val(ei) && is_val_overflow(ResultValue(ei))) {
                    context()->reporter.report_error(pkg->inst(pc())->src_loc, "value overflow in array initializer element");
                    Set_ResultError(pc());
                    return;
                }
            } else if(t != elem_type) {
                context()->reporter.report_error(pkg->inst(pc())->src_loc, "array initializer element types do not match");
                Set_ResultError(pc());
                return;
            }
        }
    }

    usize count = info.element_insts.count;

    TypeRef arr_type = array_type(elem_type, count);
    Set_ResultType(pc(), arr_type);

    if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval(info.element_insts)) {
        Value v = make_value(arr_type);
        v.set_type(arr_type);
        Array<Value> elem_values = make_array<Value>(permanent_allocator());
        for(isize i = 0; i < info.element_insts.count; i++) {
            elem_values.push_back(ResultValue(info.element_insts[i]));
        }
        v.array_element_values(elem_values);
        Set_ResultValue(pc(), v);
    }

}



void Interpreter::analyze_Index() {
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

    if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval({array_inst, index_inst})) {
        i128 idx = ResultValue(index_inst).integer_val();

        if(is_array_type(array_type_ref)) {
            if(idx < 0 || idx >= array_type_ref->array_info.count) {
                context()->reporter.report_error(pkg->inst(pc())->src_loc, "array index out of bounds");
                Set_ResultError(pc());
                return;
            }
            XP_TODO(); // compile-time array element extraction
        } else if(is_string_struct_type(array_type_ref)) {
            auto str_val = ResultValue(array_inst);

            auto str_count = str_val.struct_field_val(1).integer_val();

            if(!(idx >= 0 && idx < str_count)) {
                context()->reporter.report_error(pkg->inst(pc())->src_loc, "string index out of bounds");
                Set_ResultError(pc());
                return;
            }

            // TODO: 不保险, 有点危险
            Value char_val = make_value(str_val.struct_field_val(0).type->pointed_type);
            char_val.integer_val(str_val.struct_field_val(0).pointer_val().load(str_val.struct_field_val(0).type->pointed_type, idx, {}).integer_val()); // TODO: ASSUME: 这里的 load 不需要 allocator


            Set_ResultValue(pc(), char_val);
        } else if(is_slice_struct_type(array_type_ref)) {
            // TODO: compile-time slice element extraction
            XP_TODO(); // compile-time slice element extraction
        }
    }

}



void Interpreter::analyze_IndexPtr() {
    auto& info = pkg->inst(pc())->index_info;
    CIRInstructionRef array_inst = info.array_inst;

    if(propagate_error({array_inst, info.index_inst})) return;

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

    if(curr_eval_mode() == EvalMode::FullEval && has_instance()) {
        // TODO: ABSTRACT
        auto base_ptr = ResultValue(array_inst);
        i128 idx = ResultValue(info.index_inst).integer_val();
        isize stride = type_stride_of(elem_type);

        auto ptr = Pointer::add(base_ptr.pointer_val(), idx, stride);

        Value addr = make_value(pointer_type(elem_type));
        addr.pointer_val(ptr);

        Set_ResultValue(pc(), addr);
    }
}


// TODO: 模仿Deref的处理
void Interpreter::analyze_AddrOf() {
    auto& info = pkg->inst(pc())->addr_of_info;
    CIRInstructionRef lval_inst = info.lval_inst;

    if(propagate_error({lval_inst})) return;

    TypeRef lval_type = ResultType(lval_inst);

    if(!is_lvalue(lval_inst) && !is_function_type(lval_type)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "cannot take address of non-lvalue expression");
        Set_ResultError(pc());
        return;
    }

    if(is_function_type(lval_type)) {
        FuncValue fv;
        bool got_fv = false;

        if(has_result_val(lval_inst)) {
            fv = ResultValue(lval_inst).func_val();
            got_fv = true;
        } else {
            CIRInstruction* lval_ptr = pkg->inst(lval_inst);
            SymbolInfo* sym = (lval_ptr->symbol)();
            if(sym && sym->is_const_decl_and_func()) {
                auto r = sym->result(curr_cache_key());
                if(r.state == CIRResultState::WholeValue) {
                    fv = r.actual_val().func_val();
                    got_fv = true;
                }
            }
        }

        if(got_fv && is_generic_func(fv.func_key.cir_package, fv.func_key.cir_package->inst(fv.func_key.defining_inst)->func_decl)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "cannot take address of generic function");
            Set_ResultError(pc());
            return;
        }
    }

    TypeRef ptr_type = pointer_type(lval_type);
    Set_ResultType(pc(), ptr_type);

    if(curr_eval_mode() == EvalMode::FullEval && has_instance()) {
        if(is_lvalue(lval_inst)) {
            Value addr = ResultValue(lval_inst);
            Set_ResultValue(pc(), addr);
        }
    }

}


// 变量 / 存储 / 类型标注


void Interpreter::analyze_Load() {
    auto& load_info = pkg->inst(pc())->load_info;
    CIRInstructionRef ptr_inst = load_info.ptr_inst;

    if(propagate_error({ptr_inst})) return;

    if(is_lvalue(ptr_inst)) {
        TypeRef loaded_type = ResultType(ptr_inst);
        Set_ResultType(pc(), loaded_type);

        if(curr_eval_mode() == EvalMode::FullEval && has_instance()) {
            auto ptr = ResultValue(ptr_inst).pointer_val();
            Set_ResultValue(pc(), ptr.load(loaded_type, stage_allocator()));
        }
    } else {
        TypeRef ptr_type = ResultType(ptr_inst);
        if(!is_pointer_type(ptr_type)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "cannot load from a non-pointer value");
            Set_ResultError(pc());
            return;
        }
        Set_ResultType(pc(), ptr_type->pointed_type);

        if(curr_eval_mode() == EvalMode::FullEval && has_instance()) {
            auto ptr = ResultValue(ptr_inst).pointer_val();
            Set_ResultValue(pc(), ptr.load(ptr_type->pointed_type, stage_allocator()));
        }
    }

}



void Interpreter::analyze_VariableDecl() {
    auto& vd = pkg->inst(pc())->var_decl;
    SymbolInfo *sym = (vd.symbol)();
    XP_ASSERT_DEFAULT(sym != nullptr);

    Set_ResultType(pc(), undefined_type());
    set_lvalue(pc());
    sym->val(CIRInstUniqueKey{pkg, sym->package, pc()});
    sym->state = SymbolState::Solved;

    // FullEval 时不分配内存，TypeAscribe 会在类型已知后分配
}


void Interpreter::analyze_IdentRef() {
    SymbolInfo *info = (pkg->inst(pc())->symbol)();

    if(info == nullptr) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "undefined identifier '{}'", pkg->inst(pc())->ident);
        Set_ResultError(pc());
        return;
    }

    if(!info->is_var_decl() && !info->is_const_decl_and_func()) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "identifier '{}' is not an addressable entity", info->name);
        Set_ResultError(pc());
        return;
    }

    if(info->state == SymbolState::Unsolved) {
        new_pc_flow(info->val_as_inst_key().defining_inst);
        defer(recover_pc_flow());

        analyze_instruction();
    } else if(info->state == SymbolState::Solving) {
        context()->reporter.report_error(SourceLocation({}, pkg->inst(pc())->src_loc.span), "circular dependency detected when evaluating '{}'", info->name);
        Set_ResultError(pc());
        return;
    }

    if(info->value_store_type == ValueStoreType::InCIRInstruction) {
        if(propagate_error({info->val_as_inst_key().defining_inst})) {
            return;
        }
    }

    auto r = info->result(curr_cache_key());
    Set_ResultType(pc(), r.type());
    set_lvalue(pc());
    result_for(pc()).set_actual_type(pointer_type(r.type()));

    if(has_instance() && (curr_eval_mode() == EvalMode::FullEval || r.state == CIRResultState::WholeValue)) {
        if(info->is_var_decl()) {
            CIRInstUniqueKey var_key = info->val_as_inst_key();
            CIRVariableDecl& vd = var_key.cir_package->inst(var_key.defining_inst)->var_decl;
            auto ptr = curr_instance()->var_ptrs[vd.slot];
            ASSERT(!ptr.is_null());

            Value addr = make_value(pointer_type(r.type()));
            addr.pointer_val(ptr);
            Set_ResultValue(pc(), addr);
        } else {
            if(r.state == CIRResultState::WholeValue) {
                Set_ResultTypeAndValue(pc(), r.actual_val());
            }
        }
    } else if(info->is_const_decl_and_func() && has_result_val(info->val_as_inst_key().defining_inst)) {
        Set_ResultValue(pc(), ResultValue(info->val_as_inst_key().defining_inst));
    }

}



void Interpreter::analyze_IdentVal() {

    if(has_result_val(pc())) {
        return;
    }

    SymbolInfoRef info_ref = pkg->inst(pc())->symbol;
    auto info = info_ref();



    if(info == nullptr) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "undefined identifier '{}'", pkg->inst(pc())->ident);
        Set_ResultError(pc());
        return;
    }

    if(info->is_var_decl()) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "IdentVal should only be used for consts, '{}' is a variable", info->name);
        Set_ResultError(pc());
        return;
    }

    // 以支持顶层constDecl的顺序无关声明
    if(info->state == SymbolState::Unsolved) {
        new_pc_flow(info->val_as_inst_key().defining_inst);
        defer(recover_pc_flow());

        analyze_instruction();
    } else if(info->state == SymbolState::Solving) {
        context()->reporter.report_error(SourceLocation({}, pkg->inst(pc())->src_loc.span), "circular dependency detected when evaluating '{}'", info->name);
        Set_ResultError(pc());
        return;
    }

    auto r = info->result(curr_cache_key());
    Set_ResultType(pc(), r.type());
    if(r.state == CIRResultState::WholeValue) {
        Set_ResultTypeAndValue(pc(), r.actual_val());
    }

}

void Interpreter::analyze_Store() {
    auto& store_info = pkg->inst(pc())->store_info;
    CIRInstructionRef var_inst = store_info.var_inst;
    CIRInstructionRef value_inst = store_info.value_inst;

    if(propagate_error({var_inst, value_inst})) return;

    TypeRef target_type;
    if(is_lvalue(var_inst)) {
        target_type = ResultType(var_inst);
    } else {
        auto res = result_for(var_inst);
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
        if(is_function_type(inferred)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc,
                "cannot assign a function value to a variable, use '&' to take address explicitly");
            Set_ResultError(pc());
            return;
        }
        Set_ResultType(var_inst, inferred);
    } else {
        TypeRef value_type = ResultType(store_info.value_inst);
        if(is_function_type(value_type)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc,
                "cannot assign a function value to a variable, use '&' to take address explicitly");
            Set_ResultError(pc());
            return;
        }
        std::optional<TypeRef> implicit_type = result_for(store_info.value_inst).implicit_type;

        if(value_type != target_type && ((!implicit_type.has_value()) || implicit_type.value() != target_type)) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc,
            "try to store value with type '{}', but expected '{}'",
            value_type->t_name(), target_type->t_name());
            Set_ResultError(pc());
            return;
        }
    }

    if(curr_eval_mode() == EvalMode::FullEval && has_instance()) {
        // 泛型运行时调用：comptime 结果是 type value 或无值，实际运行时值由 LLVM 单态化提供
        if(has_result_val(store_info.value_inst)) {
            Value val = ResultValue(store_info.value_inst);
            if(val.type == type_type() && target_type != type_type()) {
                return;  // 跳过 comptime Store，由 LLVM 生成
            }
            Pointer ptr = ResultValue(var_inst).pointer_val();
            ptr.store(ResultValue(value_inst));
        }
        // else: 值在编译期不可用（泛型运行时调用），Store 推迟到 LLVM 运行时
    }

}



void Interpreter::analyze_TypeAscribe() {
    auto& info = pkg->inst(pc())->type_ascribe_info;

    if(propagate_error({info.var_inst, info.type_inst})) {
        Set_ResultError(info.var_inst);
        return;
    }

    // 限制:
    // var_inst == VariableDecl
    // type_inst.result is type_type
    XP_ASSERT_DEFAULT(pkg->inst(info.var_inst)->op == CIROperator::VariableDecl);



    if(!has_result_val(info.type_inst)) {
        // 类型位置的表达式至少有了类型信息，但不是 type_type() →
        // 说明该表达式不产生类型（如 ($T: type) -> T 的泛型运行时调用）
        if(result_for(info.type_inst).state >= CIRResultState::OnlyType) {
            TypeRef t = ResultType(info.type_inst);
            if(t != type_type() && t != undefined_type()) {
                context()->reporter.report_error(pkg->inst(pc())->src_loc,
                    "type annotation must be a type, but got '{}'", t->name());
                Set_ResultError(pc());
                Set_ResultError(info.var_inst);
            }
        }
        return;
    }

    if(!is_type_type(ResultValue(info.type_inst).type)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "type annotation must be a type, but got '{}'", ResultValue(info.type_inst).type->name());
        Set_ResultError(pc());
        Set_ResultError(info.var_inst);
        return;
    }

    TypeRef declared_type = ResultValue(info.type_inst).type_val();

    // 非编译期变量不允许类型为 type
    CIRVariableDecl& vd = pkg->inst(info.var_inst)->var_decl;
    if(!vd.is_comptime && is_type_type(declared_type)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc,
            "non-comptime variable cannot have type 'type'");
        Set_ResultError(pc());
        Set_ResultError(info.var_inst);
        return;
    }

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

    if(curr_eval_mode() == EvalMode::FullEval && has_instance()) {
        auto ptr = curr_instance()->var_ptrs[vd.slot];
        if(ptr.is_null()) {  // 参数已由 caller 分配
            isize size  = type_size_of(declared_type);
            isize align = type_align_of(declared_type);
            ptr = stack_mem.alloc_bytes(size, align);
            curr_instance()->var_ptrs[vd.slot] = ptr;
        }

        Value addr = make_value(pointer_type(declared_type));
        addr.pointer_val(ptr);
        Set_ResultValue(info.var_inst, addr);
    }

}


// 叶子值


void Interpreter::analyze_ConstantValue() {


    // TODO: EXPAND
    // 去除重复求值
    if(has_result_val(pc())) {
        return;
    }

    Value& val = pkg->inst(pc())->imm_val;

    Set_ResultTypeAndValue(pc(), val);
}



// 类型构造
void Interpreter::analyze_PointerType() {
    auto pointed_inst = pkg->inst(pc())->pointer_type_info.pointed_type_inst;

    if(propagate_error({pointed_inst})) return;


    if(ResultType(pointed_inst) != type_type()) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "pointer type requires a type argument, got '{}'", pkg->inst(pointed_inst)->to_string());
        Set_ResultError(pc());
        return;
    }

    Set_ResultType(pc(), type_type());

    if(should_eval_for_lazy_eval({pointed_inst})) {
        TypeRef pointed = ResultValue(pointed_inst).type_val();
        if(pointed == nullptr) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "pointer type requires a concrete type argument");
            Set_ResultError(pc());
            return;
        }
        TypeRef ptr_meta = type_type();
    
        Value result = make_value(ptr_meta);
        result.type_val(pointer_type(pointed));
        Set_ResultValue(pc(), result);
    }


}

void Interpreter::analyze_ArrayType() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->array_type_info;

    if(propagate_error({info.element_type_inst, info.count_inst})) return;

    if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval({info.count_inst})) {
        if(ResultType(info.element_type_inst) != type_type()) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "array type requires a type argument");
            Set_ResultError(pc());
            return;
        }

        TypeRef elem_type = ResultValue(info.element_type_inst).type_val();

        Value count_val = ResultValue(info.count_inst);
        i128 count = count_val.integer_val();

        TypeRef arr_type = array_type(elem_type, count);
        TypeRef meta = type_type();

        Value result = make_value(meta);
        result.type_val(arr_type);
        Set_ResultTypeAndValue(pc(), result);
    }

}

void Interpreter::analyze_SliceType() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->slice_type_info;

    if(propagate_error({info.element_type_inst})) return;

    if(!has_result_val(info.element_type_inst)) {
        Set_ResultType(pc(), type_type());
        return;
    }

    if(ResultType(info.element_type_inst) != type_type()) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "slice type requires a type argument");
        Set_ResultError(pc());
        return;
    }

    TypeRef elem_type = ResultValue(info.element_type_inst).type_val();

    TypeRef slice_type = slice_type_as_struct(elem_type);
    TypeRef meta = type_type();

    Value result = make_value(meta);
    result.type_val(slice_type);
    Set_ResultTypeAndValue(pc(), result);

}

void Interpreter::analyze_FuncType() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->func_type_info;

    if(propagate_error(info.param_type_insts)) return;
    if(propagate_error({info.return_type_inst})) return;

    Array<TypeRef> param_types = make_array<TypeRef>(stage_allocator());
    defer(array_free(&param_types));

    for(isize i = 0; i < info.param_type_insts.count; i++) {
        if(!has_result_val(info.param_type_insts[i])) {
            Set_ResultType(pc(), type_type());
            return;
        }
        if(ResultType(info.param_type_insts[i]) != type_type()) {
            context()->reporter.report_error(pkg->inst(pc())->src_loc, "function type requires a type argument for parameter {}", i);
            Set_ResultError(pc());
            return;
        }
        TypeRef pt = ResultValue(info.param_type_insts[i]).type_val();
        param_types.push_back(pt);
    }

    if(!has_result_val(info.return_type_inst)) {
        Set_ResultType(pc(), type_type());
        return;
    }
    if(ResultType(info.return_type_inst) != type_type()) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "function type requires a type argument for return type");
        Set_ResultError(pc());
        return;
    }
    TypeRef return_type = ResultValue(info.return_type_inst).type_val();

    TypeRef func_type = function_type(param_types, return_type);
    TypeRef meta = type_type();

    Value result = make_value(meta);
    result.type_val(func_type);
    Set_ResultTypeAndValue(pc(), result);
}




void Interpreter::analyze_TypeOfInstResult() {
    if(has_result_val(pc())) {
        return;
    }

    auto target_inst = pkg->inst(pc())->type_of_inst_result_info.target_inst;

    if(propagate_error({target_inst})) return;

    TypeRef target_type = ResultType(target_inst);
    // 如果目标结果本身就是 type value（如泛型 Call 返回的编译期类型），
    // 提取其内部的实际类型，而非把 type_type() 当作变量类型
    if(target_type == type_type() && has_result_val(target_inst)) {
        target_type = ResultValue(target_inst).type_val();
    }
    TypeRef meta = type_type();

    Value result = make_value(meta);
    result.type_val(target_type);
    Set_ResultTypeAndValue(pc(), result);

}

void Interpreter::analyze_FieldTypeOfStruct() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->field_type_of_struct_info;

    if(propagate_error({info.struct_type_inst})) return;

    if(!has_result_val(info.struct_type_inst)) {
        return;
    }
    TypeRef st = ResultValue(info.struct_type_inst).type_val();

    if(!is_struct_type(st)) {
        context()->reporter.report_error(pkg->inst(pc())->src_loc, "can't get field type from a non-struct type");
        Set_ResultError(pc());
        return;
    }

    TypeRef field_type = st->struct_info.struct_fields[info.field_index].type;
    TypeRef meta = type_type();

    Value result = make_value(meta);
    result.type_val(field_type);
    Set_ResultTypeAndValue(pc(), result);
}


void Interpreter::analyze_FuncParamType() {
    if(has_result_val(pc())) {
        return;
    }

    auto& info = pkg->inst(pc())->func_param_type_info;

    if(propagate_error({info.type_of_func_type_inst})) return;

    TypeRef func_type = ResultValue(info.type_of_func_type_inst).type_val();

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


    TypeRef meta = type_type();
    Value result = make_value(meta);
    result.type_val(param_type);
    Set_ResultTypeAndValue(pc(), result);
}


void Interpreter::analyze_EnterScope() {
    Scope *scope = pkg->inst(pc())->scope_info.scope;
    set_scope(scope);

}

void Interpreter::analyze_ExitScope() {
    Scope *scope = pkg->inst(pc())->scope_info.scope;
    set_scope(scope);

}

void Interpreter::analyze_Deref() {
    // stub — handler_Deref 在 dispatch 中优先处理
}

void Interpreter::analyze_DetermineType() {
    auto& info = pkg->inst(pc())->determine_type_info;
    auto determined_inst = info.determining_inst;
    auto expected_type_inst = info.type_inst;

    if(propagate_error({determined_inst})) return;
    if(expected_type_inst != INVALID_INST && propagate_error({expected_type_inst})) return;

    // TODO: HACK
    if(!has_result_type(determined_inst)) {
        return;
    }

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


    // 数组元素类型包含 untyped 时递归解析
    if(is_array_type(expected_type) && is_untyped_type(expected_type->array_info.element_type)) {
        TypeRef elem_t = expected_type->array_info.element_type;
        if(has_val) {
            Array<Value> elems = result_val.array_element_values();
            if(elems.count > 0) {
                elem_t = get_compliable_const_type(elems[0]);
                for(isize i = 0; i < elems.count; i++) {
                    elems[i].type = elem_t;
                }
                result_val.array_element_values(elems);
                Set_ResultValue(determined_inst, result_val);
            }
        } else {
            elem_t = default_certain_type_for_untyped_type(elem_t);
        }
        expected_type = array_type(elem_t, expected_type->array_info.count);

        // 同步更新所有元素指令的类型，防止 LLVM 生成器遇到 untyped
        auto& elems_info = pkg->inst(determined_inst)->array_init_info;
        for(isize i = 0; i < elems_info.element_insts.count; i++) {
            auto ei = elems_info.element_insts[i];
            if(is_untyped_type(ResultType(ei))) {
                Set_ResultType(ei, elem_t);
            }
        }
    }

    // TODO: 统一路径
    Set_ResultType(determined_inst, expected_type);

    if(expected_type_inst != INVALID_INST) {
        // 编译期类型的字段值本身是 type（如 enum { Variant :: TypeExpr }），跳过标签类型兼容检查
        if(determined_type == type_type()) {
            return;
        }

        if(!has_result_val(info.type_inst)) {
            return;
        }
        // @EXPLAIN: 如果有参数, 表示有目标类型
        expected_type = ResultValue(info.type_inst).type_val();

        XP_ASSERT_DEFAULT(!is_untyped_type(expected_type)); // 目标类型不应该是 untyped

        // var_arg: 不约束类型，任何实参都接受
        if(expected_type == easy_type(Type_var_arg_c)) {
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
                context()->reporter.report_error(pkg->inst(pc())->src_loc,
                    "implicit conversion from function type to function pointer is not allowed, use '&' to take address explicitly");
                Set_ResultError(pc());
                return;
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
            result_for(determined_inst).implicit_type = expected_type;
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



}




//
// eval part
//

Value Interpreter::eval_GetOrInitStruct(CIRInstructionRef ref) {
    ASSERT(pkg->inst(ref)->op == CIROperator::GetOrInitStruct);

    auto& info = pkg->inst(ref)->get_or_init_struct_info;

    std::optional<xpString> struct_name = std::nullopt;
    SymbolInfo *struct_sym = info.symbol();
    if(struct_sym != nullptr) {
        struct_name = struct_sym->name;
    }

    TypeRef st = nullptr;
    if(struct_name.has_value()) {
        st = unfinished_struct_type(info.decl_ast, struct_name.value());
    } else {
        st = unfinished_anonymous_struct_type(info.decl_ast);
    }

    TypeRef tt = type_type();
    Value v = make_value(tt);
    v.type_val(st);


    // 壳子创建后立即绑定符号，使字段内的 *Self 能查到
    {
        SymbolInfo *sym = info.symbol();
        if(sym != nullptr) {
            sym->val(v);
            sym->state = SymbolState::Solved;
        }
    }
    if(info.self_sym != nullptr) {
        info.self_sym->val(v);
        info.self_sym->state = SymbolState::Solved;
    }

    return v;
}

//
// utils
//

// Value Interpreter::get_result_val_or_eval(CIRInstructionRef ref) {
//     auto& res = pkg->Inst(ref)->result;

//     if(res.state == CIRResultType::
// }


//
// 错误传递
//
bool Interpreter::propagate_error(std::initializer_list<CIRInstructionRef> refs) {
    for(auto ref : refs) {
        if(has_error(ref)) {
            DEBUG_TRACE("propagate_error: ref: {}, op: {}, src_loc: {}", ref, pkg->inst(ref)->to_string(), pkg->inst(ref)->src_loc);
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


//
// Result 处理
// 
CIRInstResult& Interpreter::result_for(CIRInstructionRef ref) {
    if(curr_instance() && curr_instance()->result_instance != nullptr) {
        auto& results = curr_instance()->result_instance->results;
        CIRInstResult *existing = xp_hash_map_get(results, ref);
        if(!existing) {
            existing = xp_hash_map_insert(&results, ref, pkg->result_of(ref));
        }
        return *existing;
    }
    return pkg->result_of(ref);
}


bool Interpreter::has_result_val(CIRInstructionRef ref) {
    return result_for(ref).state == CIRResultState::WholeValue;
}

bool Interpreter::has_result_val(std::initializer_list<CIRInstructionRef> refs) {
    for(auto ref: refs) {
        if(!has_result_val(ref)) {
            return false;
        }
    }
    return true;
}

bool Interpreter::has_result_val(Array<CIRInstructionRef>& refs) {
    for(auto ref: refs) {
        if(!has_result_val(ref)) {
            return false;
        }
    }
    return true;
}

bool Interpreter::has_result_type(CIRInstructionRef ref) {
    auto state = result_for(ref).state;
    return state == CIRResultState::OnlyType || state == CIRResultState::WholeValue;
}

bool Interpreter::has_error(CIRInstructionRef ref) {
    return result_for(ref).state == CIRResultState::Error;
}

bool Interpreter::is_generic_func(CIRPackage *fpkg, CIRFunction& func) {
    if (func.is_comptime) return true;
    for (isize i = 0; i < func.arg_decl_insts.count; i++) {
        auto decl_inst = func.arg_decl_insts[i];
        if (decl_inst == INVALID_INST) continue;
        if (fpkg->inst(decl_inst)->var_decl.is_comptime) return true;
    }
    return false;
}

void Interpreter::Set_ResultError(CIRInstructionRef ref) {
    DEBUG_TRACE("Set_ResultError: ref: {}, op: {}, src_loc: {}", ref, pkg->inst(ref)->to_string(), pkg->inst(ref)->src_loc);

    auto& res = result_for(ref);
    if(res.state == CIRResultState::Error) {
        return;
    }

    res.state = CIRResultState::Error;

    auto tgt = targets_of(pkg->inst(ref), stage_allocator());
    for(isize i = 0; i < tgt.count; i++) {
        if(tgt[i] != INVALID_INST) Set_ResultError(tgt[i]);
    }
}



CIRResultState Interpreter::result_state(CIRInstructionRef ref) {
    return result_for(ref).state;
}

void Interpreter::set_result_state(CIRInstructionRef ref, CIRResultState state) {
    result_for(ref).state = state;
}


TypeRef Interpreter::ResultType(CIRInstructionRef ref) {
    auto& res = result_for(ref);
    XP_ASSERT_DEFAULT(res.state == CIRResultState::OnlyType || res.state == CIRResultState::WholeValue);
    return res.type();
}

void Interpreter::Set_ResultType(CIRInstructionRef ref, TypeRef type) {
    ASSERT_MSG(type != nullptr, "cannot set result type to null");
    result_for(ref).set_type(type);
}

Value Interpreter::ResultValue(CIRInstructionRef ref) {
    auto& res = result_for(ref);

    if(res.state != CIRResultState::WholeValue) {
        DEBUG_PANIC("trying to get value of instruction that doesn't have a value yet: ref: {}, op: {}, state: {}, src_loc: {}, curr_pc: {}",
            ref, pkg->inst(ref)->to_string(), (int)res.state, pkg->inst(ref)->src_loc, pc());
    }

    return res.actual_val();
}


void Interpreter::Set_ResultValue(CIRInstructionRef ref, Value val) {
    DEBUG_TRACE("Set_ResultValue: ref: {}, op: {}", ref, pkg->inst(ref)->to_string());
    result_for(ref).set_val(val);
}


void Interpreter::Set_ResultTypeAndValue(CIRInstructionRef ref, Value val) {
    Set_ResultType(ref, val.type);
    Set_ResultValue(ref, val);
}



//
// Scope 处理
//


void Interpreter::set_scope(Scope *sc) {
    scope() = sc;
}



//
// pc 流程控制
//


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



//
// EvalMode 处理
//

EvalMode Interpreter::curr_eval_mode() const {
    if(eval_mode_stack.count != 0) {
        return eval_mode_stack.back();
    }

    if(instance_stack.count != 0) {
        return EvalMode::FullEval;
    }

    return EvalMode::TypeOnly;
}



//
// LValue 处理
//

bool Interpreter::is_lvalue(CIRInstructionRef ref) {
    return result_for(ref).value_kind == CIRValueKind::LValue;
}

void Interpreter::set_lvalue(CIRInstructionRef ref) {
    result_for(ref).value_kind = CIRValueKind::LValue;
}


bool Interpreter::should_eval(std::initializer_list<CIRInstructionRef> refs) {

    if(curr_eval_mode() == EvalMode::FullEval) {
        return true;
    }


    for(auto ref: refs) {
        if(!has_result_val(ref)) {
            return false;
        }
    }

    return true;
}

bool Interpreter::should_eval_for_lazy_eval(std::initializer_list<CIRInstructionRef> refs) {
    for(auto& ref: refs) {
        if(!has_result_val(ref)) {
            return false;
        }
    }
    return true;
}

bool Interpreter::should_eval_for_lazy_eval(Array<CIRInstructionRef>& refs) {
    for(auto& ref: refs) {
        if(!has_result_val(ref)) {
            return false;
        }
    }
    return true;
}






//
//
//


EvalInstance EvalInstance::make(CIRPackage *callee_pkg, CIRInstructionRef func_decl_pc, isize var_count, xpAllocator allocator) {
    EvalInstance inst{};
    inst.callee_pkg = callee_pkg;
    inst.func_decl_pc = func_decl_pc;
    
    inst.var_ptrs = make_array_count<Pointer>(allocator, var_count);
    for(isize i = 0; i < var_count; i++) {
        inst.var_ptrs[i] = Pointer::make_null();
    }

    return inst;
}

void EvalInstance::free(EvalInstance *inst) {
    array_free(&inst->var_ptrs);
}

// 数据流声明
static Array<CIRInstructionRef> deps_of(CIRInstruction* inst, xpAllocator alloc) {
    switch(inst->op) {
        case CIROperator::ConstantValue:
        case CIROperator::IdentVal:
        case CIROperator::IdentRef:
        case CIROperator::GetOrInitStruct:
        case CIROperator::EnterScope:
        case CIROperator::ExitScope:
        case CIROperator::Block:
        case CIROperator::Loop:
        case CIROperator::Break:
        case CIROperator::ConstDecl:
        case CIROperator::FunctionDecl:
        case CIROperator::VariableDecl:
            return make_array<CIRInstructionRef>(alloc);

        case CIROperator::Binary:           { auto a = make_array_count<CIRInstructionRef>(alloc, 2); a[0]=inst->binary_info.left_inst; a[1]=inst->binary_info.right_inst; return a; }
        case CIROperator::Unary:            { auto a = make_array_count<CIRInstructionRef>(alloc, 1); a[0]=inst->unary_info.operand_inst; return a; }
        case CIROperator::Cast:             { auto a = make_array_count<CIRInstructionRef>(alloc, 2); a[0]=inst->cast_info.expr_inst; a[1]=inst->cast_info.target_type_inst; return a; }
        case CIROperator::FieldAccess:
        case CIROperator::FieldPtr:         { auto a = make_array_count<CIRInstructionRef>(alloc, 1); a[0]=inst->field_access_info.parent_inst; return a; }
        case CIROperator::Index:
        case CIROperator::IndexPtr:         { auto a = make_array_count<CIRInstructionRef>(alloc, 2); a[0]=inst->index_info.array_inst; a[1]=inst->index_info.index_inst; return a; }
        case CIROperator::AddrOf:           { auto a = make_array_count<CIRInstructionRef>(alloc, 1); a[0]=inst->addr_of_info.lval_inst; return a; }
        case CIROperator::Load:             { auto a = make_array_count<CIRInstructionRef>(alloc, 1); a[0]=inst->load_info.ptr_inst; return a; }
        case CIROperator::Deref:            { auto a = make_array_count<CIRInstructionRef>(alloc, 1); a[0]=inst->deref_info.operand_inst; return a; }
        case CIROperator::Store:            { auto a = make_array_count<CIRInstructionRef>(alloc, 2); a[0]=inst->store_info.var_inst; a[1]=inst->store_info.value_inst; return a; }
        case CIROperator::PointerType:      { auto a = make_array_count<CIRInstructionRef>(alloc, 1); a[0]=inst->pointer_type_info.pointed_type_inst; return a; }
        case CIROperator::ArrayType:        { auto a = make_array_count<CIRInstructionRef>(alloc, 2); a[0]=inst->array_type_info.element_type_inst; a[1]=inst->array_type_info.count_inst; return a; }
        case CIROperator::SliceType:        { auto a = make_array_count<CIRInstructionRef>(alloc, 1); a[0]=inst->slice_type_info.element_type_inst; return a; }
        case CIROperator::FieldTypeOfStruct:{ auto a = make_array_count<CIRInstructionRef>(alloc, 1); a[0]=inst->field_type_of_struct_info.struct_type_inst; return a; }
        case CIROperator::FuncParamType:    { auto a = make_array_count<CIRInstructionRef>(alloc, 1); a[0]=inst->func_param_type_info.type_of_func_type_inst; return a; }
        case CIROperator::TypeOfInstResult: { auto a = make_array_count<CIRInstructionRef>(alloc, 1); a[0]=inst->type_of_inst_result_info.target_inst; return a; }
        case CIROperator::TypeAscribe:      { auto a = make_array_count<CIRInstructionRef>(alloc, 2); a[0]=inst->type_ascribe_info.var_inst; a[1]=inst->type_ascribe_info.type_inst; return a; }
        case CIROperator::DetermineType:    { auto a = make_array_count<CIRInstructionRef>(alloc, 2); a[0]=inst->determine_type_info.determining_inst; a[1]=inst->determine_type_info.type_inst; return a; }
        case CIROperator::CondBr:           { auto a = make_array_count<CIRInstructionRef>(alloc, 1); a[0]=inst->condbr_info.condition_inst; return a; }
        case CIROperator::Call:             {
            auto a = make_array_count<CIRInstructionRef>(alloc, 1 + inst->call_info.arg_insts.count);
            a[0] = inst->call_info.called_thing;
            for(isize i = 0; i < inst->call_info.arg_insts.count; i++) a[1 + i] = inst->call_info.arg_insts[i];
            return a;
        }
        case CIROperator::StructField:      { auto a = make_array_count<CIRInstructionRef>(alloc, 1); a[0]=inst->struct_field_info.type_block_inst; return a; }
        case CIROperator::StructInit:       {
            auto a = make_array_count<CIRInstructionRef>(alloc, 1 + inst->struct_init_info.field_init_insts.count);
            a[0] = inst->struct_init_info.struct_type_inst;
            for(isize i = 0; i < inst->struct_init_info.field_init_insts.count; i++) a[1 + i] = inst->struct_init_info.field_init_insts[i];
            return a;
        }
        case CIROperator::ArrayInit:        return inst->array_init_info.element_insts.copy(alloc);
        case CIROperator::FinishStruct:     {
            auto a = make_array_count<CIRInstructionRef>(alloc, 1 + inst->finish_struct_info.field_insts.count);
            a[0] = inst->finish_struct_info.struct_decl_inst;
            for(isize i = 0; i < inst->finish_struct_info.field_insts.count; i++) a[1 + i] = inst->finish_struct_info.field_insts[i];
            return a;
        }
        case CIROperator::EnumDeclInit:     {
            auto a = make_array_count<CIRInstructionRef>(alloc, 1 + inst->enum_decl_init_info.fields.count);
            a[0] = inst->enum_decl_init_info.tag_type_inst;
            for(isize i = 0; i < inst->enum_decl_init_info.fields.count; i++) a[1 + i] = inst->enum_decl_init_info.fields[i].value_inst;
            return a;
        }
        case CIROperator::FuncType:         {
            auto a = make_array_count<CIRInstructionRef>(alloc, 1 + inst->func_type_info.param_type_insts.count);
            a[0] = inst->func_type_info.return_type_inst;
            for(isize i = 0; i < inst->func_type_info.param_type_insts.count; i++) a[1 + i] = inst->func_type_info.param_type_insts[i];
            return a;
        }
        default:                            return make_array<CIRInstructionRef>(alloc);
    }
}

static Array<CIRInstructionRef> targets_of(CIRInstruction* inst, xpAllocator alloc) {
    switch(inst->op) {
        case CIROperator::TypeAscribe:      { auto a = make_array_count<CIRInstructionRef>(alloc, 1); a[0]=inst->type_ascribe_info.var_inst; return a; }
        case CIROperator::DetermineType:    { auto a = make_array_count<CIRInstructionRef>(alloc, 1); a[0]=inst->determine_type_info.determining_inst; return a; }
        case CIROperator::ArrayInit:        return inst->array_init_info.element_insts.copy(alloc);
        case CIROperator::Break:            { auto a = make_array_count<CIRInstructionRef>(alloc, 1); a[0]=inst->break_info.break_block; return a; }
        default:                            return make_array<CIRInstructionRef>(alloc);
    }
}


// apply_result — 把 CIRInstResult 写入目标指令的结果
void Interpreter::apply_result(CIRInstructionRef ref, const CIRInstResult& result) {
    if(result.state == CIRResultState::Error) {
        Set_ResultError(ref);  // 自动标记 targets
        return;
    }
    auto& res = result_for(ref);
    if(result.state == CIRResultState::OnlyType) {
        ASSERT_MSG(result.type() != nullptr, "cannot set result type to null");
        res.set_type(result.type());
        res.value_kind = result.value_kind;
        if(result.implicit_type.has_value()) {
            res.implicit_type = result.implicit_type;
        }
        return;
    }
    if(result.state == CIRResultState::WholeValue) {
        ASSERT_MSG(result.type() != nullptr, "cannot set result type to null");
        

        // ASSERT_MSG(res.state != CIRResultState::WholeValue, "cannot overwrite existing result value");

        res.set_type(result.type());
        res.set_val(result.actual_val());
        res.value_kind = result.value_kind;
        if(result.implicit_type.has_value()) {
            res.implicit_type = result.implicit_type;
        }
        return;
    }
}


// 报错辅助：report_error + 返回 make_error()
template<typename... Args>
CIRInstResult inst_error(CIRInstruction* inst, std::format_string<Args...> fmt, Args&&... args) {
    context()->reporter.report_error(inst->src_loc, fmt, std::forward<Args>(args)...);
    return CIRInstResult::make_error();
}


// handler: ConstantValue
std::optional<AnalyzeResult> Interpreter::handler_ConstantValue(CIRInstruction* inst, CIRInstructionRef pc_ref) {
    Value val = inst->imm_val;
    return AnalyzeResult{CIRInstResult::make_value(val.type, val), pc_ref};
}

// handler: PointerType
std::optional<AnalyzeResult> Interpreter::handler_PointerType(CIRInstruction* inst, CIRInstructionRef pc_ref) {
    CIRInstructionRef pointed_inst = inst->pointer_type_info.pointed_type_inst;

    if(!is_type_type(ResultType(pointed_inst))) {
        return AnalyzeResult{inst_error(inst, "pointer type requires a type argument, got '{}'", pkg->inst(pointed_inst)->to_string()), pc_ref};

    }

    if(should_eval_for_lazy_eval({pointed_inst})) {
        TypeRef pointed = ResultValue(pointed_inst).type_val();
        if(pointed == nullptr) {
            return AnalyzeResult{inst_error(inst, "pointer type requires a concrete type argument"), pc_ref};
        }

        Value result = make_value(type_type());
        result.type_val(pointer_type(pointed));
        return AnalyzeResult{CIRInstResult::make_value(result), pc_ref};
    }

    return AnalyzeResult{CIRInstResult::make_type_only(type_type()), pc_ref};
}


// handler: Load
std::optional<AnalyzeResult> Interpreter::handler_Load(CIRInstruction* inst, CIRInstructionRef pc_ref) {
    CIRInstructionRef ptr_inst = inst->load_info.ptr_inst;

    if(is_lvalue(ptr_inst)) {
        TypeRef loaded_type = ResultType(ptr_inst);

        if(has_instance() && (curr_eval_mode() == EvalMode::FullEval || has_result_val(ptr_inst))) {
            Pointer ptr = ResultValue(ptr_inst).pointer_val();
            Value val = ptr.load(loaded_type, stage_allocator());
            return AnalyzeResult{CIRInstResult::make_value(loaded_type, val), pc_ref};
        }
        return AnalyzeResult{CIRInstResult::make_type_only(loaded_type), pc_ref};
    }

    TypeRef ptr_type = ResultType(ptr_inst);
    if(!is_pointer_type(ptr_type))
        return AnalyzeResult{inst_error(inst, "cannot load from a non-pointer value"), pc_ref};

    if(has_instance() && (curr_eval_mode() == EvalMode::FullEval || has_result_val(ptr_inst))) {
        Pointer ptr = ResultValue(ptr_inst).pointer_val();
        Value val = ptr.load(ptr_type->pointed_type, stage_allocator());
        return AnalyzeResult{CIRInstResult::make_value(ptr_type->pointed_type, val), pc_ref};
    }
    return AnalyzeResult{CIRInstResult::make_type_only(ptr_type->pointed_type), pc_ref};
}


std::optional<AnalyzeResult> Interpreter::handler_Deref(CIRInstruction* inst, CIRInstructionRef pc_ref) {
    CIRInstructionRef ptr_inst = inst->deref_info.operand_inst;

    TypeRef ptr_type = ResultType(ptr_inst);
    if(!is_pointer_type(ptr_type)) {
        return AnalyzeResult{inst_error(inst, "cannot dereference non-pointer type"), pc_ref};
    }

    // NOTE: 隐式约定, 既然已经保证了是指针类型, 那么就可以保证 pointed_type 不为 null
    // 且actual_type 一定是 *ptr_type, 所以也可以保证 actual_pointed 不为 null
    TypeRef pointed = ptr_type->pointed_type;
    TypeRef actual_pointed = result_for(ptr_inst).actual_type()->pointed_type;

    auto res = CIRInstResult::make_type_only(pointed);
    res.set_actual_type(actual_pointed);
    res.value_kind = result_for(ptr_inst).value_kind; // 继承属性

    if(curr_eval_mode() == EvalMode::FullEval && has_instance()) {
        Pointer ptr = ResultValue(ptr_inst).pointer_val();
        Value val = ptr.load(actual_pointed, stage_allocator());
        res.set_val(val);
    }


    return AnalyzeResult{res, pc_ref};
}
