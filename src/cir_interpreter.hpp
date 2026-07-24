#pragma once

#include <optional>

#include "array.hpp"
#include "value.hpp"
#include "cir_builder.hpp"

struct CIRPackage;

void analyze_package(Package *pkg);

using AnalyzeResult = std::tuple<CIRInstResult, CIRInstructionRef>;

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
    CIRInstructionRef caller_pc = 0;
    Scope *saved_scope = nullptr;

    CIRResultContext result_context() const { return ctx; }
};



struct Interpreter {
    Interpreter(xpAllocator allocator);
    ~Interpreter();

    void analyze_cir_package(CIRPackage* cir_package);

    void analyze_instruction(std::optional<CIROperator> expected_op = std::nullopt, AnalyzeParams params = {});

    void set_scope(Scope *scope);

    void analyze_ConstDecl();
    void analyze_FunctionDecl();
    void analyze_GetOrInitStruct();
    void analyze_StructField();
    void analyze_FinishStruct();
    void analyze_EnumDeclInit();
    void analyze_UnionDecl();
    void analyze_VariableDecl();
    void analyze_Binary();
    void analyze_Unary();
    void analyze_FieldAccess();
    void analyze_FieldPtr();
    void analyze_Call();
    void analyze_ConstantValue();
    void analyze_Cast();
    void analyze_StructInit();
    void analyze_ArrayInit();
    void analyze_Index();
    void analyze_IndexPtr();
    void analyze_AddrOf();
    void analyze_PointerType();
    void analyze_ArrayType();
    void analyze_SliceType();
    void analyze_FuncType();
    void analyze_Block(std::optional<EvalMode> force_eval_mode = std::nullopt);
    void analyze_Loop();
    void analyze_CondBr();
    void analyze_Break();
    void analyze_Load();
    void analyze_Store();
    void analyze_TypeAscribe();
    void analyze_IdentRef();
    void analyze_IdentVal();
    void analyze_EnterScope();
    void analyze_ExitScope();
    void analyze_DetermineType();
    void analyze_TypeOfInstResult();
    void analyze_FieldTypeOfStruct();
    void analyze_FuncParamType();
    void analyze_Deref();


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

    bool should_eval(std::initializer_list<CIRInstructionRef> refs = {});
    bool should_eval_for_lazy_eval(std::initializer_list<CIRInstructionRef> refs);
    bool should_eval_for_lazy_eval(Array<CIRInstructionRef>& refs);
    bool is_lvalue(CIRInstructionRef ref);
    void set_lvalue(CIRInstructionRef ref);

    void new_pc_flow(CIRInstructionRef new_pc);
    void recover_pc_flow();

    EvalMode curr_eval_mode() const;

    bool has_instance() const { return instance_stack.count > 1; }

    std::optional<FuncCallKey> curr_cache_key() const {
        if(instance_stack.count > 1) {
            return instance_stack.back().ctx.call_key();
        }
        return std::nullopt;
    }

    CIRResultContext result_context() const {
        return instance_stack.back().result_context();
    }

    void push_call_instance(EvalInstance inst);
    void pop_call_instance();

    bool is_generic_func(CIRPackage *fpkg, CIRFunction& func);

    // === 函数式重构 — 新基础设施 (WIP) ===

    void apply_result(CIRInstructionRef target, const CIRInstResult& result);

    // 新 handler 原型（接受 inst + pc_ref，不依赖 pc() 可变状态）
    // 返回 AnalyzeResult = (result, target) → dispatch 自动 apply + pc++
    // 返回 nullopt → handler 自管结果和 pc（旧代码保持）
    std::optional<AnalyzeResult> handler_ConstantValue(CIRInstruction* inst, CIRInstructionRef pc_ref);
    std::optional<AnalyzeResult> handler_PointerType(CIRInstruction* inst, CIRInstructionRef pc_ref);
    std::optional<AnalyzeResult> handler_Load(CIRInstruction* inst, CIRInstructionRef pc_ref);
    std::optional<AnalyzeResult> handler_Deref(CIRInstruction* inst, CIRInstructionRef pc_ref);

public:

    struct AnalyzeFlowState {
        Scope *scope;
        CIRInstructionRef pc;
    };

    CIRPackage *pkg;

    AnalyzeFlowState& curr_state();
    CIRInstructionRef& pc();
    Scope*&             scope();
    EvalInstance* curr_instance() {
        return instance_stack.count > 1 ? &instance_stack.back() : nullptr;
    }

    // 嵌套调用栈
    Array<EvalInstance> instance_stack;

    // 当前求值实例（null = 使用包级 store）

    Array<AnalyzeFlowState> pc_stack;

    Array<EvalMode> eval_mode_stack;
    Array<CIRInstructionRef> loop_stack;  // 当前嵌套的 Loop 指令栈

    ValueMemory stack_mem;
};
