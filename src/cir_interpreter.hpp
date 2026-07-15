#pragma once

#include <optional>

#include "array.hpp"
#include "value.hpp"
#include "cir_builder.hpp"

struct CIRPackage;

void analyze_package(Package *pkg);


enum class EvalMode {
    FullEval,
    TypeOnly,
};

struct AnalyzeParams {
    std::optional<EvalMode> block_eval_mode = std::nullopt;
};




// 求值实例 — 每次编译期函数调用创建一个
struct EvalInstance {

    static EvalInstance make(CIRPackage *callee_pkg, CIRInstructionRef func_decl_pc, isize var_count, xpAllocator allocator);

    static void free(EvalInstance *inst);


    CIRPackage *callee_pkg = nullptr;  // 被调函数所在包
    CIRInstructionRef func_decl_pc = 0;
    CIRResultInstance *result_instance = nullptr; // 被调函数的结果实例
    Array<Pointer> var_ptrs;        // 变量存储位置指针，按 func_decl 的参数顺序排列

    FuncCallKey cache_key = {};

    isize frame_base = 0; // 栈帧基址（stack_mem 中的偏移）
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


    Value eval_GetOrInitStruct(CIRInstructionRef ref);

    CIRInstResult& result_for(CIRInstructionRef ref);

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

    bool has_instance() const { return instance_stack.count > 0; }

    std::optional<FuncCallKey> curr_cache_key() const {
        if(instance_stack.count > 0) {
            return instance_stack.back().cache_key;
        }
        return std::nullopt;
    }

    bool is_pure_comptime_func(CIRFunction& func);

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
        return instance_stack.count > 0 ? &instance_stack.back() : nullptr;
    }

    // 嵌套调用栈
    Array<EvalInstance> instance_stack;

    // 当前求值实例（null = 使用包级 store）

    Array<AnalyzeFlowState> pc_stack;

    Array<EvalMode> eval_mode_stack;
    Array<CIRInstructionRef> loop_stack;  // 当前嵌套的 Loop 指令栈

    ValueMemory stack_mem;
};
