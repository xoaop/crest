#pragma once

#include <optional>

#include "array.hpp"
#include "value.hpp"
#include "cir_builder.hpp"

//
// 公共接口
//
void analyze_package(PackageRef pkg, xpAllocator allocator);





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

    static ResultDesc from(const CIRInstResult& src) {
        ASSERT(src.state == CIRResultState::WholeValue || src.state == CIRResultState::OnlyType || src.state == CIRResultState::Error);
        
        ResultDesc r;
        r.state = src.state;
        r.value_kind = src.value_kind;
        r.implicit_type = src.implicit_type;
        
        r.outstanding_type = src.type();
        if(src.state == CIRResultState::OnlyType) {
            r.val.type = src.actual_type();
        } else if(src.state == CIRResultState::WholeValue) {
            r.val = src.actual_val();
        }

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

    inline bool has_error() const {
        for(const auto& w: writes) {
            if(w.result.state == CIRResultState::Error) {
                return true;
            }
        }
        return false;
    }
};

enum class EvalMode {
    FullEval,
    TypeOnly,
};

struct AnalyzeParams {
    std::optional<EvalMode> block_eval_mode = std::nullopt;
};




// 求值实例 — 每个 interp 一个，随 interp 生灭
struct EvalInstance {

    static EvalInstance make(CIRPackageRef callee_pkg, isize var_count, xpAllocator allocator);

    static void free(EvalInstance *inst);

    CIRResultContext ctx;
    Array<Pointer> var_ptrs;
};


struct Interpreter;

// 调用帧：纯数据
struct InterpStackFrame {
    // 本帧分析的块
    CIRBlockRef stop_blk = 0;
    std::optional<CIRInstructionRef> stop_target = std::nullopt;
    bool pushed_eval_mode = false;

    // 是否由 InterpStack 驱动；false = 同步 interp（泛型具化、跨包符号）
    bool managed = false;

    // 本帧已让出（子 interp 入栈）
    bool yielded = false;

    // 调用点（无返回值报错的位置）
    CIRInstructionRef call_pc = INVALID_INST;
};

// 解释器栈 —— 编译期调用链。栈顶就是当前执行的 interp：
// 发起编译期调用 = 入栈；自己的块跑完 = 出栈。C++ 栈不随调用深度增长。
struct InterpStack {
    explicit InterpStack(xpAllocator allocator);
    ~InterpStack();

    // 栈深上限：失控递归的兜底（真正能到多深由内存决定）
    static constexpr isize MAX_DEPTH = 10000;

    Array<Interpreter*> interps;

    bool empty() const;
    Interpreter* top() const;
    isize depth() const;

    void push(Interpreter* interp);
    void pop();                              // 出栈；调用结果写回新的栈顶
    void run();                              // 驱动整条栈，直到根结束（根留给调用方销毁）
    void run_until(Interpreter* target);     // 同步 interp：跑到 target 重新成为栈顶
};





//
//
//

struct Interpreter {
    Interpreter(xpAllocator allocator, PackageRef pkg_ref, InterpStack *interp_stack, EvalInstance inst);
    ~Interpreter();

    Interpreter(const Interpreter&) = delete;

    static Interpreter* create(xpAllocator allocator, PackageRef pkg_ref, InterpStack *interp_stack, EvalInstance inst);
    static void destroy(Interpreter *interp);

    // 本 interp 的帧操作
    void start_frame(CIRBlockRef blk, std::optional<CIRInstructionRef> target, std::optional<EvalMode> force_eval_mode = std::nullopt);
    void step();
    void finish_frame();




    void collect_lcir_globals();

    bool analyze_instruction(std::optional<CIROperator> expected_op = std::nullopt, AnalyzeParams params = {});
    bool analyze_instruction_at(CIRInstructionRef at_ref);


    bool analyze_block(CIRBlockRef blk, std::optional<CIRInstructionRef> target, std::optional<EvalMode> force_eval_mode = std::nullopt);  // 进入块 blk 分析；target = 块的 handle（结果位置，可选：无结果位置的块传 nullopt）
    void analyze_loop(CIRBlockRef blk, std::optional<CIRInstructionRef> target);   // 循环块分析（完全独立）：入口 + 迭代 + 编译时检查 + 恢复
    bool analyze_block_insts(CIRBlockRef blk, std::optional<CIRInstructionRef> target);   // 迭代分析块内指令（普通块与循环块共用），直到块结果已定（有 target）或到块末尾
    bool analyze_symbol_of_package(SymbolInfoRef sym_ref);

    // 各内建 hook 的独立处理
    AnalyzeResult hook_typeof(const CIRHookInfo& info, CIRInstructionRef pc_ref);
    AnalyzeResult hook_sizeof(const CIRHookInfo& info, CIRInstructionRef pc_ref);
    AnalyzeResult hook_field_count(const CIRHookInfo& info, CIRInstructionRef pc_ref);
    AnalyzeResult hook_compile_error(const CIRHookInfo& info, CIRInstructionRef pc_ref);

    Value eval_GetOrInitStruct(CIRInstructionRef ref);

    CIRResultState result_state(CIRInstructionRef ref);
    void set_result_state(CIRInstructionRef ref, CIRResultState state);
    
    std::optional<TypeRef> ResultTypeOpt(CIRInstructionRef ref);
    std::optional<Value> ResultValueOpt(CIRInstructionRef ref);

    void Set_ResultType(CIRInstructionRef ref, TypeRef type);
    void Set_ResultValue(CIRInstructionRef ref, Value val);
    void Set_ResultTypeAndValue(CIRInstructionRef ref, Value val);
    
    bool has_result_val(CIRInstructionRef ref);
    bool has_result_val_in_curr_instance_and_pkg(CIRInstructionRef ref);   // 只问本实例算过没有（不穿实例链）
    bool has_result_val(std::initializer_list<CIRInstructionRef> refs);
    bool has_result_val(const Array<CIRInstructionRef>& refs);
    bool has_result_type(CIRInstructionRef ref);

    bool has_error(CIRInstructionRef ref);
    bool has_error(std::initializer_list<CIRInstructionRef> refs);
    void Set_ResultError(CIRInstructionRef ref);
    bool propagate_error(std::initializer_list<CIRInstructionRef> refs);
    bool propagate_error(Array<CIRInstructionRef>& refs);



    bool should_eval_for_lazy_eval(std::initializer_list<CIRInstructionRef> refs);
    bool should_eval_for_lazy_eval(const Array<CIRInstructionRef>& refs);
    bool is_lvalue(CIRInstructionRef ref);
    void set_lvalue(CIRInstructionRef ref);

    void new_analyze_flow(CIRInstructionRef new_pc);
    void recover_analyze_flow();

    EvalMode curr_eval_mode() const;

    bool has_instance() const;

    std::optional<FuncCallKey> curr_cache_key() const;

    CIRResultContext result_context() const;

    // void push_eval_instance(EvalInstance inst);   // 当前帧换新的
    // void push_comptime_frame(EvalInstance inst, FuncCallKey cache_key);   // 开一帧编译期调用帧
    // void pop_eval_instance();

    CIRInstructionRef& curr_inst_ref();   // 当前指令位置 = inst_stack 栈顶
    EvalInstance* curr_instance();   // 当前调用帧；在根（没进调用）时 nullptr


    void apply_result(CIRInstructionRef target, const ResultDesc& result);

    // 批量落结果（含函数值可达性捕获），与 dispatch 应用 writes 的语义一致
    void apply_writes(const Array<ResultWrite>& writes);

    void capture_lcir_function(CIRInstResultRef fk);   // 把被引用的函数值登记进 lcir 工作表



#define X(name) AnalyzeResult analyze_##name(const CIR##name##Info& info, CIRInstructionRef pc_ref, const AnalyzeParams& params);
    CIR_OPERATORS
#undef X

    // 用目标类型确定 determined_inst 的类型（含数组元素传染）；无目标类型则补默认类型。
    // writes 收进 out_writes；返回结果类型，nullptr = 已报错（错误记在 error_inst 上）
    TypeRef determine_type_of(CIRInstructionRef determined_inst,
                              std::optional<TypeRef> expected_type,
                              CIRInstructionRef error_inst,
                              Array<ResultWrite>& out_writes);

    // $T 实例化: 用类型实参开一个独立 CIRResultInstance, 重跑签名块 + FunctionDecl,
    // 得到本次调用的具体函数值
    std::optional<Value> instantiate_generic_func(
        FuncValue fv,
        Array<CIRInstResultRef> key_refs,
        Array<TypeRef> type_args);

    // 编译期调用执行：查缓存 / 起子帧 / 收尾，返回调用点的结果写入
    AnalyzeResult exec_comptime_call(
        const CIRCallInfo& call_info,
        CIRInstructionRef called_inst,
        CIRInstructionRef pc_ref);

public:

    // 本 interp 只看得见这一个包：跨包一律另起一个 interp
    const PackageRef pkg_ref;
    const CIRPackageRef pkg;

    // 编译期调用链
    InterpStack *const interp_stack;
    // 本 interp 的调用帧（纯数据）与求值实例
    InterpStackFrame frame;
    EvalInstance eval_instance;

    Array<CIRInstructionRef> inst_stack;  // 指令位置栈（块入口/跳转时压入保存，恢复时弹出）
    Array<EvalMode> eval_mode_stack;

    // 闲置：ValueMemory 体系的字节级栈内存
    // ValueMemory stack_mem;
};
