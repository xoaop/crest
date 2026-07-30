// ============================================================================
// Block 分类速查
// ============================================================================
// comptime block (is_comptime=true) — 编译期求值，不生成 LLVM IR
//   const 声明右值
//   函数定义外层包装块
//   函数返回类型表达式
//   参数类型表达式
//   变量类型标注
//   struct 声明
//   struct 字段类型
//   enum 类型表达式
//   enum 声明
//
// runtime block (is_comptime=false) — 必须生成 LLVM IR
//   函数体
//   嵌套块 { ... }
//   for 循环体
// ============================================================================



#include <print>
#include <cstring>

#include "cir_builder.hpp"
#include "context.hpp"





// CIRInstruction::CIRInstruction(const CIRInstruction& other) {
//     memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(CIRInstruction));
// }

// CIRInstruction::CIRInstruction(CIRInstruction&& other) {
//     memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(CIRInstruction));
// }

// CIRInstruction& CIRInstruction::operator=(const CIRInstruction& other) {
//     if (this == &other) return *this;
//     memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(CIRInstruction));
//     return *this;
// }

// CIRInstruction& CIRInstruction::operator=(CIRInstruction&& other) {
//     if (this == &other) return *this;
//     memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(CIRInstruction));
//     return *this;
// }

CIRBuilder::CIRBuilder(xpAllocator allocator) {
    loop_body_block_stack = make_array<CIRInstructionRef>(allocator);
    loop_stack = make_array<CIRInstructionRef>(allocator);
}

CIRBuilder::~CIRBuilder() {
    array_free(&loop_body_block_stack);
    array_free(&loop_stack);
}




void CIRBuilder::build_cir_package(Package *pkg) {
    curr_scope = &pkg->package_scope;

    pkg->cir_package = make_cir_package(permanent_allocator());
    curr_pkg = &pkg->cir_package;
    curr_instruction_buffer = &pkg->cir_package.instructions;

    auto& cir_pkg = pkg->cir_package;

    for(AstFile& ast_file : pkg->ast_files) {

        curr_ast_file = &ast_file;
        curr_scope = &ast_file.file_scope;

        CIRFileRange range;
        range.start = cir_pkg.instructions.count();
        range.file_scope = &ast_file.file_scope;
        cir_pkg.file_ranges.push_back(range);

        for(Ast *ast : ast_file.top_levels) {
            CIRInstructionRef top_inst = INVALID_INST;

            switch(ast->type) {
                case AstType_ConstDecl: {
                    if(ast->ConstDecl.value_ast->type == AstType_Import) {
                        break;
                    }

                    top_inst = build_inst_for_const_decl(ast);
                } break;

                default: {
                    context()->reporter.report_error(SourceLocation(ast_file.source_code, ast->src_loc.span), "Unsupported top-level AST type for CIR generation");
                } break;
            }

            if(top_inst != INVALID_INST) {
                cir_pkg.top_level_insts.push_back(top_inst);
            }
        }
    }

    curr_scope = &pkg->package_scope;

}


CIRInstructionRef CIRBuilder::build_inst_for_const_decl(Ast *const_decl_ast) {
    XP_ASSERT_DEFAULT(const_decl_ast->type == AstType_ConstDecl);
    SymbolInfo *sym = (const_decl_ast->ast_symbol)();
    XP_ASSERT_DEFAULT(sym != nullptr);

    auto const_decl = New_Instruction(CIROperator::ConstDecl, const_decl_ast);

    auto value_ast = const_decl_ast->ConstDecl.value_ast;
    curr_const_sym = const_decl_ast->ast_symbol;
    CIRInstructionRef value_inst = build_block_inst_for_expr(value_ast, true, true);
    curr_const_sym = {};

    Instruction(const_decl).const_decl = {
        .ident = const_decl_ast->ConstDecl.name,
        .symbol = const_decl_ast->ast_symbol,
        .value_inst = value_inst,
    };


    sym->val({
        .cir_package = curr_pkg,
        .inst_ref = const_decl,
    });

    return const_decl;
}




CIRInstructionRef CIRBuilder::build_func_decl(Ast *fd, std::optional<SymbolInfoRef> func_sym) {
    XP_ASSERT_DEFAULT(fd->type == AstType_FunctionDeclValue);

    auto saved_curr_func = curr_func;
    auto saved_curr_func_body_block = curr_func_body_block;
    defer({
        curr_func = saved_curr_func;
        curr_func_body_block = saved_curr_func_body_block;
    });

    auto define_func_block = Begin_Block(fd, true, true);

    CIRFunction func = {};
    func.return_count = 1;
    func.is_extern_c = fd->FunctionDeclValue.is_extern_c;
    func.is_comptime = fd->FunctionDeclValue.is_comptime;
    func.is_builtin = fd->FunctionDeclValue.is_builtin;
    func.arg_type_insts = make_array<CIRInstructionRef>(permanent_allocator());
    func.arg_decl_insts = make_array<CIRInstructionRef>(permanent_allocator());
    // func.args = make_array<CIRVariableDecl>(permanent_allocator());



    curr_func = &func;



    // 1. 每个参数：类型 block + 分配 slot
    auto param_decls = make_array<CIRVariableDecl>(stage_allocator());
    for(isize i = 0; i < fd->FunctionDeclValue.params.count; i++) {
        Ast *param = fd->FunctionDeclValue.params[i];
        XP_ASSERT_DEFAULT(param->type == AstType_ParamDecl);

        auto param_type = INVALID_INST;
        if(!param->ParamDecl.is_var_arg) {
            param_type = build_block_inst_for_expr(param->ParamDecl.type_ast, true, true);
        }

        CIRVariableDecl var = {};
        var.name = param->ParamDecl.name;
        var.symbol = param->ast_symbol;
        var.slot = i;                       // 参数槽位: 0, 1, 2, ...
        var.is_var_arg = param->ParamDecl.is_var_arg;
        var.is_comptime = param->ParamDecl.is_comptime;
        param_decls.push_back(var);
        // func.args.push_back(var);
        func.arg_type_insts.push_back(param_type);
    }


    // 返回类型 block
    if(fd->FunctionDeclValue.infer_return_type) {
        func.return_type_inst = INVALID_INST;
    } else if(fd->FunctionDeclValue.return_type_ast != nullptr) {
        func.return_type_inst = build_block_inst_for_expr(fd->FunctionDeclValue.return_type_ast, true, true);
    } else {
        auto rt_block = Begin_Block(fd, true, true);

        auto void_const = New_Instruction(CIROperator::ConstantValue, fd);
        auto val = make_value(type_type());
        val.type_val(easy_type(Type_void));
        Instruction(void_const).imm_val = val;
        
        New_Break(rt_block, void_const, fd);

        End_Block(rt_block);

        func.return_type_inst = rt_block;
    }
    
    
    
    
    func.body_inst = INVALID_INST;
    auto func_inst = New_Instruction(CIROperator::FunctionDecl, fd);
    
    Instruction(func_inst).symbol = func_sym.value_or(SymbolInfoRef{});
    Instruction(func_inst).func_decl.return_type_inst = func.return_type_inst;

    Instruction(func_inst).func_decl = func;




    // TODO: externC特殊处理, 后面完善些, 现在太丑
    if(!func.is_extern_c && !func.is_builtin) {
        auto body_inst = Begin_Block(fd->FunctionDeclValue.block, false, false);
        curr_func_body_block = body_inst;

        {
            ScopeGuard _func_scope_guard(this, fd);
            for(isize i = 0; i < param_decls.count; i++) {
                auto& var = param_decls[i];

                // NOTE: 函数参数变量的no_zero_init为false, 因为一定有值, 没必要
                auto vd = Alloc_Var(var.name, var.is_var_arg, false, fd->FunctionDeclValue.params[i]);
                Instruction(vd).var_decl.is_comptime = var.is_comptime;
                func.arg_decl_insts.push_back(vd);

                auto param_type = func.arg_type_insts[i];
                if(param_type != INVALID_INST) {
                    auto ta = New_Instruction(CIROperator::TypeAscribe, fd->FunctionDeclValue.params[i]);
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

        // 补齐 FunctionDecl 中之前占位的字段
        Instruction(func_inst).func_decl.body_inst = body_inst;
        Instruction(func_inst).func_decl.slot_count = func.slot_count;
        Instruction(func_inst).func_decl.arg_decl_insts = func.arg_decl_insts;
    }


    New_Break(define_func_block, func_inst, fd);

    End_Block(define_func_block);

    return define_func_block;
}


CIRInstructionRef CIRBuilder::build_inst_for_ast_block(Ast *block_ast, bool new_ir_block) {
    XP_ASSERT_DEFAULT(block_ast->type == AstType_Block);

    
    CIRInstructionRef block_inst = INVALID_INST;
    if(new_ir_block) {
        block_inst = Begin_Block(block_ast, false, false);
    }


    {
        auto _scope_guard = ScopeGuard(this, block_ast);
        for(Ast *stmt : block_ast->Block.statements) {
            build_inst_for_stmt(stmt);
        } 
    }


    if(new_ir_block) {
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
            auto ptr_inst   = build_ptr_inst_for_expr(stmt->Assignment.left_var_expr);
            auto value_inst = build_inst_for_expr(stmt->Assignment.right_expr);

            // auto determine_type_for_value_inst = New_Instrcution(CIROperator::DetermineType, stmt);
            // Instruction(determine_type_for_value_inst).determine_type_info = {
            //     .determining_inst = value_inst,
            //     .type_inst = INVALID_INST,
            // };

            auto typeof_inst = New_Instruction(CIROperator::TypeOfInstResult, stmt);
            Instruction(typeof_inst).type_of_inst_result_info = {
                .target_inst = ptr_inst
            };

            auto determine_type = New_Instruction(CIROperator::DetermineType, stmt);
            Instruction(determine_type).determine_type_info = {
                .determining_inst = value_inst,
                .type_inst = typeof_inst,
            };

            auto st = New_Instruction(CIROperator::Store, stmt);
            Instruction(st).store_info = {
                .var_inst   = ptr_inst,
                .value_inst = value_inst,
            };
        } break;

        case AstType_IfStmt: {
            auto& if_stmt = stmt->IfStmt;

            auto cond_inst = build_inst_for_expr(if_stmt.condition);

            // CondBr 先占位，then/else 紧跟其后作为 body
            auto condbr_inst = New_Instruction(CIROperator::CondBr, stmt);

            auto then_inst = build_inst_for_ast_block(if_stmt.then_block, true);


            auto else_inst = INVALID_INST;
            if(if_stmt.else_block != nullptr) {
                else_inst = build_inst_for_ast_block(if_stmt.else_block, true);
            } else {
                // 没有 else 块的 if, else_inst 指向一个空块
                else_inst = Begin_Block(stmt, false, false);
                New_Break(else_inst, INVALID_INST, stmt);
                End_Block(else_inst);
            }

            Instruction(condbr_inst).condbr_info = {
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
            
            
            auto loop_inst = Begin_Loop(stmt);
            
            auto cond = INVALID_INST;
            if(stmt->ForStmt.condition != nullptr) {
                cond = build_inst_for_expr(stmt->ForStmt.condition);
            } else {
                // 无条件循环, 构造一个永真条件
                
                auto true_val = make_value(easy_type(Type_bool));
                true_val.bool_val(true);
                
                cond = New_Instruction(CIROperator::ConstantValue, stmt);
                Instruction(cond).imm_val = true_val;
            }

            // CondBr 先占位，then/else 紧跟其后作为 body
            auto condbr_for_loop = New_Instruction(CIROperator::CondBr, stmt);

            auto then = INVALID_INST;
            {
                then = Begin_Block(stmt, false, false);
                defer(End_Block(then));

                // TODO: ABSTRACT
                loop_body_block_stack.push_back(then);
                defer({
                    XP_ASSERT_DEFAULT(loop_body_block_stack.back() == then);
                    loop_body_block_stack.pop_back();
                });

                build_inst_for_ast_block(stmt->ForStmt.body, false);
            }

            auto else_blk = INVALID_INST;
            {
                else_blk = Begin_Block(stmt, false, false);
                defer(End_Block(else_blk));

                New_Break(loop_inst, INVALID_INST, stmt); // TODO: 妥善处理无返回值的Break
            }

            Instruction(condbr_for_loop).condbr_info = {
                .condition_inst = cond,
                .true_block_inst = then,
                .false_block_inst = else_blk,
            };

            if(stmt->ForStmt.post != nullptr) {
                build_inst_for_stmt(stmt->ForStmt.post);
            }

            End_Loop(loop_inst);

        } break;

        case AstType_ReturnStmt: {
            build_inst_for_return_stmt(stmt);
        } break;

        case AstType_FunctionCallExpr: {
            build_inst_for_expr(stmt);
        } break;

        case AstType_Block: {
            build_inst_for_ast_block(stmt, true);
        } break;

        case AstType_Break: {
            New_Break(loop_stack.back(), INVALID_INST, stmt); // TODO: 妥善处理无返回值的Break
        } break;

        case AstType_Continue: {
            New_Break(loop_body_block_stack.back(), INVALID_INST, stmt); // TODO: 妥善处理无返回值的Break
        } break;

        case AstType_ConstDecl: {
            build_inst_for_const_decl(stmt);
        } break;

        default: {
            UNREACHABLE();
        } break;
    }

    // TODO: 妥善处理
    return INVALID_INST;
}

CIRInstructionRef CIRBuilder::build_inst_for_var_decl(Ast *var_decl_ast) {
    XP_ASSERT_DEFAULT(var_decl_ast->type == AstType_VariableDecl);

    auto& vd_ast = var_decl_ast->VariableDecl;

    // 1. 变量声明（分配存储空间）
    auto vd = Alloc_Var(vd_ast.var_name, false, vd_ast.no_zero_init, var_decl_ast);
    Instruction(vd).src_loc = var_decl_ast->src_loc;

    // 2. 类型归属 — 仅当有显式类型标注时才发射
    //    没有 TypeAscribe 意味着类型由后续 store 的值推导
    CIRInstructionRef type_block_inst = INVALID_INST;
    if(vd_ast.type_ast != nullptr) {
        type_block_inst = build_block_inst_for_expr(vd_ast.type_ast, true, true);
        auto ta = New_Instruction(CIROperator::TypeAscribe, vd_ast.type_ast);
        Instruction(ta).type_ascribe_info = {
            .var_inst  = vd,
            .type_inst = type_block_inst,
        };
    }

    // 3. 有初始值则发射 DetermineType + Store
    if(vd_ast.expr != nullptr) {
        auto value_inst = build_inst_for_expr(vd_ast.expr);

        auto determine_type = New_Instruction(CIROperator::DetermineType, vd_ast.expr);
        CIRInstructionRef type_inst = (type_block_inst != INVALID_INST) ? type_block_inst : INVALID_INST;
        Instruction(determine_type).determine_type_info = {
            .determining_inst = value_inst,
            .type_inst = type_inst,
        };

        // 无显式类型标注时，补 TypeOfInstResult + TypeAscribe 以分配内存
        if(type_block_inst == INVALID_INST) {
            auto type_of = New_Instruction(CIROperator::TypeOfInstResult, vd_ast.expr);
            Instruction(type_of).type_of_inst_result_info = {
                .target_inst = value_inst
            };
            auto ta = New_Instruction(CIROperator::TypeAscribe, vd_ast.expr);
            Instruction(ta).type_ascribe_info = {
                .var_inst  = vd,
                .type_inst = type_of,
            };
        }

        auto st = New_Instruction(CIROperator::Store, var_decl_ast);
        Instruction(st).store_info = {
            .var_inst   = vd,
            .value_inst = value_inst,
        };
    }

    return vd;
}

void CIRBuilder::build_inst_for_return_stmt(Ast *return_stmt_ast) {
    XP_ASSERT_DEFAULT(return_stmt_ast->type == AstType_ReturnStmt);
    CIRInstructionRef value_inst = INVALID_INST;
    if(return_stmt_ast->ReturnStmt.expr != nullptr) {
        value_inst = build_inst_for_expr(return_stmt_ast->ReturnStmt.expr);
    }

    if(value_inst != INVALID_INST) {
        // 如果 return 语句有表达式, 检查表达式类型是否和函数返回类型兼容
        auto determine_type = New_Instruction(CIROperator::DetermineType, return_stmt_ast);
        Instruction(determine_type).determine_type_info = {
            .determining_inst = value_inst,
            .type_inst = curr_func->return_type_inst,
        };

    } else {
        // 没有返回值的 return, 判断函数返回类型是否为 void

        // 构造一个字面量, 类型为void, 
        // 虽然这个压根不存在, 但它的类型信息可以用来判断函数返回类型是否为 void
        // 搭了determine_type需要的结构, 以复用错误检查逻辑
        auto void_type_inst = New_Instruction(CIROperator::ConstantValue, return_stmt_ast);
        auto void_type_val = make_value();
        void_type_val.set_type(easy_type(Type_void)); // 这个值本身不存在, 但我们需要它的类型信息来判断函数返回类型是否为 void
        Instruction(void_type_inst).imm_val = void_type_val;

        auto determine_type = New_Instruction(CIROperator::DetermineType, return_stmt_ast);
        Instruction(determine_type).determine_type_info = {
            .determining_inst = void_type_inst,
            .type_inst = curr_func->return_type_inst,
        };

    }


    New_Break(curr_func_body_block, value_inst, return_stmt_ast);
}





CIRInstructionRef CIRBuilder::build_block_inst_for_expr(Ast *expr, bool is_comptime_block, bool immediate_eval) {
    auto block_inst = Begin_Block(expr, is_comptime_block, immediate_eval);

    auto value_inst = build_inst_for_expr(expr);
    New_Break(block_inst, value_inst, expr);

    End_Block(block_inst);
    return block_inst;
}


CIRInstructionRef CIRBuilder::build_ptr_inst_for_expr(Ast *expr) {
    switch(expr->type) {
        case AstType_Ident: {
            auto inst = New_Instruction(CIROperator::IdentRef, expr);
            Instruction(inst).symbol = expr->ast_symbol;
            return inst;
        } break;

        case AstType_FieldAccess: {
            auto parent_ptr = build_ptr_inst_for_expr(expr->FieldAccess.parent);
            auto fa = New_Instruction(CIROperator::FieldPtr, expr);
            Instruction(fa).field_access_info = {
                .parent_inst = parent_ptr,
                .field_name = expr->FieldAccess.field_name,
            };
            return fa;
        } break;

        case AstType_IndexExpr: {
            auto array_ptr = build_ptr_inst_for_expr(expr->IndexExpr.array_var_expr);
            auto index_inst = build_inst_for_expr(expr->IndexExpr.index_expr);

            // TODO: SYMPLIFY
            auto determine_type = New_Instruction(CIROperator::DetermineType, expr);
            Instruction(determine_type).determine_type_info = {
                .determining_inst = index_inst,
                .type_inst = INVALID_INST,
            };

            auto idx = New_Instruction(CIROperator::IndexPtr, expr);
            Instruction(idx).index_info = {
                .array_inst = array_ptr,
                .index_inst = index_inst,
            };
            return idx;
        } break;

        case AstType_UnaryExpr: {
            // ^ptr 左值: 取指针值作为地址，产生左值
            if(expr->UnaryExpr.op == TokenType::Caret) {
                auto ptr_val = build_ptr_inst_for_expr(expr->UnaryExpr.operand);
                auto deref = New_Instruction(CIROperator::Deref, expr);
                Instruction(deref).deref_info.operand_inst = ptr_val;
                return deref;
            }
            return build_inst_for_expr(expr);
        } break;

        default: {
            return build_inst_for_expr(expr);
        } break;
    }
}



CIRInstructionRef CIRBuilder::build_inst_for_expr(Ast *expr) {
    
    CIRInstructionRef result = INVALID_INST;
    switch(expr->type) {
        case AstType_Ident: {
            SymbolInfo *sym = (expr->ast_symbol)();
            if(sym && sym->is_var_decl()) {
                auto ref = New_Instruction(CIROperator::IdentRef, expr);
                Instruction(ref).symbol = expr->ast_symbol;
                auto load = New_Instruction(CIROperator::Load, expr);
                Instruction(load).load_info = { .ptr_inst = ref };

                result = load;
                break;
            }
            auto inst = New_Instruction(CIROperator::IdentVal, expr);
            Instruction(inst).symbol = expr->ast_symbol;

            result = inst;
        } break;

        case AstType_FunctionCallExpr: {
                // func_type = TypeOfInstResult(func_val)
                auto called_thing_inst = build_inst_for_expr(expr->FunctionCallExpr.func_ident);
                auto typeof_func = New_Instruction(CIROperator::TypeOfInstResult, expr);
                Instruction(typeof_func).type_of_inst_result_info = {
                    .target_inst = called_thing_inst
                };

                // 每个实参: FunParamType + DetermineType
                Array<CIRInstructionRef> arg_insts = make_array<CIRInstructionRef>(stage_allocator());
                for (isize i = 0; i < expr->FunctionCallExpr.args.count; i++) {
                    Ast *arg = expr->FunctionCallExpr.args[i];
                    auto arg_inst = build_inst_for_expr(arg);

                    auto fpt = New_Instruction(CIROperator::FuncParamType, arg);
                    Instruction(fpt).func_param_type_info = {
                        .type_of_func_type_inst = typeof_func,
                        .param_index = i,
                    };

                    auto determine_type = New_Instruction(CIROperator::DetermineType, arg);
                    Instruction(determine_type).determine_type_info = {
                        .determining_inst = arg_inst,
                        .type_inst = fpt,
                    };

                    arg_insts.push_back(arg_inst);
                }

                auto call_inst = New_Instruction(CIROperator::Call, expr);
                Instruction(call_inst).call_info = {
                    .called_thing = called_thing_inst,
                    .arg_insts = arg_insts.copy(permanent_allocator()),
                };

                result = call_inst;
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
            result = bin;
        } break;

        case AstType_UnaryExpr: {
            auto op = expr->UnaryExpr.op;

            // &x: 左值取地址 → 指针值
            if(op == TokenType::And) {
                auto lval = build_ptr_inst_for_expr(expr->UnaryExpr.operand);
                auto addr_of = New_Instruction(CIROperator::AddrOf, expr);
                Instruction(addr_of).addr_of_info.lval_inst = lval;
                result = addr_of; break;
            }
            // ^ptr: 对指针值 Load
            if(op == TokenType::Caret) {
                auto ptr_val_inst = build_inst_for_expr(expr->UnaryExpr.operand);
                auto load = New_Instruction(CIROperator::Deref, expr);
                Instruction(load).deref_info.operand_inst = ptr_val_inst;
                result = load; break;
            }

            // -, !, ~, 等其他一元运算
            auto operand_inst = build_inst_for_expr(expr->UnaryExpr.operand);
            auto un = New_Instruction(CIROperator::Unary, expr);
            Instruction(un).unary_info = {
                .op = op,
                .operand_inst = operand_inst,
            };
            result = un;
        } break;

        case AstType_CastExpr: {
            auto value_inst = build_inst_for_expr(expr->CastExpr.expr);
            auto target_inst = build_inst_for_expr(expr->CastExpr.target_type_ast);
            auto cast = New_Instruction(CIROperator::Cast, expr);
            Instruction(cast).cast_info = {
                .expr_inst = value_inst,
                .target_type_inst = target_inst,
            };
            result = cast;
        } break;

        case AstType_Constant: {
            auto c = New_Instruction(CIROperator::ConstantValue, expr);
            Instruction(c).imm_val = expr->Constant.value;
            result = c;
        } break;

        case AstType_StructInitExpr: {
            auto struct_type_inst = build_block_inst_for_expr(expr->StructInitExpr.struct_type_ident, true, true);

            Array<CIRInstructionRef> field_insts = make_array<CIRInstructionRef>(stage_allocator());
            for (isize i = 0; i < expr->StructInitExpr.field_inits.count; i++) {
                Ast *field_init = expr->StructInitExpr.field_inits[i];

                auto field_value_inst = build_inst_for_expr(field_init);


                // 确定化各个field的类型
                auto field_type_of_struct = New_Instruction(CIROperator::FieldTypeOfStruct, field_init);
                Instruction(field_type_of_struct).field_type_of_struct_info = {
                    .struct_type_inst = struct_type_inst,
                    .field_index = i,
                };

                auto determine_type = New_Instruction(CIROperator::DetermineType, field_init);
                Instruction(determine_type).determine_type_info = {
                    .determining_inst = field_value_inst,
                    .type_inst = field_type_of_struct
                };

                field_insts.push_back(field_value_inst);
            }

            auto init = New_Instruction(CIROperator::StructInit, expr);
            Instruction(init).struct_init_info = {
                .struct_type_inst = struct_type_inst,
                .field_init_insts = field_insts.copy(permanent_allocator()),
            };
            result = init;
        } break;

        case AstType_FieldAccess: {
            auto parent_inst = build_inst_for_expr(expr->FieldAccess.parent);
            auto fa = New_Instruction(CIROperator::FieldAccess, expr);
            Instruction(fa).field_access_info = {
                .parent_inst = parent_inst,
                .field_name = expr->FieldAccess.field_name,
            };
            result = fa;
        } break;

        case AstType_ArrayInitExpr: {
            Array<CIRInstructionRef> element_insts = make_array<CIRInstructionRef>(stage_allocator());
            for (Ast *elem : expr->ArrayInitExpr.elements) {
                element_insts.push_back(build_inst_for_expr(elem));
            }

            auto init = New_Instruction(CIROperator::ArrayInit, expr);
            Instruction(init).array_init_info = {
                .element_insts = element_insts.copy(permanent_allocator()),
            };
            result = init;
        } break;

        case AstType_IndexExpr: {
            auto array_inst = build_inst_for_expr(expr->IndexExpr.array_var_expr);
            auto index_inst = build_inst_for_expr(expr->IndexExpr.index_expr);

            // TODO: SYMPLIFY 确定化 index_inst 的类型
            auto determine_type = New_Instruction(CIROperator::DetermineType, expr);
            Instruction(determine_type).determine_type_info = {
                .determining_inst = index_inst,
                .type_inst = INVALID_INST,
            };

            auto idx = New_Instruction(CIROperator::Index, expr);
            Instruction(idx).index_info = {
                .array_inst = array_inst,
                .index_inst = index_inst,
            };
            result = idx;
        } break;

        case AstType_StringLiteralExpr: {
            xpString str = expr->StringLiteralExpr.str;

            curr_pkg->string_literals.push_back(str);

            auto count_str = str.length;

            // 把字符串放到静态内存里
            auto ptr = context()->static_mem.alloc_bytes(count_str + 1, 1); // +1 for null terminator
            ptr.store_bytes(str.c_str, count_str);
            ptr.mem->write_bytes(ptr.offset + count_str, "\0", 1);

            // 创建 IdentVal 引用 string 符号（来自 std/builtin/string.cst），
            // 作为 StringLiteral 的数据依赖，analyze 阶段通过数据流获取类型
            auto string_ident = New_Instruction(CIROperator::IdentVal, expr);
            SymbolInfo *string_sym = find_symbol_until_global(&context()->global_blank_package->package_scope, xp_string_c("string"));
            Instruction(string_ident).symbol = SymbolInfoRef{&context()->global_blank_package->package_scope.symbols, xp_string_c("string")};

            auto s = New_Instruction(CIROperator::StringLiteral, expr);
            Instruction(s).string_literal_info.data = ptr;
            Instruction(s).string_literal_info.count = count_str;
            Instruction(s).string_literal_info.str = str;
            Instruction(s).string_literal_info.string_type_inst = string_ident;

            result = s;
        } break;

        case AstType_EasyType: {
            auto t = New_Instruction(CIROperator::ConstantValue, expr);
            auto val = make_value(type_type());
            val.type_val(easy_type(expr->EasyType.kind));
            Instruction(t).imm_val = val;
            result = t;
        } break;

        case AstType_PointerType: {
            auto pointed_inst = build_inst_for_expr(expr->PointerType.pointed_type_ast);
            auto pt = New_Instruction(CIROperator::PointerType, expr);
            Instruction(pt).pointer_type_info = {
                .pointed_type_inst = pointed_inst,
            };
            result = pt;
        } break;

        case AstType_ArrayType: {
            auto elem_inst = build_inst_for_expr(expr->ArrayType.element_type_ast);
            auto count_inst = build_inst_for_expr(expr->ArrayType.count_expr);
            auto at = New_Instruction(CIROperator::ArrayType, expr);
            Instruction(at).array_type_info = {
                .element_type_inst = elem_inst,
                .count_inst = count_inst,
            };
            result = at;
        } break;

        case AstType_SliceType: {
            auto elem_inst = build_inst_for_expr(expr->SliceType.element_type_ast);
            auto st = New_Instruction(CIROperator::SliceType, expr);
            Instruction(st).slice_type_info = {
                .element_type_inst = elem_inst,
            };
            result = st;
        } break;

        case AstType_StructDeclValue: {
            auto scope_guard = ScopeGuard(this, expr);

            auto struct_decl_block = Begin_Block(expr, true, false);

            // 1. GetOrInitStruct
            auto decl_init = New_Instruction(CIROperator::GetOrInitStruct, expr);
            Instruction(decl_init).get_or_init_struct_info = {
                .decl_ast = expr,
            };


            // 2. 每个字段: 类型 Block + StructField（纯数据，不干活）
            Array<CIRInstructionRef> field_insts = make_array<CIRInstructionRef>(stage_allocator());
            for (Ast *field : expr->StructDeclValue.fields) {
                XP_ASSERT_DEFAULT(field->type == AstType_StructField);

                auto type_block = build_block_inst_for_expr(field->StructField.type_ast, true, false);

                auto sf = New_Instruction(CIROperator::StructField, field);
                Instruction(sf).struct_field_info = {
                    .type_block_inst = type_block,
                    .name = field->StructField.name,
                };
                field_insts.push_back(sf);
            }

            // 3. FinishStruct — 集中完成所有工作
            auto finish = New_Instruction(CIROperator::FinishStruct, expr);
            Instruction(finish).finish_struct_info = {
                .struct_decl_inst = decl_init,
                .field_insts = field_insts.copy(permanent_allocator()),
            };
            New_Break(struct_decl_block, finish, expr);
            End_Block(struct_decl_block);


            result = finish;
        } break;

        case AstType_EnumDecl: {
            auto scope_guard = ScopeGuard(this, expr);

            CIRInstructionRef tag_type_inst;
            if(expr->EnumDecl.type_ast != nullptr) {
                tag_type_inst = build_block_inst_for_expr(expr->EnumDecl.type_ast, true, false);
            } else {
                auto ti_block = Begin_Block(expr, true, false);

                auto i32_type_inst = New_Instruction(CIROperator::ConstantValue, expr);
                auto val = make_value(type_type());
                val.type_val(easy_type(Type_i32));
                Instruction(i32_type_inst).imm_val = val;

                New_Break(ti_block, i32_type_inst, expr);

                End_Block(ti_block);
                tag_type_inst = ti_block;
            }

            Array<EnumFieldInit> fields = make_array<EnumFieldInit>(permanent_allocator());
            for (Ast *field : expr->EnumDecl.fields) {
                EnumFieldInit ef;
                if(field->type == AstType_ConstDecl) {
                    auto const_decl_inst = build_inst_for_const_decl(field);
                    ef.name = field->ConstDecl.name;
                    ef.value_inst = const_decl_inst;

                    auto dt = New_Instruction(CIROperator::DetermineType, field);
                    Instruction(dt).determine_type_info = {
                        .determining_inst = const_decl_inst,
                        .type_inst = tag_type_inst,
                    };
                } else {
                    ef.name = field->Ident.name;
                    ef.value_inst = INVALID_INST;
                }
                fields.push_back(ef);
            }

            auto decl_init = New_Instruction(CIROperator::EnumDeclInit, expr);
            Instruction(decl_init).enum_decl_init_info = {
                .tag_type_inst = tag_type_inst,
                .decl_ast = expr,
                .symbol = curr_const_sym,
                .scope = curr_scope,
                .fields = fields,
            };
            curr_const_sym = {};
            result = decl_init;
        } break;


        case AstType_FunctionDeclValue: {
            if(curr_const_sym.table != nullptr) {
                SymbolInfoRef func_sym = curr_const_sym;
                curr_const_sym = {};
                result = build_func_decl(expr, func_sym);
            } else {
                result = build_func_decl(expr, std::nullopt);
            }
        } break;

        case AstType_UnionDecl: {
            XP_TODO();
        } break;

        case AstType_FunctionType: {
            Array<CIRInstructionRef> param_type_insts = make_array<CIRInstructionRef>(stage_allocator());
            for(Ast *param_type: expr->FunctionType.param_types) {
                param_type_insts.push_back(build_inst_for_expr(param_type));
            }

            CIRInstructionRef return_type_inst = build_inst_for_expr(expr->FunctionType.return_type_ast);

            result = New_Instruction(CIROperator::FuncType, expr);
            Instruction(result).func_type_info = {
                .param_type_insts = param_type_insts.copy(permanent_allocator()),
                .return_type_inst = return_type_inst,
            };
        } break;

        default: {

            DEBUG_LOG("Unsupported AST type for CIR generation: {}", ast_string(expr->type));
            UNREACHABLE();
        } break;
    }


    return result;
}








CIRInstructionRef CIRBuilder::Alloc_Var(xpString name, bool is_var_arg, bool no_zero_init, Ast *ast) {
    isize slot = curr_func ? curr_func->slot_count++ : -1;
    auto vd = New_Instruction(CIROperator::VariableDecl, ast);
    Instruction(vd).var_decl = {
        .name = name,
        .symbol = ast ? ast->ast_symbol : SymbolInfoRef{},
        .slot = slot,
        .is_var_arg = is_var_arg,
        .no_zero_init = no_zero_init,
    };
    return vd;
}

CIRInstructionRef CIRBuilder::New_Break(CIRInstructionRef break_block, CIRInstructionRef break_value_inst, Ast *ast) {
    auto br = New_Instruction(CIROperator::Break, ast);
    Instruction(br).break_info = {
        .break_block = break_block,
        .break_value_inst = break_value_inst,
    };
    return br;
}




CIRInstructionRef CIRBuilder::New_Instruction(CIROperator op, Ast *ast) {
    CIRInstruction inst{};
    inst.op = op;
    inst.src_loc = ast ? ast->src_loc : SourceLocation{};
    return curr_instruction_buffer->push(inst);
}

// std::pair<CIRInstructionRef, CIRInstruction&> CIRBuilder::New_Inst(CIROperator op, Ast *ast) {
//     auto inst_ref = New_Instruction(op, ast);
//     return {inst_ref, Instruction(inst_ref)};
// }


CIRInstruction& CIRBuilder::Instruction(CIRInstructionRef ref) {
    return (*curr_instruction_buffer)[ref];
}



CIRInstructionRef CIRBuilder::Begin_Block(Ast *ast, bool is_comptime, bool immediate_eval) {
    ASSERT(!immediate_eval || is_comptime);


    auto ref = New_Instruction(CIROperator::Block, ast);
    Instruction(ref).block_info = {
        .is_comptime = is_comptime,
        .immediate_eval = immediate_eval,
        .body_len = 0, // 先占位，等 Block 结束时补齐
    };
    return ref;
}

void CIRBuilder::End_Block(CIRInstructionRef block_inst) {
    Instruction(block_inst).block_info.body_len = curr_instruction_buffer->count() - block_inst - 1;
}

CIRInstructionRef CIRBuilder::Begin_Loop(Ast *ast) {
    auto ref = New_Instruction(CIROperator::Loop, ast);

    // TODO: ABSTRACT
    loop_stack.push_back(ref);

    return ref;
}

void CIRBuilder::End_Loop(CIRInstructionRef loop_inst) {
    Instruction(loop_inst).loop_info = {
        .body_len = curr_instruction_buffer->count() - loop_inst - 1,
    };

    // TODO: ABSTRACT
    XP_ASSERT_DEFAULT(loop_stack.back() == loop_inst);
    loop_stack.pop_back();
}




bool CIRBuilder::Enter_Scope(Ast *ast) {
    Scope *child = try_enter_scope(curr_scope, ast);
    if(child == nullptr) {
        return false;
    }

    auto ref = New_Instruction(CIROperator::EnterScope, nullptr);
    Instruction(ref).scope_info.scope = child;
    curr_scope = child;

    return true;
}

void CIRBuilder::Exit_Scope() {
    Scope *parent = try_exit_scope(curr_scope);
    if(parent == nullptr) {
        return;
    }

    auto ref = New_Instruction(CIROperator::ExitScope, nullptr);
    Instruction(ref).scope_info.scope = parent;
    curr_scope = parent;
}


ScopeGuard::ScopeGuard(CIRBuilder *builder, Ast *ast): builder(builder), entered(false) {
    if(ast != nullptr) {
        entered = builder->Enter_Scope(ast);
    }
}

ScopeGuard::~ScopeGuard() {
    if(entered) {
        builder->Exit_Scope();
    }
}




bool is_cir_binary_op(TokenType type) {
    return is_binary_op(type);
}

bool is_cir_unary_op(TokenType type) {
    // NOTE: 排除解引用和取地址, 因为有专门指令
    return is_unary_op(type) && type != TokenType::Caret && type != TokenType::And;
}