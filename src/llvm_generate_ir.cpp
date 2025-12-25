#include "llvm_generate_ir.hpp"

#include "common.hpp"

#include "symbol.hpp"

void init_llvm_generator(LLVMGenerator *gen) {
    gen->ctx = LLVMContextCreate();
    gen->module = LLVMModuleCreateWithNameInContext("my_module", gen->ctx);
    gen->builder = LLVMCreateBuilderInContext(gen->ctx);

    gen->locals = xp_hash_map_make<xpString, LLVMValueRef>(permanent_allocator());

    gen->loop_stack = make_array<LLVMLoopBlocks>(permanent_allocator());
    return;
}

void free_llvm_generator(LLVMGenerator *gen) {
    LLVMDisposeBuilder(gen->builder);
    LLVMDisposeModule(gen->module);
    LLVMContextDispose(gen->ctx);

    xp_hash_map_free(gen->locals);
    array_free(&gen->loop_stack);
    return;
}




void gen_ir_function(LLVMGenerator *gen, Ast *function);
LLVMState gen_ir_variable_decl(LLVMGenerator *gen, Ast *variable_decl, LLVMState state);
LLVMState gen_ir_block(LLVMGenerator *gen, Ast *block, LLVMState state);
LLVMState gen_ir_stmt(LLVMGenerator *gen, Ast *stmt, LLVMState state);
LLVMValueRef gen_ir_expr(LLVMGenerator *gen, Ast *expr, LLVMState state);



static LLVMValueRef insert_alloca_before_last_inst_which_is_br(LLVMGenerator *gen, LLVMBasicBlockRef target_block, const char *var_name, LLVMTypeRef type) {
    LLVMBasicBlockRef curr_block = LLVMGetInsertBlock(gen->builder);
    

    LLVMPositionBuilderAtEnd(gen->builder, target_block);
    LLVMValueRef last_instr = LLVMGetLastInstruction(target_block);
    if (last_instr && LLVMIsABranchInst(last_instr)) {
        LLVMPositionBuilderBefore(gen->builder, last_instr);
    }

    LLVMValueRef alloca = LLVMBuildAlloca(gen->builder, type, var_name);

    LLVMPositionBuilderAtEnd(gen->builder, curr_block);

    return alloca;
}


void gen_ir_astfile(AstFile f) {
    LLVMInitializeNativeTarget();


    LLVMGenerator gen;
    init_llvm_generator(&gen);

    LLVMSetTarget(gen.module, "x86_64-pc-windows-msvc");

    for(isize i = 0; i < f.root.count; i++) {
        Ast *func = f.root[i];

        LLVMTypeRef i32_type = LLVMInt32TypeInContext(gen.ctx);
        Array<LLVMTypeRef> params = make_array_len<LLVMTypeRef>(temp_allocator(), func->Function.params.count);
        for(isize j = 0; j < func->Function.params.count; j++) {
            params.push_back(i32_type);
        }
        LLVMTypeRef func_type = LLVMFunctionType(i32_type, params.data, params.count, 0);
        LLVMValueRef function = LLVMAddFunction(gen.module, func->Function.name.c_str, func_type);
    }

    // char *str = LLVMPrintModuleToString(gen.module); // For debug   
    // printf("%s\n", str);

    // TODO
    for(isize i = 0; i < f.root.count; i++) {
        gen_ir_function(&gen, f.root[i]);
    }

    // 输出 .ll 文件
    char *error = nullptr;
    if (LLVMPrintModuleToFile(gen.module, "output.ll", &error) != 0) {
        fprintf(stderr, "Error writing .ll file: %s\n", error);
        LLVMDisposeMessage(error);
    }

    free_llvm_generator(&gen);
}



void gen_ir_function(LLVMGenerator *gen, Ast *function) {
    XP_ASSERT_DEFAULT(function->type == AstType_Function);
    defer(xp_arena_allocator_clear(temp_allocator()));

    LLVMTypeRef i32_type = LLVMInt32TypeInContext(gen->ctx);
    LLVMValueRef func = LLVMGetNamedFunction(gen->module, function->Function.name.c_str);

    // 1. 创建入口基本块并设置插入点
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(gen->ctx, func, "entry");
    LLVMBasicBlockRef after_entry = LLVMAppendBasicBlockInContext(gen->ctx, func, "after.entry");
    
    
    LLVMPositionBuilderAtEnd(gen->builder, entry);

    // 2. 为每个参数分配 alloca 并保存到 gen->locals
    for(isize i = 0; i < function->Function.params.count; i++) {
        Ast *param = function->Function.params[i];
        LLVMValueRef param_alloca = LLVMBuildAlloca(gen->builder, i32_type, param->VariableDecl.var_name.c_str);
        LLVMBuildStore(gen->builder, LLVMGetParam(func, i), param_alloca);
        xp_hash_map_insert(&gen->locals, param->VariableDecl.var_name, param_alloca);
    }

    LLVMBuildBr(gen->builder, after_entry);
    LLVMPositionBuilderAtEnd(gen->builder, after_entry);

    LLVMState state = save_state(func, after_entry, entry);
    gen_ir_block(gen, function->Function.block, state);



    // TODO 返回值
    LLVMBuildRet(gen->builder, LLVMConstInt(LLVMInt32TypeInContext(gen->ctx), 0, 1));
}



LLVMState gen_ir_variable_decl(LLVMGenerator *gen, Ast *variable_decl, LLVMState state) {
    XP_ASSERT_DEFAULT(variable_decl->type == AstType_VariableDecl);

    LLVMTypeRef i32_type = LLVMInt32TypeInContext(gen->ctx);

    // 1. 分配空间
    LLVMValueRef alloca = insert_alloca_before_last_inst_which_is_br(gen, state.entry, variable_decl->VariableDecl.var_name.c_str, i32_type);


    // 2. 如果有初始值，生成初始值 IR 并存储
    LLVMPositionBuilderAtEnd(gen->builder, state.curr_block);
    if (variable_decl->VariableDecl.expr) {
        LLVMValueRef init_value = gen_ir_expr(gen, variable_decl->VariableDecl.expr, state);
        LLVMBuildStore(gen->builder, init_value, alloca);
    }

    // 3. 存到变量表
    xp_hash_map_insert(&gen->locals, variable_decl->VariableDecl.var_name, alloca);

    return state;
}



LLVMState gen_ir_block(LLVMGenerator *gen, Ast *block, LLVMState state) {
    XP_ASSERT_DEFAULT(block->type == AstType_Block);

    for(isize i = 0; i < block->Block.statements.count; i++) {
        state = gen_ir_stmt(gen, block->Block.statements[i], state);
    }

    return state;
}



LLVMState gen_ir_stmt(LLVMGenerator *gen, Ast *stmt, LLVMState state) {
    load_state(gen, state);

    switch (stmt->type) {
    case AstType_VariableDecl:
        state = gen_ir_variable_decl(gen, stmt, state);
        break;
    case AstType_IfStmt: {
        
        LLVMBasicBlockRef cond_block = LLVMAppendBasicBlockInContext(gen->ctx, state.curr_function, "if.cond");
        LLVMBasicBlockRef then_block = LLVMAppendBasicBlockInContext(gen->ctx, state.curr_function, "if.then");
        LLVMBasicBlockRef else_block = LLVMAppendBasicBlockInContext(gen->ctx, state.curr_function,  "if.else");
        LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(gen->ctx, state.curr_function, "merge");
        
        LLVMBuildBr(gen->builder, cond_block);
        
        LLVMPositionBuilderAtEnd(gen->builder, cond_block);
        state.curr_block = cond_block;
        LLVMValueRef cond_val = gen_ir_expr(gen, stmt->IfStmt.condition, state);

        // TODO: 目前假设条件是 i32 类型，非零为真
        // LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(gen->ctx), 0, 0);
        // LLVMValueRef cond = LLVMBuildICmp(gen->builder, LLVMIntNE, cond_val, zero, "ifcond");

        LLVMBuildCondBr(gen->builder, cond_val, then_block, else_block);
        

        //  then 分支
        LLVMPositionBuilderAtEnd(gen->builder, then_block);
        state.curr_block = then_block;
        gen_ir_block(gen, stmt->IfStmt.then_block, state);
        LLVMBuildBr(gen->builder, merge_block);

        //  else 分支
        LLVMPositionBuilderAtEnd(gen->builder, else_block);
        state.curr_block = else_block;
        if (stmt->IfStmt.else_block) {
            gen_ir_block(gen, stmt->IfStmt.else_block, state);
        }
        LLVMBuildBr(gen->builder, merge_block);

        //  设置插入点到 merge
        LLVMPositionBuilderAtEnd(gen->builder, merge_block);

        state.curr_block = merge_block;

    } break;

    case AstType_Assignment: {
        LLVMValueRef value = gen_ir_expr(gen, stmt->Assignment.right_expr, state);

        // TODO: 目前只支持变量赋值
        LLVMValueRef *alloca = xp_hash_map_get(gen->locals, stmt->Assignment.left_var_expr->VarExpr.name);
        XP_ASSERT_DEFAULT(alloca != NULL);
        LLVMBuildStore(gen->builder, value, *alloca);
    } break;

    case AstType::AstType_ForStmt: {
        // TODO

        // 1. 创建基本块
        LLVMBasicBlockRef init_block = LLVMAppendBasicBlockInContext(gen->ctx, state.curr_function, "for.init");
        LLVMBasicBlockRef cond_block = LLVMAppendBasicBlockInContext(gen->ctx, state.curr_function, "for.cond");
        LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(gen->ctx, state.curr_function, "for.body");
        LLVMBasicBlockRef post_block = LLVMAppendBasicBlockInContext(gen->ctx, state.curr_function, "for.post");
        LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(gen->ctx, state.curr_function, "for.merge");

        array_push_back(&gen->loop_stack, LLVMLoopBlocks{
            .post_block = post_block,
            .merge_block = merge_block
        });
        defer(array_pop_back(&gen->loop_stack));

        // 2. 初始化表达式
        LLVMBuildBr(gen->builder, init_block);
        state.curr_block = init_block;
        gen_ir_stmt(gen, stmt->ForStmt.init, state);

        // 3. 跳转到条件判断
        LLVMBuildBr(gen->builder, cond_block);
        // 4. 条件判断
        LLVMPositionBuilderAtEnd(gen->builder, cond_block);
        LLVMValueRef cond_val = gen_ir_expr(gen, stmt->ForStmt.condition, state);
        LLVMBuildCondBr(gen->builder, cond_val, body_block, merge_block);
        // 5. 循环体
        LLVMPositionBuilderAtEnd(gen->builder, body_block);
        state.curr_block = body_block;

        gen_ir_block(gen, stmt->ForStmt.body, state);
        LLVMBuildBr(gen->builder, post_block);
        // 6. 后置表达式
        LLVMPositionBuilderAtEnd(gen->builder, post_block);
        state.curr_block = post_block;

        gen_ir_stmt(gen, stmt->ForStmt.post, state);
        LLVMBuildBr(gen->builder, cond_block);
        // 7. 合并块
        LLVMPositionBuilderAtEnd(gen->builder, merge_block);
        state.curr_block = merge_block;

    } break;

    case AstType_ReturnStmt: {
        LLVMValueRef ret_val = gen_ir_expr(gen, stmt->ReturnStmt.expr, state);
        LLVMBuildRet(gen->builder, ret_val);
    } break;

    case AstType_Break: {
        LLVMBuildBr(gen->builder, gen->loop_stack[gen->loop_stack.count - 1].merge_block);
    } break;
    case AstType_Continue: {
        LLVMBuildBr(gen->builder, gen->loop_stack[gen->loop_stack.count - 1].post_block);
    } break;

    default: {
        gen_ir_expr(gen, stmt, state);
        break;
    }
    }

    return state;
}



LLVMValueRef gen_ir_expr(LLVMGenerator *gen, Ast *expr, LLVMState state) {
    switch (expr->type)
    {
    case AstType_VarExpr: {
        LLVMValueRef *alloca = xp_hash_map_get(gen->locals, expr->VarExpr.name);
        XP_ASSERT_DEFAULT(alloca != NULL);
        LLVMTypeRef i32_type = LLVMInt32TypeInContext(gen->ctx);
        return LLVMBuildLoad2(gen->builder, i32_type, *alloca, expr->VarExpr.name.c_str);
    } break;
    case AstType_Constant: {
        return LLVMConstInt(LLVMInt32TypeInContext(gen->ctx), expr->Constant.value, 1);
    } break;
    case AstType_UnrayExpr: {
        // 一元表达式（如负号）
        LLVMValueRef operand = gen_ir_expr(gen, expr->UnrayExpr.operand, state);
        if (expr->UnrayExpr.op == TokenType::Minus) {
            return LLVMBuildNeg(gen->builder, operand, "negtmp");
        }

        XP_ASSERT_DEFAULT(0);
    } break;

    case AstType_BinaryExpr: {
        LLVMValueRef lhs = gen_ir_expr(gen, expr->BinaryExpr.left, state);
        LLVMValueRef rhs = gen_ir_expr(gen, expr->BinaryExpr.right, state);
        switch (expr->BinaryExpr.op) {
        case TokenType::Add: // +
            return LLVMBuildAdd(gen->builder, lhs, rhs, "addtmp");
        case TokenType::Minus: // -
            return LLVMBuildSub(gen->builder, lhs, rhs, "subtmp");
        case TokenType::Star: // *
            return LLVMBuildMul(gen->builder, lhs, rhs, "multmp");
        case TokenType::ForwardSlash: // /
            return LLVMBuildSDiv(gen->builder, lhs, rhs, "divtmp");
        case TokenType::Percent: // %
            return LLVMBuildSRem(gen->builder, lhs, rhs, "modtmp");
        case TokenType::GreaterThan: // >
            return LLVMBuildICmp(gen->builder, LLVMIntSGT, lhs, rhs, "gttmp");
        case TokenType::GreaterEqual: // >=
            return LLVMBuildICmp(gen->builder, LLVMIntSGE, lhs, rhs, "getmp");
        case TokenType::LessThan: // <
            return LLVMBuildICmp(gen->builder, LLVMIntSLT, lhs, rhs, "lttmp");
        case TokenType::LessEqual: // <=
            return LLVMBuildICmp(gen->builder, LLVMIntSLE, lhs, rhs, "letmp");
        case TokenType::DoubleEqual: // ==
            return LLVMBuildICmp(gen->builder, LLVMIntEQ, lhs, rhs, "eqtmp");
        case TokenType::ExclamationEqual: // !=
            return LLVMBuildICmp(gen->builder, LLVMIntNE, lhs, rhs, "netmp");

        default:
            XP_ASSERT_DEFAULT(0);
        }


    } break;

    case AstType_FunctionCallExpr: {
        LLVMValueRef func = LLVMGetNamedFunction(gen->module, expr->FunctionCallExpr.name.c_str);

        Array<LLVMValueRef> args = make_array_len<LLVMValueRef>(temp_allocator(), expr->FunctionCallExpr.args.count);
        for(isize i = 0; i < expr->FunctionCallExpr.args.count; i++) {
            LLVMValueRef arg_val = gen_ir_expr(gen,  expr->FunctionCallExpr.args[i], state);
            array_push_back(&args, arg_val);
        }

        LLVMTypeRef type = LLVMGlobalGetValueType(func);

        return LLVMBuildCall2(gen->builder, type, func, args.data, args.count, "calltmp");
    } break;

    default:
        printf("\n-------------------------------------------\n");
        print_ast(expr);
        XP_ASSERT_DEFAULT(0);
    }


}



void load_state(LLVMGenerator *gen, LLVMState state) {
    LLVMPositionBuilderAtEnd(gen->builder, state.curr_block);
}

LLVMState save_state(LLVMValueRef curr_function, LLVMBasicBlockRef curr_block, LLVMBasicBlockRef entry) {
    LLVMState state;
    state.curr_function = curr_function;
    state.curr_block = curr_block;
    state.entry = entry;
    return state;
}