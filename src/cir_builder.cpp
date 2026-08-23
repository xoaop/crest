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



#include "print.hpp"
#include <cstring>

#include "cir_builder.hpp"
#include "context.hpp"




CIRBuilder::CIRBuilder(xpAllocator allocator) {
    block_stack = make_array<CIRBlockRef>(allocator);
    loop_body_block_stack = make_array<CIRInstructionRef>(allocator);
    loop_stack = make_array<CIRInstructionRef>(allocator);
}

CIRBuilder::~CIRBuilder() {
    array_free(&block_stack);
    array_free(&loop_body_block_stack);
    array_free(&loop_stack);
}




void CIRBuilder::build_cir_package(RefN<Package> pkg_ref) {
    Package *pkg = &pkg_ref.unwrap();
    curr_scope = pkg->package_scope;

    pkg->cir_package = make_cir_package(permanent_allocator());
    curr_pkg = &pkg->cir_package;
    curr_pkg->package_scope = pkg->package_scope;

    curr_pkg->package_ref = pkg_ref;


    auto& cir_pkg = pkg->cir_package;

    // 添加package的顶级Block
    curr_pkg->top_blk = curr_pkg->create_block(false, false, false);
    block_stack.push_back(curr_pkg->top_blk);
    defer(block_stack.pop_back());


    for(AstFile& ast_file : pkg->ast_files) {

        curr_ast_file = &ast_file;
        curr_scope = ast_file.file_scope;

        // @new — 顶层 scope 用显式 EnterScope 指令切换（不发 ExitScope，靠下一个 EnterScope 覆盖）
        auto enter_scope = Make_Instruction<CIROperator::EnterScope>(nullptr, { .scope = ast_file.file_scope });

        for(Ast *ast : ast_file.top_levels) {
            CIRInstructionRef top_inst = INVALID_INST;

            switch(ast->type) {
                case AstType_ConstDecl: {
                    top_inst = build_inst_for_const_decl(ast);
                } break;

                default: {
                    context()->reporter.report_error(SourceLocation(ast_file.source_code, ast->src_loc.span), "Unsupported top-level AST type for CIR generation");
                } break;
            }



        }
    }

    curr_scope = pkg->package_scope;

}


CIRInstructionRef CIRBuilder::build_inst_for_const_decl(Ast *const_decl_ast) {
    XP_ASSERT_DEFAULT(const_decl_ast->type == AstType_ConstDecl);
    SymbolInfo &sym = const_decl_ast->ast_symbol.unwrap();

    // 前置：先建值块（发射 BlockRef 指令），再建 ConstDecl。
    // 迭代先分析值块，ConstDecl 只读结果——无感知。
    auto value_ast = const_decl_ast->ConstDecl.value_ast;
    curr_const_sym = const_decl_ast->ast_symbol;
    CIRInstructionRef value_inst = build_block_inst_for_expr(value_ast, true, true);
    curr_const_sym = {};

    auto const_decl = Make_Instruction<CIROperator::ConstDecl>(const_decl_ast, {
        .ident = const_decl_ast->ConstDecl.name,
        .symbol = const_decl_ast->ast_symbol,
        .value_inst = value_inst,
    });


    sym.val(Ref<CIRInstResult>::make(curr_pkg, const_decl));

    return const_decl;
}




CIRInstructionRef CIRBuilder::build_func_decl(Ast *fd, std::optional<Ref<SymbolInfo>> func_sym) {
    XP_ASSERT_DEFAULT(fd->type == AstType_FunctionDeclValue);

    auto saved_curr_func = curr_func;
    auto saved_curr_func_body_block = curr_func_body_block;
    defer({
        curr_func = saved_curr_func;
        curr_func_body_block = saved_curr_func_body_block;
    });

    auto define_func_block = Begin_Block(fd, true, true);   // CIRBlockRef

    CIRFunctionDeclInfo func = {};
    func.return_count = 1;
    func.is_extern_c = fd->FunctionDeclValue.is_extern_c;
    func.is_comptime = fd->FunctionDeclValue.is_comptime;
    func.is_builtin = fd->FunctionDeclValue.is_builtin;
    func.generic_source_arg_indices = fd->FunctionDeclValue.generic_source_arg_indices;
    func.arg_type_insts = make_array<CIRInstructionRef>(permanent_allocator());
    func.arg_decl_insts = make_array<CIRInstructionRef>(permanent_allocator());



    curr_func = &func;



    // 1. 每个参数：类型 block + 分配 slot
    auto param_decls = make_array<CIRVariableDeclInfo>(stage_allocator());
    for(isize i = 0; i < fd->FunctionDeclValue.params.count; i++) {
        Ast *param = fd->FunctionDeclValue.params[i];
        XP_ASSERT_DEFAULT(param->type == AstType_ParamDecl);

        auto param_type = INVALID_INST;
        if(!param->ParamDecl.is_var_arg) {
            param_type = build_block_inst_for_expr(param->ParamDecl.type_ast, true, true);
        }

        CIRVariableDeclInfo var = {};
        var.name = param->ParamDecl.name;
        var.symbol = param->ast_symbol;
        var.slot = i;                       // 参数槽位: 0, 1, 2, ...
        var.is_var_arg = param->ParamDecl.is_var_arg;
        param_decls.push_back(var);
        func.arg_type_insts.push_back(param_type);
    }


    // 返回类型 block
    if(fd->FunctionDeclValue.infer_return_type) {
        func.return_type_inst = INVALID_INST;
    } else if(fd->FunctionDeclValue.return_type_ast != nullptr) {
        func.return_type_inst = build_block_inst_for_expr(fd->FunctionDeclValue.return_type_ast, true, true);
    } else {
        auto rt_block = Begin_Block(fd, true, true);   // CIRBlockRef

        auto val = make_value(type_type());
        val.type_val(easy_type(Type_void));
        auto void_const = Make_Instruction<CIROperator::ConstantValue>(fd, { .value = val });

        New_Break(rt_block, void_const, fd);

        End_Block();

        func.return_type_inst = rt_block;
    }
    
    
    
    
    func.body_inst = INVALID_INST;
    auto func_inst = New_Instruction(CIROperator::FunctionDecl, fd);
    
    Instruction(func_inst).symbol = func_sym.value_or(Ref<SymbolInfo>::INVALID_REF);
    Instruction(func_inst).info<CIROperator::FunctionDecl>().return_type_inst = func.return_type_inst;

    Instruction(func_inst).info<CIROperator::FunctionDecl>() = func;




    // TODO: externC特殊处理, 后面完善些, 现在太丑
    if(!func.is_extern_c && !func.is_builtin) {
        auto body_inst = Begin_Block(fd->FunctionDeclValue.block, false, false);   // handle {parent,N}
        curr_func_body_block = body_inst;

        {
            ScopeGuard _func_scope_guard(this, fd);
            for(isize i = 0; i < param_decls.count; i++) {
                auto& var = param_decls[i];

                // NOTE: 函数参数变量的no_zero_init为false, 因为一定有值, 没必要
                auto vd = Alloc_Var(var.name, var.is_var_arg, false, fd->FunctionDeclValue.params[i]);
                func.arg_decl_insts.push_back(vd);

                auto param_type = func.arg_type_insts[i];
                if(param_type != INVALID_INST) {
                    auto ta = Make_Instruction<CIROperator::TypeAscribe>(fd->FunctionDeclValue.params[i], {
                        .var_inst  = vd,
                        .type_inst = param_type,
                    });
                }
            }

            if(fd->FunctionDeclValue.block != nullptr) {
                build_inst_for_ast_block(fd->FunctionDeclValue.block, false);
            }
        }

        End_Block();

        // 补齐 FunctionDecl 中之前占位的字段
        Instruction(func_inst).info<CIROperator::FunctionDecl>().body_inst = body_inst;
        Instruction(func_inst).info<CIROperator::FunctionDecl>().slot_count = func.slot_count;
        Instruction(func_inst).info<CIROperator::FunctionDecl>().arg_decl_insts = func.arg_decl_insts;
    }


    New_Break(define_func_block, func_inst, fd);

    End_Block();

    return define_func_block;
}


CIRInstructionRef CIRBuilder::build_inst_for_ast_block(Ast *block_ast, bool new_ir_block, bool emit_in_parent, CIRBlockRef *out_block) {
    XP_ASSERT_DEFAULT(block_ast->type == AstType_Block);

    CIRInstructionRef block_inst = INVALID_INST;
    CIRBlockRef block_blk = INVALID_BLOCK;
    if(new_ir_block) {
        if(emit_in_parent) {
            block_inst = Begin_Block(block_ast, false, false);   // handle
        } else {
            block_blk = curr_pkg->create_block(false, false, false);
            block_stack.push_back(block_blk);
        }
    }


    {
        auto _scope_guard = ScopeGuard(this, block_ast);
        for(Ast *stmt : block_ast->Block.statements) {
            build_inst_for_stmt(stmt);
        }
    }


    if(new_ir_block) {
        End_Block();
    }

    if(out_block) *out_block = block_blk;
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
            // Instruction(determine_type_for_value_inst).info<CIROperator::DetermineType>() = {
            //     .determining_inst = value_inst,
            //     .type_inst = INVALID_INST,
            // };

            auto typeof_inst = Make_Instruction<CIROperator::TypeOfInstResult>(stmt, {
                .target_inst = ptr_inst
            });

            auto determine_type = Make_Instruction<CIROperator::DetermineType>(stmt, {
                .determining_inst = value_inst,
                .type_inst = typeof_inst,
            });

            auto st = Make_Instruction<CIROperator::Store>(stmt, {
                .var_inst   = ptr_inst,
                .value_inst = value_inst,
            });
        } break;

        case AstType_IfStmt: {
            auto& if_stmt = stmt->IfStmt;

            auto cond_inst = build_inst_for_expr(if_stmt.condition);

            // CondBr 引用 then/else 独立子块（不插入父块，由 CondBr 独占引用）
            CIRBlockRef then_blk = INVALID_BLOCK;
            build_inst_for_ast_block(if_stmt.then_block, true, false, &then_blk);

            CIRBlockRef else_blk = INVALID_BLOCK;
            if(if_stmt.else_block != nullptr) {
                build_inst_for_ast_block(if_stmt.else_block, true, false, &else_blk);
            } else {
                // 没有 else 块的 if：空块，CondBr 的 exit→merge 桥接即可
                else_blk = curr_pkg->create_block(false, false, false);
                block_stack.push_back(else_blk);
                End_Block();
            }

            auto condbr_inst = Make_Instruction<CIROperator::CondBr>(stmt, {
                .condition_inst = cond_inst,
                .true_block     = then_blk,
                .false_block    = else_blk,
            });
        } break;

        case AstType_ForStmt: {
            build_inst_for_for_stmt(stmt);
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
            std::unreachable();
        } break;
    }

    // TODO: 妥善处理
    return INVALID_INST;
}

void CIRBuilder::build_inst_for_for_stmt(Ast *stmt) {
    auto _scope_guard = ScopeGuard(this, stmt);
    auto& fs = stmt->ForStmt;

    CIRInstructionRef incr_var_inst = INVALID_INST;
    CIRInstructionRef end_inst = INVALID_INST;

    // Pre-loop: build invariant values
    if(fs.iter_var != nullptr) {
        if(fs.iterable_end != nullptr) {
            fs.iter_var->VariableDecl.expr = fs.iterable;
            incr_var_inst = build_inst_for_var_decl(fs.iter_var);
            end_inst = build_inst_for_expr(fs.iterable_end);
        } else {
            context()->reporter.report_error(stmt->src_loc, "for-in over array/slice is not yet implemented");
            return;
        }
    }

    auto loop_inst = Begin_Loop(stmt);

    // Build condition inside loop (re-evaluated each iteration)
    CIRInstructionRef cond_inst;
    if(fs.iter_var != nullptr) {
        auto load_var = Make_Instruction<CIROperator::Load>(stmt, { .ptr_inst = incr_var_inst });

        cond_inst = Make_Instruction<CIROperator::Binary>(stmt, { .op = TokenType::LessThan, .left_inst = load_var, .right_inst = end_inst });
    } else if(fs.condition != nullptr) {
        cond_inst = build_inst_for_expr(fs.condition);
    } else {
        auto true_val = make_value(easy_type(Type_bool));
        true_val.bool_val(true);
        cond_inst = Make_Instruction<CIROperator::ConstantValue>(stmt, { .value = true_val });
    }

    auto body_blk = curr_pkg->create_block(false, false, false);
    block_stack.push_back(body_blk);
    {
        defer(End_Block());

        auto user_block = Begin_Block(stmt, false, false);   // handle
        {
            defer(End_Block());

            loop_body_block_stack.push_back(user_block);
            defer({
                XP_ASSERT_DEFAULT(loop_body_block_stack.back() == user_block);
                loop_body_block_stack.pop_back();
            });

            build_inst_for_ast_block(fs.body, false);
        }

        if(incr_var_inst != INVALID_INST) {
            auto load_var = Make_Instruction<CIROperator::Load>(stmt, { .ptr_inst = incr_var_inst });

            auto v = make_value(easy_type(Type_untyped_int));
            v.integer_val(1);
            auto one_val = Make_Instruction<CIROperator::ConstantValue>(stmt, { .value = v });

            auto add_inst = Make_Instruction<CIROperator::Binary>(stmt, { .op = TokenType::Add, .left_inst = load_var, .right_inst = one_val });

            auto store_inc = Make_Instruction<CIROperator::Store>(stmt, { .var_inst = incr_var_inst, .value_inst = add_inst });
        }
    }

    auto exit_blk = curr_pkg->create_block(false, false, false);
    block_stack.push_back(exit_blk);
    {
        defer(End_Block());
        New_Break(loop_inst, INVALID_INST, stmt);
    }

    auto condbr = Make_Instruction<CIROperator::CondBr>(stmt, {
        .condition_inst = cond_inst,
        .true_block     = body_blk,
        .false_block    = exit_blk,
    });

    End_Loop(loop_inst);
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
        auto ta = Make_Instruction<CIROperator::TypeAscribe>(vd_ast.type_ast, {
            .var_inst  = vd,
            .type_inst = type_block_inst,
        });
    }

    // 3. 有初始值则发射 DetermineType + Store
    if(vd_ast.expr != nullptr) {
        auto value_inst = build_inst_for_expr(vd_ast.expr);

        CIRInstructionRef type_inst = (type_block_inst != INVALID_INST) ? type_block_inst : INVALID_INST;
        auto determine_type = Make_Instruction<CIROperator::DetermineType>(vd_ast.expr, {
            .determining_inst = value_inst,
            .type_inst = type_inst,
        });

        // 无显式类型标注时，补 TypeOfInstResult + TypeAscribe 以分配内存
        if(type_block_inst == INVALID_INST) {
            auto type_of = Make_Instruction<CIROperator::TypeOfInstResult>(vd_ast.expr, {
                .target_inst = value_inst
            });
            auto ta = Make_Instruction<CIROperator::TypeAscribe>(vd_ast.expr, {
                .var_inst  = vd,
                .type_inst = type_of,
            });
        }

        auto st = Make_Instruction<CIROperator::Store>(var_decl_ast, {
            .var_inst   = vd,
            .value_inst = value_inst,
        });
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
        auto determine_type = Make_Instruction<CIROperator::DetermineType>(return_stmt_ast, {
            .determining_inst = value_inst,
            .type_inst = curr_func->return_type_inst,
        });

    } else {
        // 没有返回值的 return, 判断函数返回类型是否为 void

        // 构造一个字面量, 类型为void, 
        // 虽然这个压根不存在, 但它的类型信息可以用来判断函数返回类型是否为 void
        // 搭了determine_type需要的结构, 以复用错误检查逻辑
        auto void_type_val = make_value();
        void_type_val.set_type(easy_type(Type_void)); // 这个值本身不存在, 但我们需要它的类型信息来判断函数返回类型是否为 void
        auto void_type_inst = Make_Instruction<CIROperator::ConstantValue>(return_stmt_ast, { .value = void_type_val });

        auto determine_type = Make_Instruction<CIROperator::DetermineType>(return_stmt_ast, {
            .determining_inst = void_type_inst,
            .type_inst = curr_func->return_type_inst,
        });

    }


    New_Break(curr_func_body_block, value_inst, return_stmt_ast);
}





CIRInstructionRef CIRBuilder::build_block_inst_for_expr(Ast *expr, bool is_comptime_block, bool immediate_eval) {
    auto block_inst = Begin_Block(expr, is_comptime_block, immediate_eval);   // handle {parent,N}

    auto value_inst = build_inst_for_expr(expr);
    New_Break(block_inst, value_inst, expr);

    End_Block();
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
            auto fa = Make_Instruction<CIROperator::FieldPtr>(expr, {
                .parent_inst = parent_ptr,
                .field_name = expr->FieldAccess.field_name,
            });
            return fa;
        } break;

        case AstType_IndexExpr: {
            auto array_ptr = build_ptr_inst_for_expr(expr->IndexExpr.array_var_expr);
            auto index_inst = build_inst_for_expr(expr->IndexExpr.index_expr);

            // TODO: SYMPLIFY
            auto determine_type = Make_Instruction<CIROperator::DetermineType>(expr, {
                .determining_inst = index_inst,
                .type_inst = INVALID_INST,
            });

            auto idx = Make_Instruction<CIROperator::IndexPtr>(expr, {
                .array_inst = array_ptr,
                .index_inst = index_inst,
            });
            return idx;
        } break;

        case AstType_UnaryExpr: {
            // ^ptr 左值: 取指针值作为地址，产生左值
            if(expr->UnaryExpr.op == TokenType::Caret) {
                auto ptr_val = build_ptr_inst_for_expr(expr->UnaryExpr.operand);
                auto deref = Make_Instruction<CIROperator::Deref>(expr, { .operand_inst = ptr_val });
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
            SymbolInfo *sym = try_access_val(expr->ast_symbol);
            if(sym && sym->is_var_decl()) {
                auto ref = New_Instruction(CIROperator::IdentRef, expr);
                Instruction(ref).symbol = expr->ast_symbol;
                auto load = Make_Instruction<CIROperator::Load>(expr, { .ptr_inst = ref });

                result = load;
                break;
            }
            auto inst = New_Instruction(CIROperator::IdentVal, expr);
            Instruction(inst).symbol = expr->ast_symbol;

            result = inst;
        } break;

        case AstType_Import: {
            // import "path"  → 发射 ImportPackage，interp 解析出包值
            result = Make_Instruction<CIROperator::ImportPackage>(expr, {
                .path = expr->Import.path,
            });
        } break;

        case AstType_FunctionCallExpr: {
                // func_type = TypeOfInstResult(func_val)
                auto called_thing_inst = build_inst_for_expr(expr->FunctionCallExpr.func_ident);
                auto typeof_func = Make_Instruction<CIROperator::TypeOfInstResult>(expr, {
                    .target_inst = called_thing_inst
                });

                // 每个实参: FunParamType + DetermineType
                Array<CIRInstructionRef> arg_insts = make_array<CIRInstructionRef>(stage_allocator());
                for (isize i = 0; i < expr->FunctionCallExpr.args.count; i++) {
                    Ast *arg = expr->FunctionCallExpr.args[i];
                    auto arg_inst = build_inst_for_expr(arg);

                    auto fpt = Make_Instruction<CIROperator::FuncParamType>(arg, {
                        .type_of_func_type_inst = typeof_func,
                        .param_index = i,
                    });

                    auto determine_type = Make_Instruction<CIROperator::DetermineType>(arg, {
                        .determining_inst = arg_inst,
                        .type_inst = fpt,
                    });

                    arg_insts.push_back(arg_inst);
                }

                auto call_inst = Make_Instruction<CIROperator::Call>(expr, {
                    .called_thing = called_thing_inst,
                    .arg_insts = arg_insts.copy(permanent_allocator()),
                });

                result = call_inst;
            } break;

        case AstType_BinaryExpr: {

            if(is_logic_operator(expr->BinaryExpr.op)) {
                auto result_blk = Begin_Block(expr, false, false);   // handle {parent,N}
                defer(End_Block());

                auto left_inst  = build_inst_for_expr(expr->BinaryExpr.left);


                // @todo 报错很糟糕, 因为要是类型不对, 这些隐式指令没法映射到源码
                // 是否求值右边表达式的条件
                CIRInstructionRef cond = INVALID_INST;
                {
                    switch(expr->BinaryExpr.op) {
                        case TokenType::DoubleAnd: {
                            // &&: 左为真才求值右操作数 → cond = (left == true)
                            auto true_val = make_value(easy_type(Type_bool));
                            true_val.bool_val(true);
                            auto true_const = Make_Instruction<CIROperator::ConstantValue>(expr, { .value = true_val });

                            cond = Make_Instruction<CIROperator::Binary>(expr, {
                                .op = TokenType::DoubleEqual,
                                .left_inst  = left_inst,
                                .right_inst = true_const,
                            });
                        } break;

                        case TokenType::DoubleOr: {
                            // ||: 左为假才求值右操作数 → cond = (left == false)
                            auto false_val = make_value(easy_type(Type_bool));
                            false_val.bool_val(false);
                            auto false_const = Make_Instruction<CIROperator::ConstantValue>(expr, { .value = false_val });

                            cond = Make_Instruction<CIROperator::Binary>(expr, {
                                .op = TokenType::DoubleEqual,
                                .left_inst  = left_inst,
                                .right_inst = false_const,
                            });
                        } break;

                        default: {
                            std::unreachable();
                        } break;
                    }
                }

                CIRBlockRef true_blk = INVALID_BLOCK;
                {
                    true_blk = curr_pkg->create_block(false, false, false);
                    block_stack.push_back(true_blk);
                    defer(End_Block());

                    auto right_inst = build_inst_for_expr(expr->BinaryExpr.right);

                    auto bin = Make_Instruction<CIROperator::Binary>(expr, {
                        .op = expr->BinaryExpr.op,
                        .left_inst  = left_inst,
                        .right_inst = right_inst,
                    });

                    // NOTE: 直接返回给外层result_blk
                    New_Break(result_blk, bin, expr);
                }

                CIRBlockRef false_blk = INVALID_BLOCK;
                {
                    false_blk = curr_pkg->create_block(false, false, false);
                    block_stack.push_back(false_blk);
                    defer(End_Block());

                    auto false_val = make_value(easy_type(Type_bool));
                    false_val.bool_val(expr->BinaryExpr.op == TokenType::DoubleOr); // && → false, || → true
                    auto false_const = Make_Instruction<CIROperator::ConstantValue>(expr, { .value = false_val });

                    // NOTE: 直接返回给外层result_blk
                    New_Break(result_blk, false_const, expr);
                }

                // 判断是否需要求值右边表达式, 如果为真则求值右边表达式, 否则直接返回左边的结果
                // is_short_circuit：TypeOnly 遍历两分支时，死分支（右操作数）关闭常量折叠，
                // 活分支 RHS 常量错误（如 10/0）仍照常报出；普通 if/else 不受影响。
                auto condbr_eval_right = Make_Instruction<CIROperator::CondBr>(expr, {
                    .condition_inst = cond,
                    .true_block     = true_blk,
                    .false_block    = false_blk,
                    .is_short_circuit = true,
                });
                
                result = result_blk;
            } else {
                auto left_inst  = build_inst_for_expr(expr->BinaryExpr.left);
                auto right_inst = build_inst_for_expr(expr->BinaryExpr.right);
    
                auto bin = Make_Instruction<CIROperator::Binary>(expr, {
                    .op = expr->BinaryExpr.op,
                    .left_inst  = left_inst,
                    .right_inst = right_inst,
                });
                result = bin;
            }

        } break;

        case AstType_UnaryExpr: {
            auto op = expr->UnaryExpr.op;

            // &x: 左值取地址 → 指针值
            if(op == TokenType::And) {
                auto lval = build_ptr_inst_for_expr(expr->UnaryExpr.operand);
                auto addr_of = Make_Instruction<CIROperator::AddrOf>(expr, { .lval_inst = lval });
                result = addr_of; break;
            }
            // ^ptr: 对指针值 Load
            if(op == TokenType::Caret) {
                auto ptr_val_inst = build_inst_for_expr(expr->UnaryExpr.operand);
                auto load = Make_Instruction<CIROperator::Deref>(expr, { .operand_inst = ptr_val_inst });
                result = load; break;
            }

            // -, !, ~, 等其他一元运算
            auto operand_inst = build_inst_for_expr(expr->UnaryExpr.operand);
            auto un = Make_Instruction<CIROperator::Unary>(expr, {
                .op = op,
                .operand_inst = operand_inst,
            });
            result = un;
        } break;

        case AstType_CastExpr: {
            auto value_inst = build_inst_for_expr(expr->CastExpr.expr);
            auto target_inst = build_inst_for_expr(expr->CastExpr.target_type_ast);
            auto cast = Make_Instruction<CIROperator::Cast>(expr, {
                .expr_inst = value_inst,
                .target_type_inst = target_inst,
            });
            result = cast;
        } break;

        case AstType_Constant: {
            auto c = Make_Instruction<CIROperator::ConstantValue>(expr, { .value = expr->Constant.value });
            result = c;
        } break;

        case AstType_StructInitExpr: {
            auto struct_type_inst = build_block_inst_for_expr(expr->StructInitExpr.struct_type_ident, true, true);

            Array<CIRInstructionRef> field_insts = make_array<CIRInstructionRef>(stage_allocator());
            for (isize i = 0; i < expr->StructInitExpr.field_inits.count; i++) {
                Ast *field_init = expr->StructInitExpr.field_inits[i];

                auto field_value_inst = build_inst_for_expr(field_init);


                // 确定化各个field的类型
                auto field_type_of_struct = Make_Instruction<CIROperator::FieldTypeOfStruct>(field_init, {
                    .struct_type_inst = struct_type_inst,
                    .field_index = i,
                });

                auto determine_type = Make_Instruction<CIROperator::DetermineType>(field_init, {
                    .determining_inst = field_value_inst,
                    .type_inst = field_type_of_struct
                });

                field_insts.push_back(field_value_inst);
            }

            auto init = Make_Instruction<CIROperator::StructInit>(expr, {
                .struct_type_inst = struct_type_inst,
                .field_init_insts = field_insts.copy(permanent_allocator()),
            });
            result = init;
        } break;

        case AstType_FieldAccess: {
            auto parent_inst = build_inst_for_expr(expr->FieldAccess.parent);
            auto fa = Make_Instruction<CIROperator::FieldAccess>(expr, {
                .parent_inst = parent_inst,
                .field_name = expr->FieldAccess.field_name,
            });
            result = fa;
        } break;

        case AstType_ArrayInitExpr: {
            Array<CIRInstructionRef> element_insts = make_array<CIRInstructionRef>(stage_allocator());
            for (Ast *elem : expr->ArrayInitExpr.elements) {
                element_insts.push_back(build_inst_for_expr(elem));
            }

            auto init = Make_Instruction<CIROperator::ArrayInit>(expr, {
                .element_insts = element_insts.copy(permanent_allocator()),
            });
            result = init;
        } break;

        case AstType_IndexExpr: {
            auto array_inst = build_inst_for_expr(expr->IndexExpr.array_var_expr);
            auto index_inst = build_inst_for_expr(expr->IndexExpr.index_expr);

            // TODO: SYMPLIFY 确定化 index_inst 的类型
            auto determine_type = Make_Instruction<CIROperator::DetermineType>(expr, {
                .determining_inst = index_inst,
                .type_inst = INVALID_INST,
            });

            auto idx = Make_Instruction<CIROperator::Index>(expr, {
                .array_inst = array_inst,
                .index_inst = index_inst,
            });
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


            auto string_ident = New_Instruction(CIROperator::IdentVal, expr);
            Instruction(string_ident).symbol = find_symbol_until_global_ref(context()->global_blank_package.unwrap().package_scope, xp_string_c("string"));

            auto s = Make_Instruction<CIROperator::StringLiteral>(expr, {
                .data = ptr,
                .count = count_str,
                .str = str,
                .string_type_inst = string_ident,
            });

            result = s;
        } break;

        case AstType_EasyType: {
            auto val = make_value(type_type());
            val.type_val(easy_type(expr->EasyType.kind));
            auto t = Make_Instruction<CIROperator::ConstantValue>(expr, { .value = val });
            result = t;
        } break;

        case AstType_PointerType: {
            auto pointed_inst = build_inst_for_expr(expr->PointerType.pointed_type_ast);
            auto pt = Make_Instruction<CIROperator::PointerType>(expr, {
                .pointed_type_inst = pointed_inst,
            });
            result = pt;
        } break;

        case AstType_ArrayType: {
            auto elem_inst = build_inst_for_expr(expr->ArrayType.element_type_ast);
            auto count_inst = build_inst_for_expr(expr->ArrayType.count_expr);
            auto at = Make_Instruction<CIROperator::ArrayType>(expr, {
                .element_type_inst = elem_inst,
                .count_inst = count_inst,
            });
            result = at;
        } break;

        case AstType_SliceType: {
            auto elem_inst = build_inst_for_expr(expr->SliceType.element_type_ast);
            auto st = Make_Instruction<CIROperator::SliceType>(expr, {
                .element_type_inst = elem_inst,
            });
            result = st;
        } break;

        case AstType_StructDeclValue: {
            auto scope_guard = ScopeGuard(this, expr);

            auto struct_decl_block = Begin_Block(expr, true, false);

            // 1. GetOrInitStruct
            auto decl_init = Make_Instruction<CIROperator::GetOrInitStruct>(expr, {
                .decl_ast = expr,
                .symbol = curr_const_sym,
            });


            // 2. 每个字段: 类型 Block + StructField（纯数据，不干活）
            Array<CIRInstructionRef> field_insts = make_array<CIRInstructionRef>(stage_allocator());
            for (Ast *field : expr->StructDeclValue.fields) {
                XP_ASSERT_DEFAULT(field->type == AstType_StructField);

                auto type_block = build_block_inst_for_expr(field->StructField.type_ast, true, false);

                auto sf = Make_Instruction<CIROperator::StructField>(field, {
                    .type_block_inst = type_block,
                    .name = field->StructField.name,
                });
                field_insts.push_back(sf);
            }

            // 3. FinishStruct — 集中完成所有工作
            auto finish = Make_Instruction<CIROperator::FinishStruct>(expr, {
                .struct_decl_inst = decl_init,
                .field_insts = field_insts.copy(permanent_allocator()),
            });
            New_Break(struct_decl_block, finish, expr);
            End_Block();


            result = finish;
        } break;

        case AstType_EnumDecl: {
            auto scope_guard = ScopeGuard(this, expr);

            CIRInstructionRef tag_type_inst;
            if(expr->EnumDecl.type_ast != nullptr) {
                tag_type_inst = build_block_inst_for_expr(expr->EnumDecl.type_ast, true, false);
            } else {
                auto ti_block = Begin_Block(expr, true, false);

                auto val = make_value(type_type());
                val.type_val(easy_type(Type_i32));
                auto i32_type_inst = Make_Instruction<CIROperator::ConstantValue>(expr, { .value = val });

                New_Break(ti_block, i32_type_inst, expr);

                End_Block();
                tag_type_inst = ti_block;
            }

            Array<EnumFieldInit> fields = make_array<EnumFieldInit>(permanent_allocator());
            for (Ast *field : expr->EnumDecl.fields) {
                EnumFieldInit ef;
                if(field->type == AstType_ConstDecl) {
                    auto const_decl_inst = build_inst_for_const_decl(field);
                    ef.name = field->ConstDecl.name;
                    ef.value_inst = const_decl_inst;

                    auto dt = Make_Instruction<CIROperator::DetermineType>(field, {
                        .determining_inst = const_decl_inst,
                        .type_inst = tag_type_inst,
                    });
                } else {
                    ef.name = field->Ident.name;
                    ef.value_inst = INVALID_INST;
                }
                fields.push_back(ef);
            }

            auto decl_init = Make_Instruction<CIROperator::EnumDeclInit>(expr, {
                .tag_type_inst = tag_type_inst,
                .decl_ast = expr,
                .symbol = curr_const_sym,
                .scope = curr_scope,
                .fields = fields,
            });
            curr_const_sym = {};
            result = decl_init;
        } break;


        case AstType_FunctionDeclValue: {
            if(curr_const_sym.scope != Ref<Scope>::INVALID_REF) {
                Ref<SymbolInfo> func_sym = curr_const_sym;
                curr_const_sym = {};
                result = build_func_decl(expr, func_sym);
            } else {
                result = build_func_decl(expr, std::nullopt);
            }
        } break;

        case AstType_UnionDecl: {
            auto scope_guard = ScopeGuard(this, expr);

            auto union_decl_block = Begin_Block(expr, true, false);

            // 1. GetOrInitUnion
            auto decl_init = Make_Instruction<CIROperator::GetOrInitUnion>(expr, {
                .decl_ast = expr,
                .symbol = curr_const_sym,
                .scope = curr_scope,
            });

            // 2. 每个字段: 类型 Block + StructField（纯数据，不干活）
            Array<CIRInstructionRef> field_insts = make_array<CIRInstructionRef>(stage_allocator());
            for (Ast *field : expr->UnionDecl.fields) {
                ASSERT(field->type == AstType_StructField);

                auto type_block = build_block_inst_for_expr(field->StructField.type_ast, true, false);

                auto sf = Make_Instruction<CIROperator::StructField>(field, {
                    .type_block_inst = type_block,
                    .name = field->StructField.name,
                });
                field_insts.push_back(sf);
            }

            // 3. FinishUnion — 集中完成所有工作
            auto finish = Make_Instruction<CIROperator::FinishUnion>(expr, {
                .union_decl_inst = decl_init,
                .field_insts = field_insts.copy(permanent_allocator()),
            });
            New_Break(union_decl_block, finish, expr);
            End_Block();

            result = finish;
        } break;

        case AstType_FunctionType: {
            Array<CIRInstructionRef> param_type_insts = make_array<CIRInstructionRef>(stage_allocator());
            for(Ast *param_type: expr->FunctionType.param_types) {
                param_type_insts.push_back(build_inst_for_expr(param_type));
            }

            CIRInstructionRef return_type_inst = build_inst_for_expr(expr->FunctionType.return_type_ast);

            result = Make_Instruction<CIROperator::FuncType>(expr, {
                .param_type_insts = param_type_insts.copy(permanent_allocator()),
                .return_type_inst = return_type_inst,
            });
        } break;

        default: {

            DEBUG_LOG("Unsupported AST type for CIR generation: {}", ast_string(expr->type));
            std::unreachable();
        } break;
    }


    return result;
}








CIRInstructionRef CIRBuilder::Alloc_Var(xpString name, bool is_var_arg, bool no_zero_init, Ast *ast) {
    isize slot = curr_func ? curr_func->slot_count++ : -1;
    auto vd = Make_Instruction<CIROperator::VariableDecl>(ast, {
        .name = name,
        .symbol = ast ? ast->ast_symbol : Ref<SymbolInfo>::INVALID_REF,
        .slot = slot,
        .is_var_arg = is_var_arg,
        .no_zero_init = no_zero_init,
    });
    return vd;
}

CIRInstructionRef CIRBuilder::New_Break(CIRInstructionRef break_block, CIRInstructionRef break_value_inst, Ast *ast) {
    auto br = Make_Instruction<CIROperator::Break>(ast, {
        .break_block = break_block,
        .break_value_inst = break_value_inst,
    });
    return br;
}




CIRInstructionRef CIRBuilder::New_Instruction(CIROperator op, Ast *ast) {
    CIRInstruction inst{};
    inst.op = op;
    inst.src_loc = ast ? ast->src_loc : SourceLocation{};

    CIRBlockRef blk = block_stack.back();
    isize inst_index = curr_pkg->block(blk)->push_back_inst(inst);

    return CIRInstructionRef{ blk, inst_index, curr_pkg->package_ref.index };
}


CIRInstruction& CIRBuilder::Instruction(CIRInstructionRef ref) {
    return *curr_pkg->inst(ref);
}



CIRInstructionRef CIRBuilder::Begin_Block(Ast *ast, bool is_comptime, bool immediate_eval) {
    ASSERT(!immediate_eval || is_comptime);

    CIRBlockRef blk = curr_pkg->create_block(is_comptime, immediate_eval, false);

    auto blockref = Make_Instruction<CIROperator::BlockRef>(ast, { .block_ref = blk, .in_which_block = block_stack.back() });

    block_stack.push_back(blk);

    return blockref;   // handle {parent,N}
}

void CIRBuilder::End_Block() {
    block_stack.pop_back();
}


CIRInstructionRef CIRBuilder::Begin_Loop(Ast *ast) {
    auto ref = Begin_Block(ast, false, false);   // handle {parent,N}
    
    CIRBlockRef child = curr_pkg->inst(ref)->info<CIROperator::BlockRef>().block_ref;
    curr_pkg->block(child)->is_loop = true;

    block_stack.push_back(child);
    loop_stack.push_back(ref);
    
    return ref;
}

void CIRBuilder::End_Loop(CIRInstructionRef loop_inst) {
    XP_ASSERT_DEFAULT(loop_stack.back() == loop_inst);

    block_stack.pop_back();
    loop_stack.pop_back();
    
    End_Block();
}








bool CIRBuilder::Enter_Scope(Ast *ast) {
    Ref<Scope> child = try_enter_scope(&curr_scope.unwrap(), ast);
    if(child == Ref<Scope>::INVALID_REF) {
        return false;
    }

    RefN<Scope> child_n{child};   // 已验证存在
    auto ref = Make_Instruction<CIROperator::EnterScope>(nullptr, { .scope = child_n });
    curr_scope = child_n;

    return true;
}

void CIRBuilder::Exit_Scope() {
    Ref<Scope> parent = try_exit_scope(&curr_scope.unwrap());
    if(parent == Ref<Scope>::INVALID_REF) {
        return;
    }

    RefN<Scope> parent_n{parent.index};   // 已验证存在
    auto ref = Make_Instruction<CIROperator::ExitScope>(nullptr, { .scope = parent_n });
    curr_scope = parent_n;
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