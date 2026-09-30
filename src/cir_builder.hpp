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


struct CIRBuilder;

struct CIRBuildContext {
    xpAllocator allocator;

    CIRPackage *pkg;
    Ref<Package> pkg_ref;
    CIRFunctionDeclInfo *func;
    CIRInstructionRef func_body_block;   // 函数体 Block 指令，return 就是 break 到此 block
    Ref<Scope> scope;
    bool building_return_type_decl = false;   // 正在构建 return <type-decl>，声明块内发 PublishReturnValue

    Array<CIRBlockRef> block_stack;
    Array<CIRInstructionRef> loop_body_block_stack; // 目前用于continue知道目标在哪
    Array<CIRInstructionRef> loop_stack;            // 目前用于break知道目标在哪
};

struct ScopeGuard {
    CIRBuildContext *ctx;
    bool entered;

    ScopeGuard(CIRBuildContext& ctx, Ast *ast);
    ~ScopeGuard();
};

struct CIRBuilder {

    
    static CIRInstructionRef build_inst_for_const_decl(CIRBuildContext& ctx, Ast *const_decl_ast);
    static CIRInstructionRef build_func_decl(CIRBuildContext& ctx, Ast *fd, std::optional<Ref<SymbolInfo>> func_sym);
    static CIRInstructionRef build_inst_for_ast_block(CIRBuildContext& ctx, Ast *block_ast, bool new_ir_block, bool emit_in_parent = true, CIRBlockRef *out_block = nullptr);
    static CIRInstructionRef build_inst_for_stmt(CIRBuildContext& ctx, Ast *stmt);
    static CIRInstructionRef build_inst_for_expr(CIRBuildContext& ctx, Ast *expr);
    static CIRInstructionRef build_block_inst_for_expr(CIRBuildContext& ctx, Ast *expr, bool is_comptime_block, bool immediate_eval);
    static CIRInstructionRef build_ptr_inst_for_expr(CIRBuildContext& ctx, Ast *expr);
    
    static CIRInstructionRef build_inst_for_var_decl(CIRBuildContext& ctx, Ast *var_decl_ast);
    static void build_inst_for_return_stmt(CIRBuildContext& ctx, Ast *return_stmt_ast);
    static void build_inst_for_for_stmt(CIRBuildContext& ctx, Ast *stmt);
    
    static CIRInstructionRef New_Instruction(CIRBuildContext& ctx, CIROperator op, Ast *ast);
    static CIRInstructionRef Alloc_Var(CIRBuildContext& ctx, xpString name, bool is_var_arg, bool no_zero_init, Ast *ast, bool is_param = false);
    static CIRInstructionRef New_Break(CIRBuildContext& ctx, CIRInstructionRef break_block, CIRInstructionRef break_value_inst, Ast *ast);
    static CIRInstruction& Instruction(CIRBuildContext& ctx, CIRInstructionRef ref);
    
    
    
    template<CIROperator Op>
    static CIRInstructionRef Make_Instruction(CIRBuildContext& ctx, Ast *ast, const typename info_type<Op>::type& payload) {
        auto ref = New_Instruction(ctx, Op, ast);
        Instruction(ctx, ref).info<Op>() = payload;
        return ref;
    }

    static CIRBlockRef Begin_Block(CIRBuildContext& ctx, bool is_comptime, bool immediate_eval, bool yields_value = false);
    static CIRInstructionRef New_BlockRef(CIRBuildContext& ctx, Ast *ast, CIRBlockRef blk);
    static void End_Block(CIRBuildContext& ctx);
    static CIRBlockRef Begin_Loop(CIRBuildContext& ctx);
    static void End_Loop(CIRBuildContext& ctx, CIRInstructionRef loop_inst);
    
    
    
    static bool Enter_Scope(CIRBuildContext& ctx, Ast *ast);
    static void Exit_Scope(CIRBuildContext& ctx);
    

    CIRBuilder(xpAllocator allocator);
    ~CIRBuilder();

    void build_cir_package(Ref<Package> pkg);
public:

    CIRBuildContext ctx;
};


bool is_cir_binary_op(TokenType type);
bool is_cir_unary_op(TokenType type);
bool is_type_decl_ast(Ast *expr);
