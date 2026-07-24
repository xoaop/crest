#pragma once

#include <optional>
#include <utility>

#include "xoaop.h"
#include "array.hpp"
#include "value.hpp"
#include "ast_file.hpp"
#include "ast.hpp"
#include "span.hpp"
#include "scope.hpp"

#include "dag.hpp"


struct Package;


using CIRInstructionRef  = isize;       // 全局指令下标

constexpr CIRInstructionRef INVALID_INST = -1;


struct CIRConstDecl {
    xpString ident;
    SymbolInfoRef symbol;
    CIRInstructionRef value_inst; // 常量值的指令引用
};

struct CIRVariableDecl {
    xpString name;
    SymbolInfoRef symbol;
    isize slot;                   // 在栈帧中的槽位（参数 0..N-1，局部变量 N..）
    bool is_var_arg;              // 是否是变长参数（仅函数参数有效）
    bool is_comptime;             // 是否是编译期参数

    bool no_zero_init;
};

struct EnumFieldInit {
    xpString name;
    CIRInstructionRef value_inst;  // INVALID_INST 表示自增
};

struct CIRInstruction;


struct CIRFunction {
    // xpString name;                      // 函数名（symbol_find / call 目标）
    // SymbolInfoRef symbol;

    CIRInstructionRef body_inst;
    CIRInstructionRef return_type_inst; // 返回类型 block 引用, nullopt 表示返回类型由返回值推导
    // Array<CIRVariableDecl> args;           // 参数名 + slot + is_var_arg
    Array<CIRInstructionRef> arg_type_insts; // 每个参数的类型 block 引用（与 args 平行，var_arg 为 INVALID_INST）
    Array<CIRInstructionRef> arg_decl_insts; // 每个参数的 VariableDecl 指令引用
    isize return_count;                    // 返回值数量
    bool is_extern_c;                      // 是否是 extern "C" 函数
    bool is_comptime;                      // 是否是编译期函数


    isize slot_count;               // 局部变量数量（包括参数）
};


struct CIRLoop {
    isize body_len;
};

struct CIRCondBr {
    CIRInstructionRef condition_inst;
    CIRInstructionRef true_block_inst;
    CIRInstructionRef false_block_inst;
};

struct CIRBlock {
    bool is_comptime;             // 是否是编译时执行的 block
    bool immediate_eval;          // 是否立即求值
    isize body_len;               // Block 内指令数量
};


#define CIR_OPERATORS \
    X(ConstDecl) \
    X(FunctionDecl) \
    X(UnionDecl) \
    X(VariableDecl) \
    X(Binary) \
    X(Unary) \
    X(FieldAccess) \
    X(FieldPtr) \
    X(Call) \
    X(ConstantValue) \
    X(Cast) \
    X(StructInit) \
    X(ArrayInit) \
    X(Index) \
    X(IndexPtr) \
    X(PointerType) \
    X(ArrayType) \
    X(SliceType) \
    X(EnterScope) \
    X(ExitScope) \
    X(Block) \
    X(Loop) \
    X(CondBr) \
    X(Break) \
    X(Load) \
    X(Deref) \
    X(Store) \
    X(IdentRef) \
    X(IdentVal) \
    X(DetermineType) \
    X(TypeAscribe) \
    X(GetOrInitStruct) \
    X(StructField) \
    X(FinishStruct) \
    X(EnumDeclInit) \
    X(AddrOf) \
    X(FieldTypeOfStruct) \
    X(FuncParamType) \
    X(TypeOfInstResult) \
    X(FuncType) \

/**/

enum class CIROperator {
#define X(name) name,
    CIR_OPERATORS
#undef X
};


inline const char *string(CIROperator op) {
    switch(op) {
#define X(name) case CIROperator::name: return #name;
        CIR_OPERATORS
#undef X
        default: return "<unknown operator>";
    }
}

enum class CIRResultState: u32 {
    NothingYet,  // 还没有计算结果
    OnlyType,   // 仅仅只有类型推导
    WholeValue,  // 计算出实际值了
    Error        // 本指令分析出错，结果不可用，下游应跳过
};

enum class CIRValueKind : u8 {
    RValue,  // 实际值（参与运算、传参）
    LValue,  // 位置句柄（可被 Load/Store/FieldPtr/IndexPtr 操作）
};

struct CIRInstResult {
    CIRResultState state = CIRResultState::NothingYet;
    CIRValueKind value_kind = CIRValueKind::RValue;


    std::optional<TypeRef> implicit_type = std::nullopt;

    static CIRInstResult make_value(TypeRef t, Value v, CIRValueKind vk = CIRValueKind::RValue) {
        CIRInstResult r;
        r.state = CIRResultState::WholeValue;
        r.outstanding_type = t;
        r.val = v;
        r.value_kind = vk;
        return r;
    }

    static CIRInstResult make_value(Value v) {
        CIRInstResult r;
        r.state = CIRResultState::WholeValue;
        r.outstanding_type = v.type;
        r.val = v;
        r.value_kind = CIRValueKind::RValue;
        return r;
    }

    static CIRInstResult make_type_only(TypeRef t) {
        CIRInstResult r;
        r.state = CIRResultState::OnlyType;
        r.outstanding_type = t;
        return r;
    }

    
    static CIRInstResult make_error() {
        CIRInstResult r;
        r.state = CIRResultState::Error;
        return r;
    }

    TypeRef type() const;
    TypeRef actual_type() const;
    Value actual_val() const;

    void set_type(TypeRef new_type);
    void set_actual_type(TypeRef new_type);
    void set_val(Value new_val);

private:
    Value val;
    TypeRef outstanding_type = nullptr;
};


struct CIRInstruction {
    CIROperator       op;
    SymbolInfoRef     symbol;

    CIRInstruction() {
        op = {};
        new (&imm_val) Value();
    }

    CIRInstruction(const CIRInstruction& other) {
        memcpy((void*)this, &other, sizeof(CIRInstruction));
    }
    CIRInstruction& operator=(const CIRInstruction& other) {
        if (this == &other) return *this;
        memcpy((void*)this, &other, sizeof(CIRInstruction));
        return *this;
    }

    const char *to_string() const {
        return string(op);
    }

    isize len() const;

    union {
        Value       imm_val;
        xpString    ident;
        CIRInstructionRef inst_ref;

        CIRFunction  func_decl;
        CIRLoop      loop_info;
        CIRCondBr    condbr_info;

        CIRBlock block_info;

        struct {
            CIRInstructionRef break_block;    // 指向 Block 指令
            CIRInstructionRef break_value_inst;
        } break_info;

        struct {
            CIRInstructionRef ptr_inst;
        } load_info;

        struct {
            CIRInstructionRef var_inst;
            CIRInstructionRef value_inst;
        } store_info;

        struct {
            CIRInstructionRef var_inst;
            CIRInstructionRef type_inst;
        } type_ascribe_info;

        CIRVariableDecl var_decl;
        CIRConstDecl const_decl;

        struct {
            CIRInstructionRef called_thing;
            Array<CIRInstructionRef> arg_insts;
        } call_info;

        struct {
            TokenType op;
            CIRInstructionRef left_inst;
            CIRInstructionRef right_inst;
        } binary_info;

        struct {
            TokenType op;
            CIRInstructionRef operand_inst;
        } unary_info;

        struct {
            CIRInstructionRef expr_inst;
            CIRInstructionRef target_type_inst;
        } cast_info;

        struct {
            CIRInstructionRef parent_inst;
            xpString field_name;
        } field_access_info;

        struct {
            CIRInstructionRef array_inst;
            CIRInstructionRef index_inst;
        } index_info;

        struct {
            CIRInstructionRef struct_type_inst;
            Array<CIRInstructionRef> field_init_insts;
        } struct_init_info;

        struct {
            Array<CIRInstructionRef> element_insts;
        } array_init_info;

        struct {
            CIRInstructionRef pointed_type_inst;
        } pointer_type_info;

        struct {
            CIRInstructionRef element_type_inst;
            CIRInstructionRef count_inst;
        } array_type_info;

        struct {
            CIRInstructionRef element_type_inst;
        } slice_type_info;

        struct {
            Ast *decl_ast;
        } get_or_init_struct_info;

        struct {
            CIRInstructionRef type_block_inst;
            xpString          name;
        } struct_field_info;

        struct {
            CIRInstructionRef struct_decl_inst;
            Array<CIRInstructionRef> field_insts;
        } finish_struct_info;

        struct {
            CIRInstructionRef tag_type_inst;
            Ast *decl_ast;
            SymbolInfoRef symbol;   // 可选，ConstDecl绑定的符号，壳子创建后立即注册
            Scope *scope;
            Array<EnumFieldInit> fields;
        } enum_decl_init_info;

        struct {
            Scope *scope;
        } scope_info;

        struct {
            CIRInstructionRef determining_inst;
            CIRInstructionRef type_inst;  // INVALID_INST 表示"无值"
        } determine_type_info;

        struct {
            CIRInstructionRef lval_inst;  // 左值指令（LValue of T）
        } addr_of_info;

        struct {
            CIRInstructionRef type_of_func_type_inst; // type_type(function_type) 的指令
            isize param_index;
        } func_param_type_info;

        struct {
            CIRInstructionRef struct_type_inst;
            isize field_index;
        } field_type_of_struct_info;

        struct {
            CIRInstructionRef target_inst;  // 要提取类型的指令
        } type_of_inst_result_info;

        struct {
            Array<CIRInstructionRef> param_type_insts;
            CIRInstructionRef return_type_inst;
        } func_type_info;

        struct {
            CIRInstructionRef operand_inst;
        } deref_info;
    };


    //
    // 指令结果, 由Interpreter来提供
    //
    SourceLocation src_loc;
};


struct CIRFileRange {
    CIRInstructionRef start;  // inclusive
    Scope *file_scope;
};


// TODO: 把指针换成更清晰的引用处理结构
struct CIRResultInstance {
    xpHashMap<CIRInstructionRef, CIRInstResult> results;
};

struct CIRPackage {
    Array<CIRInstruction>     instructions;   // 全局指令数组
    
    Array<CIRInstructionRef>  top_level_insts;
    
    Array<CIRFileRange>       file_ranges;  // 按 start 升序，文件级 scope 切换点
    
    Array<xpString>           string_literals;
    

    Array<CIRInstResult>      results;
    xpHashMap<FuncCallKey, CIRResultInstance*> result_instances;
    Array<FuncCallKey> comptime_func_calls;


    CIRInstruction* inst(CIRInstructionRef ref);
    const CIRInstruction* inst(CIRInstructionRef ref) const;
    Scope *scope_for_pc(CIRInstructionRef pc) const;

    CIRResultInstance *get_result_instance(FuncCallKey key);

    CIRInstResult& result_of(CIRInstructionRef ref, std::optional<CIRResultInstance*> instance = std::nullopt);
};

CIRPackage make_cir_package(xpAllocator allocator);




struct CIRResultContext {
public:
    static CIRResultContext create(CIRPackage *pkg);

    CIRPackage *pkg() const { return _pkg; }
    void set_pkg(CIRPackage *new_pkg) { _pkg = new_pkg; }

    void enter_call(FuncCallKey key);
    void enter_call_instance(CIRResultInstance *instance);
    void exit_call();
    bool in_call() const { return _call_instance != nullptr; }
    CIRResultInstance *call_instance() const { return _call_instance; }
    const FuncCallKey &call_key() const;

    CIRInstResult &result_of(CIRInstructionRef ref) const;

private:
    CIRPackage *_pkg = nullptr;
    CIRResultInstance *_call_instance = nullptr;
    std::optional<FuncCallKey> _call_key;
};



//
// Builder
//
struct CIRBuilder;

struct ScopeGuard {
    CIRBuilder *builder;
    bool entered;

    ScopeGuard(CIRBuilder *builder, Ast *ast);
    ~ScopeGuard();
};

struct CIRBuilder {

    CIRBuilder(xpAllocator allocator);
    ~CIRBuilder();


    void build_cir_package(Package *pkg);

    CIRInstructionRef build_inst_for_const_decl(Ast *const_decl_ast);
    CIRInstructionRef build_func_decl(Ast *fd, std::optional<SymbolInfoRef> func_sym);
    CIRInstructionRef build_inst_for_ast_block(Ast *block_ast, bool new_ir_block);
    CIRInstructionRef build_inst_for_stmt(Ast *stmt);
    CIRInstructionRef build_inst_for_expr(Ast *expr);
    CIRInstructionRef build_block_inst_for_expr(Ast *expr, bool is_comptime_block, bool immediate_eval);
    CIRInstructionRef build_ptr_inst_for_expr(Ast *expr);

    CIRInstructionRef build_inst_for_var_decl(Ast *var_decl_ast);
    void build_inst_for_return_stmt(Ast *return_stmt_ast);

    CIRInstructionRef New_Instruction(CIROperator op, Ast *ast);
    // std::pair<CIRInstructionRef, CIRInstruction&> New_Inst(CIROperator op, Ast *ast);
    CIRInstructionRef Alloc_Var(xpString name, bool is_var_arg, bool no_zero_init, Ast *ast);
    CIRInstructionRef New_Break(CIRInstructionRef break_block, CIRInstructionRef break_value_inst, Ast *ast);
    CIRInstruction& Instruction(CIRInstructionRef ref);

    CIRInstructionRef Begin_Block(Ast *ast, bool is_comptime, bool immediate_eval);
    void End_Block(CIRInstructionRef block_inst);
    CIRInstructionRef Begin_Loop(Ast *ast);
    void End_Loop(CIRInstructionRef loop_inst);



    bool Enter_Scope(Ast *ast);
    void Exit_Scope();


public:


    // state
    AstFile *curr_ast_file;

    CIRPackage *curr_pkg;
    Array<CIRInstruction> *curr_instruction_buffer;
    CIRFunction *curr_func;
    CIRInstructionRef curr_func_body_block;   // 函数体 Block 指令，return 就是 break 到此 block
    CIRInstructionRef curr_block_inst;
    Scope *curr_scope;
    SymbolInfoRef curr_const_sym;   // 当前正在构建的 ConstDecl 符号，供嵌套表达式使用


    // cirbuilder所有的状态, 需要分配
    Array<CIRInstructionRef> loop_body_block_stack; // 目前用于continue知道目标在哪
    Array<CIRInstructionRef> loop_stack;            // 目前用于break知道目标在哪
};


bool is_cir_binary_op(TokenType type);
bool is_cir_unary_op(TokenType type);

bool is_pure_comptime_func(CIRFunction& func, const CIRResultContext& ctx);

void dump_cir_package(CIRPackage *file);
