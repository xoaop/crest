#include "llvm_generate_ir.hpp"

#include "common.hpp"

#include "symbol.hpp"

#include "ast.hpp"

#include "analyser.hpp"

#include "context.hpp"




struct IRScope {
    IRScope *parent;
    Scope *scope;

    xpHashMap<SymbolInfo *, LLVMValueRef> local_vals;
};





struct LLVMLoopBlocks {
    LLVMBasicBlockRef post_block;
    LLVMBasicBlockRef merge_block;
};



// LLVM IR 生成器, 保存生成一个Module所需的状态
// 目前一个Module就代表一个package
struct LLVMGenerator {
    LLVMContextRef ctx;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMTargetMachineRef target_machine;
    LLVMTargetDataRef target_data;

    
    Array<LLVMLoopBlocks> loop_stack;
    
    xpHashMap<xpString, LLVMTypeRef> struct_types;
    
    xpHashSet<xpString> declared_extern_functions;
    
    Package *pkg;

    

    IRScope *curr_ir_scope;
};




struct LLVMState {
    LLVMValueRef curr_function;
    LLVMBasicBlockRef curr_block;
    LLVMBasicBlockRef entry;
};










IRScope *alloc_ir_scope(xpAllocator allocator) {
    return cast(IRScope *)xp_alloc(allocator, sizeof(IRScope));
}


IRScope *new_ir_scope(IRScope *parent, Scope *scope) {
    IRScope *new_scope = alloc_ir_scope(stage_allocator());
    new_scope->parent = parent;
    new_scope->scope = scope;
    new_scope->local_vals = xp_hash_map_make<SymbolInfo *, LLVMValueRef>(stage_allocator());
    return new_scope;
}


IRScope *new_ir_scope(IRScope *parent, Ast *related_ast) {
    Scope **scope = xp_hash_map_get(context()->ast_scope_map, related_ast);
    XP_ASSERT_DEFAULT(scope != NULL);

    return new_ir_scope(parent, *scope);
}

void exit_ir_scope(LLVMGenerator *gen) {
    IRScope *curr_scope = gen->curr_ir_scope;
    gen->curr_ir_scope = curr_scope->parent;
    return;
}




LLVMValueRef look_up_local_vals(IRScope *ir_scope, SymbolInfo *symbol_info) {
    IRScope *curr_scope = ir_scope;
    while(curr_scope != NULL) {
        LLVMValueRef *val = xp_hash_map_get(curr_scope->local_vals, symbol_info);
        if(val != NULL) {
            return *val;
        }
        curr_scope = curr_scope->parent;
    }
    return NULL;
}

IRScope *climb_to_spec_scope(IRScope *ir_scope, ScopeType spec_type) {
    IRScope *curr_scope = ir_scope;
    while(curr_scope != NULL) {
        if(curr_scope->scope->scope_type == spec_type) {
            return curr_scope;
        }
        curr_scope = curr_scope->parent;
    }
    return NULL;
}

LLVMValueRef *add_local_val(IRScope *ir_scope, SymbolInfo *symbol_info, LLVMValueRef val) {
    return xp_hash_map_insert(&ir_scope->local_vals, symbol_info, val);
}


LLVMValueRef *add_local_val_spec_scope(IRScope *ir_scope, ScopeType scope_type, SymbolInfo *symbol_info, LLVMValueRef val) {
    return xp_hash_map_insert(&climb_to_spec_scope(ir_scope, scope_type)->local_vals, symbol_info, val);
}





void load_state(LLVMGenerator *gen, LLVMState state);
LLVMState save_state(LLVMValueRef curr_function, LLVMBasicBlockRef curr_block, LLVMBasicBlockRef entry);


LLVMTypeRef get_llvm_type_from_type(LLVMGenerator *gen, TypeRef type);


void gen_ir_package(Package *pkg, LLVMIRGenerateConfig config);
void gen_ir_astfile(AstFile f, LLVMGenerator *gen);
void gen_ir_function(LLVMGenerator *gen, Ast *function);
LLVMState gen_ir_variable_decl(LLVMGenerator *gen, Ast *variable_decl, LLVMState state);
LLVMState gen_ir_block(LLVMGenerator *gen, Ast *block, LLVMState state, bool need_new_scope = true);
LLVMState gen_ir_stmt(LLVMGenerator *gen, Ast *stmt, LLVMState state);
LLVMValueRef gen_ir_expr(LLVMGenerator *gen, Ast *expr, LLVMState state, bool is_lvalue_expr = false);
LLVMValueRef gen_ir_compare_expr(LLVMGenerator *gen, Ast *expr, LLVMState state, bool is_not_equal = false);
LLVMValueRef gen_array_value_to_slice_cast(LLVMGenerator *gen, LLVMValueRef array_value_ptr, TypeRef array_value_type);


void init_llvm() {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeDisassembler();
}

void init_llvm_generator(LLVMGenerator *gen, Package *pkg, xpAllocator allocator) {
    gen->ctx = LLVMContextCreate();
    gen->module = LLVMModuleCreateWithNameInContext(pkg->path.c_str, gen->ctx);
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
        "x86-64",
        "",
        LLVMCodeGenLevelDefault,
        LLVMRelocDefault,
        LLVMCodeModelDefault
    );
    gen->target_data = LLVMCreateTargetDataLayout(gen->target_machine);





    gen->loop_stack = make_array<LLVMLoopBlocks>(allocator);

    gen->struct_types = xp_hash_map_make<xpString, LLVMTypeRef>(allocator);

    gen->declared_extern_functions = xp_hash_set_make<xpString>(allocator);

    gen->pkg = pkg;

    gen->curr_ir_scope = new_ir_scope(NULL, &pkg->package_scope);
    return;
}

void free_llvm_generator(LLVMGenerator *gen) {
    LLVMDisposeBuilder(gen->builder);
    LLVMDisposeModule(gen->module);
    LLVMContextDispose(gen->ctx);
    LLVMDisposeTargetData(gen->target_data);
    LLVMDisposeTargetMachine(gen->target_machine);

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



void llvm_build_br_when_no_br(LLVMGenerator *gen, LLVMBasicBlockRef from, LLVMBasicBlockRef to) {
    if(!LLVMGetBasicBlockTerminator(from)) {
        LLVMBuildBr(gen->builder, to);
    }
}



SymbolInfo *get_extern_func_sym_by_full_ident_ast(Ast *field_access, LLVMGenerator *gen) {
    XP_ASSERT_DEFAULT(field_access->type == AstType_FieldAccess);

    xpString parent_name = field_access->FieldAccess.parent->Ident.name;
    xpString field_name = field_access->FieldAccess.field_name;

    SymbolInfo *import_symbol = find_symbol_until_spec_v(
        ScopeType::File,
        gen->curr_ir_scope->scope,
        parent_name,
        Type_package
    );

    Package *imported_package = get_package_value(import_symbol->value);

    SymbolInfo *func_symbol = find_symbol_until_spec_v(
        ScopeType::Package,
        &imported_package->package_scope,
        field_name,
        Type_function
    );
    XP_ASSERT_DEFAULT(func_symbol != NULL);

    return func_symbol;
}


LLVMValueRef get_ptr_of_llvm_value(LLVMGenerator *gen, LLVMValueRef value, bool allow_alloc_value_to_get_ptr = true) {
    if(LLVMIsALoadInst(value)) {
        // 如果是load指令, 直接返回被加载的地址

        return LLVMGetOperand(value, 0);
    } else if(LLVMIsAAllocaInst(value)) {
        // 如果是alloca指令, 直接返回
        return value;
    } else if(LLVMIsAGlobalVariable(value)) {
        // 如果是全局变量, 直接返回
        return value;
    }

    // 否则, 需要先把值存到内存中, 再返回地址
    if(allow_alloc_value_to_get_ptr) {
        LLVMTypeRef value_type = LLVMTypeOf(value);
        LLVMValueRef temp_alloca = insert_alloca_before_last_inst_which_is_br(gen, LLVMGetInsertBlock(gen->builder), "temp", value_type);
        LLVMBuildStore(gen->builder, value, temp_alloca);
        return temp_alloca;
    } else {
        XP_ASSERT_DEFAULT(0 && "Compiler Internal Error: Cannot get pointer of value, and not allowed to alloc value to get pointer");
    }
}


xpString gen_func_full_name(Package *package, xpString func_name, bool is_extern_c, xpAllocator allocator) {
    if(xp_string_equal(func_name, xp_string_c("main")) || is_extern_c) {
        return func_name;
    }

    return xp_string_concat_mid(
        package->path,
        func_name,
        xpOption<xpString>(xp_string_c(".")),
        allocator
    );
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

            xpString full_type_name = xp_string_concat_mid(
                type->struct_info.pkg->path, 
                type->type_name,
                xpOption<xpString>(xp_string_c(".")),
                stage_allocator()
            );

            // 如果已经存在该结构体类型, 直接返回
            LLVMTypeRef *existing_struct_type = xp_hash_map_get(gen->struct_types, full_type_name);
            if(existing_struct_type != NULL) {
                return *existing_struct_type;
            }

            LLVMTypeRef struct_ty = LLVMStructCreateNamed(gen->ctx, full_type_name.c_str);
            LLVMTypeRef *struct_type = xp_hash_map_insert(&gen->struct_types, full_type_name, struct_ty);
            
            
            Array<LLVMTypeRef> field_types = make_array_len<LLVMTypeRef>(stage_allocator(), type->struct_info.struct_fields.count);
            
            for(isize i = 0; i < type->struct_info.struct_fields.count; i++) {
                StructField field = type->struct_info.struct_fields[i];
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

        case Type_function: {
            Array<LLVMTypeRef> params = make_array_len<LLVMTypeRef>(stage_allocator(), type->function_info.param_types.count);
            for(isize i = 0; i < type->function_info.param_types.count; i++) {
                array_push_back(&params, get_llvm_type_from_type(gen, type->function_info.param_types[i]));
            }
            
            LLVMTypeRef func_type = LLVMFunctionType(get_llvm_type_from_type(gen, type->function_info.return_type), params.data, params.count, 0);
            
            return func_type;
        } break;

        // *NOTE: 这两种类型只会在常量出现, 且一定会被转为其它类型
        case Type_untyped_int:
            return LLVMInt128TypeInContext(gen->ctx);
        case Type_untyped_float:
            return LLVMDoubleTypeInContext(gen->ctx);


        default:
            XP_ASSERT_DEFAULT(0);
    }
}


void gen_ir_all_packages(Array<Package> all_packages, LLVMIRGenerateConfig config) {
    defer(xp_arena_allocator_clear(stage_allocator()));


    for(isize i = 0; i < all_packages.count; i++) {
        gen_ir_package(&all_packages[i], config);
    }

}


LLVMValueRef get_llvm_func_val_in_scope_by_ori_name(LLVMGenerator *gen, xpString ori_func_name) {
    SymbolInfo *symbol_info = find_symbol_until_spec_v(
        ScopeType::Package,
        gen->curr_ir_scope->scope,
        ori_func_name,
        Type_function
    );
    XP_ASSERT_DEFAULT(symbol_info != NULL);

    LLVMValueRef func_val = look_up_local_vals(gen->curr_ir_scope, symbol_info);
    XP_ASSERT_DEFAULT(func_val != NULL);

    return func_val;
}


void gen_ir_package(Package *pkg, LLVMIRGenerateConfig config) {
    LLVMGenerator gen;
    init_llvm_generator(&gen, pkg, stage_allocator());
    defer(free_llvm_generator(&gen));

    // 声明当前包中的所有函数/全局常量
    for(isize i = 0; i < pkg->ast_files.count; i++) {
        AstFile f = pkg->ast_files[i];

        for(isize j = 0; j < f.top_levels.count; j++) {
            Ast *top_level = f.top_levels[j];

            if(top_level->type == AstType_ConstDecl) {
                SymbolInfo *symbol_info = find_symbol_until(
                    ScopeType::Package,
                    &f.file_scope,
                    top_level->ConstDecl.name
                );

                if(top_level->ConstDecl.value_ast->type == AstType_FunctionDeclValue) {
                    // 有定义的函数, 如
                    // main :: () -> i32 { 
                    //     return 0; 
                    // }
                    // 

                    xpString full_func_name = gen_func_full_name(symbol_info->package, top_level->ConstDecl.name, symbol_info->value.function_value.is_extern_c, stage_allocator());
    
                    LLVMValueRef function = LLVMAddFunction(
                        gen.module, 
                        full_func_name.c_str,
                        get_llvm_type_from_type(&gen, top_level->v_type)
                    );

                    add_local_val_spec_scope(gen.curr_ir_scope, ScopeType::Package, symbol_info, function);

                }


            }
        }
    }

    for(isize i = 0; i < pkg->ast_files.count; i++) {
        // gen.curr_file_scope = &pkg->ast_files[i].file_scope;

        gen.curr_ir_scope = new_ir_scope(gen.curr_ir_scope, &pkg->ast_files[i].file_scope);
        defer(exit_ir_scope(&gen));
    
        gen_ir_astfile(pkg->ast_files[i], &gen);
    }


    // 输出 .ll 文件
    char *error = NULL;

    xpString obj_file_path = xp_make_string(stage_allocator(), "output/");
    xpString obj_file_name = xp_string_replace_char(pkg->path, '/', '_', stage_allocator());
    xpString ll_file_path = xp_string_copy(stage_allocator(), obj_file_path);
    xp_string_append(&obj_file_path, obj_file_name);
    xp_string_append(&obj_file_path, xp_string_c(".o"));
    
    xp_string_append(&ll_file_path, obj_file_name);
    xp_string_append(&ll_file_path, xp_string_c(".ll"));

    
    if(LLVMPrintModuleToFile(gen.module, ll_file_path.c_str, &error)) {
        fprintf(stderr, "Error writing .ll file: %s\n", error);
        LLVMDisposeMessage(error);
    }
    
    if(LLVMVerifyModule(gen.module, LLVMReturnStatusAction, &error)) {
        fprintf(stderr, "Module verification failed: %s\n", error);
        LLVMDisposeMessage(error);
        XP_ASSERT_DEFAULT(0);
    }

    // 优化
    LLVMPassBuilderOptionsRef options = LLVMCreatePassBuilderOptions();
    defer(LLVMDisposePassBuilderOptions(options));

    // 选择优化级别
    char const *passes = NULL;
    switch(config.optimization_level) {
        case LLVMIROptimizationLevel::O0:
            passes = "default<O0>";
            break;
        case LLVMIROptimizationLevel::O1:
            passes = "default<O1>";
            break;
        case LLVMIROptimizationLevel::O2:
            passes = "default<O2>";
            break;
        case LLVMIROptimizationLevel::O3:
            passes = "default<O3>";
            break;
        case LLVMIROptimizationLevel::Os:
            passes = "default<Os>";
            break;
        case LLVMIROptimizationLevel::Oz:
            passes = "default<Oz>";
            break;
    }

    // 运行优化
    LLVMRunPasses(gen.module, passes, gen.target_machine, options);

    // 生成.o文件
    if(LLVMTargetMachineEmitToFile(
        gen.target_machine,
        gen.module,
        obj_file_path.c_str,
        LLVMObjectFile,
        &error
    )) {
        fprintf(stderr, "Error writing object file: %s\n", error);
        LLVMDisposeMessage(error);
    }
    

}


// TODO: 浮点数!!
LLVMValueRef gen_ir_cast(LLVMGenerator *gen, TypeRef from_type, TypeRef to_type, LLVMValueRef value) {

    if(is_array_type(from_type) && is_slice_struct_type(to_type)) {
        // 数组到切片的隐式转换
        return gen_array_value_to_slice_cast(gen, get_ptr_of_llvm_value(gen, value), from_type);
    }

    // 相等, 直接返回
    if(from_type == to_type) {
        return value;
    }


    if(is_pointer_type(from_type) && is_pointer_type(to_type)) {
        // 指针类型之间的转换, 不用转化, 直接返回
        return value;
    }

    // 下面是整数,浮点数之间的转换
    bool short_to_long = size_of_type(gen, from_type) < size_of_type(gen, to_type);
    bool long_to_short = size_of_type(gen, from_type) > size_of_type(gen, to_type);
    bool same_size = size_of_type(gen, from_type) == size_of_type(gen, to_type);
    

    if(is_float_or_untyped_type(from_type) && is_float_or_untyped_type(to_type)) {
        // 浮点数之间的转换
        if(short_to_long) {
            // 扩展
            return LLVMBuildFPExt(gen->builder, value, get_llvm_type_from_type(gen, to_type), "fpexttmp");
        } else if(long_to_short) {
            // 截断
            return LLVMBuildFPTrunc(gen->builder, value, get_llvm_type_from_type(gen, to_type), "fptrunctmp");
        } else if(same_size){
            // 相等，直接返回
            return value;
        }
    }

    if(is_integer_or_untyped_type(from_type) && is_float_or_untyped_type(to_type)) {
        // 整数到浮点数的转换
        if(is_signed_type(from_type)) {
            // 有符号整数到浮点数
            return LLVMBuildSIToFP(gen->builder, value, get_llvm_type_from_type(gen, to_type), "sitofptmp");
        } else {
            // 无符号整数到浮点数
            return LLVMBuildUIToFP(gen->builder, value, get_llvm_type_from_type(gen, to_type), "uitofptmp");
        }
    }

    if(is_float_or_untyped_type(from_type) && is_integer_or_untyped_type(to_type)) {
        // 浮点数到整数的转换
        if(is_signed_type(to_type)) {
            // 浮点数到有符号整数
            return LLVMBuildFPToSI(gen->builder, value, get_llvm_type_from_type(gen, to_type), "fptositmp");
        } else {
            // 浮点数到无符号整数
            return LLVMBuildFPToUI(gen->builder, value, get_llvm_type_from_type(gen, to_type), "fptouitmp");
        }
    }


    // 整数
    if(is_integer_or_untyped_type(from_type) && is_integer_or_untyped_type(to_type)) {
        if(short_to_long) {
            // 扩展
            if(is_signed_type(from_type)) {
                // 有符号扩展
                return LLVMBuildSExt(gen->builder, value, get_llvm_type_from_type(gen, to_type), "sexttmp");
            } else {
                // 无符号扩展
                return LLVMBuildZExt(gen->builder, value, get_llvm_type_from_type(gen, to_type), "zexttmp");
            }
            
        } else if(long_to_short) {
            // 截断
            return LLVMBuildTrunc(gen->builder, value, get_llvm_type_from_type(gen, to_type), "trunctmp");
        } else if(same_size){
            // 相等，直接返回
            return value;
        }
    }

    UNREACHABLE();
}



LLVMValueRef gen_llvm_val_by_value(LLVMGenerator *gen, Value& value, xpOption<TypeRef> expected_type) {
     // TODO 其他类型的常量

    LLVMValueRef llvm_val = NULL;
    switch(value.type->kind) {
        case Type_i8:
        case Type_u8:
        case Type_i32:
        case Type_u32:
        case Type_i64:
        case Type_u64:
            llvm_val = LLVMConstInt(get_llvm_type_from_type(gen, value.type), cast(unsigned long long)get_integer_value(value), is_signed_or_bool_type(value.type));
            break;
        case Type_bool:
            llvm_val = LLVMConstInt(get_llvm_type_from_type(gen, value.type), cast(unsigned long long)get_bool_value(value), is_signed_or_bool_type(value.type));
            break;
        case Type_f32:
        case Type_f64:
            llvm_val = LLVMConstReal(get_llvm_type_from_type(gen, value.type), cast(double)get_float_value(value));
            break;

        case Type_untyped_int: 
            llvm_val = LLVMConstInt(LLVMInt128TypeInContext(gen->ctx), cast(unsigned long long)get_integer_value(value), false);
            break;

        case Type_untyped_float:
            llvm_val = LLVMConstReal(LLVMDoubleTypeInContext(gen->ctx), cast(double)get_float_value(value));
            break;

        case Type_array: {
            XP_TODO;
        } break;

        case Type_struct: {
            XP_TODO;
        } break;

        default:
            // TODO 其他类型的常量
            UNREACHABLE();
            return NULL;
            break;
    }

    if(expected_type.has_value()) {
        return gen_ir_cast(gen, value.type, expected_type.unwrap(), llvm_val);
    } else {
        return llvm_val;
    }

}

bool is_func_defined_in_file(Ast *func_ast) {
    return func_ast->type == AstType_ConstDecl && func_ast->ConstDecl.value_ast->type == AstType_FunctionDeclValue;
}

SymbolInfo *get_ori_func_symbol_by_func_value(Value& val) {
    XP_ASSERT_DEFAULT(is_function_type(val.type));

    Ast *ori_func_const_decl = val.val_ast;
    XP_ASSERT_DEFAULT(ori_func_const_decl != NULL);

    SymbolInfo *ori_func_sym = ori_func_const_decl->ast_symbol;
    XP_ASSERT_DEFAULT(ori_func_sym != NULL);

    return ori_func_sym;
}

void gen_ir_const_decl(Ast *const_decl, LLVMGenerator *gen) {
    XP_ASSERT_DEFAULT(const_decl->type == AstType_ConstDecl);
    Ast *value_ast = const_decl->ConstDecl.value_ast;

    SymbolInfo *symbol_info = find_symbol_until(
        ScopeType::Package,
        gen->curr_ir_scope->scope,
        const_decl->ConstDecl.name
    );
    TypeRef val_type = symbol_info->value.type;


    if(is_function_type(val_type)) {
        // TODO: ABSTRACT: 获取原始函数符号, 并生成LLVM函数(如没生成)
        SymbolInfo *ori_func_sym = get_ori_func_symbol_by_func_value(symbol_info->value);

        LLVMValueRef ori_func_llvm_val = look_up_local_vals(gen->curr_ir_scope, ori_func_sym);

        if(ori_func_llvm_val == NULL) {
            xpString func_full_name = gen_func_full_name(ori_func_sym->package, ori_func_sym->name, ori_func_sym->value.function_value.is_extern_c, stage_allocator());
            LLVMValueRef func_val = LLVMAddFunction(gen->module, func_full_name.c_str, get_llvm_type_from_type(gen, val_type));
            add_local_val_spec_scope(gen->curr_ir_scope, ScopeType::Package, ori_func_sym, func_val);
        }

    }

}




void gen_ir_astfile(AstFile f, LLVMGenerator *gen) {
    

    for(isize i = 0; i < f.top_levels.count; i++) {
        switch(f.top_levels[i]->type) {

            case AstType_ConstDecl:
                gen_ir_const_decl(f.top_levels[i], gen);
                break;
            
            case AstType_VariableDecl:
                XP_TODO;
                break;

            default:
                UNREACHABLE();
        }
    }


    for(isize i = 0; i < f.top_levels.count; i++) {
        // 有block的函数

        if(is_func_defined_in_file(f.top_levels[i])) {
            gen_ir_function(gen, f.top_levels[i]);
        }
    }

}



void gen_ir_function(LLVMGenerator *gen, Ast *function) {
    XP_ASSERT_DEFAULT(function->type == AstType_ConstDecl);

    Ast *val_ast = function->ConstDecl.value_ast;
    XP_ASSERT_DEFAULT(val_ast->type == AstType_FunctionDeclValue);

    // TODO: TEST extern_C
    if(val_ast->FunctionDeclValue.is_extern_c) {
        return;
    }

    gen->curr_ir_scope = new_ir_scope(gen->curr_ir_scope, function);
    defer(exit_ir_scope(gen));

    LLVMValueRef func = look_up_local_vals(gen->curr_ir_scope, function->ast_symbol);

    // 1. 创建入口基本块并设置插入点
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(gen->ctx, func, "entry");
    LLVMBasicBlockRef after_entry = LLVMAppendBasicBlockInContext(gen->ctx, func, "after.entry");
    
    
    LLVMPositionBuilderAtEnd(gen->builder, entry);

    // 2. 为每个参数分配 alloca 并保存到 gen->locals
    for(isize i = 0; i < val_ast->FunctionDeclValue.params.count; i++) {
        Ast *param = val_ast->FunctionDeclValue.params[i];
        SymbolInfo *param_symbol = find_symbol_until_global(
            gen->curr_ir_scope->scope,
            param->VariableDecl.var_name
        );


        LLVMValueRef param_alloca = LLVMBuildAlloca(gen->builder, get_llvm_type_from_type(gen, param->v_type), param->VariableDecl.var_name.c_str);
        LLVMBuildStore(gen->builder, LLVMGetParam(func, i), param_alloca);


        add_local_val(gen->curr_ir_scope, param_symbol, param_alloca);
    }

    LLVMBuildBr(gen->builder, after_entry);
    LLVMPositionBuilderAtEnd(gen->builder, after_entry);

    LLVMState state = save_state(func, after_entry, entry);
    gen_ir_block(gen, val_ast->FunctionDeclValue.block, state);


    // TypeRef func_type = find_symbol(symbol_table(), function->Function.name)->type;
    XP_ASSERT_DEFAULT(function->ast_symbol != NULL);
    TypeRef func_type = function->ast_symbol->value.type;
    

    // if(func_type->function_info.return_type == easy_type(Type_void)) {
    //     // void 函数自动添加返回

    //     LLVMBuildRetVoid(gen->builder);
    // }
    
    if(!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(gen->builder))) {
        // 如果最后一个基本块没有终止指令, 则添加unreachable
        LLVMBuildUnreachable(gen->builder);
    }

    
    
}


LLVMState gen_ir_variable_decl(LLVMGenerator *gen, Ast *variable_decl, LLVMState state) {
    XP_ASSERT_DEFAULT(variable_decl->type == AstType_VariableDecl);

    // 1. 分配空间
    LLVMValueRef alloca = insert_alloca_before_last_inst_which_is_br(gen, state.entry, variable_decl->VariableDecl.var_name.c_str, get_llvm_type_from_type(gen, variable_decl->v_type));


    // 2. 如果有初始值，生成初始值 IR 并存储
    LLVMPositionBuilderAtEnd(gen->builder, state.curr_block);

    SymbolInfo *var_info = find_symbol_until_global(
        gen->curr_ir_scope->scope,
        variable_decl->VariableDecl.var_name
    );

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
            
            LLVMTypeRef var_type = get_llvm_type_from_type(gen, var_info->value.type);
            LLVMValueRef zero_value = LLVMConstNull(var_type);
            LLVMBuildStore(gen->builder, zero_value, alloca);
        }
    }

    add_local_val(gen->curr_ir_scope, var_info, alloca);

    return state;
}




LLVMState gen_ir_block(LLVMGenerator *gen, Ast *block, LLVMState state, bool need_new_scope) {
    XP_ASSERT_DEFAULT(block->type == AstType_Block);

    if(need_new_scope) {
        gen->curr_ir_scope = new_ir_scope(gen->curr_ir_scope, block);
    } 

    for(isize i = 0; i < block->Block.statements.count; i++) {
        state = gen_ir_stmt(gen, block->Block.statements[i], state);
    }

    if(need_new_scope) {
        exit_ir_scope(gen);
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
        
        // LLVMBuildBr(gen->builder, merge_block);
        llvm_build_br_when_no_br(gen, LLVMGetInsertBlock(gen->builder), merge_block);

        //  else 分支
        LLVMPositionBuilderAtEnd(gen->builder, else_block);
        state.curr_block = else_block;
        if (stmt->IfStmt.else_block) {
            gen_ir_block(gen, stmt->IfStmt.else_block, state);
        }
        llvm_build_br_when_no_br(gen, LLVMGetInsertBlock(gen->builder), merge_block);

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

        gen->curr_ir_scope = new_ir_scope(gen->curr_ir_scope, stmt);
        defer(exit_ir_scope(gen));

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

        gen_ir_block(gen, stmt->ForStmt.body, state, false);
        // LLVMBuildBr(gen->builder, post_block);
        llvm_build_br_when_no_br(gen, LLVMGetInsertBlock(gen->builder), post_block);

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

        if(stmt->ReturnStmt.expr == NULL) {
            // void 返回
            LLVMBuildRetVoid(gen->builder);
            break;
        }

        LLVMValueRef ret_val = gen_ir_expr(gen, stmt->ReturnStmt.expr, state);
        LLVMBuildRet(gen->builder, ret_val);
    } break;

    case AstType_Break: {
        LLVMBuildBr(gen->builder, gen->loop_stack[gen->loop_stack.count - 1].merge_block);
    } break;
    case AstType_Continue: {
        LLVMBuildBr(gen->builder, gen->loop_stack[gen->loop_stack.count - 1].post_block);
    } break;


    case AstType_Block: {
        state = gen_ir_block(gen, stmt, state);
    } break;

    case AstType_ConstDecl: {
        // 局部ConstDecl, 不用处理, 因为都已经在语义分析阶段计算好值并存到符号表里了, 直接gen_ir_expr的时候从符号表里取值就行了
    } break;

    default: {
        gen_ir_expr(gen, stmt, state);
        break;
    }


    }

    return state;
}

LLVMValueRef gen_array_value_to_slice_cast(LLVMGenerator *gen, LLVMValueRef array_value_ptr, TypeRef array_value_type) {
    // NOTE: 目前为止, array_type的来源只有数组变量和数组字面量, 注意在处理这两种类型都要检查要不要调用这个函数来进行数组到切片的隐式转换


    LLVMTypeRef array_type = get_llvm_type_from_type(gen, array_value_type);
    LLVMTypeRef slice_struct_type = get_llvm_type_from_type(gen, slice_type_as_struct(array_value_type->array_info.element_type));
    
    LLVMValueRef slice_struct_value = LLVMGetUndef(slice_struct_type);

    LLVMValueRef indices[2] = {
        LLVMConstInt(LLVMInt32TypeInContext(gen->ctx), 0, 0),
        LLVMConstInt(LLVMInt32TypeInContext(gen->ctx), 0, 0)
    };
    // 设置数据指针
    LLVMValueRef data_ptr = LLVMBuildGEP2(gen->builder, array_type, array_value_ptr, indices, 2, "arraydataptrtmp");
    slice_struct_value = LLVMBuildInsertValue(gen->builder, slice_struct_value, data_ptr, 0, "insertsliceptrtmp");
    
    // 设置count
    // TODO i64 换成 isize
    LLVMValueRef count_value = LLVMConstInt(LLVMInt64TypeInContext(gen->ctx), array_value_type->array_info.count, 0);

    slice_struct_value = LLVMBuildInsertValue(gen->builder, slice_struct_value, count_value, 1, "insertslicecounttmp");

    return slice_struct_value;
}



LLVMValueRef gen_ir_cast_expr(LLVMGenerator *gen, Ast *cast_expr, LLVMState state) {
    return gen_ir_cast(gen, cast_expr->CastExpr.expr->v_type, cast_expr->CastExpr.target_type, gen_ir_expr(gen, cast_expr->CastExpr.expr, state));
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
        return gen_ir_compare_expr(gen, expr, state, false);
    case TokenType::ExclamationEqual: // !=
        return gen_ir_compare_expr(gen, expr, state, true);
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

    case AstType_FieldAccess: {
        // parent是 package名(如fmt) 或 结构体变量名(如 person) 
        Ast *parent = expr->FieldAccess.parent;

        LLVMValueRef parent_value = gen_ir_expr(gen, parent, state);
        if(is_package_type(parent->v_type)) {
            // 包外变量/常量访问

            // 既然是包名, 那一定只是单个ident
            XP_ASSERT_DEFAULT(parent->type == AstType_Ident);
            xpString package_name = parent->Ident.name;
            SymbolInfo *symbol_info = find_symbol_until(
                ScopeType::File,
                gen->curr_ir_scope->scope,
                package_name
            );
            Package *pkg = get_package_value(symbol_info->value);
            SymbolInfo *outer_val_info = find_symbol_until(
                ScopeType::Package,
                &pkg->package_scope,
                expr->FieldAccess.field_name
            );

            return gen_llvm_val_by_value(gen, outer_val_info->value, xpOption<TypeRef>(expr->v_type));
        }


        //
        // 目前这里一定是结构体字段访问, 不用分支
        //

        Ast *parent_expr = NULL;
        if(is_struct_type(expr->FieldAccess.parent->v_type)) {
            parent_expr = expr->FieldAccess.parent;
        } else if(is_pointer_type(expr->FieldAccess.parent->v_type) && is_struct_type(get_pointed_type(expr->FieldAccess.parent->v_type))) {
            
            // 如果是指向结构体的指针，则需要先解引用
            parent_expr = ast_alloc(AstType_UnaryExpr, stage_allocator());
            parent_expr->UnaryExpr.op = TokenType::Star;
            parent_expr->UnaryExpr.operand = expr->FieldAccess.parent;
            parent_expr->v_type = get_pointed_type(expr->FieldAccess.parent->v_type);
        } else {
            XP_ASSERT_DEFAULT(0);
        }

        TypeRef struct_type = parent_expr->v_type;
        // Type struct_type_detail = get_type_detail_if_have(symbol_table(), struct_type);
        
        
        isize field_index = -1;
        for(isize i = 0; i < struct_type->struct_info.struct_fields.count; i++) {
            if(struct_type->struct_info.struct_fields[i].name == expr->FieldAccess.field_name) {
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

    case AstType_Ident: {
        SymbolInfo *info = find_symbol_until_global(gen->curr_ir_scope->scope, expr->Ident.name);
        XP_ASSERT_DEFAULT(info != NULL);


        // 非常量/变量, 
        if(!is_value_type(info->value.type)) {
            // 包外函数调用中的包名访问, 直接返回NULL, 在gen_ir_function_call_expr中根据这个名字和函数名构造完整的函数名来获取函数地址
            return NULL;
        }

        if(info->value.is_runtime_value) {
            // 变量

            LLVMValueRef alloca = look_up_local_vals(gen->curr_ir_scope, info);
            XP_ASSERT_DEFAULT(alloca != NULL);
    
            // 数组到切片的隐式转换
            if(expr->implicit_conversion_tag == ImplicitConversionTag::ArrayToSliceStruct) {
                return gen_array_value_to_slice_cast(gen, alloca, expr->v_type);
            }
    
    
            if(is_lvalue_expr) {
                return alloca;
            } else {
                return LLVMBuildLoad2(gen->builder, get_llvm_type_from_type(gen, expr->v_type), alloca, expr->Ident.name.c_str);
            }

        } else if(!info->value.is_runtime_value){
            // 常量, 直接从scope获取值生成llvmvalue
            LLVMValueRef const_val = gen_llvm_val_by_value(gen, info->value, xpOption<TypeRef>(expr->v_type));
            XP_ASSERT_DEFAULT(const_val != NULL);

            return const_val;
        } else {
            UNREACHABLE();
        }


    } break;
    case AstType_Constant: {
        // if(is_float_type(expr->v_type)) {
        //     // 浮点数常量
        //     double float_value = expr->Constant.float_value;
        //     return LLVMConstReal(get_llvm_type_from_type(gen,  expr->v_type), float_value);
        // }

        if(expr->is_null) {
            // null 常量
            // 在llvm为*i8类型的指针
            return LLVMConstNull(LLVMPointerType(LLVMVoidTypeInContext(gen->ctx), 0));
        }

        return gen_llvm_val_by_value(gen, expr->Constant.value, xpOption<TypeRef>(expr->v_type));

        // LLVMBool is_signed = cast(LLVMBool) is_signed_or_bool_type(expr->v_type);
        // return LLVMConstInt(get_llvm_type_from_type(gen, expr->v_type), expr->Constant.value, is_signed);
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

        SymbolInfo *func_symbol = NULL;
        if(expr->FunctionCallExpr.func_ident->type == AstType_FieldAccess) {
            func_symbol = get_extern_func_sym_by_full_ident_ast(expr->FunctionCallExpr.func_ident, gen);
        } else if(expr->FunctionCallExpr.func_ident->type == AstType_Ident) {
            func_symbol = find_symbol_until_global(gen->curr_ir_scope->scope, expr->FunctionCallExpr.func_ident->Ident.name);
        } else {
            UNREACHABLE();
        }
        TypeRef val_type = func_symbol->value.type;


        // TODO: ABSTRACT: 获取原始函数符号, 并生成LLVM函数(如没生成)
        SymbolInfo *ori_func_sym = get_ori_func_symbol_by_func_value(func_symbol->value);
        XP_ASSERT_DEFAULT(ori_func_sym != NULL);
        LLVMValueRef ori_func_llvm_val = look_up_local_vals(gen->curr_ir_scope, ori_func_sym);

        LLVMValueRef func = NULL;
        if(ori_func_llvm_val == NULL) {
            xpString func_full_name = gen_func_full_name(ori_func_sym->package, ori_func_sym->name, ori_func_sym->value.function_value.is_extern_c, stage_allocator());
            LLVMValueRef func_val = LLVMAddFunction(gen->module, func_full_name.c_str, get_llvm_type_from_type(gen, val_type));
            add_local_val_spec_scope(gen->curr_ir_scope, ScopeType::Package, ori_func_sym, func_val);

            func = func_val;
        } else {
            func = ori_func_llvm_val;
        }




        Array<LLVMValueRef> args = make_array_len<LLVMValueRef>(stage_allocator(), expr->FunctionCallExpr.args.count);
        for(isize i = 0; i < expr->FunctionCallExpr.args.count; i++) {
            LLVMValueRef arg_val = gen_ir_expr(gen,  expr->FunctionCallExpr.args[i], state);
            array_push_back(&args, arg_val);
        }

        LLVMTypeRef type = LLVMGlobalGetValueType(func);

        TypeRef return_type = expr->v_type;

        if(return_type == easy_type(Type_void)) {
            // 无返回值函数调用

            return LLVMBuildCall2(gen->builder, type, func, args.data, args.count, "");
        } else {
            // 有返回值函数调用

            return LLVMBuildCall2(gen->builder, type, func, args.data, args.count, "calltmp");
        }
    } break;

    case AstType_CastExpr: {
        return gen_ir_cast_expr(gen, expr, state);
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

        if(expr->implicit_conversion_tag == ImplicitConversionTag::ArrayToSliceStruct) {
            // 数组到切片的隐式转换
            return gen_array_value_to_slice_cast(gen, get_ptr_of_llvm_value(gen, array_val), expr->v_type);
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

        } else if(is_slice_struct_type(expr->IndexExpr.array_var_expr->v_type) || is_string_struct_type(expr->IndexExpr.array_var_expr->v_type)) {
            // 切片索引 或 字符串索引
            
            LLVMValueRef slice_val = LLVMBuildLoad2(gen->builder, get_llvm_type_from_type(gen, expr->IndexExpr.array_var_expr->v_type), array_or_slice_ptr, "loadslicetmp");
            LLVMValueRef data_raw = LLVMBuildExtractValue(gen->builder, slice_val, 0, "slicedataptrtmp");

            TypeRef data_ptr_type = expr->IndexExpr.array_var_expr->v_type->struct_info.struct_fields[0].type;
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

    case AstType_StringLiteralExpr: {

        LLVMValueRef str_const = LLVMBuildGlobalString(
            gen->builder, 

            // 这里是为了让字符串能由token里的带有""而无\0结尾的字符串转换而来, 使得llvm能正确识别, 
            // 不把""当成字符串内容
            xp_make_string_capacity(stage_allocator(), expr->StringLiteralExpr.str.c_str, expr->StringLiteralExpr.str.length).c_str, 
            "stringliteraltmp"
        );

        LLVMValueRef str_struct = LLVMGetUndef(get_llvm_type_from_type(gen, expr->v_type));
        
        str_struct = LLVMBuildInsertValue(gen->builder, str_struct, str_const, 0, "insertstrptrtmp");

        str_struct = LLVMBuildInsertValue(gen->builder, str_struct, LLVMConstInt(LLVMInt64TypeInContext(gen->ctx), expr->StringLiteralExpr.str.length, 0), 1, "insertstrlenntmp");

        return str_struct;
    } break;


    default:
        printf("\n-------------------------------------------\n");
        // print_ast(expr);
        UNREACHABLE();
    }


}


static LLVMValueRef compare_two_values(LLVMGenerator *gen, LLVMValueRef left, LLVMValueRef right, TypeRef type, bool is_not_equal) {
    if(is_struct_type(type)) {
        // Type type_detail = get_type_detail_if_have(symbol_table(), type);
        
        
        LLVMValueRef result = NULL;
        for(isize i = 0; i < type->struct_info.struct_fields.count; i++) {
            LLVMValueRef left_field = LLVMBuildExtractValue(gen->builder, left, i, "leftextracttmp");
            LLVMValueRef right_field = LLVMBuildExtractValue(gen->builder, right, i, "rightextracttmp");
            
            LLVMValueRef field_cmp = compare_two_values(gen, left_field, right_field, type->struct_info.struct_fields[i].type, false);
            
            if(result == NULL) {
                result = field_cmp;
            } else {
                result = LLVMBuildAnd(gen->builder, result, field_cmp, "andeqtmp");
            }
        }

        if(is_not_equal) {
            result = LLVMBuildNot(gen->builder, result, "noteqtmp");
        }

        return result;
    } else if(is_float_type(type)) {

        if(is_not_equal) {
            return LLVMBuildFCmp(gen->builder, LLVMRealONE, left, right, "noteqtmp");
        } 
        return LLVMBuildFCmp(gen->builder, LLVMRealOEQ, left, right, "eqtmp");
    } else {
        if(is_not_equal) {
            return LLVMBuildICmp(gen->builder, LLVMIntNE, left, right, "noteqtmp");
        }
        return LLVMBuildICmp(gen->builder, LLVMIntEQ, left, right, "eqtmp");
    }

}

LLVMValueRef gen_ir_compare_expr(LLVMGenerator *gen, Ast *expr, LLVMState state, bool is_not_equal) {
    XP_ASSERT_DEFAULT(expr->type == AstType_BinaryExpr);
    return compare_two_values(gen, gen_ir_expr(gen, expr->BinaryExpr.left, state), gen_ir_expr(gen, expr->BinaryExpr.right, state), expr->BinaryExpr.left->v_type, is_not_equal);
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