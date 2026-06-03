#include <print>

#include "cir_builder.hpp"
#include "context.hpp"






CIRFile make_cir_file(xpAllocator allocator) {
    CIRFile cir_file = {};
    cir_file.instructions = make_array<CIRInstruction>(allocator);
    cir_file.top_level_insts = make_array<CIRInstructionRef>(allocator);

    return cir_file;
}

CIRFile CIRBuilder::build_cir_file(AstFile *ast_file) {

    curr_ast_file = ast_file;
    curr_scope  = &ast_file->file_scope;   // 从文件作用域开始

    CIRFile cir_file = make_cir_file(permanent_allocator());
    curr_file = &cir_file;
    curr_instruction_buffer = &cir_file.instructions;

    for(Ast *ast : ast_file->top_levels) {
        CIRInstructionRef top_inst = INVALID_INST;

        switch(ast->type) {
            case AstType_ConstDecl: {
                if(ast->ConstDecl.value_ast->type == AstType_Import) {
                    break;
                }

                top_inst = build_cir_for_const_decl(ast);
            } break;

            default: {
                context()->reporter.report_error(ast->span, ast_file->source_code, "Unsupported top-level AST type for CIR generation");
            } break;
        }

        if (top_inst != INVALID_INST) {
            cir_file.top_level_insts.push_back(top_inst);
        }
    }

    return cir_file;
}


CIRInstructionRef CIRBuilder::build_cir_for_const_decl(Ast *const_decl_ast) {
    XP_ASSERT_DEFAULT(const_decl_ast->type == AstType_ConstDecl);

    auto const_decl = New_Instruction(CIROperator::ConstDecl, const_decl_ast);
    auto value_ast = const_decl_ast->ConstDecl.value_ast;

    if (value_ast->type == AstType_FunctionDeclValue) {
        auto func_inst = build_func_decl(const_decl_ast->ConstDecl.name, value_ast);
        Instruction(const_decl).const_decl = {
            .ident = const_decl_ast->ConstDecl.name,
            .value_inst = func_inst
        };
        return const_decl;
    }


    auto value_block_inst = build_block_inst_for_expr(value_ast);
    Instruction(const_decl).const_decl = {
        .ident = const_decl_ast->ConstDecl.name,
        .value_inst = value_block_inst
    };

    return const_decl;
}




CIRInstructionRef CIRBuilder::build_func_decl(xpString name, Ast *fd) {
    XP_ASSERT_DEFAULT(fd->type == AstType_FunctionDeclValue);

    auto outer_block = Push_Instruction(CIROperator::Block);

    CIRFunction func = {};
    func.name = name;
    func.return_count = 1;
    func.is_extern_c = fd->FunctionDeclValue.is_extern_c;
    func.args = make_array<CIRVariableDecl>(permanent_allocator());
    func.arg_type_insts = make_array<CIRInstructionRef>(permanent_allocator());

    curr_func = &func;
    defer(curr_func = nullptr);

    // 0. 返回类型
    if (fd->FunctionDeclValue.return_type_ast != nullptr) {
        func.return_type_inst = build_block_inst_for_expr(fd->FunctionDeclValue.return_type_ast);
    } else {
        func.return_type_inst = INVALID_INST; // void
    }

    // 1. 每个参数：类型 block + 分配 slot
    for (isize i = 0; i < fd->FunctionDeclValue.params.count; i++) {
        Ast *param = fd->FunctionDeclValue.params[i];
        XP_ASSERT_DEFAULT(param->type == AstType_ParamDecl);

        auto param_type = INVALID_INST;
        if(!param->ParamDecl.is_var_arg) {
            param_type = build_block_inst_for_expr(param->ParamDecl.type_ast);
        }

        CIRVariableDecl var = {};
        var.name = param->ParamDecl.name;
        var.slot = i;                       // 参数槽位: 0, 1, 2, ...
        var.is_var_arg = param->ParamDecl.is_var_arg;
        func.args.push_back(var);
        func.arg_type_insts.push_back(param_type);
    }

    // 2. 函数体 block
    auto body_inst = Begin_Block();

    
    curr_func_body_block = body_inst;
    
    {
        ScopeGuard _func_scope_guard(this, fd);
        for (isize i = 0; i < func.args.count; i++) {
            auto& var = func.args[i];
            auto vd = Alloc_Var(var.name, var.is_var_arg);

            auto param_type = func.arg_type_insts[i];
            if (param_type != INVALID_INST) {
                auto ta = New_Instruction(CIROperator::TypeAscribe);
                Instruction(ta).type_ascribe_info = {
                    .var_inst  = vd,
                    .type_inst = param_type,
                };
            }
        }

        if(fd->FunctionDeclValue.block != nullptr) {
            build_inst_for_ast_block(fd->FunctionDeclValue.block, false);
        }
    }

    End_Block(body_inst);
    func.entry_inst = body_inst;

    auto func_inst = New_Instruction(CIROperator::FunctionDecl, fd);
    Instruction(func_inst).func_decl = func;

    New_Break(outer_block, func_inst);
    Instruction(outer_block).body_len = curr_instruction_buffer->count - outer_block - 1;
    return outer_block;
}


CIRInstructionRef CIRBuilder::build_inst_for_ast_block(Ast *block_ast, bool new_ir_block) {
    XP_ASSERT_DEFAULT(block_ast->type == AstType_Block);

    
    CIRInstructionRef block_inst = INVALID_INST;
    if (new_ir_block) {
        block_inst = Begin_Block();
    }


    {
        auto _scope_guard = ScopeGuard(this, block_ast);
        for (Ast *stmt : block_ast->Block.statements) {
            build_inst_for_stmt(stmt);
        }
    }


    if (new_ir_block) {
        End_Block(block_inst);
    }

    return block_inst;
}


CIRInstructionRef CIRBuilder::build_inst_for_stmt(Ast *stmt) {

    switch(stmt->type) {
        case AstType_VariableDecl: {
            build_inst_for_var_decl(stmt);
        } break;

        case AstType_Assignment: {
            // 左值求值为"指针"，右值求值为"值"
            auto ptr_inst   = build_inst_for_expr(stmt->Assignment.left_var_expr);
            auto value_inst = build_inst_for_expr(stmt->Assignment.right_expr);
            auto st = New_Instruction(CIROperator::Store, stmt);
            Instruction(st).store_info = {
                .var_inst   = ptr_inst,
                .value_inst = value_inst,
            };
        } break;

        case AstType_IfStmt: {
            auto& if_stmt = stmt->IfStmt;

            auto cond_inst = build_block_inst_for_expr(if_stmt.condition);
            auto then_inst = build_inst_for_ast_block(if_stmt.then_block, true);

            CIRInstructionRef else_inst = INVALID_INST;
            if (if_stmt.else_block != nullptr) {
                else_inst = build_inst_for_ast_block(if_stmt.else_block, true);
            }

            auto if_inst = New_Instruction(CIROperator::If, stmt);
            Instruction(if_inst).if_info = {
                .condition_inst    = cond_inst,
                .true_block_inst   = then_inst,
                .false_block_inst  = else_inst,
            };
        } break;

        case AstType_ForStmt: {
            auto _scope_guard = ScopeGuard(this, stmt);

            if(stmt->ForStmt.init != nullptr) {
                build_inst_for_stmt(stmt->ForStmt.init);
            }

            auto loop_inst = New_Instruction(CIROperator::Loop, stmt);
            CIRLoop loop_info = {};
            
            if (stmt->ForStmt.condition != nullptr) {
                loop_info.condition_inst = build_block_inst_for_expr(stmt->ForStmt.condition);
            } else {
                auto cond_inst = Begin_Block();

                auto true_val_inst = New_Instruction(CIROperator::ConstantValue);
                auto true_val = make_value();
                Instruction(true_val_inst).imm_val.set_bool_value(true);
                New_Break(cond_inst, true_val_inst);

                End_Block(cond_inst);
                loop_info.condition_inst = cond_inst;
            }

            auto body_inst = Begin_Block();

            build_inst_for_ast_block(stmt->ForStmt.body, false);

            if (stmt->ForStmt.post != nullptr) {
                build_inst_for_stmt(stmt->ForStmt.post);
            }

            End_Block(body_inst);
            loop_info.loop_body_inst = body_inst;

            Instruction(loop_inst).loop_info = loop_info;
        } break;

        case AstType_ReturnStmt: {
            CIRInstructionRef value_inst = INVALID_INST;
            if (stmt->ReturnStmt.expr != nullptr) {
                value_inst = build_block_inst_for_expr(stmt->ReturnStmt.expr);
            }
            New_Break(curr_func_body_block, value_inst);
        } break;

        case AstType_FunctionCallExpr: {
            build_inst_for_expr(stmt);
        } break;

        case AstType_Block: {
            build_inst_for_ast_block(stmt, true);
        } break;

        case AstType_Break: {
            New_Instruction(CIROperator::LoopBreak, stmt);
        } break;

        case AstType_Continue: {
            New_Instruction(CIROperator::Continue, stmt);
        } break;

        case AstType_ConstDecl: {
            build_cir_for_const_decl(stmt);
        } break;

        default: {
            UNREACHABLE();
        } break;
    }


    return INVALID_INST;
}

CIRInstructionRef CIRBuilder::build_inst_for_var_decl(Ast *var_decl_ast) {
    XP_ASSERT_DEFAULT(var_decl_ast->type == AstType_VariableDecl);

    auto& vd_ast = var_decl_ast->VariableDecl;

    // 1. 变量声明（分配存储空间）
    auto vd = Alloc_Var(vd_ast.var_name, false);
    Instruction(vd).span = var_decl_ast->span;

    // 2. 类型归属 — 仅当有显式类型标注时才发射
    //    没有 TypeAscribe 意味着类型由后续 store 的值推导
    if (vd_ast.type_ast != nullptr) {
        auto type_inst = build_block_inst_for_expr(vd_ast.type_ast);
        auto ta = New_Instruction(CIROperator::TypeAscribe);
        Instruction(ta).type_ascribe_info = {
            .var_inst  = vd,
            .type_inst = type_inst,
        };
    }

    // 3. 有初始值则单独发射 store
    if (vd_ast.expr != nullptr) {
        auto value_inst = build_inst_for_expr(vd_ast.expr);
        auto st = New_Instruction(CIROperator::Store);
        Instruction(st).store_info = {
            .var_inst   = vd,
            .value_inst = value_inst,
        };
    }

    return vd;
}

CIRInstructionRef CIRBuilder::build_block_inst_for_expr(Ast *expr) {
    auto block_inst = Push_Instruction(CIROperator::Block);

    auto value_inst = build_inst_for_expr(expr);
    New_Break(block_inst, value_inst);

    Instruction(block_inst).body_len = curr_instruction_buffer->count - block_inst - 1;
    return block_inst;
}


CIRInstructionRef CIRBuilder::build_inst_for_expr(Ast *expr) {
    switch(expr->type) {
        case AstType_Ident: {
            auto inst = New_Instruction(CIROperator::IdentRef, expr);
            Instruction(inst).ident = expr->Ident.name;
            return inst;
        } break;

        case AstType_FunctionCallExpr: {
            Array<CIRInstructionRef> arg_insts = make_array<CIRInstructionRef>(stage_allocator());
            for (Ast *arg : expr->FunctionCallExpr.args) {
                arg_insts.push_back(build_inst_for_expr(arg));
            }

            auto called_thing_inst = build_inst_for_expr(expr->FunctionCallExpr.func_ident);
            auto call_inst = New_Instruction(CIROperator::Call, expr);
            Instruction(call_inst).call_info = {
                .called_thing = called_thing_inst,
                .arg_insts = array_copy(&arg_insts, permanent_allocator()),
            };
            return call_inst;
        } break;

        case AstType_BinaryExpr: {
            auto left_inst  = build_inst_for_expr(expr->BinaryExpr.left);
            auto right_inst = build_inst_for_expr(expr->BinaryExpr.right);

            auto bin = New_Instruction(CIROperator::Binary, expr);
            Instruction(bin).binary_info = {
                .op = expr->BinaryExpr.op,
                .left_inst  = left_inst,
                .right_inst = right_inst,
            };
            return bin;
        } break;

        case AstType_UnaryExpr: {
            auto operand_inst = build_inst_for_expr(expr->UnaryExpr.operand);
            auto un = New_Instruction(CIROperator::Unary, expr);
            Instruction(un).unary_info = {
                .op = expr->UnaryExpr.op,
                .operand_inst = operand_inst,
            };
            return un;
        } break;

        case AstType_CastExpr: {
            auto value_inst = build_inst_for_expr(expr->CastExpr.expr);
            auto target_inst = build_inst_for_expr(expr->CastExpr.target_type_ast);
            auto cast = New_Instruction(CIROperator::Cast, expr);
            Instruction(cast).cast_info = {
                .expr_inst = value_inst,
                .target_type_inst = target_inst,
            };
            return cast;
        } break;

        case AstType_Constant: {
            auto c = New_Instruction(CIROperator::ConstantValue, expr);
            Instruction(c).imm_val = expr->Constant.value;
            return c;
        } break;

        case AstType_StructInitExpr: {
            auto struct_type_inst = build_inst_for_expr(expr->StructInitExpr.struct_type_ident);

            Array<CIRInstructionRef> field_insts = make_array<CIRInstructionRef>(stage_allocator());
            for (Ast *field_init : expr->StructInitExpr.field_inits) {
                field_insts.push_back(build_inst_for_expr(field_init));
            }

            auto init = New_Instruction(CIROperator::StructInit, expr);
            Instruction(init).struct_init_info = {
                .struct_type_inst = struct_type_inst,
                .field_init_insts = array_copy(&field_insts, permanent_allocator()),
            };
            return init;
        } break;

        case AstType_FieldAccess: {
            auto parent_inst = build_inst_for_expr(expr->FieldAccess.parent);
            auto fa = New_Instruction(CIROperator::FieldAccess, expr);
            Instruction(fa).field_access_info = {
                .parent_inst = parent_inst,
                .field_name = expr->FieldAccess.field_name,
            };
            return fa;
        } break;

        case AstType_ArrayInitExpr: {
            Array<CIRInstructionRef> element_insts = make_array<CIRInstructionRef>(stage_allocator());
            for (Ast *elem : expr->ArrayInitExpr.elements) {
                element_insts.push_back(build_inst_for_expr(elem));
            }

            auto init = New_Instruction(CIROperator::ArrayInit, expr);
            Instruction(init).array_init_info = {
                .element_insts = array_copy(&element_insts, permanent_allocator()),
            };
            return init;
        } break;

        case AstType_IndexExpr: {
            auto array_inst = build_inst_for_expr(expr->IndexExpr.array_var_expr);
            auto index_inst = build_inst_for_expr(expr->IndexExpr.index_expr);
            auto idx = New_Instruction(CIROperator::Index, expr);
            Instruction(idx).index_info = {
                .array_inst = array_inst,
                .index_inst = index_inst,
            };
            return idx;
        } break;

        case AstType_StringLiteralExpr: {
            auto s = New_Instruction(CIROperator::StringLiteral, expr);
            Instruction(s).ident = expr->StringLiteralExpr.str;
            return s;
        } break;

        case AstType_EasyType: {
            auto t = New_Instruction(CIROperator::TypeKind, expr);
            Instruction(t).type_kind_info = {
                .kind = expr->EasyType.kind,
            };
            return t;
        } break;

        case AstType_PointerType: {
            auto pointed_inst = build_inst_for_expr(expr->PointerType.pointed_type_ast);
            auto pt = New_Instruction(CIROperator::PointerType, expr);
            Instruction(pt).pointer_type_info = {
                .pointed_type_inst = pointed_inst,
            };
            return pt;
        } break;

        case AstType_ArrayType: {
            auto elem_inst = build_inst_for_expr(expr->ArrayType.element_type_ast);
            auto count_inst = build_inst_for_expr(expr->ArrayType.count_expr);
            auto at = New_Instruction(CIROperator::ArrayType, expr);
            Instruction(at).array_type_info = {
                .element_type_inst = elem_inst,
                .count_inst = count_inst,
            };
            return at;
        } break;

        case AstType_SliceType: {
            auto elem_inst = build_inst_for_expr(expr->SliceType.element_type_ast);
            auto st = New_Instruction(CIROperator::SliceType, expr);
            Instruction(st).slice_type_info = {
                .element_type_inst = elem_inst,
            };
            return st;
        } break;

        case AstType_StructDeclValue: {
            CIRStructDeclInfo info = {};

            auto scope_guard = ScopeGuard(this, expr);
            
            
            Array<CIRStructFieldDecl> fields = make_array<CIRStructFieldDecl>(stage_allocator());
            for (Ast *field : expr->StructDeclValue.fields) {
                XP_ASSERT_DEFAULT(field->type == AstType_StructField);
                auto type_inst = build_inst_for_expr(field->StructField.type_ast);
                CIRStructFieldDecl fd = {};
                fd.name = field->StructField.name;
                fd.type_inst = type_inst;
                fields.push_back(fd);
            }
            info.fields = array_copy(&fields, permanent_allocator());

            auto decl = New_Instruction(CIROperator::StructDecl, expr);
            Instruction(decl).struct_decl_info = info;
            return decl;
        } break;

        case AstType_EnumDecl: {
            CIREnumDeclInfo info = {};

            auto scope_guard = ScopeGuard(this, expr);

            if (expr->EnumDecl.type_ast != nullptr) {
                info.tag_type_inst = build_inst_for_expr(expr->EnumDecl.type_ast);
            } else {
                auto ti = New_Instruction(CIROperator::IdentRef);
                Instruction(ti).ident = xp_string_c("i32");
                info.tag_type_inst = ti;
            }

            Array<CIREnumFieldDecl> fields = make_array<CIREnumFieldDecl>(stage_allocator());
            for (Ast *field : expr->EnumDecl.fields) {
                CIREnumFieldDecl ef = {};
                if (field->type == AstType_ConstDecl) {
                    ef.name = field->ConstDecl.name;
                    ef.value_inst = build_inst_for_expr(field->ConstDecl.value_ast);
                } else {
                    ef.name = field->Ident.name;
                    ef.value_inst = INVALID_INST; // 自动递增，后端处理
                }
                fields.push_back(ef);
            }
            info.fields = array_copy(&fields, permanent_allocator());

            auto decl = New_Instruction(CIROperator::EnumDecl, expr);
            Instruction(decl).enum_decl_info = info;
            return decl;
        } break;

        
        case AstType_UnionDecl: {
            XP_TODO();
        } break;
        default: {
            UNREACHABLE();
        } break;
    }

    return INVALID_INST;
}








CIRInstructionRef CIRBuilder::Push_Instruction(CIROperator op) {
    CIRInstruction inst = {};
    inst.op = op;
    curr_instruction_buffer->push_back(inst);
    return curr_instruction_buffer->count - 1;
}

CIRInstructionRef CIRBuilder::Alloc_Var(xpString name, bool is_var_arg) {
    isize slot = curr_func ? curr_func->slot_count++ : -1;
    auto vd = New_Instruction(CIROperator::VariableDecl);
    Instruction(vd).var_decl = {
        .name = name,
        .slot = slot,
        .is_var_arg = is_var_arg,
    };
    return vd;
}

CIRInstructionRef CIRBuilder::New_Break(CIRInstructionRef break_block, CIRInstructionRef break_value_inst) {
    auto br = New_Instruction(CIROperator::Break);
    Instruction(br).break_info = {
        .break_block = break_block,
        .break_value_inst = break_value_inst,
    };
    return br;
}



CIRInstructionRef CIRBuilder::New_Instruction(CIROperator op) {
    CIRInstruction inst{};
    inst.op = op;
    curr_instruction_buffer->push_back(inst);
    return curr_instruction_buffer->count - 1;
}

CIRInstructionRef CIRBuilder::New_Instruction(CIROperator op, Ast *ast) {
    auto inst_ref = New_Instruction(op);
    Instruction(inst_ref).span = ast->span;
    return inst_ref;
}


CIRInstruction& CIRBuilder::Instruction(CIRInstructionRef ref) {
    return (*curr_instruction_buffer)[ref];
}



CIRInstructionRef CIRBuilder::Begin_Block() {
    return Push_Instruction(CIROperator::Block);
}

void CIRBuilder::End_Block(CIRInstructionRef block_inst) {
    Instruction(block_inst).body_len = curr_instruction_buffer->count - block_inst - 1;
}




bool CIRBuilder::Enter_Scope(Ast *ast) {
    if (curr_scope == nullptr) return false;

    Scope *child = try_enter_scope(curr_scope, ast);
    if (child == nullptr) return false;

    New_Instruction(CIROperator::EnterScope);
    curr_scope = child;
    return true;
}

bool CIRBuilder::Enter_Scope_Silent(Ast *ast) {
    if (curr_scope == nullptr) return false;

    Scope *child = try_enter_scope(curr_scope, ast);
    if (child == nullptr) return false;

    curr_scope = child;
    return true;
}

void CIRBuilder::Exit_Scope() {
    if (curr_scope == nullptr) return;

    Scope *parent = try_exit_scope(curr_scope);
    if (parent == nullptr) return;   // 不退出全局作用域

    curr_scope = parent;
    New_Instruction(CIROperator::ExitScope);
}


ScopeGuard::ScopeGuard(CIRBuilder *builder, Ast *ast)
    : builder(builder), entered(false)
{
    if (ast != nullptr) {
        entered = builder->Enter_Scope(ast);
    }
}

ScopeGuard::~ScopeGuard() {
    if (entered) {
        builder->Exit_Scope();
    }
}


//
// debug
//


static void dump_inst_compact(CIRFile *file, CIRInstructionRef ref) {
    auto& inst = file->instructions[ref];
    std::print("  %{} = ", ref);

    switch (inst.op) {
    case CIROperator::VariableDecl:
        std::println("VariableDecl({}, slot={})", inst.var_decl.name, inst.var_decl.slot);
        break;
    case CIROperator::ConstDecl:
        std::println("ConstDecl({}, value=%{})", inst.const_decl.ident, inst.const_decl.value_inst);
        break;
    case CIROperator::FunctionDecl: {
        auto& f = inst.func_decl;
        std::print("FunctionDecl({}, return_type=%{}, return_count={}, slot_count={}, is_extern_c={}",
            f.name, f.return_type_inst, f.return_count, f.slot_count, f.is_extern_c);
        if (f.args.count > 0) {
            std::print(", params=[");
            for (isize i = 0; i < f.args.count; i++) {
                if (i > 0) std::print(", ");
                std::print("{} slot={} type=%{}", f.args[i].name, f.args[i].slot, f.arg_type_insts[i]);
            }
            std::print("]");
        }
        std::println(", body=%{})", f.entry_inst);
        break;
    }
    case CIROperator::Block:
        std::println("Block(body_len={})", inst.body_len);
        break;
    case CIROperator::Break:
        if (inst.break_info.break_value_inst != INVALID_INST) {
            std::println("Break(%{}, %{})", inst.break_info.break_block, inst.break_info.break_value_inst);
        } else {
            std::println("Break(%{}, void)", inst.break_info.break_block);
        }
        break;
    case CIROperator::LoopBreak:
        std::println("LoopBreak");
        break;
    case CIROperator::Continue:
        std::println("Continue");
        break;
    case CIROperator::Store:
        std::println("Store(%{}, %{})", inst.store_info.var_inst, inst.store_info.value_inst);
        break;
    case CIROperator::TypeAscribe:
        std::println("TypeAscribe(%{}, %{})", inst.type_ascribe_info.var_inst, inst.type_ascribe_info.type_inst);
        break;
    case CIROperator::Binary:
        std::println("Binary({}, %{}, %{})",
            token_strings[(int)inst.binary_info.op],
            inst.binary_info.left_inst, inst.binary_info.right_inst);
        break;
    case CIROperator::Unary:
        std::println("Unary({}, %{})",
            token_strings[(int)inst.unary_info.op], inst.unary_info.operand_inst);
        break;
    case CIROperator::Call:
        std::print("Call(%{}, [", inst.call_info.called_thing);
        for (isize i = 0; i < inst.call_info.arg_insts.count; i++) {
            if (i > 0) std::print(", ");
            std::print("%{}", inst.call_info.arg_insts[i]);
        }
        std::println("])");
        break;
    case CIROperator::Cast:
        std::println("Cast(%{}, %{})", inst.cast_info.expr_inst, inst.cast_info.target_type_inst);
        break;
    case CIROperator::FieldAccess:
        std::println("FieldAccess({}, %{})", inst.field_access_info.field_name, inst.field_access_info.parent_inst);
        break;
    case CIROperator::Index:
        std::println("Index(%{}, %{})", inst.index_info.array_inst, inst.index_info.index_inst);
        break;
    case CIROperator::StructInit:
        std::print("StructInit(%{}, [", inst.struct_init_info.struct_type_inst);
        for (isize i = 0; i < inst.struct_init_info.field_init_insts.count; i++) {
            if (i > 0) std::print(", ");
            std::print("%{}", inst.struct_init_info.field_init_insts[i]);
        }
        std::println("])");
        break;
    case CIROperator::ArrayInit:
        std::print("ArrayInit([");
        for (isize i = 0; i < inst.array_init_info.element_insts.count; i++) {
            if (i > 0) std::print(", ");
            std::print("%{}", inst.array_init_info.element_insts[i]);
        }
        std::println("])");
        break;
    case CIROperator::ConstantValue:
        std::println("Const({})", inst.imm_val);
        break;
    case CIROperator::StringLiteral:
        std::println("StringLiteral({})", inst.ident);
        break;
    case CIROperator::IdentRef:
        std::println("IdentRef({})", inst.ident);
        break;
    case CIROperator::TypeKind:
        std::println("TypeKind({})", (int)inst.type_kind_info.kind);
        break;
    case CIROperator::PointerType:
        std::println("PointerType(%{})", inst.pointer_type_info.pointed_type_inst);
        break;
    case CIROperator::ArrayType:
        std::println("ArrayType(%{}, %{})", inst.array_type_info.element_type_inst, inst.array_type_info.count_inst);
        break;
    case CIROperator::SliceType:
        std::println("SliceType(%{})", inst.slice_type_info.element_type_inst);
        break;
    case CIROperator::StructDecl: {
        auto& si = inst.struct_decl_info;
        std::print("StructDecl(");
        if (si.name.length > 0) std::print("{}, ", si.name);
        for (isize i = 0; i < si.fields.count; i++) {
            if (i > 0) std::print(", ");
            auto& f = si.fields[i];
            std::print("{}: %{}", f.name, f.type_inst);
        }
        std::println(")");
        break;
    }
    case CIROperator::EnumDecl: {
        auto& ei = inst.enum_decl_info;
        std::print("EnumDecl(");
        if (ei.name.length > 0) std::print("{}, ", ei.name);
        std::print("tag=%{}, ", ei.tag_type_inst);
        for (isize i = 0; i < ei.fields.count; i++) {
            if (i > 0) std::print(", ");
            auto& f = ei.fields[i];
            if (f.value_inst != INVALID_INST) {
                std::print("{}: %{}", f.name, f.value_inst);
            } else {
                std::print("{}: auto", f.name);
            }
        }
        std::println(")");
        break;
    }
    case CIROperator::If:
        std::println("If(cond=%{}, then=%{}, else={})",
            inst.if_info.condition_inst, inst.if_info.true_block_inst,
            inst.if_info.false_block_inst == INVALID_INST ? "none" : std::format("%{}", inst.if_info.false_block_inst));
        break;
    case CIROperator::Loop:
        std::println("Loop(cond=%{}, body=%{})",
            inst.loop_info.condition_inst, inst.loop_info.loop_body_inst);
        break;
    case CIROperator::EnterScope:
        std::println("EnterScope");
        break;
    case CIROperator::ExitScope:
        std::println("ExitScope");
        break;
    default:
        std::println("{}", string(inst.op));
        break;
    }
}

void dump_cir_file(CIRFile *file) {
    std::println("CIRFile {{");
    for (isize i = 0; i < file->instructions.count; i++) {
        dump_inst_compact(file, i);
    }
    std::println("}}");

    std::println("\n--- total instructions: {} ---", file->instructions.count);
}
