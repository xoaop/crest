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
#include "stable_ordered_array.hpp"


#include "cir_package.hpp"
#include "package.hpp"


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


    void build_cir_package(Ref<Package> pkg);

    CIRInstructionRef build_inst_for_const_decl(Ast *const_decl_ast);
    CIRInstructionRef build_func_decl(Ast *fd, std::optional<Ref<SymbolInfo>> func_sym);
    CIRInstructionRef build_inst_for_ast_block(Ast *block_ast, bool new_ir_block, bool emit_in_parent = true, CIRBlockRef *out_block = nullptr);
    CIRInstructionRef build_inst_for_stmt(Ast *stmt);
    CIRInstructionRef build_inst_for_expr(Ast *expr);
    CIRInstructionRef build_block_inst_for_expr(Ast *expr, bool is_comptime_block, bool immediate_eval);
    CIRInstructionRef build_ptr_inst_for_expr(Ast *expr);

    CIRInstructionRef build_inst_for_var_decl(Ast *var_decl_ast);
    void build_inst_for_return_stmt(Ast *return_stmt_ast);
    void build_inst_for_for_stmt(Ast *stmt);

    CIRInstructionRef New_Instruction(CIROperator op, Ast *ast);
    CIRInstructionRef Alloc_Var(xpString name, bool is_var_arg, bool no_zero_init, Ast *ast);
    CIRInstructionRef New_Break(CIRInstructionRef break_block, CIRInstructionRef break_value_inst, Ast *ast);
    CIRInstruction& Instruction(CIRInstructionRef ref);


    
    template<CIROperator Op>
    CIRInstructionRef Make_Instruction(Ast *ast, const typename info_type<Op>::type& payload) {
        auto ref = New_Instruction(Op, ast);
        Instruction(ref).info<Op>() = payload;
        return ref;
    }

    CIRInstructionRef Begin_Block(Ast *ast, bool is_comptime, bool immediate_eval);  // 创建子块 + BlockRef 指令并压栈，返回 handle（尾随 BlockRef 指令位置）
    void End_Block();                                                           // 弹栈
    CIRInstructionRef Begin_Loop(Ast *ast);
    void End_Loop(CIRInstructionRef loop_inst);



    bool Enter_Scope(Ast *ast);
    void Exit_Scope();


public:


    // state
    AstFile *curr_ast_file;

    CIRPackage *curr_pkg;
    Ref<Package> curr_pkg_ref;
    CIRFunctionDeclInfo *curr_func;
    CIRInstructionRef curr_func_body_block;   // 函数体 Block 指令，return 就是 break 到此 block
    CIRInstructionRef curr_block_inst;
    Ref<Scope> curr_scope;
    Ref<SymbolInfo> curr_const_sym;   // 当前正在构建的 ConstDecl 符号，供嵌套表达式使用
    bool building_return_type_decl = false;   // 正在构建 return <type-decl>，声明块内发 PublishReturnValue


    // cirbuilder所有的状态, 需要分配
    Array<CIRBlockRef> block_stack;
    Array<CIRInstructionRef> loop_body_block_stack; // 目前用于continue知道目标在哪
    Array<CIRInstructionRef> loop_stack;            // 目前用于break知道目标在哪
};


bool is_cir_binary_op(TokenType type);
bool is_cir_unary_op(TokenType type);
bool is_type_decl_ast(Ast *expr);
