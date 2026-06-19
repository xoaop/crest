#pragma once

#include <optional>

#include "array.hpp"
#include "value.hpp"
#include "cir_builder.hpp"

struct CIRPackage;

void analyze_package(Package *pkg);


enum class EvalMode { 
    FullEval, 
    TypeOnly 
};




struct Interpreter {
    Interpreter(xpAllocator allocator);
    ~Interpreter();

    void analyze_cir_package(CIRPackage* cir_package);

    void analyze_instruction();

    void set_scope(Scope *scope);

    void analyze_const_decl();
    void analyze_function_decl();
    void analyze_get_or_init_struct();
    void analyze_struct_field();
    void analyze_finish_struct();
    void analyze_enum_decl_init();
    void analyze_union_decl();
    void analyze_variable_decl();
    void analyze_binary();
    void analyze_unary();
    void analyze_field_access();
    void analyze_field_ptr();
    void analyze_call();
    void analyze_constant_value();
    void analyze_cast();
    void analyze_struct_init();
    void analyze_array_init();
    void analyze_index();
    void analyze_index_ptr();
    void analyze_deref();
    void analyze_addr_of();
    void analyze_pointer_type();
    void analyze_array_type();
    void analyze_slice_type();
    void analyze_block(std::optional<EvalMode> force_eval_mode = std::nullopt);
    void analyze_loop();
    void analyze_if();
    void analyze_break();
    void analyze_loop_break();
    void analyze_continue();
    void analyze_load();
    void analyze_store();
    void analyze_type_ascribe();
    void analyze_ident_ref();
    void analyze_ident_val();
    void analyze_enter_scope();
    void analyze_exit_scope();
    void analyze_determine_type();
    void analyze_type_of_inst_result();
    void analyze_field_type_of_struct();

    CIRResultType result_state(CIRInstructionRef ref);
    void set_result_state(CIRInstructionRef ref, CIRResultType state);
    TypeRef ResultType(CIRInstructionRef ref);
    Value ResultValue(CIRInstructionRef ref);
    void Set_ResultType(CIRInstructionRef ref, TypeRef type);
    void Set_ResultValue(CIRInstructionRef ref, Value val);
    void Set_ResultTypeAndValue(CIRInstructionRef ref, Value val);
    bool has_result_val(CIRInstructionRef ref);
    bool has_result_type(CIRInstructionRef ref);
    bool has_error(CIRInstructionRef ref);
    void Set_ResultError(CIRInstructionRef ref);
    bool propagate_error(std::initializer_list<CIRInstructionRef> refs);
    bool propagate_error(Array<CIRInstructionRef>& refs);

    bool is_lvalue(CIRInstructionRef ref);
    void set_lvalue(CIRInstructionRef ref);

    void new_pc_flow(CIRInstructionRef new_pc);
    void recover_pc_flow();

    EvalMode curr_eval_mode() const;

public:

    struct AnalyzeFlowState {
        Scope *scope;
        CIRInstructionRef pc;
    };

    CIRPackage *pkg;
    Scope *curr_scope;
    CIRInstructionRef pc;

    Array<AnalyzeFlowState> pc_stack;

    Array<EvalMode> eval_mode_stack;
    Array<CIRInstructionRef> loop_stack;  // 当前嵌套的 Loop 指令栈
};
