#include "cir_interpreter.hpp"
#include "compile.hpp"

#include "context.hpp"

#include "type.hpp"

#include "package.hpp"
#include "scope.hpp"
#include "symbol.hpp"

#include "value_ops.hpp"
#include "type_check.hpp"

#include "analyser.hpp"
#include "cir_builder.hpp"
#include "path.hpp"

#include <filesystem>

#include "print.hpp"


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
    inst_stack = make_array<CIRInstructionRef>(allocator);
    eval_mode_stack = make_array<EvalMode>(allocator);
    loop_stack = make_array<CIRInstructionRef>(allocator);
    instance_stack = make_array<EvalInstance>(allocator);

    stack_mem.init(MemoryKind::Stack, allocator);
}

Interpreter::~Interpreter() {
    array_free(&inst_stack);
    array_free(&eval_mode_stack);
    array_free(&instance_stack);

    stack_mem.free();
}

CIRInstructionRef& Interpreter::curr_inst_ref() {
    return inst_stack.back();
}


//
// 分析入口
//
void analyze_package(Ref<Package> pkg) {
    DEBUG_TRACE("start analyzing package {}", pkg.unwrap().path);

    Interpreter interpreter(permanent_allocator());
    interpreter.analyze_cir_package(&pkg.unwrap().cir_package);
}

//
// interp 入口
//
void Interpreter::analyze_cir_package(CIRPackage* cir_package) {
    pkg = cir_package;

    auto root = EvalInstance{};
    root.ctx = CIRResultContext::create(cir_package);
    instance_stack.push_back(root);

    inst_stack.clear();
    inst_stack.push_back(CIRInstructionRef{pkg->top_blk, 0, pkg->package_ref.index});
    scope = pkg->package_scope;

    analyze_block(pkg->top_blk, std::nullopt);
}



static Array<CIRInstructionRef> deps_of(CIRInstruction* inst, xpAllocator alloc);
static Array<CIRInstructionRef> targets_of(CIRInstruction* inst, xpAllocator alloc);


void Interpreter::analyze_instruction(std::optional<CIROperator> expected_op, AnalyzeParams params) {

    CIRInstruction *inst = pkg->inst(curr_inst_ref());

    DEBUG_TRACE("curr curr_inst(): {}, inst: {}, src_loc: {}, evalmode: {}", curr_inst_ref(), inst->to_string(), inst->src_loc, (int)curr_eval_mode());

    if(expected_op.has_value()) {
        if(inst->op != expected_op.value()) {
            DEBUG_PANIC("Expected instruction {} at curr_inst() {}, but got {}", string(expected_op.value()), curr_inst_ref(), string(inst->op));
        }
    }

    auto dep_arr = deps_of(inst, stage_allocator());

    std::optional<AnalyzeResult> result_opt = std::nullopt;

    
    // 自动 deps 错误传播
    for(const auto& dependent_inst_ref: dep_arr) {
        if(dependent_inst_ref == INVALID_INST) {
            continue;
        }

        if(has_error(dependent_inst_ref)) {
            Set_ResultError(curr_inst_ref());

            auto tgt = targets_of(inst, permanent_allocator());
            for(auto& target_inst: tgt) {
                if(target_inst != INVALID_INST) {
                    Set_ResultError(target_inst);
                }
            }

            goto end;
        }
    }

#define X(name) if(inst->op == CIROperator::name) { result_opt = analyze_##name(inst->info<CIROperator::name>(), curr_inst_ref(), params); goto end; }
    CIR_OPERATORS
#undef X

end:
    if(result_opt.has_value()) {
        for(auto& w : result_opt->writes) {
            apply_result(w.ref, w.result);
        }
        if(result_opt->next_pc.has_value()) {
            curr_inst_ref() = *result_opt->next_pc;
            return;
        }
    }
    curr_inst_ref().advance();
}

void Interpreter::analyze_instruction_at(CIRInstructionRef at_ref) {
    new_analyze_flow(at_ref);
    analyze_instruction();
    recover_analyze_flow();
}


void Interpreter::analyze_block(CIRBlockRef blk, std::optional<CIRInstructionRef> target, std::optional<EvalMode> force_eval_mode) {
    auto& block_info = *pkg->block(blk);

    new_analyze_flow(CIRInstructionRef{blk, 0, pkg->package_ref.index});
    bool pushed_eval_mode = false;
    if(force_eval_mode.has_value()) {
        eval_mode_stack.push_back(*force_eval_mode);
        pushed_eval_mode = true;
    } else if(block_info.immediate_eval) {
        eval_mode_stack.push_back(EvalMode::FullEval);
        pushed_eval_mode = true;
    }

    analyze_block_insts(blk, target);

    if(pushed_eval_mode) {
        eval_mode_stack.pop_back();
    }
    recover_analyze_flow();
}

void Interpreter::analyze_loop(CIRBlockRef blk, std::optional<CIRInstructionRef> target) {
    auto& block_info = *pkg->block(blk);

    new_analyze_flow(CIRInstructionRef{blk, 0, pkg->package_ref.index});

    
    // TODO: 实现编译期循环
    if(curr_eval_mode() == EvalMode::FullEval) {
        context()->reporter.report_error(pkg->inst(curr_inst_ref())->src_loc, "compile-time loop evaluation is not yet implemented");
    } else {
        analyze_block_insts(blk, target);
    }


    recover_analyze_flow();
}

void Interpreter::analyze_block_insts(CIRBlockRef blk, std::optional<CIRInstructionRef> target) {
    auto& block_info = *pkg->block(blk);

    for(;;) {

        // @note: 目前假设执行一个block时, 执行完一条指令, package不会被修改, 即使是Call了不同package的function, 执行完了也会回到当前package
        bool is_in_same_block = curr_inst_ref().block_ref == blk;
        bool is_in_bounds = curr_inst_ref().inst_index < block_info.insts.count();
        bool no_target_result = !(target.has_value() && has_result_val(target.value()));

        if(is_in_same_block && is_in_bounds && no_target_result) {
            analyze_instruction();
        } else {
            break;
        }
    }
}

Value Interpreter::eval_GetOrInitStruct(CIRInstructionRef ref) {
    ASSERT(pkg->inst(ref)->op == CIROperator::GetOrInitStruct);

    auto& info = pkg->inst(ref)->info<CIROperator::GetOrInitStruct>();

    TypeRef st = unfinished_anonymous_struct_type(info.decl_ast);

    TypeRef tt = type_type();
    Value v = make_value(tt);
    v.type_val(st);


    return v;
}

//
// utils
//


//
// 错误传递
//
bool Interpreter::propagate_error(std::initializer_list<CIRInstructionRef> refs) {
    for(auto ref : refs) {
        if(has_error(ref)) {
            DEBUG_TRACE("propagate_error: ref: {}", ref);
            Set_ResultError(curr_inst_ref());
            return true;
        }
    }
    return false;
}

bool Interpreter::propagate_error(Array<CIRInstructionRef>& refs) {
    for (isize i = 0; i < refs.count; i++)
        if (has_error(refs[i])) {
            Set_ResultError(curr_inst_ref());
            return true;
        }
    return false;
}


void Interpreter::push_eval_instance(EvalInstance inst) {
    inst.caller_pkg = pkg;
    inst.caller_pc = curr_inst_ref();
    inst.caller_scope = scope;
    instance_stack.push_back(std::move(inst));
}

void Interpreter::pop_eval_instance() {
    ASSERT_MSG(instance_stack.count > 1, "cannot pop root instance");
    auto& inst = instance_stack.back();
    stack_mem.bytes.count = inst.frame_base;
    scope = inst.caller_scope;
    pkg = inst.caller_pkg;
    curr_inst_ref() = inst.caller_pc;
    EvalInstance::free(&inst);
    instance_stack.pop_back();
}

bool Interpreter::has_result_val(CIRInstructionRef ref) {
    return result_context().result_of(ref).state == CIRResultState::WholeValue;
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
    auto state = result_context().result_of(ref).state;
    return state == CIRResultState::OnlyType || state == CIRResultState::WholeValue;
}

bool Interpreter::has_error(CIRInstructionRef ref) {
    return result_context().result_of(ref).state == CIRResultState::Error;
}


void Interpreter::Set_ResultError(CIRInstructionRef ref) {
    DEBUG_TRACE("Set_ResultError: ref: {}", ref);

    auto& res = result_context().result_of(ref);
    if(res.state == CIRResultState::Error) {
        return;
    }

    res.state = CIRResultState::Error;

    // ref 始终是真实指令（handle 也是真实 BlockRef 指令），targets 为空则自然不传播
    auto tgt = targets_of(pkg->inst(ref), permanent_allocator());
    for(isize i = 0; i < tgt.count; i++) {
        if(tgt[i] != INVALID_INST) Set_ResultError(tgt[i]);
    }
}



CIRResultState Interpreter::result_state(CIRInstructionRef ref) {
    return result_context().result_of(ref).state;
}

void Interpreter::set_result_state(CIRInstructionRef ref, CIRResultState state) {
    result_context().result_of(ref).state = state;
}


TypeRef Interpreter::ResultType(CIRInstructionRef ref) {
    auto& res = result_context().result_of(ref);
    XP_ASSERT_DEFAULT(res.state == CIRResultState::OnlyType || res.state == CIRResultState::WholeValue);
    return res.type();
}

void Interpreter::Set_ResultType(CIRInstructionRef ref, TypeRef type) {
    ASSERT_MSG(type != nullptr, "cannot set result type to null");
    result_context().result_of(ref).set_type(type);
}

Value Interpreter::ResultValue(CIRInstructionRef ref) {
    auto& res = result_context().result_of(ref);

    if(res.state != CIRResultState::WholeValue) {
        DEBUG_PANIC("trying to get value of instruction that doesn't have a value yet: ref: {}, state: {}, curr_pc: {}",
            ref, (int)res.state, curr_inst_ref());
    }

    return res.actual_val();
}


void Interpreter::Set_ResultValue(CIRInstructionRef ref, Value val) {
    DEBUG_TRACE("Set_ResultValue: ref: {}", ref);
    result_context().result_of(ref).set_val(val);
}


void Interpreter::Set_ResultTypeAndValue(CIRInstructionRef ref, Value val) {
    Set_ResultType(ref, val.type);
    Set_ResultValue(ref, val);
}



//
// Scope 处理
//


void Interpreter::set_scope(Ref<Scope> sc) {
    scope = sc;
}



//
// curr_inst 流程控制
//


void Interpreter::new_analyze_flow(CIRInstructionRef new_pc) {
    inst_stack.push_back(new_pc);
}

void Interpreter::recover_analyze_flow() {
    inst_stack.pop_back();
}



//
// EvalMode 处理
//

EvalMode Interpreter::curr_eval_mode() const {
    if(eval_mode_stack.count > 0) {
        return eval_mode_stack.back();
    }

    // 空栈 = 顶层/运行时函数体上下文，默认类型推导
    return EvalMode::TypeOnly;
}



//
// LValue 处理
//

bool Interpreter::is_lvalue(CIRInstructionRef ref) {
    return result_context().result_of(ref).value_kind == CIRValueKind::LValue;
}

void Interpreter::set_lvalue(CIRInstructionRef ref) {
    result_context().result_of(ref).value_kind = CIRValueKind::LValue;
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


EvalInstance EvalInstance::make(CIRPackage *callee_pkg, isize var_count, xpAllocator allocator) {
    EvalInstance inst{};
    inst.ctx = CIRResultContext::create(callee_pkg);
    inst.var_ptrs = make_array_count<Pointer>(allocator, var_count);
    for(isize i = 0; i < var_count; i++) {
        inst.var_ptrs[i] = Pointer::make_null();
    }

    return inst;
}

void EvalInstance::free(EvalInstance *inst) {
    array_free(&inst->var_ptrs);
}

// 数据流声明：委托给各 payload 的 refs()/targets()（字段枚举归结构体自己）

// 模板里 if constexpr 才真正丢弃分支：有 refs() 用，没有返回空
template<typename T>
static Array<CIRInstructionRef> deps_of_info(T& payload, xpAllocator alloc) {
    if constexpr (HasRefs<T>) {
        return payload.refs(alloc);
    } else {
        return make_array<CIRInstructionRef>(alloc);
    }
}

static Array<CIRInstructionRef> deps_of(CIRInstruction* inst, xpAllocator alloc) {
    switch(inst->op) {
#define X(name) case CIROperator::name: return deps_of_info(inst->info<CIROperator::name>(), alloc);
        CIR_OPERATORS
#undef X
        default: return make_array<CIRInstructionRef>(alloc);
    }
}

// 模板里 if constexpr 才真正丢弃分支：有 targets() 用，没有返回空
template<typename T>
static Array<CIRInstructionRef> targets_of_info(T& payload, xpAllocator alloc) {
    if constexpr (HasTargets<T>) {
        return payload.targets(alloc);
    } else {
        return make_array<CIRInstructionRef>(alloc);
    }
}

static Array<CIRInstructionRef> targets_of(CIRInstruction* inst, xpAllocator alloc) {
    switch(inst->op) {
#define X(name) case CIROperator::name: return targets_of_info(inst->info<CIROperator::name>(), alloc);
        CIR_OPERATORS
#undef X
        default: return make_array<CIRInstructionRef>(alloc);
    }
}


// apply_result — 把 CIRInstResult 写入目标指令的结果
void Interpreter::apply_result(CIRInstructionRef ref, const CIRInstResult& result) {
    if(result.state == CIRResultState::Error) {
        Set_ResultError(ref);  // 自动标记 targets
        return;
    }
    auto& res = result_context().result_of(ref);
    if(result.state == CIRResultState::OnlyType) {
        ASSERT_MSG(result.type() != nullptr, "cannot set result type to null");
        res.set_type(result.type());
        res.set_actual_type(result.actual_type());   // 传播实际类型（IdentRef/Deref 仅 type 时仍需 *logical 供 codegen）
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
CIRInstResult inst_error(CIRInstructionRef pc_ref, std::format_string<Args...> fmt, Args&&... args) {
    context()->reporter.report_error(inst(pc_ref)->src_loc, fmt, std::forward<Args>(args)...);
    return CIRInstResult::make_error();
}

AnalyzeResult::AnalyzeResult() : writes(make_array<ResultWrite>(permanent_allocator())) {}

AnalyzeResult::AnalyzeResult(CIRInstResult result, CIRInstructionRef ref) {
    writes = make_array<ResultWrite>(permanent_allocator());
    writes.push_back({ref, result});
}

// 便捷：构造单个结果写入的 AnalyzeResult（写到指定指令）
static AnalyzeResult make_result(CIRInstructionRef ref, CIRInstResult result) {
    AnalyzeResult r;
    r.writes = make_array<ResultWrite>(permanent_allocator());
    r.writes.push_back({ref, result});
    return r;
}

// 便捷：构造多个结果写入的 AnalyzeResult（{ref, result} 列表，单/多结果统一入口）
static AnalyzeResult make_result(std::initializer_list<ResultWrite> writes) {
    AnalyzeResult r;
    r.writes = make_array<ResultWrite>(permanent_allocator());
    for(auto& w : writes) {
        r.writes.push_back(w);
    }
    return r;
}


// handler: ConstantValue
std::optional<AnalyzeResult> Interpreter::analyze_ConstantValue(CIRConstantValueInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    Value val = inst(pc_ref)->info<CIROperator::ConstantValue>().value;
    return AnalyzeResult{CIRInstResult::make_value(val.type, val), pc_ref};
}

// handler: StringLiteral
std::optional<AnalyzeResult> Interpreter::analyze_StringLiteral(CIRStringLiteralInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    // 从 IdentVal 依赖获取 string 类型（由 std/builtin/string.cst 注册并解析）
    TypeRef string_type = ResultValue(inst(pc_ref)->info<CIROperator::StringLiteral>().string_type_inst).type_val();
    if (string_type == nullptr) {
        return AnalyzeResult{CIRInstResult::make_error(), pc_ref};
    }

    Value v = make_value(string_type);
    Array<Value> field_values = make_array<Value>(permanent_allocator());

    Value data_field = make_value(string_type->struct_info.struct_fields[0].type);
    data_field.pointer_val(info.data);
    field_values.push_back(data_field);

    Value count_field = make_value(string_type->struct_info.struct_fields[1].type);
    count_field.integer_val(info.count);
    field_values.push_back(count_field);

    v.struct_fields_val(field_values);

    return AnalyzeResult{CIRInstResult::make_value(string_type, v), pc_ref};
}


// handler: PointerType
std::optional<AnalyzeResult> Interpreter::analyze_PointerType(CIRPointerTypeInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    CIRInstructionRef pointed_inst = inst(pc_ref)->info<CIROperator::PointerType>().pointed_type_inst;

    if(!is_type_type(ResultType(pointed_inst))) {
        return AnalyzeResult{inst_error(pc_ref, "指针类型需要一个类型参数，但收到 '{}'", pkg->inst(pointed_inst)->to_string()), pc_ref};

    }

    if(should_eval_for_lazy_eval({pointed_inst})) {
        TypeRef pointed = ResultValue(pointed_inst).type_val();
        if(pointed == nullptr) {
            return AnalyzeResult{inst_error(pc_ref, "指针类型需要一个具体的类型参数"), pc_ref};
        }

        Value result = make_value(type_type());
        result.type_val(pointer_type(pointed));
        return AnalyzeResult{CIRInstResult::make_value(result), pc_ref};
    }

    return AnalyzeResult{CIRInstResult::make_type_only(type_type()), pc_ref};
}


// handler: Load
std::optional<AnalyzeResult> Interpreter::analyze_Load(CIRLoadInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    CIRInstructionRef ptr_inst = inst(pc_ref)->info<CIROperator::Load>().ptr_inst;

    if(is_lvalue(ptr_inst)) {
        TypeRef loaded_type = ResultType(ptr_inst);

        if(has_instance() && (curr_eval_mode() == EvalMode::FullEval || has_result_val(ptr_inst))) {
            Pointer ptr = ResultValue(ptr_inst).pointer_val();
            Value val = ptr.load(loaded_type, permanent_allocator());
            return AnalyzeResult{CIRInstResult::make_value(loaded_type, val), pc_ref};
        }
        return AnalyzeResult{CIRInstResult::make_type_only(loaded_type), pc_ref};
    }

    TypeRef ptr_type = ResultType(ptr_inst);
    if(!is_pointer_type(ptr_type)) {
        return AnalyzeResult{inst_error(pc_ref, "不能加载非指针值，实际类型 '{}'", ptr_type->name()), pc_ref};
    }

    if(has_instance() && (curr_eval_mode() == EvalMode::FullEval || has_result_val(ptr_inst))) {
        Pointer ptr = ResultValue(ptr_inst).pointer_val();
        Value val = ptr.load(ptr_type->pointed_type, permanent_allocator());
        return AnalyzeResult{CIRInstResult::make_value(ptr_type->pointed_type, val), pc_ref};
    }
    return AnalyzeResult{CIRInstResult::make_type_only(ptr_type->pointed_type), pc_ref};
}


std::optional<AnalyzeResult> Interpreter::analyze_Deref(CIRDerefInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    CIRInstructionRef ptr_inst = inst(pc_ref)->info<CIROperator::Deref>().operand_inst;

    TypeRef ptr_type = ResultType(ptr_inst);
    if(!is_pointer_type(ptr_type)) {
        return AnalyzeResult{inst_error(pc_ref, "不能解引用非指针类型，实际类型 '{}'", ptr_type->name()), pc_ref};
    }

    // NOTE: 隐式约定, 既然已经保证了是指针类型, 那么就可以保证 pointed_type 不为 null
    // 且actual_type 一定是 *ptr_type, 所以也可以保证 actual_pointed 不为 null
    TypeRef pointed = ptr_type->pointed_type;
    TypeRef actual_pointed = result_context().result_of(ptr_inst).actual_type()->pointed_type;

    auto res = CIRInstResult::make_type_only(pointed);
    res.set_actual_type(actual_pointed);
    res.value_kind = result_context().result_of(ptr_inst).value_kind; // 继承属性

    if(curr_eval_mode() == EvalMode::FullEval && has_instance()) {
        Pointer ptr = ResultValue(ptr_inst).pointer_val();
        Value val = ptr.load(actual_pointed, permanent_allocator());
        res.set_val(val);
    }


    return AnalyzeResult{res, pc_ref};
}

// handler: Unary
std::optional<AnalyzeResult> Interpreter::analyze_Unary(CIRUnaryInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) {
        return std::nullopt;
    }

    auto& unary_info = inst(pc_ref)->info<CIROperator::Unary>();

    auto op = unary_info.op;
    auto operand_inst = unary_info.operand_inst;


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
        return make_result(pc_ref, inst_error(pc_ref, "未知的一元运算符"));
    }

    //
    // 类型检查
    //
    if(op == TokenType::Minus) {
        if(!is_number_type(operand_type)) {
            return make_result(pc_ref, inst_error(pc_ref, "一元负号要求数值类型操作数，实际类型 '{}'", operand_type->name()));
        }
    } else if(op == TokenType::Exclamation) {
        if(operand_type != easy_type(Type_bool)) {
            return make_result(pc_ref, inst_error(pc_ref, "逻辑非要求布尔类型操作数，实际类型 '{}'", operand_type->name()));
        }
    }

    XP_ASSERT_DEFAULT(result_type != nullptr);

    if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval({operand_inst})) {
        Value operand_val = ResultValue(operand_inst);

        ValueResult exec_res = exec_unary(operand_val, op);
        if(exec_res.is_err()) {
            return make_result(pc_ref, inst_error(pc_ref, "一元表达式求值失败"));
        }

        Value result_val = exec_res.as_ok();
        return make_result(pc_ref, CIRInstResult::make_value(result_type, result_val));
    }

    return make_result(pc_ref, CIRInstResult::make_type_only(result_type));
}

// handler: Cast
std::optional<AnalyzeResult> Interpreter::analyze_Cast(CIRCastInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) {
        return std::nullopt;
    }

    auto& cast_info = inst(pc_ref)->info<CIROperator::Cast>();

    auto expr_inst = cast_info.expr_inst;
    auto target_type_inst = cast_info.target_type_inst;


    if(!has_result_val(target_type_inst)) {
        return std::nullopt;
    }

    TypeRef expr_type = ResultType(expr_inst);
    TypeRef target_type = ResultValue(target_type_inst).type_val();

    if(!check_explicit_type_cast(pkg, expr_inst, expr_type, target_type)) {
        return make_result(pc_ref, inst_error(pc_ref, "不能在这些类型之间进行转换：'{}' → '{}'", expr_type->name(), target_type->name()));
    }

    TypeRef result_type = target_type;
    XP_ASSERT_DEFAULT(result_type != nullptr);

    if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval({expr_inst})) {
        Value expr_val = ResultValue(expr_inst);

        ValueResult exec_res = exec_cast(expr_val, target_type);
        if(exec_res.is_err()) {
            return make_result(pc_ref, inst_error(pc_ref, "转换表达式求值失败"));
        }

        Value result_val = exec_res.as_ok();
        return make_result(pc_ref, CIRInstResult::make_value(result_type, result_val));
    }

    return make_result(pc_ref, CIRInstResult::make_type_only(result_type));
}

// handler: ArrayType
std::optional<AnalyzeResult> Interpreter::analyze_ArrayType(CIRArrayTypeInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) {
        return std::nullopt;
    }



    if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval({info.count_inst})) {
        if(ResultType(info.element_type_inst) != type_type()) {
            return make_result(pc_ref, inst_error(pc_ref, "数组类型需要一个类型参数，实际收到 '{}'", ResultType(info.element_type_inst)->name()));
        }

        TypeRef elem_type = ResultValue(info.element_type_inst).type_val();

        // count 未求值（变量/错误表达式）时 FullEval 下会 panic，这里显式报错
        if(!has_result_val(info.count_inst)) {
            return make_result(pc_ref, inst_error(pc_ref, "数组长度必须是编译期常量，实际 '{}' 未求值", pkg->inst(info.count_inst)->to_string()));
        }

        Value count_val = ResultValue(info.count_inst);
        i128 count = count_val.integer_val();

        TypeRef arr_type = array_type(elem_type, count);
        TypeRef meta = type_type();

        Value result = make_value(meta);
        result.type_val(arr_type);
        return make_result(pc_ref, CIRInstResult::make_value(result));
    }

    return std::nullopt;   // 非 FullEval 不写结果（与原代码一致）
}

// handler: SliceType
std::optional<AnalyzeResult> Interpreter::analyze_SliceType(CIRSliceTypeInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) {
        return std::nullopt;
    }



    if(!has_result_val(info.element_type_inst)) {
        return make_result(pc_ref, CIRInstResult::make_type_only(type_type()));
    }

    if(ResultType(info.element_type_inst) != type_type()) {
        return make_result(pc_ref, inst_error(pc_ref, "切片类型需要一个类型参数，实际收到 '{}'", ResultType(info.element_type_inst)->name()));
    }

    TypeRef elem_type = ResultValue(info.element_type_inst).type_val();

    TypeRef slice_type = slice_type_as_struct(elem_type);
    TypeRef meta = type_type();

    Value result = make_value(meta);
    result.type_val(slice_type);
    return make_result(pc_ref, CIRInstResult::make_value(result));
}

// handler: FuncType
std::optional<AnalyzeResult> Interpreter::analyze_FuncType(CIRFuncTypeInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) {
        return std::nullopt;
    }



    Array<TypeRef> param_types = make_array<TypeRef>(permanent_allocator());
    defer(array_free(&param_types));

    for(isize i = 0; i < info.param_type_insts.count; i++) {
        if(!has_result_val(info.param_type_insts[i])) {
            return make_result(pc_ref, CIRInstResult::make_type_only(type_type()));
        }
        if(ResultType(info.param_type_insts[i]) != type_type()) {
            return make_result(pc_ref, inst_error(pc_ref, "函数类型第 {} 个参数需要一个类型参数", i));
        }
        TypeRef pt = ResultValue(info.param_type_insts[i]).type_val();
        param_types.push_back(pt);
    }

    if(!has_result_val(info.return_type_inst)) {
        return make_result(pc_ref, CIRInstResult::make_type_only(type_type()));
    }
    if(ResultType(info.return_type_inst) != type_type()) {
        return make_result(pc_ref, inst_error(pc_ref, "函数类型需要一个类型参数作为返回类型"));
    }
    TypeRef return_type = ResultValue(info.return_type_inst).type_val();

    TypeRef func_type = function_type(param_types, return_type);
    TypeRef meta = type_type();

    Value result = make_value(meta);
    result.type_val(func_type);
    return make_result(pc_ref, CIRInstResult::make_value(result));
}

// handler: TypeOfInstResult
std::optional<AnalyzeResult> Interpreter::analyze_TypeOfInstResult(CIRTypeOfInstResultInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) {
        return std::nullopt;
    }

    auto target_inst = inst(pc_ref)->info<CIROperator::TypeOfInstResult>().target_inst;


    TypeRef target_type = ResultType(target_inst);
    // 如果目标结果本身就是 type value（如泛型 Call 返回的编译期类型），
    // 提取其内部的实际类型，而非把 type_type() 当作变量类型
    if(target_type == type_type() && has_result_val(target_inst)) {
        target_type = ResultValue(target_inst).type_val();
    }
    TypeRef meta = type_type();

    Value result = make_value(meta);
    result.type_val(target_type);
    return make_result(pc_ref, CIRInstResult::make_value(result));
}

// handler: FieldTypeOfStruct
std::optional<AnalyzeResult> Interpreter::analyze_FieldTypeOfStruct(CIRFieldTypeOfStructInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) {
        return std::nullopt;
    }



    if(!has_result_val(info.struct_type_inst)) {
        return std::nullopt;
    }
    TypeRef st = ResultValue(info.struct_type_inst).type_val();

    if(!is_struct_type(st)) {
        return make_result(pc_ref, inst_error(pc_ref, "不能从非结构体类型获取字段类型，实际类型 '{}'", st->name()));
    }

    TypeRef field_type = st->struct_info.struct_fields[info.field_index].type;
    TypeRef meta = type_type();

    Value result = make_value(meta);
    result.type_val(field_type);
    return make_result(pc_ref, CIRInstResult::make_value(result));
}

// handler: FuncParamType
std::optional<AnalyzeResult> Interpreter::analyze_FuncParamType(CIRFuncParamTypeInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) {
        return std::nullopt;
    }



    TypeRef func_type = ResultValue(info.type_of_func_type_inst).type_val();

    if(is_pointer_type(func_type) && is_function_type(func_type->pointed_type)) {
        func_type = func_type->pointed_type;
    }

    if(!is_function_type(func_type)) {
        return make_result(pc_ref, inst_error(pc_ref, "不能从非函数类型获取参数类型，实际类型 '{}'", func_type->name()));
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
            return make_result(pc_ref, inst_error(pc_ref, "参数索引 {} 越界（函数共有 {} 个参数）", info.param_index, param_types.count));
        }
        param_type = easy_type(Type_var_arg_c);
    }

    TypeRef meta = type_type();
    Value result = make_value(meta);
    result.type_val(param_type);
    return make_result(pc_ref, CIRInstResult::make_value(result));
}

// handler: StructInit
std::optional<AnalyzeResult> Interpreter::analyze_StructInit(CIRStructInitInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) {
        return std::nullopt;
    }



    if(!has_result_val(info.struct_type_inst)) {
        return std::nullopt;
    }

    TypeRef st = ResultValue(info.struct_type_inst).type_val();
    // XP_ASSERT_DEFAULT(is_struct_type(st));
    if(!is_struct_type(st)) {
        return make_result(pc_ref, inst_error(pc_ref, "你不能把非结构体类型 '{}' 当作结构体来初始化", st->name()));
    }

    if(info.field_init_insts.count != st->struct_info.struct_fields.count) {
        return make_result(pc_ref, inst_error(pc_ref, "结构体初始化字段数量与结构体定义不匹配（{} vs {}）", info.field_init_insts.count, st->struct_info.struct_fields.count));
    }

    // 字段类型检查
    for(isize i = 0; i < info.field_init_insts.count; i++) {
        TypeRef field_type = ResultType(info.field_init_insts[i]);
        TypeRef expected = st->struct_info.struct_fields[i].type;
        std::optional<TypeRef> implicit_type = result_context().result_of(info.field_init_insts[i]).implicit_type;
        if(field_type != expected && ((!implicit_type.has_value()) || implicit_type != expected)) {
            return make_result(pc_ref, inst_error(pc_ref, "结构体初始化字段类型与定义不匹配（字段 {}：实际 '{}'，期望 '{}'）", i, field_type->name(), expected->name()));
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
        return make_result(pc_ref, CIRInstResult::make_value(v));
    }

    return make_result(pc_ref, CIRInstResult::make_type_only(st));
}

// handler: ArrayInit（含 untyped 元素传染，副作用进 writes）
std::optional<AnalyzeResult> Interpreter::analyze_ArrayInit(CIRArrayInitInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) {
        return std::nullopt;
    }



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

    // 有具体类型则将其他 untyped 元素传染为该类型（副作用进 writes；溢出检查用传染后的类型）
    AnalyzeResult r;   // 累积 writes：传染 + 自身结果
    if(elem_type != nullptr && !is_untyped_type(elem_type)) {
        for(isize i = 0; i < info.element_insts.count; i++) {
            auto ei = info.element_insts[i];
            TypeRef t = ResultType(ei);

            if(is_untyped_type(t)) {
                Value v = ResultValue(ei);
                v.set_type(elem_type);   // 模拟传染后的类型做溢出检查
                if(is_val_overflow(v)) {
                    return make_result(pc_ref, inst_error(pc_ref, "数组初始化元素值溢出（元素 {}）", i));
                }
                r.writes.push_back({ei, CIRInstResult::make_type_only(elem_type)});   // 传染
            } else if(t != elem_type) {
                return make_result(pc_ref, inst_error(pc_ref, "数组初始化元素类型不一致（元素 {}：'{}'，期望 '{}'）", i, t->name(), elem_type->name()));
            }
        }
    }

    usize count = info.element_insts.count;

    TypeRef arr_type = array_type(elem_type, count);

    if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval(info.element_insts)) {
        Value v = make_value(arr_type);
        v.set_type(arr_type);
        Array<Value> elem_values = make_array<Value>(permanent_allocator());
        for(isize i = 0; i < info.element_insts.count; i++) {
            Value ev = ResultValue(info.element_insts[i]);
            if(is_untyped_type(ev.type) && elem_type != nullptr) {
                ev.set_type(elem_type);   // 传染延迟生效：模拟元素值传染
            }
            elem_values.push_back(ev);
        }
        v.array_element_values(elem_values);
        r.writes.push_back({pc_ref, CIRInstResult::make_value(v)});
    } else {
        r.writes.push_back({pc_ref, CIRInstResult::make_type_only(arr_type)});
    }

    return r;
}

// handler: Index
std::optional<AnalyzeResult> Interpreter::analyze_Index(CIRIndexInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) {
        return std::nullopt;
    }



    TypeRef array_type_ref = ResultType(info.array_inst);

    TypeRef elem_type = nullptr;
    if(is_array_type(array_type_ref)) {

        elem_type = array_type_ref->array_info.element_type;

    } else if(is_slice_struct_type(array_type_ref)) {

        elem_type = array_type_ref->struct_info.struct_fields[0].type->pointed_type;

    } else {
        return make_result(pc_ref, inst_error(pc_ref, "索引不支持的类型的值，实际类型 '{}'", array_type_ref->name()));
    }

    if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval({info.array_inst, info.index_inst})) {
        i128 idx = ResultValue(info.index_inst).integer_val();

        if(is_array_type(array_type_ref)) {
            if(idx < 0 || idx >= array_type_ref->array_info.count) {
                return make_result(pc_ref, inst_error(pc_ref, "数组索引越界（索引 {}，长度 {}）", idx, array_type_ref->array_info.count));
            }
            return make_result(pc_ref, inst_error(pc_ref, "编译期数组元素提取尚未实现"));
        } else if(is_slice_struct_type(array_type_ref)) {
            return make_result(pc_ref, inst_error(pc_ref, "编译期切片元素提取尚未实现"));
        }
    }

    return make_result(pc_ref, CIRInstResult::make_type_only(elem_type));
}

// handler: StructField
std::optional<AnalyzeResult> Interpreter::analyze_StructField(CIRStructFieldInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    auto type_block_inst = inst(pc_ref)->info<CIROperator::StructField>().type_block_inst;


    if(has_result_type(type_block_inst)) {
        CIRInstResult r;
        r.set_type(ResultType(type_block_inst));
        if(has_result_val(type_block_inst)) {
            r.set_val(ResultValue(type_block_inst));
        }
        return make_result(pc_ref, r);
    }

    return std::nullopt;
}

// handler: FieldAccess
std::optional<AnalyzeResult> Interpreter::analyze_FieldAccess(CIRFieldAccessInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) {
        return std::nullopt;
    }

    CIRInstructionRef parent_inst = info.parent_inst;

    TypeRef parent_type = ResultType(parent_inst);

    // 解析结构体类型（支持指针自动解引用）
    auto get_struct_type = [](TypeRef t) -> TypeRef {
        if(is_struct_type(t)) return t;
        if(is_pointer_type(t) && is_struct_type(t->pointed_type)) return t->pointed_type;
        return nullptr;
    };
    auto get_union_type = [](TypeRef t) -> TypeRef {
        if(is_union_type(t)) return t;
        if(is_pointer_type(t) && is_union_type(t->pointed_type)) return t->pointed_type;
        return nullptr;
    };

    if(is_package_type(parent_type)) {
        // 包成员访问: pkg.symbol
        Ref<Package> pkg_val = parent_type->package_info;
        SymbolInfo *field_sym = find_symbol_curr(&pkg_val.unwrap().package_scope.unwrap(), info.field_name);
        if(field_sym == nullptr) {
            return make_result(pc_ref, inst_error(pc_ref, "包成员 '{}' 不存在", info.field_name));
        }

        auto r = field_sym->result(curr_cache_key());
        if(r.state == CIRResultState::WholeValue) {
            // type = 成员的类型，actual = 成员的实际值（可能来自 value_inst，类型不同）
            return make_result(pc_ref, CIRInstResult::make_value(r.type(), r.actual_val()));
        }
        return make_result(pc_ref, CIRInstResult::make_type_only(r.type()));

    } else if(TypeRef struct_type = get_struct_type(parent_type)) {

        for(isize i = 0; i < struct_type->struct_info.struct_fields.count; i++) {
            if(xp_string_equal(struct_type->struct_info.struct_fields[i].name, info.field_name)) {
                TypeRef field_type = struct_type->struct_info.struct_fields[i].type;

                if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval({parent_inst})) {
                    Value field_val = ResultValue(parent_inst).struct_field_val(i);
                    return make_result(pc_ref, CIRInstResult::make_value(field_type, field_val));
                }

                return make_result(pc_ref, CIRInstResult::make_type_only(field_type));
            }
        }

        return make_result(pc_ref, inst_error(pc_ref, "结构体字段 '{}' 不存在", info.field_name));

    } else if(TypeRef union_type = get_union_type(parent_type)) {

        Ref<Scope> union_scope = union_type->union_info.union_scope;
        TypeRef field_type = nullptr;
        for(const auto& entry : *union_scope) {
            if(xp_string_equal(entry.value.name, info.field_name)) {
                field_type = union_field_type(union_type, entry.value.name);
                break;
            }
        }

        if(field_type == nullptr) {
            return make_result(pc_ref, inst_error(pc_ref, "联合体字段 '{}' 不存在", info.field_name));
        }

        if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval({parent_inst})) {
            context()->reporter.report_error(inst(pc_ref)->src_loc, "联合体字段访问的编译期求值尚未实现");
        }

        return make_result(pc_ref, CIRInstResult::make_type_only(field_type));

    } else if(has_result_val(parent_inst) && is_enum_type(ResultValue(parent_inst).type_val())) {
        // 枚举成员访问: EnumType.Variant
        TypeRef enum_type = ResultValue(parent_inst).type_val();
        SymbolInfo *field_sym = find_symbol_curr(&enum_type->enum_info.enum_scope.unwrap(), info.field_name);
        if(field_sym == nullptr) {
            return make_result(pc_ref, inst_error(pc_ref, "枚举变体 '{}' 不存在", info.field_name));
        }
        auto r = field_sym->result(curr_cache_key());
        if(r.state == CIRResultState::WholeValue) {
            // 枚举成员的值本身是 tag（u8），但结果类型强制为枚举类型本身
            return make_result(pc_ref, CIRInstResult::make_value(enum_type, r.actual_val()));
        }
        return make_result(pc_ref, CIRInstResult::make_type_only(enum_type));

    } else {
        return make_result(pc_ref, inst_error(pc_ref, "字段访问不支持的类型的值，实际类型 '{}'", parent_type->name()));
    }
}

// handler: FieldPtr（结果 = LValue）
std::optional<AnalyzeResult> Interpreter::analyze_FieldPtr(CIRFieldPtrInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    CIRInstructionRef parent_inst = info.parent_inst;

    if(!is_lvalue(parent_inst)) {
        return make_result(pc_ref, inst_error(pc_ref, "不能对非左值表达式取字段指针"));
    }

    TypeRef parent_type = ResultType(parent_inst);

    // union 分支（所有成员 offset 0）
    if(is_union_type(parent_type) || (is_pointer_type(parent_type) && is_union_type(parent_type->pointed_type))) {
        TypeRef union_type = is_union_type(parent_type) ? parent_type : parent_type->pointed_type;

        TypeRef field_type = union_field_type(union_type, info.field_name);
        if(field_type == nullptr) {
            return make_result(pc_ref, inst_error(pc_ref, "FieldPtr：联合体字段 '{}' 不存在", info.field_name));
        }

        if(curr_eval_mode() == EvalMode::FullEval && has_instance()) {
            context()->reporter.report_error(inst(pc_ref)->src_loc, "联合体字段指针访问的编译期求值尚未实现");
        }

        return make_result(pc_ref, CIRInstResult::make_type_only(field_type, CIRValueKind::LValue));
    }

    TypeRef struct_type = nullptr;
    if(is_struct_type(parent_type)) {
        struct_type = parent_type;
    } else if(is_pointer_type(parent_type) && is_struct_type(parent_type->pointed_type)) {
        struct_type = parent_type->pointed_type;
    } else {
        return make_result(pc_ref, inst_error(pc_ref, "字段指针访问仅支持结构体/联合体类型或指向它们的指针，实际收到 '{}'", parent_type->name()));
    }

    for(isize i = 0; i < struct_type->struct_info.struct_fields.count; i++) {
        if(xp_string_equal(struct_type->struct_info.struct_fields[i].name, info.field_name)) {
            TypeRef field_type = struct_type->struct_info.struct_fields[i].type;

            if(curr_eval_mode() == EvalMode::FullEval && has_instance()) {
                auto ptr = ResultValue(parent_inst).pointer_val();
                if(is_pointer_type(ResultType(parent_inst)) && is_struct_type(ResultType(parent_inst)->pointed_type)) {
                    Value inner = ptr.load(ResultType(parent_inst), permanent_allocator());
                    ptr = inner.pointer_val();
                }
                isize field_off = field_serialize_offset(struct_type, i);
                isize total_off = ptr.offset + field_off;

                Value addr = make_value(pointer_type(field_type));

                // TODO: ABSTRACT ptr.mem
                addr.pointer_val(Pointer::make(ptr.mem, total_off));

                return make_result(pc_ref, CIRInstResult::make_value(field_type, addr, CIRValueKind::LValue));
            }
            return make_result(pc_ref, CIRInstResult::make_type_only(field_type, CIRValueKind::LValue));
        }
    }

    return make_result(pc_ref, inst_error(pc_ref, "FieldPtr：结构体字段 '{}' 不存在", info.field_name));
}

// handler: IndexPtr（结果 = LValue）
std::optional<AnalyzeResult> Interpreter::analyze_IndexPtr(CIRIndexPtrInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    CIRInstructionRef array_inst = info.array_inst;

    if(!is_lvalue(array_inst)) {
        return make_result(pc_ref, inst_error(pc_ref, "不能对非左值表达式取索引指针"));
    }

    TypeRef array_type_ref = ResultType(array_inst);
    TypeRef elem_type = nullptr;
    if(is_array_type(array_type_ref)) {
        elem_type = array_type_ref->array_info.element_type;
    } else if(is_slice_struct_type(array_type_ref)) {
        elem_type = array_type_ref->struct_info.struct_fields[0].type->pointed_type;
    } else {
        return make_result(pc_ref, inst_error(pc_ref, "索引指针不支持的类型的值，实际类型 '{}'", array_type_ref->name()));
    }

    if(curr_eval_mode() == EvalMode::FullEval && has_instance()) {
        // TODO: ABSTRACT
        auto base_ptr = ResultValue(array_inst);
        i128 idx = ResultValue(info.index_inst).integer_val();
        isize stride = type_serialize_stride(elem_type);

        auto ptr = Pointer::add(base_ptr.pointer_val(), idx, stride);

        Value addr = make_value(pointer_type(elem_type));
        addr.pointer_val(ptr);

        return make_result(pc_ref, CIRInstResult::make_value(elem_type, addr, CIRValueKind::LValue));
    }
    return make_result(pc_ref, CIRInstResult::make_type_only(elem_type, CIRValueKind::LValue));
}

// TODO: 模仿Deref的处理
// handler: AddrOf
std::optional<AnalyzeResult> Interpreter::analyze_AddrOf(CIRAddrOfInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    CIRInstructionRef lval_inst = info.lval_inst;

    TypeRef lval_type = ResultType(lval_inst);
    if(!is_lvalue(lval_inst) && !is_function_type(lval_type)) {
        return make_result(pc_ref, inst_error(pc_ref, "不能对非左值表达式取地址"));
    }

    if(is_function_type(lval_type)) {
        FuncValue fv;
        bool got_fv = false;
        if(has_result_val(lval_inst)) {
            fv = ResultValue(lval_inst).func_val();
            got_fv = true;
        } else {
            CIRInstruction* lval_ptr = pkg->inst(lval_inst);
            SymbolInfo* sym = try_access_val(lval_ptr->symbol);
            if(sym && sym->is_const_decl_and_func()) {
                auto r = sym->result(curr_cache_key());
                if(r.state == CIRResultState::WholeValue) {
                    fv = r.actual_val().func_val();
                    got_fv = true;
                }
            }
        }
        if(got_fv && is_pure_comptime_func(fv.func_key.cir_package->inst(fv.func_key.inst_ref)->info<CIROperator::FunctionDecl>(), result_context())) {
            return make_result(pc_ref, inst_error(pc_ref, "不能取泛型函数的地址"));
        }
    }

    TypeRef ptr_type = pointer_type(lval_type);
    if(curr_eval_mode() == EvalMode::FullEval && has_instance()) {
        if(is_lvalue(lval_inst)) {
            Value addr = ResultValue(lval_inst);
            return make_result(pc_ref, CIRInstResult::make_value(ptr_type, addr));
        }
    }
    return make_result(pc_ref, CIRInstResult::make_type_only(ptr_type));
}

// handler: Binary（含 untyped 类型传染，副作用进 writes）
std::optional<AnalyzeResult> Interpreter::analyze_Binary(CIRBinaryInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) return std::nullopt;

    auto& binary_info = inst(pc_ref)->info<CIROperator::Binary>();
    auto op = binary_info.op;
    auto left_inst = binary_info.left_inst;
    auto right_inst = binary_info.right_inst;

    TypeRef left_type = ResultType(left_inst);
    TypeRef right_type = ResultType(right_inst);

    // untyped 被数值/指针类型传染：写操作数类型（副作用进 writes）+ 溢出检查（用传染后类型）
    AnalyzeResult r;
    CIRInstructionRef contagion_target = INVALID_INST;
    TypeRef contagion_type = nullptr;
    if(is_number_type(left_type) && is_untyped_type(right_type)) {
        contagion_type = left_type;
        contagion_target = right_inst;
    } else if(is_untyped_type(left_type) && is_number_type(right_type)) {
        contagion_type = right_type;
        contagion_target = left_inst;
    } else if(is_pointer_type(left_type) && is_untyped_type(right_type)) {
        contagion_type = easy_type(Type_i64);
        contagion_target = right_inst;
    } else if(is_untyped_type(left_type) && is_pointer_type(right_type)) {
        contagion_type = easy_type(Type_i64);
        contagion_target = left_inst;
    }
    if(contagion_target != INVALID_INST) {
        r.writes.push_back({contagion_target, CIRInstResult::make_type_only(contagion_type)});
        // 模拟 apply：writes 延迟生效，handler 内后续需用传染后的类型
        if(contagion_target == right_inst) {
            right_type = contagion_type;
        }
        if(contagion_target == left_inst) {
            left_type = contagion_type;
        }
        if(has_result_val(contagion_target)) {
            Value v = ResultValue(contagion_target);
            v.set_type(contagion_type);   // 模拟传染后的类型做溢出检查
            if(is_val_overflow(v)) {
                return make_result(pc_ref, inst_error(pc_ref, "类型传染时值溢出（传染后类型 '{}'）", contagion_type->name()));
            }
        }
    }

    // 确定结果类型
    TypeRef result_type;
    if(is_return_bool_operator(op)) {
        result_type = easy_type(Type_bool);
    } else if(is_pointer_type(left_type) || is_pointer_type(right_type)) {
        result_type = is_pointer_type(left_type) ? left_type : right_type;
    } else {
        result_type = left_type;
    }

    // 类型检查
    if(is_pointer_type(left_type) && is_pointer_type(right_type)) {
        if(!is_equal_compare_operator(op)) {
            return make_result(pc_ref, inst_error(pc_ref, "指针类型之间只允许相等比较"));
        }
        if(left_type != right_type) {
            if(get_innermost_type_of_pointer(left_type) != easy_type(Type_void) &&
               get_innermost_type_of_pointer(right_type) != easy_type(Type_void)) {
                return make_result(pc_ref, inst_error(pc_ref, "比较的指针类型必须一致（'{}' vs '{}'）", left_type->name(), right_type->name()));
            }
        }
    } else if(is_pointer_type(left_type) || is_pointer_type(right_type)) {
        TypeRef ptr   = is_pointer_type(left_type) ? left_type : right_type;
        TypeRef other = is_pointer_type(left_type) ? right_type : left_type;
        if(!is_add_sub_operator(op)) {
            return make_result(pc_ref, inst_error(pc_ref, "指针算术只允许 + 和 - 运算符"));
        }
        if(get_innermost_type_of_pointer(ptr) == easy_type(Type_void)) {
            return make_result(pc_ref, inst_error(pc_ref, "不允许对 void 指针进行指针算术"));
        }
        if(!is_integer_or_untyped_type(other)) {
            return make_result(pc_ref, inst_error(pc_ref, "指针算术只允许在指针和整数类型之间进行（实际 '{}' 与 '{}'）", left_type->name(), right_type->name()));
        }
    } else {
        if(left_type != right_type) {
            return make_result(pc_ref, inst_error(pc_ref, "二元运算符要求两个操作数类型相同（实际 '{}' 与 '{}'）", left_type->name(), right_type->name()));
        }
        if(is_struct_type(left_type) || is_array_type(left_type)) {
            if(!is_equal_compare_operator(op)) {
                return make_result(pc_ref, inst_error(pc_ref, "结构体和数组类型只允许相等比较"));
            }
        }
        if(is_enum_type(left_type)) {
            if(!is_equal_compare_operator(op)) {
                return make_result(pc_ref, inst_error(pc_ref, "枚举类型只允许相等比较和布尔运算符"));
            }
        }
        if(left_type == easy_type(Type_bool)) {
            if(!is_operator_for_bool(op)) {
                return make_result(pc_ref, inst_error(pc_ref, "布尔类型只允许布尔运算符和相等比较"));
            }
        }
        if(op == TokenType::Percent && is_float_type(left_type)) {
            return make_result(pc_ref, inst_error(pc_ref, "浮点类型不允许取模运算符"));
        }
    }

    if(curr_eval_mode() == EvalMode::FullEval || should_eval_for_lazy_eval({left_inst, right_inst})) {
        Value left_val = ResultValue(left_inst);
        Value right_val = ResultValue(right_inst);
        // 传染延迟生效：模拟操作数值的传染后类型（writes 尚未 apply）
        if(contagion_target == left_inst) {
            left_val.set_type(contagion_type);
        }
        if(contagion_target == right_inst) {
            right_val.set_type(contagion_type);
        }
        ValueResult exec_res = exec_binary(left_val, right_val, op);
        if(exec_res.is_err()) {
            return make_result(pc_ref, inst_error(pc_ref, "二元表达式求值失败"));
        }
        r.writes.push_back({pc_ref, CIRInstResult::make_value(result_type, exec_res.as_ok())});
        return r;
    }
    r.writes.push_back({pc_ref, CIRInstResult::make_type_only(result_type)});
    return r;
}

// handler: Break（写目标块 handle 的结果，外层块循环据此退出）
std::optional<AnalyzeResult> Interpreter::analyze_Break(CIRBreakInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    CIRInstructionRef target_block = info.break_block;
    CIRBlockRefInfo& block_ref_info = pkg->inst(target_block)->info<CIROperator::BlockRef>();

    if(info.break_value_inst != INVALID_INST) {
        AnalyzeResult r;   // 累积 target_block 的写入（类型 + 值）
        // 已有类型：检查是否匹配
        if(result_state(target_block) >= CIRResultState::OnlyType) {
            if(has_error(target_block)) {
                return make_result(pc_ref, CIRInstResult::make_error());
            }
            TypeRef existing_type = ResultType(target_block);
            TypeRef new_type = ResultType(info.break_value_inst);
            if(existing_type != new_type) {
                context()->reporter.report_error(inst(pc_ref)->src_loc, "break 值类型不匹配（现有 '{}'，传入 '{}'）", existing_type->name(), new_type->name());
                return make_result(pc_ref, CIRInstResult::make_error());
            }
        } else {
            // 无类型：设置类型（不 return，继续写值）
            if(has_result_type(info.break_value_inst)) {
                r.writes.push_back({target_block, CIRInstResult::make_type_only(ResultType(info.break_value_inst))});
            }
        }

        // @bug: 目前在编译期执行时, 还不存在无target的break, 只有loop block的break才无target, 但是现在还不支持loop block的编译期执行, 2x2的bool矩阵正好只有对角线, 才无问题。
        // @bug: 如果要修复, 得支持在fulleval时修改curr_inst_ref, 让上层循环可以通过curr_inst_ref来判断是否要退出循环, 而不是目前的通过target_block来判断
        // FullEval：写值到目标块
        if(curr_eval_mode() == EvalMode::FullEval && has_result_val(info.break_value_inst)) {

            // @attention: 测试中
            auto new_curr_inst_ref = curr_inst_ref();
            new_curr_inst_ref.block_ref = block_ref_info.in_which_block;
            new_curr_inst_ref.inst_index = target_block.inst_index;
            curr_inst_ref() = new_curr_inst_ref;

            r.writes.push_back({target_block, CIRInstResult::make_value(ResultValue(info.break_value_inst))});
        }
        if(r.writes.count > 0) {
            return r;
        }
    }
    return std::nullopt;
}

// handler: TypeAscribe（写 var_inst 的类型 + lvalue 地址）
std::optional<AnalyzeResult> Interpreter::analyze_TypeAscribe(CIRTypeAscribeInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {


    // 限制:
    // var_inst == VariableDecl
    // type_inst.result is type_type
    XP_ASSERT_DEFAULT(pkg->inst(info.var_inst)->op == CIROperator::VariableDecl);

    if(!has_result_val(info.type_inst)) {
        // 类型位置无 type 值：可能是泛型运行时调用，只报"非类型标注"错误
        if(result_context().result_of(info.type_inst).state >= CIRResultState::OnlyType) {
            TypeRef t = ResultType(info.type_inst);
            if(t != type_type() && t != undefined_type()) {
                context()->reporter.report_error(inst(pc_ref)->src_loc, "类型标注必须是类型，实际收到 '{}'", t->name());
                return make_result(pc_ref, CIRInstResult::make_error());
            }
        }
        return std::nullopt;
    }

    if(!is_type_type(ResultValue(info.type_inst).type)) {
        context()->reporter.report_error(inst(pc_ref)->src_loc, "类型标注必须是类型，实际收到 '{}'", ResultValue(info.type_inst).type->name());
        return make_result(pc_ref, CIRInstResult::make_error());
    }

    TypeRef declared_type = ResultValue(info.type_inst).type_val();
    CIRVariableDeclInfo& vd = pkg->inst(info.var_inst)->info<CIROperator::VariableDecl>();

    AnalyzeResult r;   // 写 var_inst（变量恒为 LValue，保留 VariableDecl 设置的 lvalue 语义）
    TypeRef existing = ResultType(info.var_inst);
    if(existing == undefined_type()) {
        r.writes.push_back({info.var_inst, CIRInstResult::make_type_only(declared_type, CIRValueKind::LValue)});
    } else {
        if(existing != declared_type) {
            context()->reporter.report_error(inst(pc_ref)->src_loc, "类型标注与推导出的变量类型冲突");
            return make_result(pc_ref, CIRInstResult::make_error());
        }
    }

    if(curr_eval_mode() == EvalMode::FullEval && has_instance()) {
        auto ptr = curr_instance()->var_ptrs[vd.slot];
        if(ptr.is_null()) {   // 参数已由 caller 分配
            isize size  = type_serialize_size(declared_type);
            isize align = type_serialize_align(declared_type);
            // 副作用：FullEval 分配变量内存 + 写入 instance 变量槽（stack_mem 帧内）
            ptr = stack_mem.alloc_bytes(size, align);
            curr_instance()->var_ptrs[vd.slot] = ptr;
        }
        Value addr = make_value(pointer_type(declared_type));
        addr.pointer_val(ptr);
        // type = 变量逻辑类型，actual = 地址（*declared_type）
        r.writes.push_back({info.var_inst, CIRInstResult::make_value(declared_type, addr, CIRValueKind::LValue)});
    }
    return r;
}

// handler: Store（statement，无自身结果；类型推断写 var_inst）
std::optional<AnalyzeResult> Interpreter::analyze_Store(CIRStoreInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    auto& store_info = inst(pc_ref)->info<CIROperator::Store>();
    CIRInstructionRef var_inst = store_info.var_inst;
    CIRInstructionRef value_inst = store_info.value_inst;

    TypeRef target_type;
    if(is_lvalue(var_inst)) {
        target_type = ResultType(var_inst);
    } else {
        TypeRef ptr_type = ResultType(var_inst);
        if(!is_pointer_type(ptr_type)) {
            return make_result(pc_ref, inst_error(pc_ref, "只能赋值给变量、字段、数组元素或解引用后的指针"));
        }
        target_type = ptr_type->pointed_type;
    }

    if(target_type == undefined_type()) {
        TypeRef inferred = ResultType(store_info.value_inst);
        if(is_function_type(inferred)) {
            return make_result(pc_ref, inst_error(pc_ref, "不能将函数值赋值给变量，请使用 '&' 显式取地址"));
        }
        // 类型推断：写 var_inst
        return make_result(var_inst, CIRInstResult::make_type_only(inferred));
    } else {
        TypeRef value_type = ResultType(store_info.value_inst);
        if(is_function_type(value_type)) {
            return make_result(pc_ref, inst_error(pc_ref, "不能将函数值赋值给变量，请使用 '&' 显式取地址"));
        }
        std::optional<TypeRef> implicit_type = result_context().result_of(store_info.value_inst).implicit_type;
        if(value_type != target_type && ((!implicit_type.has_value()) || implicit_type.value() != target_type)) {
            return make_result(pc_ref, inst_error(pc_ref, "尝试存储类型为 '{}' 的值，但期望类型为 '{}'", value_type->t_name(), target_type->t_name()));
        }
    }

    if(curr_eval_mode() == EvalMode::FullEval && has_instance()) {
        // 泛型运行时调用：comptime 结果是 type value 或无值，实际运行时值由 LLVM 单态化提供
        if(has_result_val(store_info.value_inst)) {
            Value val = ResultValue(store_info.value_inst);
            if(val.type == type_type() && target_type != type_type()) {
                return std::nullopt;   // 跳过 comptime Store，由 LLVM 生成
            }
            Pointer ptr = ResultValue(var_inst).pointer_val();
            // 副作用：FullEval 写内存（stack_mem 帧内）
            ptr.store(ResultValue(value_inst));
        }
    }
    return std::nullopt;
}

// handler: DetermineType（写 determined_inst 的类型 + 元素传染）
std::optional<AnalyzeResult> Interpreter::analyze_DetermineType(CIRDetermineTypeInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    auto determined_inst = info.determining_inst;
    auto expected_type_inst = info.type_inst;

    // TODO: HACK
    if(!has_result_type(determined_inst)) {
        return std::nullopt;
    }

    TypeRef determined_type = ResultType(determined_inst);
    bool has_val = has_result_val(determined_inst);
    Value result_val;
    if(has_val) {
        result_val = ResultValue(determined_inst);
    }

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

    AnalyzeResult r;   // 累积 writes 到 determined_inst + 元素传染
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
                r.writes.push_back({determined_inst, CIRInstResult::make_value(result_val)});
            }
        } else {
            elem_t = default_certain_type_for_untyped_type(elem_t);
        }
        expected_type = array_type(elem_t, expected_type->array_info.count);

        // 同步更新所有元素指令的类型，防止 LLVM 生成器遇到 untyped
        auto& elems_info = pkg->inst(determined_inst)->info<CIROperator::ArrayInit>();
        for(isize i = 0; i < elems_info.element_insts.count; i++) {
            auto ei = elems_info.element_insts[i];
            if(is_untyped_type(ResultType(ei))) {
                r.writes.push_back({ei, CIRInstResult::make_type_only(elem_t)});
            }
        }
    }

    // 写 determined_inst 的类型
    r.writes.push_back({determined_inst, CIRInstResult::make_type_only(expected_type)});

    if(expected_type_inst != INVALID_INST) {
        // 编译期类型的字段值本身是 type（如 enum { Variant :: TypeExpr }），跳过标签类型兼容检查
        if(determined_type == type_type()) {
            return r;
        }
        if(!has_result_val(info.type_inst)) {
            return r;
        }

        expected_type = ResultValue(info.type_inst).type_val();
        if(is_untyped_type(expected_type)) {
            // 目标类型不应该是 untyped（原为断言，改正常报错）
            return make_result(pc_ref, inst_error(pc_ref, "类型确定的目标类型不能是未定类型，实际 '{}'", expected_type->name()));
        }

        // var_arg: 不约束类型
        if(expected_type == easy_type(Type_var_arg_c)) {
            return r;
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
                context()->reporter.report_error(inst(pc_ref)->src_loc,
                    "implicit conversion from function type to function pointer is not allowed, use '&' to take address explicitly");
                return make_result(pc_ref, CIRInstResult::make_error());
            }
        } else if(determined_type == expected_type) {
            ok = true;
        }

        if(!ok) {
            context()->reporter.report_error(inst(pc_ref)->src_loc, "无法确定类型：期望 {}，实际 {}", expected_type->t_name(), determined_type->t_name());
            return make_result(pc_ref, CIRInstResult::make_error());
        }

        if(!is_implicit_cast) {
            // 覆盖写类型
            r.writes.clear();
            r.writes.push_back({determined_inst, CIRInstResult::make_type_only(expected_type)});
        } else {
            // 副作用：直接写 determined_inst 的 implicit_type（绕过 apply_result，供 codegen 隐式转换识别）
            result_context().result_of(determined_inst).implicit_type = expected_type;
        }

        if(has_val) {
            Value check_val = ResultValue(determined_inst);
            // untyped 字面量自身不触发 is_val_overflow，先按目标类型模拟检查范围
            check_val.set_type(expected_type);
            if(is_val_overflow(check_val)) {
                context()->reporter.report_error(inst(pc_ref)->src_loc, "类型确定时值溢出（目标类型 '{}'）", expected_type->name());
                return make_result(pc_ref, CIRInstResult::make_error());
            }
        }
    }

    return r;
}

// handler: ConstDecl（写自身结果；符号缓存副作用保留）
std::optional<AnalyzeResult> Interpreter::analyze_ConstDecl(CIRConstDeclInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) {
        return std::nullopt;
    }

    SymbolInfo &sym = info.symbol.unwrap();

    CIRInstructionRef value_inst = info.value_inst;

    if(sym.state == SymbolState::Solved) {
        // 已求解过：值已缓存
        return std::nullopt;
    }

    sym.state = SymbolState::Solving;   // 副作用：符号状态 → Solving（求解防重入）

    // 前置：正常顺序下值块已由前面的 BlockRef 指令分析（immediate_eval=true → FullEval），这里只读。
    // 但前向引用/乱序进入（如函数体在声明前引用此 const）时值块尚未分析，需按需进入。
    if(!has_result_val(value_inst)) {
        // 副作用：按需进入值块分析（push/pop 已封装，越过 dispatch 直接求值依赖）
        analyze_instruction_at(value_inst);
    }
    // 注：value_inst 错误不能交给 dispatch 短路（前向引用时短路检查时尚未分析），这里显式检查
    if(has_error(info.value_inst)) {
        return make_result(pc_ref, CIRInstResult::make_error());
    }

    if(!has_result_val(info.value_inst)) {
        if(has_result_type(info.value_inst)) {
            // 副作用：符号绑定到本指令结果 + Solved（仅类型，无值）
            sym.val(Ref<CIRInstResult>::make(pkg, pc_ref, {}));
            sym.state = SymbolState::Solved;
            return make_result(pc_ref, CIRInstResult::make_type_only(ResultType(info.value_inst)));
        }
        return std::nullopt;
    }

    Value result = ResultValue(info.value_inst);
    result = clone_value(result, permanent_allocator());
    // 副作用：符号绑定到本指令结果 + Solved（值已缓存）
    sym.val(Ref<CIRInstResult>::make(pkg, pc_ref, {}));
    sym.state = SymbolState::Solved;

    return AnalyzeResult{CIRInstResult::make_value(result.type, result), pc_ref};
}

// handler: VariableDecl（写自身类型 = undefined + LValue；符号注册副作用保留）
std::optional<AnalyzeResult> Interpreter::analyze_VariableDecl(CIRVariableDeclInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    auto& vd = inst(pc_ref)->info<CIROperator::VariableDecl>();
    SymbolInfo &sym = vd.symbol.unwrap();

    // 副作用：符号绑定到本指令结果 + Solved（类型由 TypeAscribe 后续补全）
    sym.val(Ref<CIRInstResult>::make(pkg, pc_ref, {}));
    sym.state = SymbolState::Solved;

    // FullEval 时不分配内存，TypeAscribe 会在类型已知后分配
    return make_result(pc_ref, CIRInstResult::make_type_only(undefined_type(), CIRValueKind::LValue));
}

// handler: EnumDeclInit（写自身 type/value；枚举字段注册副作用保留）
std::optional<AnalyzeResult> Interpreter::analyze_EnumDeclInit(CIREnumDeclInitInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {

    if(!should_eval_for_lazy_eval({info.tag_type_inst})) {
        return make_result({{pc_ref, CIRInstResult::make_type_only(type_type())}});
    }

    TypeRef elem_type = ResultValue(info.tag_type_inst).type_val();
    if(!is_integer_type(elem_type)) {
        return make_result(pc_ref, inst_error(pc_ref, "枚举的底层类型必须是整数类型，实际类型 '{}'", elem_type->name()));
    }

    std::optional<xpString> enum_name = std::nullopt;
    SymbolInfo *enum_sym = try_access_val(info.symbol);
    if(enum_sym != nullptr) {
        enum_name = enum_sym->name;
    }

    TypeRef enum_type = enum_type_impl(info.decl_ast, enum_name, elem_type, info.scope);

    TypeRef meta = type_type();
    Value v = make_value(meta);
    v.type_val(enum_type);

    {
        // 副作用：枚举符号绑定 type 值 + Solved
        SymbolInfo *enum_sym2 = try_access_val(info.symbol);
        if(enum_sym2 != nullptr) {
            enum_sym2->val(v);
            enum_sym2->state = SymbolState::Solved;
        }
    }

    // 注册枚举字段到 enum_scope
    Ref<Scope> enum_scope = enum_type->enum_info.enum_scope;
    i128 next_auto_value = 0;
    for(isize i = 0; i < info.fields.count; i++) {
        auto& ef = info.fields[i];

        Value field_val;
        if(ef.value_inst != INVALID_INST) {
            field_val = ResultValue(ef.value_inst);

            XP_ASSERT_DEFAULT(field_val.type == elem_type);
            next_auto_value = field_val.integer_val() + 1;
            field_val.set_type(enum_type);
        } else {
            field_val = make_value(elem_type);
            field_val.integer_val(next_auto_value);

            if(check_integer_overflow(field_val.integer_val(), elem_type)) {
                return make_result(pc_ref, inst_error(pc_ref, "枚举判别值溢出（底层类型 '{}'）", elem_type->name()));
            }

            next_auto_value += 1;
        }
        field_val.set_type(enum_type);

        // 副作用：注册枚举字段符号到 enum_scope + Solved
        SymbolInfo *field_sym = find_symbol_curr(&enum_scope.unwrap(), ef.name);
        XP_ASSERT_DEFAULT(field_sym != nullptr);
        if(ef.value_inst != INVALID_INST) {
            field_sym->val(Ref<CIRInstResult>::make(pkg, ef.value_inst, {}));
        } else {
            field_sym->val(field_val);
        }
        field_sym->state = SymbolState::Solved;
    }

    return make_result({{pc_ref, CIRInstResult::make_type_only(type_type())}, {pc_ref, CIRInstResult::make_value(v)}});
}

// handler: FinishStruct（写自身 type/value；填字段副作用保留）
std::optional<AnalyzeResult> Interpreter::analyze_FinishStruct(CIRFinishStructInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    auto struct_decl_inst = info.struct_decl_inst;

    // 有具体字段值时，完成结构体类型（填字段）
    if(curr_eval_mode() == EvalMode::FullEval || (should_eval_for_lazy_eval({struct_decl_inst}) && should_eval_for_lazy_eval(info.field_insts))) {

        TypeRef st = ResultValue(struct_decl_inst).type_val();

        bool already_finished = st->struct_info.struct_fields.count > 0;
        if(!already_finished) {
            Array<StructField> fields = make_array<StructField>(type_allocator());
            for(isize i = 0; i < info.field_insts.count; i++) {
                auto field_inst = info.field_insts[i];
                auto& field_info = pkg->inst(field_inst)->info<CIROperator::StructField>();

                TypeRef field_type = ResultValue(field_info.type_block_inst).type_val();

                StructField sf;
                sf.name = field_info.name;
                sf.type = field_type;
                fields.push_back(sf);

                if(type_contains_by_value(sf.type, st)) {
                    return make_result(pc_ref, inst_error(pc_ref, "结构体 '{}' 包含自身（未通过指针间接引用），将导致无限大小", sf.name));
                }
            }

            finish_unfinish_struct_type(st, fields);
        }

        return make_result({{pc_ref, CIRInstResult::make_type_only(type_type())}, {pc_ref, CIRInstResult::make_value(ResultValue(struct_decl_inst))}});
    }

    return make_result({{pc_ref, CIRInstResult::make_type_only(type_type())}});
}







// handler: GetOrInitUnion（写自身 type/value；未完成 union 类型 + 早绑符号）
std::optional<AnalyzeResult> Interpreter::analyze_GetOrInitUnion(CIRGetOrInitUnionInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    std::optional<xpString> union_name = std::nullopt;
    SymbolInfo *union_sym = try_access_val(info.symbol);
    if(union_sym != nullptr) {
        union_name = union_sym->name;
    }

    // 未完成 union 类型（带 resolve 建的共享 scope，字段符号值稍后由 FinishUnion 填充）
    TypeRef st = union_type_impl(info.decl_ast, union_name, info.scope);

    // 保存创建时的调用实例：泛型逐实例解析字段类型用（null = 包级走全局）
    st->union_info.creation_instance = result_context().call_instance();

    // 副作用：未完成类型创建后立即绑定符号（自引用字段 *U 不再误判循环依赖，镜像函数签名确定即绑定）
    if(union_sym != nullptr && union_sym->state != SymbolState::Solved) {
        union_sym->val(Ref<CIRInstResult>::make(pkg, pc_ref, result_context().call_instance()));
        union_sym->state = SymbolState::Solved;
    }

    TypeRef tt = type_type();
    Value v = make_value(tt);
    v.type_val(st);

    if(!has_result_type(pc_ref)) {
        return make_result({{pc_ref, CIRInstResult::make_type_only(type_type())}, {pc_ref, CIRInstResult::make_value(v)}});
    }
    return make_result({{pc_ref, CIRInstResult::make_value(v)}});
}




// handler: FinishUnion（写自身 type/value；填 union_scope 字段符号副作用保留）
std::optional<AnalyzeResult> Interpreter::analyze_FinishUnion(CIRFinishUnionInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    auto union_decl_inst = info.union_decl_inst;

    if(curr_eval_mode() == EvalMode::FullEval || (should_eval_for_lazy_eval({union_decl_inst}) && should_eval_for_lazy_eval(info.field_insts))) {
        TypeRef ut = ResultValue(union_decl_inst).type_val();
        Ref<Scope> union_scope = ut->union_info.union_scope;

        // 字段符号已由 FinishUnion 填充 → 跳过（重入/缓存命中）
        bool already_finished = false;
        for (const auto& entry : *union_scope) {
            const SymbolInfo& sym = entry.value;
            if(sym.value_store_type != ValueStoreType::Nothing) {
                already_finished = true;
                break;
            }
        }

        if(!already_finished) {
            for(isize i = 0; i < info.field_insts.count; i++) {
                auto field_inst = info.field_insts[i];
                auto& field_info = pkg->inst(field_inst)->info<CIROperator::StructField>();

                TypeRef field_type = ResultValue(field_info.type_block_inst).type_val();

                if(type_contains_by_value(field_type, ut)) {
                    return make_result(pc_ref, inst_error(pc_ref, "联合体 '{}' 包含自身（未通过指针间接引用），将导致无限大小", field_info.name));
                }

                SymbolInfo *field_sym = find_symbol_curr(&union_scope.unwrap(), field_info.name);
                XP_ASSERT_DEFAULT(field_sym != nullptr);

                // 绑定 InCIRInstruction（镜像 enum）：字段类型由 result(type 的 creation_key) 解析
                field_sym->val(Ref<CIRInstResult>::make(pkg, field_info.type_block_inst, {}));
                field_sym->state = SymbolState::Solved;
            }
        }

        return make_result({{pc_ref, CIRInstResult::make_type_only(type_type())}, {pc_ref, CIRInstResult::make_value(ResultValue(union_decl_inst))}});
    }

    return make_result({{pc_ref, CIRInstResult::make_type_only(type_type())}});
}

// handler: GetOrInitStruct（写自身 type/value）
std::optional<AnalyzeResult> Interpreter::analyze_GetOrInitStruct(CIRGetOrInitStructInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    auto res = eval_GetOrInitStruct(pc_ref);

    // 副作用：未完成类型创建后立即绑定符号（自引用字段 *S 不再误判循环依赖，镜像函数签名确定即绑定）
    SymbolInfo *sym = try_access_val(info.symbol);
    if(sym != nullptr && sym->state != SymbolState::Solved) {
        sym->val(Ref<CIRInstResult>::make(pkg, pc_ref, result_context().call_instance()));
        sym->state = SymbolState::Solved;
    }

    if(!has_result_type(pc_ref)) {
        // TODO: type_type(undefined_type())得换成更规范的表示
        return make_result({{pc_ref, CIRInstResult::make_type_only(type_type())}, {pc_ref, CIRInstResult::make_value(res)}});
    }
    return make_result({{pc_ref, CIRInstResult::make_value(res)}});
}

// handler: BlockRef（进入子 Block 分析；pc 由 new_analyze_flow/recover 自管，dispatch 末尾 advance 越过）
std::optional<AnalyzeResult> Interpreter::analyze_BlockRef(CIRBlockRefInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    CIRBlockRef blk = inst(pc_ref)->info<CIROperator::BlockRef>().block_ref;
    if(pkg->block(blk)->is_loop) {
        analyze_loop(blk, pc_ref);   // 循环不需要 force_eval_mode
    } else {
        // 副作用：进入子 Block 分析（内部 new_analyze_flow 压栈，可能 push FullEval for immediate_eval）
        analyze_block(blk, pc_ref, params.block_eval_mode);
    }
    return std::nullopt;
}

// handler: CondBr（进入分支 Block 分析；pc 由 new_analyze_flow/recover 自管）
std::optional<AnalyzeResult> Interpreter::analyze_CondBr(CIRCondBrInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    XP_ASSERT_DEFAULT(inst(pc_ref)->op == CIROperator::CondBr);

    auto& if_info = inst(pc_ref)->info<CIROperator::CondBr>();
    auto cond_inst = if_info.condition_inst;
    CIRBlockRef true_blk = if_info.true_block;
    CIRBlockRef false_blk = if_info.false_block;

    XP_ASSERT_DEFAULT(cond_inst != INVALID_INST);

    TypeRef cond_type = ResultType(cond_inst);
    if(cond_type != easy_type(Type_bool)) {
        return make_result(pc_ref, inst_error(pc_ref, "if 语句的条件表达式必须是布尔类型，实际类型 '{}'", cond_type->name()));
    }

    if(curr_eval_mode() == EvalMode::FullEval) {
        // 执行模式：只分析选中的分支（另一分支 codegen 仍需要，交由 OnlyType 路径补齐）
        Value cond_val = ResultValue(cond_inst);
        bool cond = cond_val.bool_val();

        CIRBlockRef chosen_blk = cond ? true_blk : false_blk;
        if(chosen_blk != INVALID_BLOCK) {
            analyze_block(chosen_blk, pc_ref, params.block_eval_mode);
        }
    } else {
        // 类型模式：短路 &&/||（is_short_circuit）且 cond 已知时，只分析活分支——
        // 死分支（右操作数）整体跳过：常量错误不报，也不产出任何结果
        // （codegen 端同样跳过该分支，见 CondBr 生成逻辑）。
        // cond 未知（左操作数为运行时值）时两分支都分析，照常求值定型。
        if(if_info.is_short_circuit && has_result_val(cond_inst)) {
            CIRBlockRef live_blk = ResultValue(cond_inst).bool_val() ? true_blk : false_blk;
            if(live_blk != INVALID_BLOCK) {
                analyze_block(live_blk, pc_ref, EvalMode::TypeOnly);
            }
        } else {
            analyze_block(true_blk, pc_ref, EvalMode::TypeOnly);
            if(false_blk != INVALID_BLOCK) {
                analyze_block(false_blk, pc_ref, EvalMode::TypeOnly);
            }
        }
    }
    return std::nullopt;
}

// handler: FunctionDecl（写自身函数类型值；body 跳转用 next_pc 表达）
std::optional<AnalyzeResult> Interpreter::analyze_FunctionDecl(CIRFunctionDeclInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    auto& func = inst(pc_ref)->info<CIROperator::FunctionDecl>();

    // 解析参数类型
    Array<TypeRef> param_types = make_array<TypeRef>(permanent_allocator());
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
        if(!func.is_comptime && func.arg_type_insts[i] != INVALID_INST) {
            if(is_type_type(ResultValue(func.arg_type_insts[i]).type_val())) {
                return make_result(pc_ref, inst_error(func.arg_decl_insts[i],
                    "类型 'type' 的参数要求函数声明为 comptime（用 '$()' 包裹参数列表）"));
            }
        }
    }

    TypeRef return_type = undefined_type();
    if(func.return_type_inst != INVALID_INST) {
        if(has_result_val(func.return_type_inst)) {
            return_type = ResultValue(func.return_type_inst).type_val();
        }
    }

    AnalyzeResult r;   // 自身函数类型值 + 可选 body 跳转

    {
        TypeRef func_type_type = function_type(param_types, return_type);
        Value v = make_value(func_type_type);

        SymbolInfo* sym = try_access_val(inst(pc_ref)->symbol);

        {
            auto func_key = Ref<CIRInstResult>::make(pkg, pc_ref, result_context().call_instance());

            if (func.is_builtin && sym != nullptr && sym->name == xp_string_c("sizeof")) {
                v.func_val(func_key, BuiltinKind::SizeOf);
            } else {
                v.func_val(func_key);
            }
        }

        // DEBUG_TRACE("to set type and result for func {}", sym ? sym->name : xp_string_c("<anon>"));

        r.writes.push_back({pc_ref, CIRInstResult::make_value(v)});
        // 副作用：函数值同步写进 pkg->results（递归引用需在 body 分析前可查）
        pkg->result_of(pc_ref).set_val(clone_value(v, permanent_allocator()));
        if(sym != nullptr) {
            // 副作用：函数符号签名确定即绑定 FunctionDecl 并置 Solved（递归引用不误判循环依赖）
            sym->val(Ref<CIRInstResult>::make(pkg, pc_ref, {}));
            sym->state = SymbolState::Solved;
        }
    }

    if(func.is_extern_c || func.is_builtin) {
        // extern "C" / #builtin 没有函数体，不分析了
    } else {
        ASSERT_MSG(func.body_inst != INVALID_INST, "non-extern function must have body");


        if(is_pure_comptime_func(func, result_context())) {
            // 纯编译期函数：跳过 body（body BlockRef 紧跟 FunctionDecl）
            // old 代码 curr_inst() = next() 后再由外层 advance() 越过 → next_pc 需要补一格
            r.next_pc = pc_ref.next(2);
        } else {
            // 普通运行时函数：TypeOnly body — 外层 const 块可能是 FullEval，需强制 TypeOnly 隔离
            CIRInstructionRef func_pc = pc_ref;
            // 副作用：TypeOnly 分析函数体（new_analyze_flow 进入 body BlockRef，绕过外层 eval mode）
            new_analyze_flow(func.body_inst);
            analyze_instruction({}, {.block_eval_mode = EvalMode::TypeOnly});
            recover_analyze_flow();
            // 跳过紧随的 body BlockRef，避免外层迭代二次分析（同上补一格）
            r.next_pc = func_pc.next(2);
        }
    }

    return r;
}

// handler: Call（写自身返回值；编译期调用 push/pop instance 副作用保留）
std::optional<AnalyzeResult> Interpreter::analyze_Call(CIRCallInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    auto& call_info = inst(pc_ref)->info<CIROperator::Call>();
    CIRInstructionRef called_inst = call_info.called_thing;

    TypeRef called_type = ResultType(called_inst);

    if(is_pointer_type(called_type) && is_function_type(called_type->pointed_type)) {
        called_type = called_type->pointed_type;
    }

    if(!is_function_type(called_type)) {
        return make_result(pc_ref, inst_error(pc_ref, "调用了非函数值（实际类型 '{}'）", called_type->name()));
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
            return make_result(pc_ref, inst_error(pc_ref, "实参过少：至少需要 {} 个，实际 {} 个", non_var_arg_count, arg_count));
        }
    } else {
        if(arg_count != param_types.count) {
            return make_result(pc_ref, inst_error(pc_ref, "实参数量不匹配：期望 {} 个，实际 {} 个", param_types.count, arg_count));
        }
    }

    AnalyzeResult r;   // 自身返回值
    TypeRef return_type = called_type->function_info.return_type;
    r.writes.push_back({pc_ref, CIRInstResult::make_type_only(return_type)});

    // TODO: 递归类型（自引用指针 struct，如链表 next: *Node）编译期求值会死循环——
    //       实例化 Node(T) 构造 struct 时，next 字段再次求值 Node(T)，泛型实例无"构建中"复用，
    //       无限递归（与 eval mode 无关）。彻底方案：泛型实例构建中复用。

    bool force_eval = false;

    // 如果调用的是纯编译期函数, 则强制 FullEval
    if(has_result_val(called_inst)) {
        Value called_val = ResultValue(called_inst);
        auto fv = called_val.func_val();
        auto func_info = fv.func_key.inst()->info<CIROperator::FunctionDecl>();

        auto ctx = CIRResultContext::create(fv.func_key.cir_package);
        if(fv.func_key.result_instance != Ref<CIRResultInstance>::INVALID_REF) {
            ctx.enter_call_instance(fv.func_key.result_instance);
        }
        if(is_pure_comptime_func(func_info, ctx)) {
            force_eval = true;
        }
    }

    if(force_eval) {
        // 副作用：压入 FullEval（纯编译期函数调用强制求值）
        eval_mode_stack.push_back(EvalMode::FullEval);
    }

    // ── 编译期执行（FullEval only）────────────────────────────
    if(curr_eval_mode() == EvalMode::FullEval) {
        // 编译期调用是 C++ 递归，Debug 下 ~63 层即爆 1MB 栈，阈值需远小于此
        constexpr auto MAX_CALL_DEPTH = 15;
        if(instance_stack.count > MAX_CALL_DEPTH) {
            r.writes.push_back({pc_ref, inst_error(pc_ref, "循环依赖或递归过深（最大调用深度 {}）", MAX_CALL_DEPTH)});
            goto end;
        }

        ASSERT(has_result_val(called_inst));

        Value called_val = ResultValue(called_inst);
        FuncValue fv = called_val.func_val();
        CIRPackage *callee_pkg = fv.func_key.cir_package;

        CIRInstructionRef func_decl_pc = fv.func_key.inst_ref;
        CIRFunctionDeclInfo& func = callee_pkg->inst(func_decl_pc)->info<CIROperator::FunctionDecl>();

        if(fv.builtin_kind != BuiltinKind::None) {
            switch(fv.builtin_kind) {
                case BuiltinKind::None: break;
                case BuiltinKind::SizeOf: {
                    Value arg_val = ResultValue(call_info.arg_insts[0]);
                    TypeRef target_type = arg_val.type_val();
                    isize size = type_size_of(target_type);
                    Value result = make_value(easy_type(Type_usize));
                    result.integer_val(size);
                    r.writes.push_back({pc_ref, CIRInstResult::make_value(result)});
                } goto end;
            }
        }

        isize var_count = func.slot_count;
        isize func_arg_count = func.arg_decl_insts.count;

        if(!has_result_val(call_info.arg_insts)) {
            r.writes.push_back({pc_ref, inst_error(pc_ref, "编译期无法调用实参不是常量的函数")});
            goto end;
        }

        // 构建编译期函数调用结果查询键
        FuncCallKey cache_key = {
            .func_decl_pc = func_decl_pc,
            .comptime_arg_refs = make_array<Ref<CIRInstResult>>(permanent_allocator())
        };
        Ref<CIRResultInstance> parent_instance;
        if(auto* inst_ptr = curr_instance()) { parent_instance = inst_ptr->ctx.call_instance(); }
        for(isize i = 0; i < call_info.arg_insts.count; i++) {
            cache_key.comptime_arg_refs.push_back(Ref<CIRInstResult>::make(pkg, call_info.arg_insts[i], parent_instance));
        }

        // 查询编译期函数调用结果缓存
        Ref<CIRResultInstance> callee_result_instance = callee_pkg->get_result_instance(cache_key);
        {
            CIRInstResult *cached_body = callee_result_instance->result_ptr_of(func.body_inst);
            if(cached_body && cached_body->state == CIRResultState::WholeValue) {
                r.writes.push_back({pc_ref, CIRInstResult::make_value(cached_body->actual_val())});
                goto end;
            }
        }

        if(func.is_extern_c) {
            r.writes.push_back({pc_ref, inst_error(pc_ref, "编译期无法调用 extern \"C\" 函数")});
            goto end;
        }

        auto eval_inst = EvalInstance::make(callee_pkg, var_count, permanent_allocator());
        eval_inst.frame_base = stack_mem.bytes.count;
        eval_inst.ctx.enter_call(cache_key);

        for(isize i = 0; i < func_arg_count; i++) {
            TypeRef arg_type = ResultType(call_info.arg_insts[i]);
            isize size  = type_serialize_size(arg_type);
            isize align = type_serialize_align(arg_type);

            // 副作用：参数内存分配 + 写值（stack_mem 帧内）
            auto ptr = stack_mem.alloc_bytes(size, align);
            ptr.store(ResultValue(call_info.arg_insts[i]));

            eval_inst.var_ptrs[i] = ptr;
        }

        // 副作用：压入编译期调用实例（保存 caller pkg/scope/pc，切到 callee）
        push_eval_instance(std::move(eval_inst));
        pkg = callee_pkg;
        // callee 初始 scope 已由 push_eval_instance 压入（nullptr，body 的 EnterScope 会覆盖）

        // 副作用：分析 callee 函数体（dispatch 循环，结果写 callee 的 result instance）
        analyze_instruction_at(func.body_inst);

        bool body_has_error = has_error(func.body_inst);

        bool has_return = has_result_val(func.body_inst);
        Value return_val = {};
        if(has_return){
            return_val = ResultValue(func.body_inst);
        }

        // 副作用：弹出编译期调用实例（恢复 caller pkg/scope/pc + 回收 stack_mem 帧）
        pop_eval_instance();

        if(body_has_error) {
            r.writes.push_back({pc_ref, CIRInstResult::make_error()});
        }

        if(has_return) {
            return_val = clone_value(return_val, permanent_allocator());
            // 副作用：返回结果写入 callee 结果实例缓存（供后续同 key 调用命中）
            callee_result_instance->result_of(func.body_inst).set_val(return_val);
            r.writes.push_back({pc_ref, CIRInstResult::make_value(return_val)});
        }
    }

end:

    if(force_eval) {
        // 副作用：恢复 eval mode
        eval_mode_stack.pop_back();
    }

    return r;
}

std::optional<AnalyzeResult> Interpreter::analyze_EnterScope(CIREnterScopeInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    Ref<Scope> scope = info.scope;
    set_scope(scope);
    return std::nullopt;
}

std::optional<AnalyzeResult> Interpreter::analyze_ExitScope(CIRExitScopeInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    Ref<Scope> scope = info.scope;
    set_scope(scope);
    return std::nullopt;
}

// handler: IdentRef（写自身 LValue 类型/地址；符号按需分析副作用保留）
std::optional<AnalyzeResult> Interpreter::analyze_IdentRef(CIRIdentRefInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    SymbolInfo *sym = try_access_val(inst(pc_ref)->symbol);

    if(sym == nullptr) {
        return make_result(pc_ref, inst_error(pc_ref, "未定义标识符 '{}'", inst(pc_ref)->info<CIROperator::IdentRef>().ident));
    }

    if(!sym->is_var_decl() && !sym->is_const_decl_and_func()) {
        return make_result(pc_ref, inst_error(pc_ref, "标识符 '{}' 不是可寻址实体", sym->name));
    }

    if(sym->state == SymbolState::Unsolved) {
        // 临时把当前实例上下文切到符号所属包的全局上下文，结果落全局，不污染触发实例

        CIRResultContext saved_ctx = instance_stack.back().ctx;
        instance_stack.back().ctx = CIRResultContext::create(sym->inst_key.cir_package);
        defer(instance_stack.back().ctx = saved_ctx);

        new_analyze_flow(sym->val_as_inst_key().inst_ref);
        defer(recover_analyze_flow());

        analyze_instruction();
    } else if(sym->state == SymbolState::Solving) {
        return make_result(pc_ref, inst_error(pc_ref, "求值 '{}' 时检测到循环依赖", sym->name));
    }

    // 注：val 绑定指令不是本 op 的 refs（符号经 symbol 字段），dispatch 短路不覆盖，这里显式传播
    if(sym->value_store_type == ValueStoreType::InCIRInstruction) {
        if(propagate_error({sym->val_as_inst_key().inst_ref})) {
            return std::nullopt;
        }
    }

    auto r = sym->result(curr_cache_key());
    if(r.state == CIRResultState::Error || r.state == CIRResultState::NothingYet) {
        return make_result(pc_ref, inst_error(pc_ref, "标识符 '{}' 无可用结果（符号未求值或出错）", sym->name));
    }
    if(has_instance() && (curr_eval_mode() == EvalMode::FullEval || r.state == CIRResultState::WholeValue)) {
        if(sym->is_var_decl()) {
            Ref<CIRInstResult> var_key = sym->val_as_inst_key();
            CIRVariableDeclInfo& vd = var_key.cir_package->inst(var_key.inst_ref)->info<CIROperator::VariableDecl>();
            auto ptr = curr_instance()->var_ptrs[vd.slot];
            ASSERT(!ptr.is_null());

            Value addr = make_value(pointer_type(r.type()));
            addr.pointer_val(ptr);
            // type = 变量逻辑类型，actual = 地址（*r.type()）
            return AnalyzeResult{CIRInstResult::make_value(r.type(), addr, CIRValueKind::LValue), pc_ref};
        } else {
            if(r.state == CIRResultState::WholeValue) {
                return AnalyzeResult{CIRInstResult::make_value(r.actual_val().type, r.actual_val(), CIRValueKind::LValue), pc_ref};
            }
        }
    } else if(sym->is_const_decl_and_func() && has_result_val(sym->val_as_inst_key().inst_ref)) {
        Value const_val = ResultValue(sym->val_as_inst_key().inst_ref);
        return AnalyzeResult{CIRInstResult::make_value(const_val.type, const_val, CIRValueKind::LValue), pc_ref};
    }

    // 仅类型：恒为 LValue，actual = 指向逻辑类型的指针（codegen 据此分配/取地址）
    CIRInstResult res = CIRInstResult::make_type_only(r.type(), CIRValueKind::LValue);
    res.set_actual_type(pointer_type(r.type()));
    return AnalyzeResult{res, pc_ref};
}

// handler: IdentVal（写自身 type/value）
std::optional<AnalyzeResult> Interpreter::analyze_IdentVal(CIRIdentValInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) {
        return std::nullopt;
    }

    Ref<SymbolInfo> info_ref = inst(pc_ref)->symbol;
    auto sym = try_access_val(info_ref);

    if(sym == nullptr) {
        return make_result(pc_ref, inst_error(pc_ref, "未定义标识符 '{}'", inst(pc_ref)->info<CIROperator::IdentVal>().ident));
    }

    if(sym->is_var_decl()) {
        return make_result(pc_ref, inst_error(pc_ref, "IdentVal 只应作用于常量，'{}' 是变量", sym->name));
    }

    // 以支持顶层constDecl的顺序无关声明
    if(sym->state == SymbolState::Unsolved) {

        // 临时把当前实例上下文切到符号所属包的全局上下文，结果落全局，不污染触发实例
        CIRResultContext saved_ctx = instance_stack.back().ctx;
        instance_stack.back().ctx = CIRResultContext::create(sym->inst_key.cir_package);
        defer(instance_stack.back().ctx = saved_ctx);

        new_analyze_flow(sym->val_as_inst_key().inst_ref);
        defer(recover_analyze_flow());

        analyze_instruction();
    } else if(sym->state == SymbolState::Solving) {
        return make_result(pc_ref, inst_error(pc_ref, "求值 '{}' 时检测到循环依赖", sym->name));
    }

    auto r = sym->result(curr_cache_key());
    if(r.state == CIRResultState::Error || r.state == CIRResultState::NothingYet) {
        return make_result(pc_ref, inst_error(pc_ref, "标识符 '{}' 无可用结果（符号未求值或出错）", sym->name));
    }
    if(r.state == CIRResultState::WholeValue) {
        return AnalyzeResult{CIRInstResult::make_value(r.actual_val()), pc_ref};
    }
    return make_result(pc_ref, CIRInstResult::make_type_only(r.type()));
}



std::optional<AnalyzeResult> Interpreter::analyze_ImportPackage(CIRImportPackageInfo& info, CIRInstructionRef pc_ref, const AnalyzeParams& params) {
    if(has_result_val(pc_ref)) {
        return std::nullopt;
    }

    // import 是编译期求值：compile_package_from_import 解析路径→查表/现场构建（循环 import 在查表时检测报错）
    xpString cycle_err = xp_string_c("");
    auto pkg_opt = compile_package_from_import(info.path, &cycle_err);
    if(pkg_opt.is_none()) {
        if(cycle_err.length > 0) {
            return make_result(pc_ref, inst_error(pc_ref, "{}", cycle_err));
        }
        return make_result(pc_ref, inst_error(pc_ref, "找不到包 '{}'", info.path));
    }

    Ref<Package> pkg_ref{pkg_opt.unwrap().index};
    Value v = make_value(package_type(pkg_ref));
    v.package_val(pkg_ref);
    return make_result(pc_ref, CIRInstResult::make_value(v.type, v));
}
