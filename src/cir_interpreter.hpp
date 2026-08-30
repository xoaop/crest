#pragma once

#include <optional>

#include "array.hpp"
#include "value.hpp"
#include "cir_builder.hpp"

//
// 公共接口
//
void analyze_package(Ref<Package> pkg, xpAllocator allocator);





//
//
//

struct CIRPackage;

// 轻量结果描述：handler 产出的临时结果（不持池槽），apply_result 时统一落进目标结果槽
struct ResultDesc {
    CIRResultState state = CIRResultState::NothingYet;
    CIRValueKind value_kind = CIRValueKind::RValue;

    std::optional<TypeRef> implicit_type = std::nullopt;

    static ResultDesc make_value(TypeRef t, Value v, CIRValueKind vk = CIRValueKind::RValue) {
        ResultDesc r;
        r.state = CIRResultState::WholeValue;
        r.outstanding_type = t;
        r.val = v;
        r.value_kind = vk;
        return r;
    }

    static ResultDesc make_value(Value v) {
        ResultDesc r;
        r.state = CIRResultState::WholeValue;
        r.outstanding_type = v.type;
        r.val = v;
        r.value_kind = CIRValueKind::RValue;
        return r;
    }

    static ResultDesc make_type_only(TypeRef t, CIRValueKind vk = CIRValueKind::RValue) {
        ResultDesc r;
        r.state = CIRResultState::OnlyType;
        r.outstanding_type = t;
        r.val.type = t;
        r.value_kind = vk;
        return r;
    }

    static ResultDesc make_error() {
        ResultDesc r;
        r.state = CIRResultState::Error;
        return r;
    }

    TypeRef type() const { return outstanding_type; }
    TypeRef actual_type() const { return val.type; }
    const Value& val_ref() const { return val; }

    void set_type(TypeRef new_type) {
        outstanding_type = new_type;
        val.type = new_type;
        if(state == CIRResultState::NothingYet) {
            state = CIRResultState::OnlyType;
        }
    }
    void set_actual_type(TypeRef new_type) {
        val.type = new_type;
    }
    void set_val(Value new_val) {
        val = new_val;
        if(state == CIRResultState::NothingYet || state == CIRResultState::OnlyType) {
            state = CIRResultState::WholeValue;
        }
    }

private:
    Value val;                     // 临时值（WholeValue 携带；OnlyType 时 .type 作实际类型）
    TypeRef outstanding_type = nullptr;
};

struct ResultWrite {
    CIRInstructionRef ref;
    ResultDesc result;
};

struct AnalyzeResult {
    Array<ResultWrite> writes;
    std::optional<CIRInstructionRef> next_pc = std::nullopt;

    AnalyzeResult();
    AnalyzeResult(ResultDesc result, CIRInstructionRef ref);
};

enum class EvalMode {
    FullEval,
    TypeOnly,
};

struct AnalyzeParams {
    std::optional<EvalMode> block_eval_mode = std::nullopt;
};




// 求值实例 — 每次编译期函数调用创建一个
struct EvalInstance {

    static EvalInstance make(CIRPackage *callee_pkg, isize var_count, xpAllocator allocator);

    static void free(EvalInstance *inst);

    CIRResultContext ctx;
    Array<Pointer> var_ptrs;

    isize frame_base = 0; // 栈帧基址（stack_mem 中的偏移）

    // 用于恢复调用者上下文
    CIRPackage *caller_pkg = nullptr;
    CIRInstructionRef caller_pc = INVALID_INST;   // 返回地址（pc 会被 body 覆盖，必须留帧里）
    Ref<Scope> caller_scope;
};




//
//
//

struct Interpreter {
    Interpreter(xpAllocator allocator);
    ~Interpreter();



    void set_scope(Ref<Scope> scope);

    void analyze_cir_package(Ref<Package> pkg_ref);

    void analyze_instruction(std::optional<CIROperator> expected_op = std::nullopt, AnalyzeParams params = {});
    void analyze_instruction_at(CIRInstructionRef at_ref);


    void analyze_block(CIRBlockRef blk, std::optional<CIRInstructionRef> target, std::optional<EvalMode> force_eval_mode = std::nullopt);  // 进入块 blk 分析；target = 块的 handle（结果位置，可选：无结果位置的块传 nullopt）
    void analyze_loop(CIRBlockRef blk, std::optional<CIRInstructionRef> target);   // 循环块分析（完全独立）：入口 + 迭代 + 编译时检查 + 恢复
    void analyze_block_insts(CIRBlockRef blk, std::optional<CIRInstructionRef> target);   // 迭代分析块内指令（普通块与循环块共用），直到块结果已定（有 target）或到块末尾
    bool analyze_symbol_of_package(Ref<SymbolInfo> sym_ref);

    Value eval_GetOrInitStruct(CIRInstructionRef ref);

    CIRResultState result_state(CIRInstructionRef ref);
    void set_result_state(CIRInstructionRef ref, CIRResultState state);
    TypeRef ResultType(CIRInstructionRef ref);
    Value ResultValue(CIRInstructionRef ref);
    void Set_ResultType(CIRInstructionRef ref, TypeRef type);
    void Set_ResultValue(CIRInstructionRef ref, Value val);
    void Set_ResultTypeAndValue(CIRInstructionRef ref, Value val);
    bool has_result_val(CIRInstructionRef ref);
    bool has_result_val(std::initializer_list<CIRInstructionRef> refs);
    bool has_result_val(Array<CIRInstructionRef>& refs);
    bool has_result_type(CIRInstructionRef ref);
    bool has_error(CIRInstructionRef ref);
    void Set_ResultError(CIRInstructionRef ref);
    bool propagate_error(std::initializer_list<CIRInstructionRef> refs);
    bool propagate_error(Array<CIRInstructionRef>& refs);

    bool should_eval_for_lazy_eval(std::initializer_list<CIRInstructionRef> refs);
    bool should_eval_for_lazy_eval(Array<CIRInstructionRef>& refs);
    bool is_lvalue(CIRInstructionRef ref);
    void set_lvalue(CIRInstructionRef ref);

    void new_analyze_flow(CIRInstructionRef new_pc);
    void recover_analyze_flow();

    EvalMode curr_eval_mode() const;

    bool has_instance() const { return instance_stack.count > 1; }

    std::optional<FuncCallKey> curr_cache_key() const {
        if(instance_stack.count > 1) {
            return instance_stack.back().ctx.call_key();
        }
        return std::nullopt;
    }

    CIRResultContext result_context() const {
        return instance_stack.back().ctx;
    }

    void push_eval_instance(EvalInstance inst);
    void pop_eval_instance();

    CIRInstructionRef& curr_inst_ref();   // 当前指令位置 = inst_stack 栈顶
    EvalInstance* curr_instance() {
        return instance_stack.count > 1 ? &instance_stack.back() : nullptr;
    }


    void apply_result(CIRInstructionRef target, const ResultDesc& result);



#define X(name) std::optional<AnalyzeResult> analyze_##name(CIR##name##Info& info, CIRInstructionRef pc_ref, const AnalyzeParams& params);
    CIR_OPERATORS
#undef X

public:

    Ref<Package> pkg_ref;
    CIRPackage *pkg;

    Ref<Scope> scope;

    Array<EvalInstance> instance_stack; // 嵌套调用栈
    Array<CIRInstructionRef> inst_stack;  // 指令位置栈（块入口/跳转时压入保存，恢复时弹出）
    Array<EvalMode> eval_mode_stack;
    Array<CIRInstructionRef> loop_stack; // 当前嵌套的 Loop 指令栈

    ValueMemory stack_mem;
};
