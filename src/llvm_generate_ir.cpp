#include "llvm_generate_ir.hpp"

#include "common.hpp"

#include "symbol.hpp"

#include "ast.hpp"


void load_state(LLVMGenerator *gen, LLVMState state);
LLVMState save_state(LLVMValueRef curr_function, LLVMBasicBlockRef curr_block, LLVMBasicBlockRef entry);


LLVMTypeRef get_llvm_type_from_type(LLVMGenerator *gen, TypeRef type);

void gen_ir_function(LLVMGenerator *gen, Ast *function);
LLVMState gen_ir_variable_decl(LLVMGenerator *gen, Ast *variable_decl, LLVMState state);
LLVMState gen_ir_block(LLVMGenerator *gen, Ast *block, LLVMState state);
LLVMState gen_ir_stmt(LLVMGenerator *gen, Ast *stmt, LLVMState state);
LLVMValueRef gen_ir_expr(LLVMGenerator *gen, Ast *expr, LLVMState state, bool is_lvalue_expr = false);
LLVMValueRef gen_ir_compare_expr(LLVMGenerator *gen, Ast *expr, LLVMState state);



void init_llvm_generator(LLVMGenerator *gen) {
    gen->ctx = LLVMContextCreate();
    gen->module = LLVMModuleCreateWithNameInContext("my_module", gen->ctx);
    gen->builder = LLVMCreateBuilderInContext(gen->ctx);

    LLVMTargetRef target;
    char *error = NULL;
    if(LLVMGetTargetFromTriple(LLVMGetDefaultTargetTriple(), &target, &error)) {
        printf("Error getting target: %s\n", error);
        LLVMDisposeMessage(error);
        XP_ASSERT_DEFAULT(0);
    }

    gen->target_machine = LLVMCreateTargetMachine(
        target,
        LLVMGetDefaultTargetTriple(),
        "x86_64",
        "",
        LLVMCodeGenLevelDefault,
        LLVMRelocDefault,
        LLVMCodeModelDefault
    );
    gen->target_data = LLVMCreateTargetDataLayout(gen->target_machine);




    gen->locals = xp_hash_map_make<xpString, LLVMValueRef>(permanent_allocator());

    gen->loop_stack = make_array<LLVMLoopBlocks>(permanent_allocator());

    gen->struct_types = xp_hash_map_make<xpString, LLVMTypeRef>(permanent_allocator());
    return;
}

void free_llvm_generator(LLVMGenerator *gen) {
    LLVMDisposeBuilder(gen->builder);
    LLVMDisposeModule(gen->module);
    LLVMContextDispose(gen->ctx);
    LLVMDisposeTargetData(gen->target_data);
    LLVMDisposeTargetMachine(gen->target_machine);

    xp_hash_map_free(gen->locals);
    array_free(&gen->loop_stack);
    xp_hash_map_free(gen->struct_types);
    return;
}




int size_of_type(LLVMGenerator *gen, TypeRef type) {
    return (int)LLVMStoreSizeOfType(gen->target_data, get_llvm_type_from_type(gen, type));
}





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


LLVMTypeRef get_llvm_type_from_type(LLVMGenerator *gen, TypeRef type) {
    switch(type->kind) {
        case Type_void:
            return LLVMVoidTypeInContext(gen->ctx);
        case Type_i8:
        case Type_u8:
            return LLVMInt8TypeInContext(gen->ctx);
        case Type_i32:
        case Type_u32:
            return LLVMInt32TypeInContext(gen->ctx);
        case Type_i64:
        case Type_u64:
            return LLVMInt64TypeInContext(gen->ctx);
        case Type_bool:
            return LLVMInt1TypeInContext(gen->ctx);
        case Type_f32:
            return LLVMFloatTypeInContext(gen->ctx);
        case Type_f64:
            return LLVMDoubleTypeInContext(gen->ctx);
        case Type_pointer: {
            LLVMTypeRef pointed_type = get_llvm_type_from_type(gen, type->pointed_type);
            return LLVMPointerType(pointed_type, 0);
        }
        case Type_struct: {

            // 如果已经存在该结构体类型, 直接返回
            LLVMTypeRef *existing_struct_type = xp_hash_map_get(gen->struct_types, type->type_name);
            if(existing_struct_type != NULL) {
                return *existing_struct_type;
            }


            LLVMTypeRef *struct_type = xp_hash_map_insert(&gen->struct_types, type->type_name, LLVMStructCreateNamed(gen->ctx, type->type_name.c_str));
            
            
            Array<LLVMTypeRef> field_types = make_array_len<LLVMTypeRef>(stage_allocator(), type->struct_fields.count);
            
            for(isize i = 0; i < type->struct_fields.count; i++) {
                StructField field = type->struct_fields[i];
                LLVMTypeRef field_llvm_type = get_llvm_type_from_type(gen, field.type);
                array_push_back(&field_types, field_llvm_type);
            }

            LLVMStructSetBody(*struct_type, field_types.data, field_types.count, 0);

            return *struct_type;
        }

        case Type_array: {
            LLVMTypeRef element_type = get_llvm_type_from_type(gen, type->array_info.element_type);
            return LLVMArrayType(element_type, (unsigned)type->array_info.count);
        }

        default:
            XP_ASSERT_DEFAULT(0);
    }
}



void gen_ir_astfile(AstFile f) {

    defer(xp_arena_allocator_clear(temp_allocator()));
    defer(xp_arena_allocator_clear(stage_allocator()));

    LLVMInitializeNativeTarget();


    LLVMGenerator gen;
    init_llvm_generator(&gen);

    LLVMSetTarget(gen.module, "x86_64-pc-windows-msvc");


    for(isize i = 0; i < f.root.count; i++) {
        Ast *top_level = f.root[i];
        if(top_level->type == AstType_Function) {
            Array<LLVMTypeRef> params = make_array_len<LLVMTypeRef>(stage_allocator(), top_level->Function.params.count);
            for(isize j = 0; j < top_level->Function.params.count; j++) {
                array_push_back(&params, get_llvm_type_from_type(&gen, top_level->Function.params[j]->v_type));
            }
            LLVMTypeRef func_type = LLVMFunctionType(get_llvm_type_from_type(&gen, top_level->v_type->function_info.return_type), params.data, params.count, 0);
            LLVMValueRef function = LLVMAddFunction(gen.module, top_level->Function.name.c_str, func_type);
        }
    }

    // char *str = LLVMPrintModuleToString(gen.module); // For debug
    // printf("%s\n", str);

    // TODO
    for(isize i = 0; i < f.root.count; i++) {
        switch(f.root[i]->type) {
            case AstType_Function:
                gen_ir_function(&gen, f.root[i]);
                break;
            case AstType_StructDecl:
                // TODO 结构体声明处理
                break;
            default:
                XP_ASSERT_DEFAULT(0);
        }
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

    LLVMTypeRef i32_type = LLVMInt32TypeInContext(gen->ctx);
    LLVMValueRef func = LLVMGetNamedFunction(gen->module, function->Function.name.c_str);

    // 1. 创建入口基本块并设置插入点
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(gen->ctx, func, "entry");
    LLVMBasicBlockRef after_entry = LLVMAppendBasicBlockInContext(gen->ctx, func, "after.entry");
    
    
    LLVMPositionBuilderAtEnd(gen->builder, entry);

    // 2. 为每个参数分配 alloca 并保存到 gen->locals
    for(isize i = 0; i < function->Function.params.count; i++) {
        Ast *param = function->Function.params[i];
        LLVMValueRef param_alloca = LLVMBuildAlloca(gen->builder, get_llvm_type_from_type(gen, param->v_type), param->VariableDecl.var_name.c_str);
        LLVMBuildStore(gen->builder, LLVMGetParam(func, i), param_alloca);
        xp_hash_map_insert(&gen->locals, param->VariableDecl.var_name, param_alloca);
    }

    LLVMBuildBr(gen->builder, after_entry);
    LLVMPositionBuilderAtEnd(gen->builder, after_entry);

    LLVMState state = save_state(func, after_entry, entry);
    gen_ir_block(gen, function->Function.block, state);


    
    // 处理函数末尾没有返回语句的情况
    LLVMBasicBlockRef last_block = LLVMGetLastBasicBlock(func);
    LLVMPositionBuilderAtEnd(gen->builder, last_block);
    LLVMBuildUnreachable(gen->builder);
    
    
    // 3. 处理没有返回语句的控制流路径

    // // TODO 临时返回值 实现了返回检查后再删除
    // if(function->v_type.function_info.return_type->kind == Type_void) {
    //     LLVMBuildRetVoid(gen->builder);
    // } else {
    //     // TODO 临时返回 0
    //     if(is_float_type(*function->v_type.function_info.return_type)) {
    //         LLVMBuildRet(gen->builder, LLVMConstReal(get_llvm_type_from_type(gen, *function->v_type.function_info.return_type), 0.0));
    //     } else {
    //         LLVMBuildRet(gen->builder, LLVMConstInt(get_llvm_type_from_type(gen, *function->v_type.function_info.return_type), 0, is_signed_or_bool_type(*function->v_type.function_info.return_type)));
    //     }

    // }
}


LLVMState gen_ir_variable_decl(LLVMGenerator *gen, Ast *variable_decl, LLVMState state) {
    XP_ASSERT_DEFAULT(variable_decl->type == AstType_VariableDecl);

    // 1. 分配空间
    LLVMValueRef alloca = insert_alloca_before_last_inst_which_is_br(gen, state.entry, variable_decl->VariableDecl.var_name.c_str, get_llvm_type_from_type(gen, variable_decl->v_type));


    // 2. 如果有初始值，生成初始值 IR 并存储
    LLVMPositionBuilderAtEnd(gen->builder, state.curr_block);

    if(variable_decl->VariableDecl.expr != NULL) {
        // 有初始化表达式

        LLVMValueRef init_value = gen_ir_expr(gen, variable_decl->VariableDecl.expr, state);
        LLVMBuildStore(gen->builder, init_value, alloca);
    } else {
        // 无初始化表达式

        if(variable_decl->VariableDecl.no_zero_init) {
            // 无初始化, 不做处理
            
            // 无事发生
        } else {
            // 零初始化
            
            LLVMTypeRef var_type = get_llvm_type_from_type(gen, variable_decl->v_type);
            LLVMValueRef zero_value = LLVMConstNull(var_type);
            LLVMBuildStore(gen->builder, zero_value, alloca);
        }
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

        LLVMValueRef left_value = gen_ir_expr(gen, stmt->Assignment.left_var_expr, state, true);
        LLVMValueRef value = gen_ir_expr(gen, stmt->Assignment.right_expr, state);

        // LLVMValueRef *alloca = xp_hash_map_get(gen->locals, stmt->Assignment.left_var_expr->VarExpr.name);
        // XP_ASSERT_DEFAULT(alloca != NULL);
        // LLVMBuildStore(gen->builder, value, *alloca);

        LLVMBuildStore(gen->builder, value, left_value);
    } break;

    case AstType::AstType_ForStmt: {

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
        LLVMPositionBuilderAtEnd(gen->builder, init_block);
        state.curr_block = init_block;

        if(stmt->ForStmt.init != NULL) {
            gen_ir_stmt(gen, stmt->ForStmt.init, state);
        }
        
        // 3. 跳转到条件判断
        LLVMBuildBr(gen->builder, cond_block);


        // 4. 条件判断
        LLVMPositionBuilderAtEnd(gen->builder, cond_block);

        if(stmt->ForStmt.condition != NULL) {
            // 有条件循环
            LLVMValueRef cond_val = gen_ir_expr(gen, stmt->ForStmt.condition, state);
            LLVMBuildCondBr(gen->builder, cond_val, body_block, merge_block);
        } else {
            // 无条件循环
            LLVMBuildBr(gen->builder, body_block);
        }
        
        
        // 5. 循环体
        LLVMPositionBuilderAtEnd(gen->builder, body_block);
        state.curr_block = body_block;

        gen_ir_block(gen, stmt->ForStmt.body, state);
        LLVMBuildBr(gen->builder, post_block);

        // 6. 后置表达式
        LLVMPositionBuilderAtEnd(gen->builder, post_block);
        state.curr_block = post_block;

        if(stmt->ForStmt.post != NULL) {
            gen_ir_stmt(gen, stmt->ForStmt.post, state);
        }
        
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


LLVMValueRef gen_ir_cast_expr(LLVMGenerator *gen, Ast *cast_expr, LLVMState state) {
    
    LLVMValueRef expr_val = gen_ir_expr(gen, cast_expr->CastExpr.expr, state);

    if(size_of_type(gen, cast_expr->CastExpr.target_type) > size_of_type(gen, cast_expr->CastExpr.expr->v_type)) {
        // 扩展
        if(is_signed_type(cast_expr->CastExpr.expr->v_type)) {
            // 有符号扩展
            return LLVMBuildSExt(gen->builder, expr_val, get_llvm_type_from_type(gen, cast_expr->CastExpr.target_type), "sexttmp");
        } else {
            // 无符号扩展
            return LLVMBuildZExt(gen->builder, expr_val, get_llvm_type_from_type(gen, cast_expr->CastExpr.target_type), "zexttmp");
        }
        
    } else if(size_of_type(gen, cast_expr->CastExpr.target_type) < size_of_type(gen, cast_expr->CastExpr.expr->v_type)) {
        // 截断
        return LLVMBuildTrunc(gen->builder, expr_val, get_llvm_type_from_type(gen, cast_expr->CastExpr.target_type), "trunctmp");
    } else {
        // 相等，直接返回
        return expr_val;
    }
}


LLVMValueRef gen_ir_binary_expr(LLVMGenerator *gen, Ast *expr, LLVMState state) {
    LLVMValueRef left = gen_ir_expr(gen, expr->BinaryExpr.left, state);
    LLVMValueRef right = gen_ir_expr(gen, expr->BinaryExpr.right, state);
    switch (expr->BinaryExpr.op) {
    case TokenType::Add: // +
        if(is_float_type(expr->BinaryExpr.left->v_type)) {
            return LLVMBuildFAdd(gen->builder, left, right, "addtmp");
        } 
        if(is_pointer_type(expr->BinaryExpr.left->v_type) || is_pointer_type(expr->BinaryExpr.right->v_type)) {
            // TODO 指针加法

            LLVMValueRef pointer_expr_val = is_pointer_type(expr->BinaryExpr.left->v_type) ? left : right;
            LLVMValueRef integer_expr_val = is_pointer_type(expr->BinaryExpr.left->v_type) ? right : left;
            Ast *pointer_expr = is_pointer_type(expr->BinaryExpr.left->v_type) ? expr->BinaryExpr.left :  expr->BinaryExpr.right;
            Ast *integer_expr = is_pointer_type(expr->BinaryExpr.left->v_type) ? expr->BinaryExpr.right :  expr->BinaryExpr.left;



            LLVMValueRef indices[] = { integer_expr_val };

            return LLVMBuildGEP2(gen->builder, get_llvm_type_from_type(gen, pointer_expr->v_type->pointed_type), pointer_expr_val, indices, 1, "ptraddtmp");
        }

        return LLVMBuildAdd(gen->builder, left, right, "addtmp");
    case TokenType::Minus: // -
        if(is_float_type(expr->BinaryExpr.left->v_type)) {
            return LLVMBuildFSub(gen->builder, left, right, "subtmp");
        }

        if(is_pointer_type(expr->BinaryExpr.left->v_type) || is_pointer_type(expr->BinaryExpr.right->v_type)) {
            // TODO 指针减法

            LLVMValueRef pointer_expr_val = is_pointer_type(expr->BinaryExpr.left->v_type) ? left : right;
            LLVMValueRef integer_expr_val = is_pointer_type(expr->BinaryExpr.left->v_type) ? right : left;
            Ast *pointer_expr = is_pointer_type(expr->BinaryExpr.left->v_type) ? expr->BinaryExpr.left :  expr->BinaryExpr.right;
            Ast *integer_expr = is_pointer_type(expr->BinaryExpr.left->v_type) ? expr->BinaryExpr.right :  expr->BinaryExpr.left;

            LLVMValueRef indices[] = { LLVMBuildNeg(gen->builder, integer_expr_val, "negtmp") };

            return LLVMBuildGEP2(gen->builder, get_llvm_type_from_type(gen, pointer_expr->v_type->pointed_type), pointer_expr_val, indices, 1, "ptrsubtmp");
        }

        return LLVMBuildSub(gen->builder, left, right, "subtmp");
    case TokenType::Star: // *
        if(is_float_type(expr->BinaryExpr.left->v_type)) {
            return LLVMBuildFMul(gen->builder, left, right, "multmp");
        }

        return LLVMBuildMul(gen->builder, left, right, "multmp");
    case TokenType::ForwardSlash: // /

        if(is_float_type(expr->BinaryExpr.left->v_type)) {
            return LLVMBuildFDiv(gen->builder, left, right, "divtmp");
        }

        if(is_signed_type(expr->BinaryExpr.left->v_type)) {
            return LLVMBuildSDiv(gen->builder, left, right, "divtmp");
        } else {
            return LLVMBuildUDiv(gen->builder, left, right, "divtmp");
        }
    case TokenType::Percent: // % 
        if(is_float_type(expr->BinaryExpr.left->v_type)) {
            XP_ASSERT_DEFAULT(0); // 浮点数不支持取模运算
        }

        if(is_signed_type(expr->BinaryExpr.left->v_type)) {
            return LLVMBuildSRem(gen->builder, left, right, "modtmp");
        } else {
            return LLVMBuildURem(gen->builder, left, right, "modtmp");
        }
    case TokenType::GreaterThan: // >
        if(is_float_type(expr->BinaryExpr.left->v_type)) {
            return LLVMBuildFCmp(gen->builder, LLVMRealOGT, left, right, "gttmp");
        }

        if(is_signed_type(expr->BinaryExpr.left->v_type)) {
            return LLVMBuildICmp(gen->builder, LLVMIntSGT, left, right, "gttmp");
        } else {
            return LLVMBuildICmp(gen->builder, LLVMIntUGT, left, right, "gttmp");
        }
    case TokenType::GreaterEqual: // >=
        if(is_float_type(expr->BinaryExpr.left->v_type)) {
            return LLVMBuildFCmp(gen->builder, LLVMRealOGE, left, right, "getmp");
        }

        if(is_signed_type(expr->BinaryExpr.left->v_type)) {
            return LLVMBuildICmp(gen->builder, LLVMIntSGE, left, right, "getmp");
        } else {
            return LLVMBuildICmp(gen->builder, LLVMIntUGE, left, right, "getmp");
        }
    case TokenType::LessThan: // <
        if(is_float_type(expr->BinaryExpr.left->v_type)) {
            return LLVMBuildFCmp(gen->builder, LLVMRealOLT, left, right, "lttmp");
        }

        if(is_signed_type(expr->BinaryExpr.left->v_type)) {
            return LLVMBuildICmp(gen->builder, LLVMIntSLT, left, right, "lttmp");
        } else {
            return LLVMBuildICmp(gen->builder, LLVMIntULT, left, right, "lttmp");
        }
    case TokenType::LessEqual: // <=
        if(is_float_type(expr->BinaryExpr.left->v_type)) {
            return LLVMBuildFCmp(gen->builder, LLVMRealOLE, left, right, "letmp");
        }

        if(is_signed_type(expr->BinaryExpr.left->v_type)) {
            return LLVMBuildICmp(gen->builder, LLVMIntSLE, left, right, "letmp");
        } else {
            return LLVMBuildICmp(gen->builder, LLVMIntULE, left, right, "letmp");
        }
    case TokenType::DoubleEqual: // ==
        return gen_ir_compare_expr(gen, expr, state);
    case TokenType::ExclamationEqual: // !=
        if(is_float_type(expr->BinaryExpr.left->v_type)) {
            return LLVMBuildFCmp(gen->builder, LLVMRealONE, left, right, "netmp");
        }

        return LLVMBuildICmp(gen->builder, LLVMIntNE, left, right, "netmp");
    case TokenType::DoubleAnd: // &&
        return LLVMBuildAnd(gen->builder, left, right, "andtmp");
    case TokenType::DoubleOr: // ||
        return LLVMBuildOr(gen->builder, left, right, "ortmp");


    default:
        XP_ASSERT_DEFAULT(0);
    }
}


LLVMValueRef gen_ir_expr(LLVMGenerator *gen, Ast *expr, LLVMState state, bool is_lvalue_expr) {
    switch (expr->type)
    {
    case AstType_VarExpr: {
        LLVMValueRef *alloca = xp_hash_map_get(gen->locals, expr->VarExpr.name);
        XP_ASSERT_DEFAULT(alloca != NULL);

        // TODO 处理数组转为切片
        if(expr->implicit_conversion_tag == ImplicitConversionTag::ArrayToSliceStruct) {

            LLVMTypeRef array_type = get_llvm_type_from_type(gen, expr->v_type);
            LLVMTypeRef slice_struct_type = get_llvm_type_from_type(gen, slice_type_as_struct(expr->v_type->array_info.element_type));
            
            LLVMValueRef slice_struct_value = LLVMGetUndef(slice_struct_type);

            LLVMValueRef indices[2] = {
                LLVMConstInt(LLVMInt32TypeInContext(gen->ctx), 0, 0),
                LLVMConstInt(LLVMInt32TypeInContext(gen->ctx), 0, 0)
            };
            // 设置数据指针
            LLVMValueRef data_ptr = LLVMBuildGEP2(gen->builder, array_type, *alloca, indices, 2, "arraytoslice.data.ptr");
            slice_struct_value = LLVMBuildInsertValue(gen->builder, slice_struct_value, data_ptr, 0, "insertsliceptrtmp");
            
            // 设置count
            // TODO i64 换成 isize
            LLVMValueRef count_value = LLVMConstInt(LLVMInt64TypeInContext(gen->ctx), expr->v_type->array_info.count, 0);

            slice_struct_value = LLVMBuildInsertValue(gen->builder, slice_struct_value, count_value, 1, "insertslicecounttmp");

            return slice_struct_value;
        }


        if(is_lvalue_expr) {
            return *alloca;
        } else {
            return LLVMBuildLoad2(gen->builder, get_llvm_type_from_type(gen, expr->v_type), *alloca, expr->VarExpr.name.c_str);
        }
    } break;
    case AstType_Constant: {
        if(is_float_type(expr->v_type)) {
            // 浮点数常量
            double float_value = expr->Constant.float_value;
            return LLVMConstReal(get_llvm_type_from_type(gen,  expr->v_type), float_value);
        }

        if(expr->is_null) {
            // null 常量
            // 在llvm为*i8类型的指针
            return LLVMConstNull(LLVMPointerType(LLVMVoidTypeInContext(gen->ctx), 0));
        }

        LLVMBool is_signed = cast(LLVMBool) is_signed_or_bool_type(expr->v_type);
        return LLVMConstInt(get_llvm_type_from_type(gen, expr->v_type), expr->Constant.value, is_signed);
    } break;
    case AstType_UnaryExpr: {
        // 一元表达式（如负号）
        LLVMValueRef operand = gen_ir_expr(gen, expr->UnaryExpr.operand, state);
        if (expr->UnaryExpr.op == TokenType::Minus) {
            if(is_float_type(expr->UnaryExpr.operand->v_type)) {
                LLVMValueRef zero = LLVMConstReal(get_llvm_type_from_type(gen, expr->UnaryExpr.operand->v_type), 0.0);
                return LLVMBuildFSub(gen->builder, zero, operand, "fnegtmp");
            }

            return LLVMBuildNeg(gen->builder, operand, "negtmp");
        } else if(expr->UnaryExpr.op == TokenType::Exclamation) {
            if(is_float_type(expr->UnaryExpr.operand->v_type)) {
                XP_ASSERT_DEFAULT(0); // 浮点数不支持逻辑非运算
            }

            return LLVMBuildNot(gen->builder, operand, "nottmp");
        } else if(expr->UnaryExpr.op == TokenType::And) {
            // 取地址运算符

            // TODO 改成通过gen_ir_expr获取
            // LLVMValueRef *alloca = xp_hash_map_get(gen->locals, expr->UnaryExpr.operand->VarExpr.name);
            // XP_ASSERT_DEFAULT(alloca != NULL);
            // return *alloca;

            LLVMValueRef ptr = gen_ir_expr(gen, expr->UnaryExpr.operand, state, true);
            return ptr;

        } else if(expr->UnaryExpr.op == TokenType::Star) {
            // 解引用运算符
            LLVMValueRef ptr = gen_ir_expr(gen, expr->UnaryExpr.operand, state);
            if(is_lvalue_expr) {
                return ptr;
            } else {
                return LLVMBuildLoad2(gen->builder, get_llvm_type_from_type(gen, expr->v_type), ptr, "loadtmp");
            }
        }

        XP_ASSERT_DEFAULT(0);
    } break;

    case AstType_BinaryExpr: {
        return gen_ir_binary_expr(gen, expr, state);
    } break;

    case AstType_FunctionCallExpr: {
        LLVMValueRef func = LLVMGetNamedFunction(gen->module, expr->FunctionCallExpr.name.c_str);

        Array<LLVMValueRef> args = make_array_len<LLVMValueRef>(stage_allocator(), expr->FunctionCallExpr.args.count);
        for(isize i = 0; i < expr->FunctionCallExpr.args.count; i++) {
            LLVMValueRef arg_val = gen_ir_expr(gen,  expr->FunctionCallExpr.args[i], state);
            array_push_back(&args, arg_val);
        }

        LLVMTypeRef type = LLVMGlobalGetValueType(func);

        return LLVMBuildCall2(gen->builder, type, func, args.data, args.count, "calltmp");
    } break;

    case AstType_CastExpr: {
        return gen_ir_cast_expr(gen, expr, state);
    } break;

    case AstType_FieldAccessExpr: {
        Ast *parent_expr = NULL;
        if(is_struct_type(expr->FieldAccessExpr.parent_expr->v_type)) {
            parent_expr = expr->FieldAccessExpr.parent_expr;
        } else if(is_pointer_type(expr->FieldAccessExpr.parent_expr->v_type) && is_struct_type(get_pointed_type(expr->FieldAccessExpr.parent_expr->v_type))) {
            
            // 如果是指向结构体的指针，则需要先解引用
            parent_expr = ast_alloc(AstType_UnaryExpr, stage_allocator());
            parent_expr->UnaryExpr.op = TokenType::Star;
            parent_expr->UnaryExpr.operand = expr->FieldAccessExpr.parent_expr;
            parent_expr->v_type = get_pointed_type(expr->FieldAccessExpr.parent_expr->v_type);
        } else {
            XP_ASSERT_DEFAULT(0);
        }

        TypeRef struct_type = parent_expr->v_type;
        // Type struct_type_detail = get_type_detail_if_have(symbol_table(), struct_type);
        
        
        isize field_index = -1;
        for(isize i = 0; i < struct_type->struct_fields.count; i++) {
            if(struct_type->struct_fields[i].name == expr->FieldAccessExpr.field_name) {
                field_index = i;
                break;
            }
        }
        XP_ASSERT_DEFAULT(field_index != -1);
        

        if(is_lvalue_expr) {
            // 这里正好发现is_lvalue_expr用来获取指针的场景
            LLVMValueRef struct_ptr = gen_ir_expr(gen, parent_expr, state, true);

            LLVMValueRef field_ptr = LLVMBuildStructGEP2(gen->builder, get_llvm_type_from_type(gen, struct_type), struct_ptr, field_index, "fieldptrtmp");
            return field_ptr;
        } else {
            LLVMValueRef struct_val = gen_ir_expr(gen, parent_expr, state);
            LLVMValueRef field_val = LLVMBuildExtractValue(gen->builder, struct_val, field_index, "fieldextractedtmp");
            return field_val;
        }

    } break;
    
    case AstType_StructInitExpr: {
        LLVMTypeRef struct_type = get_llvm_type_from_type(gen, expr->v_type);

        // 先用 undef 初始化
        LLVMValueRef struct_val = LLVMGetUndef(struct_type);

        for(isize i = 0; i < expr->StructInitExpr.field_inits.count; i++) {
            LLVMValueRef field_value = gen_ir_expr(gen, expr->StructInitExpr.field_inits[i], state);
            struct_val = LLVMBuildInsertValue(gen->builder, struct_val, field_value, i, "insertvaltmp");
        }

        return struct_val;

    } break;

    case AstType_ArrayInitExpr: {
        LLVMTypeRef array_type = get_llvm_type_from_type(gen, expr->v_type);

        // 先用 undef 初始化
        LLVMValueRef array_val = LLVMGetUndef(array_type);

        for(isize i = 0; i < expr->ArrayInitExpr.elements.count; i++) {
            LLVMValueRef element_value = gen_ir_expr(gen, expr->ArrayInitExpr.elements[i], state);
            array_val = LLVMBuildInsertValue(gen->builder, array_val, element_value, i, "arrayinsertvaltmp");
        }

        return array_val;

    } break;

    case AstType_IndexExpr: {
        LLVMValueRef array_or_slice_ptr = gen_ir_expr(gen, expr->IndexExpr.array_var_expr, state, true);
        LLVMValueRef index_val = gen_ir_expr(gen, expr->IndexExpr.index_expr, state);


        LLVMValueRef indices[2] = { NULL };
        LLVMValueRef elem_ptr = NULL;
        if(is_array_type(expr->IndexExpr.array_var_expr->v_type)) {
            // 数组索引
            indices[0] = LLVMConstInt(LLVMInt32TypeInContext(gen->ctx), 0, 0); // 取数组指针的第0个元素, 就是数组本身
            indices[1] = index_val; // 索引值

            elem_ptr = LLVMBuildGEP2(gen->builder, get_llvm_type_from_type(gen, expr->IndexExpr.array_var_expr->v_type), array_or_slice_ptr, indices, 2, "arrayelementptrtmp");

        } else if(is_slice_struct_type(expr->IndexExpr.array_var_expr->v_type)) {
            // 切片索引
            
            LLVMValueRef slice_val = LLVMBuildLoad2(gen->builder, get_llvm_type_from_type(gen, expr->IndexExpr.array_var_expr->v_type), array_or_slice_ptr, "loadslicetmp");
            LLVMValueRef data_raw = LLVMBuildExtractValue(gen->builder, slice_val, 0, "slicedataptrtmp");

            TypeRef data_ptr_type = expr->IndexExpr.array_var_expr->v_type->struct_fields[0].type;
            LLVMTypeRef data_ptr_llvm_type = get_llvm_type_from_type(gen, data_ptr_type);
            


            LLVMValueRef data_typed_ptr = LLVMBuildBitCast(gen->builder, data_raw, data_ptr_llvm_type, "slicedataptrtypedtmp");
            
            indices[0] = index_val; // 索引值
            elem_ptr = LLVMBuildGEP2(gen->builder, get_llvm_type_from_type(gen, data_ptr_type->pointed_type), data_typed_ptr, indices, 1, "sliceelementptrtmp");

        } else {
            XP_ASSERT_DEFAULT(0);
        }


        if(is_lvalue_expr) {
            return elem_ptr;
        } else {
            return LLVMBuildLoad2(gen->builder, get_llvm_type_from_type(gen, expr->v_type), elem_ptr, "loadelementtmp");
        }

    } break;

    default:
        printf("\n-------------------------------------------\n");
        print_ast(expr);
        XP_ASSERT_DEFAULT(0);
    }


}


static LLVMValueRef compare_two_values(LLVMGenerator *gen, LLVMValueRef left, LLVMValueRef right, TypeRef type) {
    if(is_struct_type(type)) {
        // Type type_detail = get_type_detail_if_have(symbol_table(), type);
        
        
        LLVMValueRef result = nullptr;
        for(isize i = 0; i < type->struct_fields.count; i++) {
            LLVMValueRef left_field = LLVMBuildExtractValue(gen->builder, left, i, "leftextracttmp");
            LLVMValueRef right_field = LLVMBuildExtractValue(gen->builder, right, i, "rightextracttmp");
            
            LLVMValueRef field_cmp = compare_two_values(gen, left_field, right_field, type->struct_fields[i].type);
            
            if(result == nullptr) {
                result = field_cmp;
            } else {
                result = LLVMBuildAnd(gen->builder, result, field_cmp, "andeqtmp");
            }
        }

        return result;

    } else if(is_float_type(type)) {
        return LLVMBuildFCmp(gen->builder, LLVMRealOEQ, left, right, "eqtmp");
    } else {
        return LLVMBuildICmp(gen->builder, LLVMIntEQ, left, right, "eqtmp");
    }

}

LLVMValueRef gen_ir_compare_expr(LLVMGenerator *gen, Ast *expr, LLVMState state) {
    XP_ASSERT_DEFAULT(expr->type == AstType_BinaryExpr);
    return compare_two_values(gen, gen_ir_expr(gen, expr->BinaryExpr.left, state), gen_ir_expr(gen, expr->BinaryExpr.right, state), expr->BinaryExpr.left->v_type);
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