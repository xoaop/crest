#pragma once

#include <optional>

#include "xoaop.h"
#include "array.hpp"
#include "value.hpp"
#include "package.hpp"
#include "ast_file.hpp"
#include "ast.hpp"
#include "span.hpp"
#include "scope.hpp"


/*

Design:

a file, such as foo.cst be like:

CIRFile {

%0(
    attr: const_decl(ident: fn1)
) = function_decl(
        arg1_type: block :a1 {
            %1 = symbol_find("i32")
            break :a1 %1
        }
        arg2_type: block :a2 {
            %2 = call ...
            break :a2 %2
        }
        count_of_args: [%3, %4]
        count_of_returns: 1
        body: block :entry {
            %5 = local i32, 0       // var x: i32 = 0
            %6 = add(%5, 1)         // x + 1
            store %5, %6            // x = x + 1

            %7 = call "fn2" with (%5, %6)

            // block 表达式
            %8 = block :blk8 {
                %9 = add(1, 2)
                break :blk8 %9
            }

            // if 表达式 (branch)
            %10 = greater(%5, 10)
            %result = branch(%10) {
                true: block :then {
                    %11 = sub(%5, 10)
                    break :then %11
                }
                false: block :else {
                    %12 = add(%5, 10)
                    break :else %12
                }
            }

            // while 循环
            loop :my_loop {
                condition: block :cond {
                    %cond = less(%i, %n)
                    break :cond %cond
                }
                body: block :body {
                    %i = add(%i, 1)
                    break :body         // void → 回到 condition
                }
            }

            break :entry %8
        }
    )


%13(
    attr: const_decl(ident: n1)
) = block :n1_init {
        %14 = symbol_find("i32")
        break :n1_init %14
    }


}

*/


using CIRInstructionRef  = isize;       // 全局指令下标

constexpr CIRInstructionRef INVALID_INST = -1;


struct CIRConstDecl {
    xpString ident;
    CIRInstructionRef value_inst; // 常量值的指令引用
};

struct CIRVariableDecl {
    xpString name;
    isize slot;                   // 在栈帧中的槽位（参数 0..N-1，局部变量 N..）
    bool is_var_arg;              // 是否是变长参数（仅函数参数有效）
};

struct CIRStructFieldDecl {
    xpString name;
    CIRInstructionRef type_inst;
};

struct CIRStructDeclInfo {
    xpString name;
    Array<CIRStructFieldDecl> fields;
};

struct CIREnumFieldDecl {
    xpString name;
    CIRInstructionRef value_inst;  // INVALID_INST if no explicit value
};

struct CIREnumDeclInfo {
    xpString name;
    CIRInstructionRef tag_type_inst;
    Array<CIREnumFieldDecl> fields;
};


struct CIRFunction {
    xpString name;                      // 函数名（symbol_find / call 目标）

    CIRInstructionRef entry_inst;
    CIRInstructionRef return_type_inst;    // 返回类型 block 引用（void 时为 INVALID_INST）
    Array<CIRVariableDecl> args;           // 参数名 + slot + is_var_arg
    Array<CIRInstructionRef> arg_type_insts; // 每个参数的类型 block 引用（与 args 平行，var_arg 为 INVALID_INST）
    isize return_count;                    // 返回值数量
    bool is_extern_c;                      // 是否是 extern "C" 函数


    isize slot_count;               // 局部变量数量（包括参数）
};


struct CIRLoop {
    CIRInstructionRef condition_inst;
    CIRInstructionRef loop_body_inst;
};

struct CIRIf {
    CIRInstructionRef condition_inst;
    CIRInstructionRef true_block_inst;
    CIRInstructionRef false_block_inst;
};


#define CIR_OPERATORS \
    X(ConstDecl) \
    X(FunctionDecl) \
    X(StructDecl) \
    X(EnumDecl) \
    X(UnionDecl) \
    X(VariableDecl) \
    X(Binary) \
    X(Unary) \
    X(FieldAccess) \
    X(Call) \
    X(ConstantValue) \
    X(Cast) \
    X(StructInit) \
    X(ArrayInit) \
    X(Index) \
    X(PointerType) \
    X(ArrayType) \
    X(SliceType) \
    X(TypeKind) \
    X(StringLiteral) \
    X(EnterScope) \
    X(ExitScope) \
    X(Block) \
    X(Loop) \
    X(If) \
    X(Break) \
    X(LoopBreak) \
    X(Continue) \
    X(Store) \
    X(TypeAscribe) \
    X(IdentRef) \


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






struct CIRInstruction {
    CIROperator       op;

    // @SHIT
    CIRInstruction() {
        op = {};
        new (&imm_val) Value();
    }

    union {
        Value       imm_val;
        xpString    ident;
        CIRInstructionRef inst_ref;
        isize       body_len;       // CIROperator::Block 的 body 指令数量

        CIRFunction  func_decl;
        CIRLoop      loop_info;
        CIRIf        if_info;

        struct {
            CIRInstructionRef break_block;    // 指向 Block 指令
            CIRInstructionRef break_value_inst;
        } break_info;

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
            TypeKind kind;
        } type_kind_info;

        CIRStructDeclInfo struct_decl_info;
        CIREnumDeclInfo   enum_decl_info;
    };

    Span span;
};


struct CIRFile {
    Array<CIRInstruction>     instructions;   // 全局指令数组
    Array<CIRInstructionRef>  top_level_insts;
};

CIRFile make_cir_file(xpAllocator allocator);








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

    // state
    AstFile *curr_ast_file;

    CIRFile *curr_file;
    Array<CIRInstruction> *curr_instruction_buffer;
    CIRFunction *curr_func;
    CIRInstructionRef curr_func_body_block;   // 函数体 Block 指令，return 就是 break 到此 block
    Scope *curr_scope;


    CIRFile build_cir_file(AstFile *ast_file);

    CIRInstructionRef build_cir_for_const_decl(Ast *const_decl_ast);
    CIRInstructionRef build_func_decl(xpString name, Ast *fd);
    CIRInstructionRef build_inst_for_ast_block(Ast *block_ast, bool new_ir_block);
    CIRInstructionRef build_inst_for_stmt(Ast *stmt);
    CIRInstructionRef build_inst_for_expr(Ast *expr);
    CIRInstructionRef build_block_inst_for_expr(Ast *expr);

    CIRInstructionRef build_inst_for_var_decl(Ast *var_decl_ast);

    CIRInstructionRef New_Instruction(CIROperator op);
    CIRInstructionRef New_Instruction(CIROperator op, Ast *ast);
    CIRInstructionRef Push_Instruction(CIROperator op);
    CIRInstructionRef Alloc_Var(xpString name, bool is_var_arg);
    CIRInstructionRef New_Break(CIRInstructionRef break_block, CIRInstructionRef break_value_inst);
    CIRInstruction& Instruction(CIRInstructionRef ref);

    CIRInstructionRef Begin_Block();
    void End_Block(CIRInstructionRef block_inst);

    // 作用域管理：利用 analyser 阶段建立的 ast→scope 映射，在 CIR 生成时重入作用域树
    bool Enter_Scope(Ast *ast);
    bool Enter_Scope_Silent(Ast *ast);
    void Exit_Scope();
};

void dump_cir_file(CIRFile *file);
