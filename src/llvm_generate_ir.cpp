#include "llvm_generate_ir.hpp"

// #include "common.hpp"

// #include "symbol.hpp"

// #include "ast.hpp"

// #include "analyser.hpp"

#include "context.hpp"

#include "path.hpp"

#include <print>



struct LLVMGenerator;



struct IRSymbolTable {
    xpHashMap<SymbolInfo *, LLVMValueRef> local_vals;
};


LLVMValueRef look_up_local_vals(IRSymbolTable *ir_syms, SymbolInfo *symbol_info) {
    LLVMValueRef *val = xp_hash_map_get(ir_syms->local_vals, symbol_info);
    if(val != nullptr) {
        return *val;
    }
    return nullptr;
}



LLVMValueRef *add_local_val(IRSymbolTable *ir_syms, SymbolInfo *symbol_info, LLVMValueRef val) {
    return xp_hash_map_insert(&ir_syms->local_vals, symbol_info, val);
}





struct LLVMLoopBlocks {
    LLVMBasicBlockRef cond_block;
    LLVMBasicBlockRef merge_block;
};



struct LLVMState {
    LLVMValueRef curr_function;
    LLVMBasicBlockRef curr_block;
    LLVMBasicBlockRef entry;
};










// LLVM IR 生成器, 保存生成一个Module所需的状态
// 目前一个Module就代表一个package
struct LLVMGenerator {

    void init(Package *pkg, xpAllocator allocator);
    void deinit();

    int size_of_type(TypeRef type);
    LLVMValueRef insert_alloca_before_last_inst_which_is_br(LLVMBasicBlockRef target_block, const char *var_name, LLVMTypeRef type);
    LLVMTypeRef get_llvm_type_from_type(TypeRef type);
    void gen_ir_function(CIRInstruction *func_inst);
    isize gen_ir_inst(CIRInstructionRef ref);
    void gen_ir_variable_decl(CIRInstructionRef ref, CIRInstruction* inst);
    void gen_ir_binary_expr(CIRInstructionRef inst);
    void gen_ir_unary(CIRInstructionRef inst);
    void llvm_build_br_when_no_br(LLVMBasicBlockRef from, LLVMBasicBlockRef to);
    xpString gen_ir_package(Package *pkg, LLVMIRGenerateConfig config);
    
    LLVMValueRef gen_ir_string_struct_value(xpString str);
    
    LLVMValueRef gen_ir_cast(TypeRef from_type, TypeRef to_type, LLVMValueRef value);
    LLVMValueRef gen_array_value_to_slice_cast(LLVMValueRef array_value_ptr, TypeRef array_value_type);
    
    LLVMValueRef get_ptr_of_llvm_value(LLVMValueRef value, bool allow_alloc_value_to_get_ptr = false);
    LLVMValueRef gen_llvm_val_by_value(Value& value, std::optional<TypeRef> expected_type = std::nullopt);
    
    LLVMValueRef get_llvm_value_from_inst_ref(CIRInstructionRef ref);
    void save_llvm_val_of_inst(CIRInstructionRef ref, LLVMValueRef llvm_val);

    LLVMValueRef compare_two_values(LLVMValueRef left, LLVMValueRef right, TypeRef type, bool is_not_equal);

public:
    LLVMContextRef ctx;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMTargetMachineRef target_machine;
    LLVMTargetDataRef target_data;

    
    Array<LLVMLoopBlocks> loop_stack;
    
    xpHashMap<TypeHashKey, LLVMTypeRef> struct_types;
    
    
    Package *pkg;

    
    Scope *curr_scope;
    IRSymbolTable syms;

    isize pc;

    CIRFunction curr_func_info;
    CIRInstructionRef curr_func_body_inst;

    xpHashMap<CIRInstructionRef, LLVMValueRef> inst_vals;
    xpHashMap<CIRInstUniqueKey, LLVMValueRef> func_decls;

    LLVMState curr_state;
};


void looing_llvm_diagnostic(LLVMDiagnosticInfoRef di, void *) {
    char *msg = LLVMGetDiagInfoDescription(di);
    fprintf(stderr, "[LLVM Diag] %s\n", msg);
    LLVMDisposeMessage(msg);
}

void LLVMGenerator::init(Package *pkg, xpAllocator allocator) {
    ctx = LLVMContextCreate();
    LLVMContextSetDiagnosticHandler(ctx, looing_llvm_diagnostic, nullptr);
    module = LLVMModuleCreateWithNameInContext(pkg->path.c_str, ctx);
    builder = LLVMCreateBuilderInContext(ctx);

    LLVMTargetRef target;
    char *error = nullptr;
    if(LLVMGetTargetFromTriple(LLVMGetDefaultTargetTriple(), &target, &error)) {
        printf("Error getting target: %s\n", error);
        LLVMDisposeMessage(error);
        XP_ASSERT_DEFAULT(0);
    }

    target_machine = LLVMCreateTargetMachine(
        target,
        LLVMGetDefaultTargetTriple(),
        "x86-64",
        "",
        LLVMCodeGenLevelDefault,
        LLVMRelocDefault,
        LLVMCodeModelDefault
    );
    target_data = LLVMCreateTargetDataLayout(target_machine);

    loop_stack = make_array<LLVMLoopBlocks>(allocator);
    struct_types = xp_hash_map_make<TypeHashKey, LLVMTypeRef>(allocator);
    this->pkg = pkg;
    syms = IRSymbolTable{
        .local_vals = xp_hash_map_make<SymbolInfo *, LLVMValueRef>(allocator)
    };
    inst_vals = xp_hash_map_make<CIRInstructionRef, LLVMValueRef>(allocator);
    func_decls = xp_hash_map_make<CIRInstUniqueKey, LLVMValueRef>(allocator);
}


void LLVMGenerator::deinit() {
    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(module);
    LLVMContextDispose(ctx);
    LLVMDisposeTargetData(target_data);
    LLVMDisposeTargetMachine(target_machine);

    array_free(&loop_stack);
    xp_hash_map_free(struct_types);
    xp_hash_map_free(func_decls);
}

















// void load_state(LLVMGenerator *gen, LLVMState state);
// LLVMState save_state(LLVMValueRef curr_function, LLVMBasicBlockRef curr_block, LLVMBasicBlockRef entry);


// LLVMTypeRef get_llvm_type_from_type(LLVMGenerator *gen, TypeRef type);

// LLVMState gen_ir_block(LLVMGenerator *gen, Ast *block, LLVMState state, bool need_new_scope = true);
// LLVMState gen_ir_stmt(LLVMGenerator *gen, Ast *stmt, LLVMState state);
// LLVMValueRef gen_ir_expr(LLVMGenerator *gen, Ast *expr, LLVMState state, bool is_lvalue_expr = false);
// LLVMValueRef gen_ir_compare_expr(LLVMGenerator *gen, Ast *expr, LLVMState state, bool is_not_equal = false);
// LLVMValueRef gen_array_value_to_slice_cast(LLVMGenerator *gen, LLVMValueRef array_value_ptr, TypeRef array_value_type);
// LLVMValueRef gen_ir_string_struct_value(LLVMGenerator *gen, xpString str);

void init_llvm() {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeDisassembler();
}




int LLVMGenerator::size_of_type(TypeRef type) {
    return (int)LLVMStoreSizeOfType(target_data, get_llvm_type_from_type(type));
}





LLVMValueRef LLVMGenerator::insert_alloca_before_last_inst_which_is_br(LLVMBasicBlockRef target_block, const char *var_name, LLVMTypeRef type) {
    LLVMBasicBlockRef curr_block = LLVMGetInsertBlock(builder);
    

    LLVMPositionBuilderAtEnd(builder, target_block);
    LLVMValueRef last_instr = LLVMGetLastInstruction(target_block);
    if (last_instr && LLVMIsABranchInst(last_instr)) {
        LLVMPositionBuilderBefore(builder, last_instr);
    }

    fprintf(stderr, "[DEBUG] BuildAlloca: var=%s, type_kind=%d\n", var_name, (int)LLVMGetTypeKind(type));
    XP_ASSERT_DEFAULT(type != nullptr);
    LLVMValueRef alloca = LLVMBuildAlloca(builder, type, var_name);
    fprintf(stderr, "[DEBUG] BuildAlloca OK\n");

    LLVMPositionBuilderAtEnd(builder, curr_block);

    return alloca;
}

// bool is_curr_basic_block_has_terminator(LLVMGenerator *gen) {
//     LLVMBasicBlockRef curr_block = LLVMGetInsertBlock(builder);
//     return LLVMGetBasicBlockTerminator(curr_block) != nullptr;
// }

void LLVMGenerator::llvm_build_br_when_no_br(LLVMBasicBlockRef from, LLVMBasicBlockRef to) {
    if(!LLVMGetBasicBlockTerminator(from)) {
        LLVMBuildBr(builder, to);
    }
}



// SymbolInfo *get_extern_func_sym_by_full_ident_ast(Ast *field_access, LLVMGenerator *gen) {
//     XP_ASSERT_DEFAULT(field_access->type == AstType_FieldAccess);

//     xpString parent_name = field_access->FieldAccess.parent->Ident.name;
//     xpString field_name = field_access->FieldAccess.field_name;

//     SymbolInfo *import_symbol = find_symbol_until_spec_v(
//         ScopeType::File,
//         gen->curr_ir_scope->scope,
//         parent_name,
//         Type_package
//     );

//     Package *imported_package = import_symbol->value.get_package_value();

//     SymbolInfo *func_symbol = find_symbol_until_spec_v(
//         ScopeType::Package,
//         &imported_package->package_scope,
//         field_name,
//         Type_function
//     );
//     XP_ASSERT_DEFAULT(func_symbol != nullptr);

//     return func_symbol;
// }


LLVMValueRef LLVMGenerator::get_ptr_of_llvm_value(LLVMValueRef value, bool allow_alloc_value_to_get_ptr) {
    if(LLVMIsALoadInst(value)) {
        // 如果是load指令, 直接返回被加载的地址

        return LLVMGetOperand(value, 0);
    } else if(LLVMIsAAllocaInst(value)) {
        // 如果是alloca指令, 直接返回
        return value;
    } else if(LLVMIsAGlobalVariable(value)) {
        // 如果是全局变量, 直接返回
        return value;
    } else if(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind) {
        // 如果已经是GEP等运算产生的指针值, 直接返回
        return value;
    }

    // 否则, 需要先把值存到内存中, 再返回地址
    if(allow_alloc_value_to_get_ptr) {
        LLVMTypeRef value_type = LLVMTypeOf(value);
        LLVMValueRef temp_alloca = insert_alloca_before_last_inst_which_is_br(LLVMGetInsertBlock(builder), "temp", value_type);
        LLVMBuildStore(builder, value, temp_alloca);
        return temp_alloca;
    } else {
        XP_ASSERT_DEFAULT(0 && "Compiler Internal Error: Cannot get pointer of value, and not allowed to alloc value to get pointer");
    }

    return nullptr;
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


LLVMTypeRef LLVMGenerator::get_llvm_type_from_type(TypeRef type) {
    switch(type->kind) {
        case Type_void:
            return LLVMVoidTypeInContext(ctx);
        case Type_i8:
        case Type_u8:
            return LLVMInt8TypeInContext(ctx);
        case Type_i32:
        case Type_u32:
            return LLVMInt32TypeInContext(ctx);
        case Type_i64:
        case Type_u64:
            return LLVMInt64TypeInContext(ctx);
        case Type_bool:
            return LLVMInt1TypeInContext(ctx);
        case Type_f32:
            return LLVMFloatTypeInContext(ctx);
        case Type_f64:
            return LLVMDoubleTypeInContext(ctx);
        case Type_pointer: {
            LLVMTypeRef pointed_type = get_llvm_type_from_type(type->pointed_type);
            return LLVMPointerType(pointed_type, 0);
        }
        case Type_struct: {

            TypeHashKey& key = type->struct_info.hash_key;

            // 如果已经存在该结构体类型, 直接返回
            LLVMTypeRef *existing_struct_type = xp_hash_map_get(struct_types, key);
            if(existing_struct_type != nullptr) {
                return *existing_struct_type;
            }

            // TODO: 规范
            auto name_maybe = type->struct_info.hash_key.name;
            xpString name = name_maybe.has_value() ? type->struct_info.hash_key.name.value() : xp_string_c("anonymous_struct");

            LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx, xp_string_to_c_style(name, stage_allocator()).c_str);
            LLVMTypeRef *struct_type = xp_hash_map_insert(&struct_types, key, struct_ty);
            
            
            Array<LLVMTypeRef> field_types = make_array_len<LLVMTypeRef>(stage_allocator(), type->struct_info.struct_fields.count);
            
            for(isize i = 0; i < type->struct_info.struct_fields.count; i++) {
                StructField field = type->struct_info.struct_fields[i];
                LLVMTypeRef field_llvm_type = get_llvm_type_from_type(field.type);
                array_push_back(&field_types, field_llvm_type);
            }

            LLVMStructSetBody(*struct_type, field_types.data, field_types.count, 0);

            return *struct_type;
        }

        case Type_enum: {
            // 直接获取原始类型的LLVM类型, 因为枚举类型在LLVM IR中就是一个整数类型

            TypeRef actual_elem_type = type->enum_info.element_type;
            return get_llvm_type_from_type(actual_elem_type);
        }

        case Type_array: {
            LLVMTypeRef element_type = get_llvm_type_from_type(type->array_info.element_type);
            return LLVMArrayType(element_type, (unsigned)type->array_info.count);
        }

        case Type_function: {
            Array<LLVMTypeRef> params = make_array_len<LLVMTypeRef>(stage_allocator(), type->function_info.param_types.count);


            isize fix_param_count = type->function_info.param_types.count - (is_var_arg_function(type) ? 1 : 0);
            for(isize i = 0; i < fix_param_count; i++) {
                array_push_back(&params, get_llvm_type_from_type(type->function_info.param_types[i]));
            }
            
            LLVMTypeRef func_type = LLVMFunctionType(get_llvm_type_from_type(type->function_info.return_type), params.data, fix_param_count, is_var_arg_function(type) ? 1 : 0);
            return func_type;
        } break;

        // // *NOTE: 这两种类型只会在常量出现, 且一定会被转为其它类型
        // case Type_untyped_int:
        //     return LLVMInt128TypeInContext(ctx);
        // case Type_untyped_float:
        //     return LLVMDoubleTypeInContext(ctx);


        default:
            UNREACHABLE();
    }

    return nullptr;
}


Array<xpString> gen_ir_all_packages(Array<Package>* all_packages, LLVMIRGenerateConfig config) {
    defer(xp_arena_allocator_clear(stage_allocator()));

    Array<xpString> obj_paths = make_array<xpString>(permanent_allocator());
    for(Package& pkg: *all_packages) {
        LLVMGenerator gen;
        gen.init(&pkg, stage_allocator());
        defer(gen.deinit());
        array_push_back(&obj_paths, gen.gen_ir_package(&pkg, config));
    }

    return obj_paths;
}


// LLVMValueRef get_llvm_func_val_in_scope_by_ori_name(LLVMGenerator *gen, xpString ori_func_name) {
//     SymbolInfo *symbol_info = find_symbol_until_spec_v(
//         ScopeType::Package,
//         gen->curr_ir_scope->scope,
//         ori_func_name,
//         Type_function
//     );
//     XP_ASSERT_DEFAULT(symbol_info != nullptr);

//     LLVMValueRef func_val = look_up_local_vals(gen->curr_ir_scope, symbol_info);
//     XP_ASSERT_DEFAULT(func_val != nullptr);

//     return func_val;
// }


xpString LLVMGenerator::gen_ir_package(Package *pkg, LLVMIRGenerateConfig config) {
    CIRPackage& cir_pkg = pkg->cir_package;
    fprintf(stderr, "[DEBUG] gen_ir_package: %lld functions\n", (long long)cir_pkg.all_func_inst_sym_scopes.count);

    // 声明当前包中的所有函数
    for(auto& [func_ref, symbol_info, _] : cir_pkg.all_func_inst_sym_scopes) {
        CIRInstruction *func_inst = cir_pkg.inst(func_ref);

        xpString func_name = func_inst->func_decl.name;


        // 有定义的函数, 如
        // main :: () -> i32 { 
        //     return 0; 
        // }
        // 

        xpString full_func_name = gen_func_full_name(symbol_info->package, func_name, func_inst->func_decl.is_extern_c, stage_allocator());

        LLVMValueRef function = LLVMAddFunction(
            module, 
            xp_string_to_c_style(full_func_name, stage_allocator()).c_str,
            get_llvm_type_from_type(cir_pkg.inst(func_ref)->result.val.type)
        );

        add_local_val(&syms, symbol_info, function);
    }

        for(auto& [func_ref, symbol_info, scope] : cir_pkg.all_func_inst_sym_scopes) {
        CIRInstruction *func_inst = cir_pkg.inst(func_ref);
        curr_scope = scope;

        gen_ir_function(func_inst);
    }


    // 输出 .ll 文件
    char *error = nullptr;
    std::string file_name = to_path(pkg->path).generic_string();
    std::ranges::replace(file_name, '/', '_');
    std::ranges::replace(file_name, ':', '_');

    std::filesystem::path &output_path = context()->output_path;

    std::filesystem::path obj_file_path = output_path / (file_name + ".o");
    std::filesystem::path ll_file_path = output_path / (file_name + ".ll");
    

    if(LLVMPrintModuleToFile(module, ll_file_path.generic_string().c_str(), &error)) {
        fprintf(stderr, "Error writing .ll file: %s\n", error);
        LLVMDisposeMessage(error);
    }
    
    if(LLVMVerifyModule(module, LLVMReturnStatusAction, &error)) {
        fprintf(stderr, "Module verification failed: %s\n", error);
        LLVMDisposeMessage(error);
        XP_ASSERT_DEFAULT(0);
    }

    // 优化
    LLVMPassBuilderOptionsRef options = LLVMCreatePassBuilderOptions();
    defer(LLVMDisposePassBuilderOptions(options));

    // 选择优化级别
    char const *passes = nullptr;
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
    LLVMRunPasses(module, passes, target_machine, options);

    // 生成.o文件
    if(LLVMTargetMachineEmitToFile(
        target_machine,
        module,
        obj_file_path.generic_string().c_str(),
        LLVMObjectFile,
        &error
    )) {
        fprintf(stderr, "Error writing object file: %s\n", error);
        LLVMDisposeMessage(error);
    }
    
    std::string obj_file_path_str = obj_file_path.generic_string();
    xpString obj_file_path_str_permanent = xp_make_string_count(permanent_allocator(), obj_file_path_str.c_str(), obj_file_path_str.size());

    return obj_file_path_str_permanent;
}


LLVMValueRef LLVMGenerator::get_llvm_value_from_inst_ref(CIRInstructionRef ref) {
    LLVMValueRef *val = xp_hash_map_get(inst_vals, ref);
    if (val != nullptr) {
        return *val;
    }

    auto result = pkg->cir_package.result_of(ref);
    if (result.type == CIRResultType::WholeValue) {
        Value &v = result.val;
        LLVMValueRef llvm_val = gen_llvm_val_by_value(v);
        xp_hash_map_insert(&inst_vals, ref, llvm_val);
        return llvm_val;
    }

    UNREACHABLE();
    return nullptr;
}

void LLVMGenerator::save_llvm_val_of_inst(CIRInstructionRef ref, LLVMValueRef llvm_val) {
    xp_hash_map_insert(&inst_vals, ref, llvm_val);
}



LLVMValueRef LLVMGenerator::gen_ir_cast(TypeRef from_type, TypeRef to_type, LLVMValueRef value) {

    // TODO: 统一数组→切片隐式转换的调用点
    if(is_array_type(from_type) && is_slice_struct_type(to_type)) {
        return gen_array_value_to_slice_cast(get_ptr_of_llvm_value(value), from_type);
    }

    // 相等, 直接返回
    if(from_type == to_type) {
        return value;
    }


    if(is_pointer_type(from_type) && is_pointer_type(to_type)) {
        // 指针类型之间的转换, 不用转化, 直接返回
        return value;
    }

    if(is_enum_type(from_type) && is_integer_type(to_type)) {
        // 枚举到整数的转换, 直接返回, 因为枚举在LLVM IR中就是整数
        return value;
    }

    // 下面是整数,浮点数之间的转换
    bool short_to_long = size_of_type(from_type) < size_of_type(to_type);
    bool long_to_short = size_of_type(from_type) > size_of_type(to_type);
    bool same_size = size_of_type(from_type) == size_of_type(to_type);
    

    if(is_float_or_untyped_type(from_type) && is_float_or_untyped_type(to_type)) {
        // 浮点数之间的转换
        if(short_to_long) {
            // 扩展
            return LLVMBuildFPExt(builder, value, get_llvm_type_from_type(to_type), "fpexttmp");
        } else if(long_to_short) {
            // 截断
            return LLVMBuildFPTrunc(builder, value, get_llvm_type_from_type(to_type), "fptrunctmp");
        } else if(same_size){
            // 相等，直接返回
            return value;
        }
    }

    if(is_integer_or_untyped_type(from_type) && is_float_or_untyped_type(to_type)) {
        // 整数到浮点数的转换
        if(is_signed_type(from_type)) {
            // 有符号整数到浮点数
            return LLVMBuildSIToFP(builder, value, get_llvm_type_from_type(to_type), "sitofptmp");
        } else {
            // 无符号整数到浮点数
            return LLVMBuildUIToFP(builder, value, get_llvm_type_from_type(to_type), "uitofptmp");
        }
    }

    if(is_float_or_untyped_type(from_type) && is_integer_or_untyped_type(to_type)) {
        // 浮点数到整数的转换
        if(is_signed_type(to_type)) {
            // 浮点数到有符号整数
            return LLVMBuildFPToSI(builder, value, get_llvm_type_from_type(to_type), "fptositmp");
        } else {
            // 浮点数到无符号整数
            return LLVMBuildFPToUI(builder, value, get_llvm_type_from_type(to_type), "fptouitmp");
        }
    }


    // 整数
    if(is_integer_or_untyped_type(from_type) && is_integer_or_untyped_type(to_type)) {
        if(short_to_long) {
            // 扩展
            if(is_signed_type(from_type)) {
                // 有符号扩展
                return LLVMBuildSExt(builder, value, get_llvm_type_from_type(to_type), "sexttmp");
            } else {
                // 无符号扩展
                return LLVMBuildZExt(builder, value, get_llvm_type_from_type(to_type), "zexttmp");
            }
            
        } else if(long_to_short) {
            // 截断
            return LLVMBuildTrunc(builder, value, get_llvm_type_from_type(to_type), "trunctmp");
        } else if(same_size){
            // 相等，直接返回
            return value;
        }
    }

    UNREACHABLE();

    return nullptr;
}




LLVMValueRef LLVMGenerator::gen_ir_string_struct_value(xpString str) {
    LLVMValueRef str_const = LLVMBuildGlobalString(
        builder, 

        // 这里是为了让字符串能由token里的带有""而无\0结尾的字符串转换而来, 使得llvm能正确识别, 
        // 不把""当成字符串内容
        xp_string_to_c_style(str, stage_allocator()).c_str,
        "stringliteraltmp"
    );

    LLVMValueRef str_struct = LLVMGetUndef(get_llvm_type_from_type(string_type_as_struct()));
    
    // TODO: 现在是硬编码
    str_struct = LLVMBuildInsertValue(builder, str_struct, str_const, 0, "insertstrptrtmp");

    str_struct = LLVMBuildInsertValue(builder, str_struct, LLVMConstInt(LLVMInt64TypeInContext(ctx), str.length, 0), 1, "insertstrlenntmp");

    return str_struct;
}


LLVMValueRef LLVMGenerator::gen_llvm_val_by_value(Value& value, std::optional<TypeRef> expected_type) {
     // TODO 其他类型的常量

    LLVMValueRef llvm_val = nullptr;
    switch(value.type->kind) {
        case Type_i8:
        case Type_u8:
        case Type_i32:
        case Type_u32:
        case Type_i64:
        case Type_u64:
            llvm_val = LLVMConstInt(get_llvm_type_from_type(value.type), cast(unsigned long long)value.integer_val(), is_signed_or_bool_type(value.type));
            break;
        case Type_bool:
            llvm_val = LLVMConstInt(get_llvm_type_from_type(value.type), cast(unsigned long long)value.bool_val(), is_signed_or_bool_type(value.type));
            break;
        case Type_f32:
        case Type_f64:
            llvm_val = LLVMConstReal(get_llvm_type_from_type(value.type), cast(double)value.float_val());
            break;

        // case Type_untyped_int:
        //     llvm_val = LLVMConstInt(LLVMInt128TypeInContext(ctx), cast(unsigned long long)value.integer_val(), false);
        //     break;

        // case Type_untyped_float:
        //     llvm_val = LLVMConstReal(LLVMDoubleTypeInContext(ctx), cast(double)value.float_val());
        //     break;

        case Type_array: {
            Array<LLVMValueRef> element_values = make_array<LLVMValueRef>(stage_allocator());
            
            Array<Value> elem_vals = value.array_element_values();
            for(isize i = 0; i < elem_vals.count; i++) {
                auto& val = elem_vals[i];
                array_push_back(&element_values, gen_llvm_val_by_value(val));
            }

            llvm_val = LLVMConstArray2(get_llvm_type_from_type(value.type), element_values.data, element_values.count);
        } break;

        case Type_struct: {
            if(is_string_struct_type(value.type)) {
                llvm_val = gen_ir_string_struct_value(value.string_val());
            } else if(is_struct_type(value.type)) {
                Array<LLVMValueRef> field_values = make_array<LLVMValueRef>(stage_allocator());

                Array<Value> field_vals = value.struct_fields_val();
                for(isize i = 0; i < field_vals.count; i++) {

                    LLVMValueRef field_llvm_val = gen_llvm_val_by_value(field_vals[i]);
                    array_push_back(&field_values, field_llvm_val);
                }

                llvm_val = LLVMConstNamedStruct(get_llvm_type_from_type(value.type), field_values.data, cast(unsigned)field_values.count);
            } else {
                UNREACHABLE();
            }

        } break;

        default: {
            // TODO 其他类型的常量
            UNREACHABLE();
        } break;
    }

    if(expected_type.has_value()) {
        return gen_ir_cast(value.type, expected_type.value(), llvm_val);
    } else {
        return llvm_val;
    }

}

// bool is_func_defined_in_file(Ast *func_ast) {
//     return func_ast->type == AstType_ConstDecl && func_ast->ConstDecl.value_ast->type == AstType_FunctionDeclValue;
// }

// SymbolInfo *get_ori_func_symbol_by_func_value(Value& val) {
//     XP_ASSERT_DEFAULT(is_function_type(val.type));

//     Ast *ori_func_const_decl = val.val_ast;
//     XP_ASSERT_DEFAULT(ori_func_const_decl != nullptr);

//     SymbolInfo *ori_func_sym = ori_func_const_decl->ast_symbol;
//     XP_ASSERT_DEFAULT(ori_func_sym != nullptr);

//     return ori_func_sym;
// }

// void gen_ir_const_decl(Ast *const_decl, LLVMGenerator *gen) {
//     XP_ASSERT_DEFAULT(const_decl->type == AstType_ConstDecl);
//     Ast *value_ast = const_decl->ConstDecl.value_ast;

//     SymbolInfo *symbol_info = find_symbol_until(
//         ScopeType::Package,
//         gen->curr_ir_scope->scope,
//         const_decl->ConstDecl.name
//     );
//     TypeRef val_type = symbol_info->value.type;


//     if(is_function_type(val_type)) {
//         // TODO: ABSTRACT: 获取原始函数符号, 并生成LLVM函数(如没生成)
//         SymbolInfo *ori_func_sym = get_ori_func_symbol_by_func_value(symbol_info->value);

//         LLVMValueRef ori_func_llvm_val = look_up_local_vals(gen->curr_ir_scope, ori_func_sym);

//         if(ori_func_llvm_val == nullptr) {
//             xpString func_full_name = gen_func_full_name(ori_func_sym->package, ori_func_sym->name, ori_func_sym->value.get_function_is_extern_c(), stage_allocator());
//             LLVMValueRef func_val = LLVMAddFunction(module, func_full_name.c_str, get_llvm_type_from_type(val_type));
//             add_local_val_spec_scope(gen->curr_ir_scope, ScopeType::Package, ori_func_sym, func_val);
//         }

//     }

// }




// void gen_ir_astfile(AstFile f, LLVMGenerator *gen) {
    

//     for(isize i = 0; i < f.top_levels.count; i++) {
//         switch(f.top_levels[i]->type) {

//             case AstType_ConstDecl:
//                 gen_ir_const_decl(f.top_levels[i], gen);
//                 break;
            
//             case AstType_VariableDecl:
//                 XP_TODO();
//                 break;

//             default:
//                 UNREACHABLE();
//         }
//     }


//     for(isize i = 0; i < f.top_levels.count; i++) {
//         // 有block的函数

//         if(is_func_defined_in_file(f.top_levels[i])) {
//             gen_ir_function(f.top_levels[i]);
//         }
//     }

// }





void LLVMGenerator::gen_ir_function(CIRInstruction *func_inst) {
    XP_ASSERT_DEFAULT(func_inst->op == CIROperator::FunctionDecl);

    // TODO: TEST extern_C
    if(func_inst->func_decl.is_extern_c) {
        return;
    }

    SymbolInfo *sym = find_symbol_until_global(curr_scope, func_inst->func_decl.name);
    LLVMValueRef func = look_up_local_vals(&syms, sym);
    fprintf(stderr, "[DEBUG] gen_ir_function: '%s', func=%p\n", func_inst->func_decl.name.c_str, (void*)func);

    // 1. 创建入口基本块并设置插入点
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx, func, "entry");
    LLVMBasicBlockRef after_entry = LLVMAppendBasicBlockInContext(ctx, func, "after.entry");
    
    
    LLVMPositionBuilderAtEnd(builder, entry);


    LLVMBuildBr(builder, after_entry);
    LLVMPositionBuilderAtEnd(builder, after_entry);

    curr_state = {func, after_entry, entry};

    curr_func_info = func_inst->func_decl;

    CIRInstructionRef func_body_ref = func_inst->func_decl.body_inst;
    curr_func_body_inst = func_body_ref;


    gen_ir_inst(func_body_ref);
    


//     // TypeRef func_type = find_symbol(symbol_table(), function->Function.name)->type;
//     XP_ASSERT_DEFAULT(function->ast_symbol != nullptr);
//     TypeRef func_type = function->ast_symbol->value.type;
    

    // if(func_type->function_info.return_type == easy_type(Type_void)) {
    //     // void 函数自动添加返回

    //     LLVMBuildRetVoid(builder);
    // }
    
    if(!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(builder))) {
        // 如果最后一个基本块没有终止指令, 则添加unreachable
        LLVMBuildUnreachable(builder);
    }
    
}


isize LLVMGenerator::gen_ir_inst(CIRInstructionRef ref) {
    CIRInstruction *inst = pkg->cir_package.inst(ref);
    fprintf(stderr, "[TRACE] gen_ir_inst ref=%%%lld op=%s\n", (long long)ref, string(inst->op));
    
    switch(inst->op) {

        // 这些都是编译期指令, 不生成IR
        case CIROperator::ConstDecl:
        case CIROperator::FunctionDecl:
        case CIROperator::UnionDecl:
        case CIROperator::PointerType:
        case CIROperator::ArrayType:
        case CIROperator::SliceType:
        case CIROperator::TypeAscribe:
        case CIROperator::DetermineType:
        case CIROperator::GetOrInitStruct:
        case CIROperator::StructField:
        case CIROperator::FinishStruct:
        case CIROperator::EnumDeclInit:
        case CIROperator::SizeOf:
        case CIROperator::FieldTypeOfStruct:
        case CIROperator::TypeOfInstResult: {

        } break;

        case CIROperator::Block: {
            if(inst->block_info.is_comptime) {
                return 1 + inst->block_info.body_len;
            }
            CIRInstructionRef body_start = ref + 1;
            CIRInstructionRef body_end = body_start + inst->block_info.body_len;
            CIRInstructionRef body_pc = body_start;
            while(body_pc < body_end) {
                body_pc += gen_ir_inst(body_pc);
            }
            return 1 + inst->block_info.body_len;
        } break;
        
        
        
        
        // 这些与状态相关的指令, 需要更新状态, 但不生成IR
        case CIROperator::EnterScope:
            curr_scope = inst->scope_info.scope;
            break;
        case CIROperator::ExitScope:
            curr_scope = inst->scope_info.scope;
            break;
        
        
        // 不需要生成IR
        case CIROperator::ConstantValue:
            break;
        
        // 这些是运行时指令, 可能需要生成IR
        case CIROperator::StructInit: {
            
            if(pkg->cir_package.result_of(ref).type == CIRResultType::WholeValue) {
                return 1;
            }

            auto &info = inst->struct_init_info;
            TypeRef struct_type = extract_type_from_val_as_type(pkg->cir_package.result_of(info.struct_type_inst).val);
            LLVMTypeRef llvm_struct_type = get_llvm_type_from_type(struct_type);
            LLVMValueRef struct_val = LLVMGetUndef(llvm_struct_type);

            for (isize i = 0; i < info.field_init_insts.count; i++) {
                LLVMValueRef field_val = get_llvm_value_from_inst_ref(info.field_init_insts[i]);
                struct_val = LLVMBuildInsertValue(builder, struct_val, field_val, (unsigned)i, "insertfieldtmp");
            }
            save_llvm_val_of_inst(ref, struct_val);
        } break;
        case CIROperator::ArrayInit: {
            // 老代码:
            //     LLVMValueRef array_val = LLVMGetUndef(array_type);
            //     for(...) array_val = LLVMBuildInsertValue(builder, array_val, element_value, i, "arrayinsertvaltmp");

                        
            if(pkg->cir_package.result_of(ref).type == CIRResultType::WholeValue) {
                return 1;
            }


            auto &info = inst->array_init_info;
            TypeRef array_type = pkg->cir_package.result_of(ref).val.type;
            LLVMTypeRef llvm_array_type = get_llvm_type_from_type(array_type);
            LLVMValueRef array_val = LLVMGetUndef(llvm_array_type);

            for (isize i = 0; i < info.element_insts.count; i++) {
                LLVMValueRef elem_val = get_llvm_value_from_inst_ref(info.element_insts[i]);
                array_val = LLVMBuildInsertValue(builder, array_val, elem_val, (unsigned)i, "insertelemtmp");
            }
            save_llvm_val_of_inst(ref, array_val);
        } break;

        case CIROperator::VariableDecl: {
            gen_ir_variable_decl(ref, inst);
        } break;
            
        case CIROperator::Binary: {
            gen_ir_binary_expr(ref);
        } break;

        case CIROperator::Unary: {
            gen_ir_unary(ref);
        } break;

        case CIROperator::Deref: {
            auto info = inst->deref_info;
            LLVMValueRef operand = get_llvm_value_from_inst_ref(info.ptr_val_inst);
            TypeRef operand_type = pkg->cir_package.result_of(info.ptr_val_inst).val.type;
    
            LLVMValueRef result = LLVMBuildLoad2(builder, get_llvm_type_from_type(operand_type), operand, "loadtmp");
            save_llvm_val_of_inst(ref, result);

        } break;

        case CIROperator::AddrOf: {
            auto info = inst->addr_of_info;
            LLVMValueRef operand = get_llvm_value_from_inst_ref(info.lval_inst);
    
            LLVMValueRef result = get_ptr_of_llvm_value(operand);
            save_llvm_val_of_inst(ref, result);
        } break;

        case CIROperator::FieldAccess: {
            // 老代码:
            //     LLVMValueRef struct_val = gen_ir_expr(gen, parent_expr, state);
            //     LLVMValueRef field_val = LLVMBuildExtractValue(builder, struct_val, field_index, "fieldextractedtmp");
            //     return field_val;
            //
            // 包成员访问和枚举成员访问由 Interpreter WholeValue 处理

            if(pkg->cir_package.result_of(ref).type == CIRResultType::WholeValue) {
                // WholeValue 的成员访问由 Interpreter 处理, 不生成IR
                return 1;
            }

            auto &info = inst->field_access_info;



            LLVMValueRef parent_val = get_llvm_value_from_inst_ref(info.parent_inst);
            TypeRef parent_type = pkg->cir_package.result_of(info.parent_inst).val.type;

            TypeRef struct_type = nullptr;
            if (is_struct_type(parent_type)) {
                struct_type = parent_type;
            } else if (is_pointer_type(parent_type) && is_struct_type(parent_type->pointed_type)) {
                struct_type = parent_type->pointed_type;
                parent_val = LLVMBuildLoad2(builder, get_llvm_type_from_type(struct_type), parent_val, "loadtmp");
            }

            if (struct_type) {
                isize field_idx = -1;
                for (isize i = 0; i < struct_type->struct_info.struct_fields.count; i++) {
                    if (xp_string_equal(struct_type->struct_info.struct_fields[i].name, info.field_name)) {
                        field_idx = i;
                        break;
                    }
                }
                XP_ASSERT_DEFAULT(field_idx != -1);

                LLVMValueRef field_val = LLVMBuildExtractValue(builder, parent_val, (unsigned)field_idx, "fieldtmp");
                save_llvm_val_of_inst(ref, field_val);
            }
        } break;

        case CIROperator::FieldPtr: {
            // 老代码:
            //     LLVMValueRef struct_ptr = gen_ir_expr(gen, parent_expr, state, true);
            //     LLVMValueRef field_ptr = LLVMBuildStructGEP2(builder, get_llvm_type_from_type(struct_type), struct_ptr, field_index, "fieldptrtmp");
            //     return field_ptr;

            auto &info = inst->field_access_info;
            LLVMValueRef struct_ptr = get_llvm_value_from_inst_ref(info.parent_inst);
            TypeRef parent_type = pkg->cir_package.result_of(info.parent_inst).val.type;

            TypeRef struct_type = is_struct_type(parent_type) ? parent_type :
                (is_pointer_type(parent_type) && is_struct_type(parent_type->pointed_type)) ? parent_type->pointed_type : nullptr;
            XP_ASSERT_DEFAULT(struct_type != nullptr);

            isize field_idx = -1;
            for (isize i = 0; i < struct_type->struct_info.struct_fields.count; i++) {
                if (xp_string_equal(struct_type->struct_info.struct_fields[i].name, info.field_name)) {
                    field_idx = i;
                    break;
                }
            }
            XP_ASSERT_DEFAULT(field_idx != -1);

            LLVMValueRef field_ptr = LLVMBuildStructGEP2(builder, get_llvm_type_from_type(struct_type), struct_ptr, (unsigned)field_idx, "fieldptrtmp");
            save_llvm_val_of_inst(ref, field_ptr);
        } break;

        case CIROperator::Index: {
            // 老代码:
            //     LLVMValueRef elem_ptr = ...GEP...;
            //     return LLVMBuildLoad2(builder, get_llvm_type_from_type(expr->v_type), elem_ptr, "loadelementtmp");

            if(pkg->cir_package.result_of(ref).type == CIRResultType::WholeValue) {
                // WholeValue 的索引访问由 Interpreter 处理, 不生成IR
                return 1;
            }

            auto &info = inst->index_info;
            LLVMValueRef array_val = get_llvm_value_from_inst_ref(info.array_inst);
            LLVMValueRef index_val = get_llvm_value_from_inst_ref(info.index_inst);
            TypeRef array_type = pkg->cir_package.result_of(info.array_inst).val.type;

            LLVMValueRef indices[2] = { nullptr };
            LLVMValueRef elem_ptr = nullptr;
            bool array_is_ptr = LLVMGetTypeKind(LLVMTypeOf(array_val)) == LLVMPointerTypeKind;

            if(is_array_type(array_type)) {
                LLVMValueRef array_ptr = array_is_ptr ? array_val : get_ptr_of_llvm_value(array_val, true);
                indices[0] = LLVMConstInt(LLVMInt32TypeInContext(ctx), 0, 0);
                indices[1] = index_val;
                elem_ptr = LLVMBuildGEP2(builder, get_llvm_type_from_type(array_type), array_ptr, indices, 2, "arrayelemptrtmp");
            } else if(is_slice_struct_type(array_type) || is_string_struct_type(array_type)) {
                LLVMValueRef slice_val = array_is_ptr ? LLVMBuildLoad2(builder, get_llvm_type_from_type(array_type), array_val, "loadslicetmp") : array_val;
                LLVMValueRef data_raw = LLVMBuildExtractValue(builder, slice_val, 0, "slicedataptrtmp");
                TypeRef data_ptr_type = array_type->struct_info.struct_fields[0].type;
                LLVMTypeRef data_ptr_llvm_type = get_llvm_type_from_type(data_ptr_type);
                LLVMValueRef data_typed_ptr = LLVMBuildBitCast(builder, data_raw, data_ptr_llvm_type, "slicedatatypedptrtmp");
                indices[0] = index_val;
                elem_ptr = LLVMBuildGEP2(builder, get_llvm_type_from_type(data_ptr_type->pointed_type), data_typed_ptr, indices, 1, "sliceelemptrtmp");
            } else {
                UNREACHABLE();
            }

            TypeRef val_type = pkg->cir_package.result_of(ref).val.type;
            LLVMValueRef loaded = LLVMBuildLoad2(builder, get_llvm_type_from_type(val_type), elem_ptr, "loadindextmp");
            save_llvm_val_of_inst(ref, loaded);
        } break;

        case CIROperator::IndexPtr: {
            // 老代码:
            //     LLVMValueRef elem_ptr = ...GEP...;
            //     return elem_ptr;  // is_lvalue_expr 分支

            auto &info = inst->index_info;
            LLVMValueRef array_val = get_llvm_value_from_inst_ref(info.array_inst);
            LLVMValueRef index_val = get_llvm_value_from_inst_ref(info.index_inst);
            TypeRef array_type = pkg->cir_package.result_of(info.array_inst).val.type;

            LLVMValueRef indices[2] = { nullptr };
            LLVMValueRef elem_ptr = nullptr;
            bool array_is_ptr = LLVMGetTypeKind(LLVMTypeOf(array_val)) == LLVMPointerTypeKind;

            if(is_array_type(array_type)) {
                LLVMValueRef array_ptr = array_is_ptr ? array_val : get_ptr_of_llvm_value(array_val, true);
                indices[0] = LLVMConstInt(LLVMInt32TypeInContext(ctx), 0, 0);
                indices[1] = index_val;
                elem_ptr = LLVMBuildGEP2(builder, get_llvm_type_from_type(array_type), array_ptr, indices, 2, "arrayelemptrtmp");
            } else if(is_slice_struct_type(array_type) || is_string_struct_type(array_type)) {
                LLVMValueRef slice_val = array_is_ptr ? LLVMBuildLoad2(builder, get_llvm_type_from_type(array_type), array_val, "loadslicetmp") : array_val;
                LLVMValueRef data_raw = LLVMBuildExtractValue(builder, slice_val, 0, "slicedataptrtmp");
                TypeRef data_ptr_type = array_type->struct_info.struct_fields[0].type;
                LLVMTypeRef data_ptr_llvm_type = get_llvm_type_from_type(data_ptr_type);
                LLVMValueRef data_typed_ptr = LLVMBuildBitCast(builder, data_raw, data_ptr_llvm_type, "slicedatatypedptrtmp");
                indices[0] = index_val;
                elem_ptr = LLVMBuildGEP2(builder, get_llvm_type_from_type(data_ptr_type->pointed_type), data_typed_ptr, indices, 1, "sliceelemptrtmp");
            } else {
                UNREACHABLE();
            }

            save_llvm_val_of_inst(ref, elem_ptr);
        } break;

        case CIROperator::Call: {
            auto &info = inst->call_info;

            auto& called_val = pkg->cir_package.inst(info.called_thing)->result.val;
            XP_ASSERT_DEFAULT(is_function_type(called_val.type));

            // 取得 LLVM 函数：用 CIRInstUniqueKey 做缓存，LLVMGetNamedFunction 查重避免重复声明
            const auto& fk = called_val.func_val().func_key;
            LLVMValueRef *cached = xp_hash_map_get(func_decls, fk);
            LLVMValueRef llvm_func;
            if(cached != nullptr) {
                llvm_func = *cached;
            } else {
                auto& func_info = fk.cir_package->inst(fk.defining_inst)->func_decl;
                xpString func_full_name = gen_func_full_name(fk.package, func_info.name, func_info.is_extern_c, stage_allocator());
                const char *c_name = xp_string_to_c_style(func_full_name, stage_allocator()).c_str;
                llvm_func = LLVMGetNamedFunction(module, c_name);
                if (!llvm_func) {
                    llvm_func = LLVMAddFunction(module, c_name, get_llvm_type_from_type(called_val.type));
                }
                xp_hash_map_insert(&func_decls, fk, llvm_func);
            }

            TypeRef func_type = called_val.type;




            Array<LLVMValueRef> args = make_array_len<LLVMValueRef>(stage_allocator(), info.arg_insts.count);
            for(isize i = 0; i < info.arg_insts.count; i++) {
                LLVMValueRef arg_val = get_llvm_value_from_inst_ref(info.arg_insts[i]);


                if(func_type && is_function_type(func_type) && i < func_type->function_info.param_types.count) {
                    TypeRef from_type = pkg->cir_package.result_of(info.arg_insts[i]).val.type;
                    TypeRef to_type   = func_type->function_info.param_types[i];

                    // TODO: 统一数组→切片隐式转换的调用点
                    if(from_type && to_type && is_array_type(from_type) && is_slice_struct_type(to_type)) {
                        arg_val = gen_ir_cast(from_type, to_type, arg_val);
                    }
                }


                array_push_back(&args, arg_val);
            }


            LLVMTypeRef fn_type = LLVMGlobalGetValueType(llvm_func);
            TypeRef return_type = pkg->cir_package.result_of(ref).val.type;
            char const *name = (return_type && return_type->kind == Type_void) ? "" : "calltmp";
            LLVMValueRef result = LLVMBuildCall2(builder, fn_type, llvm_func, args.data, (unsigned)args.count, name);
            save_llvm_val_of_inst(ref, result);
        } break;
        
        case CIROperator::Cast: {

            if(pkg->cir_package.result_of(ref).type == CIRResultType::WholeValue) {
                return 1;
            }

            auto &info = inst->cast_info;

            LLVMValueRef expr_val = get_llvm_value_from_inst_ref(info.expr_inst);
            TypeRef from_type = pkg->cir_package.result_of(info.expr_inst).val.type;

            // TODO: 可以直接获取result.val.type也行, 不用现在那么麻烦
            TypeRef to_type = extract_type_from_val_as_type(pkg->cir_package.result_of(info.target_type_inst).val);


            LLVMValueRef result = gen_ir_cast(from_type, to_type, expr_val);
            save_llvm_val_of_inst(ref, result);
        } break;

        case CIROperator::Loop: {
            auto &info = inst->loop_info;

            LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(ctx, curr_state.curr_function, "loop.cond");
            LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(ctx, curr_state.curr_function, "loop.body");
            LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(ctx, curr_state.curr_function, "loop.merge");

            array_push_back(&loop_stack, LLVMLoopBlocks{
                .cond_block = cond_bb,
                .merge_block = merge_bb
            });

            LLVMBuildBr(builder, cond_bb);

            // 条件块
            LLVMPositionBuilderAtEnd(builder, cond_bb);
            curr_state.curr_block = cond_bb;
            isize skip = 1;
            if (info.condition_inst != INVALID_INST) {
                skip += gen_ir_inst(info.condition_inst);
                LLVMValueRef cond_val = get_llvm_value_from_inst_ref(info.condition_inst);
                LLVMBuildCondBr(builder, cond_val, body_bb, merge_bb);
            } else {
                LLVMBuildBr(builder, body_bb);
            }

            // 循环体
            LLVMPositionBuilderAtEnd(builder, body_bb);
            curr_state.curr_block = body_bb;
            skip += gen_ir_inst(info.loop_body_inst);
            llvm_build_br_when_no_br(LLVMGetInsertBlock(builder), cond_bb);

            // 继续在 merge 块生成后续指令
            LLVMPositionBuilderAtEnd(builder, merge_bb);
            curr_state.curr_block = merge_bb;

            array_pop_back(&loop_stack);

            return skip;
        } break;

        case CIROperator::If: {
            auto &info = inst->if_info;

            LLVMValueRef cond_val = get_llvm_value_from_inst_ref(info.condition_inst);

            LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(ctx, curr_state.curr_function, "if.then");
            LLVMBasicBlockRef else_bb = LLVMAppendBasicBlockInContext(ctx, curr_state.curr_function, "if.else");
            LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(ctx, curr_state.curr_function, "if.merge");

            LLVMBuildCondBr(builder, cond_val, then_bb, else_bb);

            // then 分支
            LLVMPositionBuilderAtEnd(builder, then_bb);
            curr_state.curr_block = then_bb;
            isize skip = 1 + gen_ir_inst(info.true_block_inst);
            llvm_build_br_when_no_br(LLVMGetInsertBlock(builder), merge_bb);

            // else 分支
            LLVMPositionBuilderAtEnd(builder, else_bb);
            curr_state.curr_block = else_bb;
            if (info.false_block_inst != INVALID_INST) {
                skip += gen_ir_inst(info.false_block_inst);
            }
            llvm_build_br_when_no_br(LLVMGetInsertBlock(builder), merge_bb);

            // 继续在 merge 块生成后续指令
            LLVMPositionBuilderAtEnd(builder, merge_bb);
            curr_state.curr_block = merge_bb;

            return skip;
        } break;

        case CIROperator::Break: {
            auto &info = inst->break_info;
            if (info.break_block == curr_func_body_inst) {
                // 函数 return
                if (info.break_value_inst != INVALID_INST) {
                    LLVMValueRef ret_val = get_llvm_value_from_inst_ref(info.break_value_inst);
                    LLVMBuildRet(builder, ret_val);
                } else {
                    LLVMBuildRetVoid(builder);
                }
            } else {
                // Block break：把 break 值存入 block 指令的 inst_vals
                if (info.break_value_inst != INVALID_INST) {
                    LLVMValueRef break_val = get_llvm_value_from_inst_ref(info.break_value_inst);
                    save_llvm_val_of_inst(info.break_block, break_val);
                }
            }
        } break;

        case CIROperator::LoopBreak: {
            auto &top = loop_stack[loop_stack.count - 1];
            LLVMBuildBr(builder, top.merge_block);
        } break;

        case CIROperator::Continue: {
            auto &top = loop_stack[loop_stack.count - 1];
            LLVMBuildBr(builder, top.cond_block);
        } break;

        case CIROperator::Load: {
            // 老代码:
            // LLVMBuildLoad2(builder, get_llvm_type_from_type(expr->v_type), alloca, expr->Ident.name.c_str);

            auto &info = inst->load_info;
            LLVMValueRef ptr = get_llvm_value_from_inst_ref(info.ptr_inst);
            TypeRef val_type = pkg->cir_package.result_of(ref).val.type;
            LLVMValueRef loaded = LLVMBuildLoad2(builder, get_llvm_type_from_type(val_type), ptr, "loadtmp");
            save_llvm_val_of_inst(ref, loaded);
        } break;

        case CIROperator::Store: {
            // 老代码:
            // LLVMValueRef left_value = gen_ir_expr(gen, stmt->Assignment.left_var_expr, state, true);
            // LLVMValueRef value = gen_ir_expr(gen, stmt->Assignment.right_expr, state);
            // LLVMBuildStore(builder, value, left_value);

            auto &info = inst->store_info;
            LLVMValueRef ptr = get_llvm_value_from_inst_ref(info.var_inst);
            LLVMValueRef val = get_llvm_value_from_inst_ref(info.value_inst);

            TypeRef from_type = pkg->cir_package.result_of(info.value_inst).val.type;
            TypeRef to_type   = pkg->cir_package.result_of(info.var_inst).val.type;

            // TODO: 统一数组→切片隐式转换的调用点
            if (from_type && to_type && is_array_type(from_type) && is_slice_struct_type(to_type)) {
                val = gen_ir_cast(from_type, to_type, val);
            }

            LLVMBuildStore(builder, val, ptr);
        } break;


        case CIROperator::IdentRef: {
            // 老代码:
            //     LLVMValueRef alloca = look_up_local_vals(gen->curr_ir_scope, info);
            //     return alloca;  // is_lvalue_expr 分支

            SymbolInfo *sym = find_symbol_until_global(curr_scope, inst->ident);
            XP_ASSERT_DEFAULT(sym != nullptr);
            LLVMValueRef alloca = look_up_local_vals(&syms, sym);
            XP_ASSERT_DEFAULT(alloca != nullptr);
            
            save_llvm_val_of_inst(ref, alloca);
        } break;


        case CIROperator::IdentVal: {
            SymbolInfo *sym = find_symbol_until_global(curr_scope, inst->ident);
            XP_ASSERT_DEFAULT(sym != nullptr);

            if(is_function_type(sym->value.type)) {
                LLVMValueRef func_val = look_up_local_vals(&syms, sym);
                XP_ASSERT_DEFAULT(func_val != nullptr);
                save_llvm_val_of_inst(ref, func_val);
            }
        } break;


    }
    return 1;
}


void LLVMGenerator::gen_ir_variable_decl(CIRInstructionRef ref, CIRInstruction* inst) {
    XP_ASSERT_DEFAULT(inst->op == CIROperator::VariableDecl);

    // 1. 分配空间
    LLVMValueRef alloca = insert_alloca_before_last_inst_which_is_br(curr_state.entry, xp_string_to_c_style(inst->var_decl.name, stage_allocator()).c_str, get_llvm_type_from_type(inst->result.val.type));


    // 2. 如果有初始值，生成初始值 IR 并存储
    LLVMPositionBuilderAtEnd(builder, curr_state.curr_block);

    SymbolInfo *var_info = find_symbol_until_global(
        curr_scope,
        inst->var_decl.name
    );

    if(inst->var_decl.no_zero_init) {
        // 无初始化, 不做处理

        // 无事发生
    } else {
        // 零初始化

        LLVMTypeRef var_type = get_llvm_type_from_type(inst->result.val.type);
        LLVMValueRef zero_value = LLVMConstNull(var_type);
        LLVMBuildStore(builder, zero_value, alloca);
    }


    auto arg_slot_count = curr_func_info.args.count;

    // 如果这是函数的参数 (slot >= 0), 则应把函数入参存入该 alloca 而不是保持零初始化。
    // Alloc_Var 在为函数参数分配时会给出 slot (0,1,2...)。此处尝试从当前函数中读取对应参数。
    if(inst->var_decl.slot < arg_slot_count && curr_state.curr_function != nullptr) {
        unsigned param_idx = (unsigned)inst->var_decl.slot;
        // LLVMGetParam 在 C API 中按索引取得函数参数
        LLVMValueRef param_val = nullptr;
        // 保护性检查: 如果参数索引在函数参数范围内则读取
        // LLVM函数类型的参数量可用 LLVMCountParams/LLVMGetParams，但这里只尝试读取并在失败时忽略
        param_val = LLVMGetParam(curr_state.curr_function, param_idx);
        LLVMBuildStore(builder, param_val, alloca);
    }


    add_local_val(&syms, var_info, alloca);
    save_llvm_val_of_inst(ref, alloca);
}




// LLVMState gen_ir_block(LLVMGenerator *gen, Ast *block, LLVMState state, bool need_new_scope) {
//     XP_ASSERT_DEFAULT(block->type == AstType_Block);

//     if(need_new_scope) {
//         gen->curr_ir_scope = new_ir_scope(gen->curr_ir_scope, block);
//     } 

//     for(isize i = 0; i < block->Block.statements.count; i++) {

//         // TODO: 想一个更妥当的办法来处理这个问题, 如建模BasicBlock
//         if(is_curr_basic_block_has_terminator(gen)) {
//             // 如果当前基本块已经有终止指令了, 就不继续生成了
//             break;
//         }

//         state = gen_ir_stmt(gen, block->Block.statements[i], state);
//     }

//     if(need_new_scope) {
//         exit_ir_scope(gen);
//     }

//     return state;
// }



// LLVMState gen_ir_stmt(LLVMGenerator *gen, Ast *stmt, LLVMState state) {
//     load_state(state);

//     switch (stmt->type) {
//     case AstType_VariableDecl:
//         state = gen_ir_variable_decl(stmt, state);
//         break;
//     case AstType_IfStmt: {
        
//         LLVMBasicBlockRef cond_block = LLVMAppendBasicBlockInContext(ctx, state.curr_function, "if.cond");
//         LLVMBasicBlockRef then_block = LLVMAppendBasicBlockInContext(ctx, state.curr_function, "if.then");
//         LLVMBasicBlockRef else_block = LLVMAppendBasicBlockInContext(ctx, state.curr_function,  "if.else");
//         LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(ctx, state.curr_function, "merge");
        
//         LLVMBuildBr(builder, cond_block);
        
//         LLVMPositionBuilderAtEnd(builder, cond_block);
//         state.curr_block = cond_block;
//         LLVMValueRef cond_val = gen_ir_expr(gen, stmt->IfStmt.condition, state);


//         LLVMBuildCondBr(builder, cond_val, then_block, else_block);
        

//         //  then 分支
//         LLVMPositionBuilderAtEnd(builder, then_block);
//         state.curr_block = then_block;
//         gen_ir_block(gen, stmt->IfStmt.then_block, state);
        
//         // LLVMBuildBr(builder, merge_block);
//         llvm_build_br_when_no_br(gen, LLVMGetInsertBlock(builder), merge_block);

//         //  else 分支
//         LLVMPositionBuilderAtEnd(builder, else_block);
//         state.curr_block = else_block;
//         if (stmt->IfStmt.else_block) {
//             gen_ir_block(gen, stmt->IfStmt.else_block, state);
//         }
//         llvm_build_br_when_no_br(gen, LLVMGetInsertBlock(builder), merge_block);

//         //  设置插入点到 merge
//         LLVMPositionBuilderAtEnd(builder, merge_block);

//         state.curr_block = merge_block;

//     } break;

//     case AstType_Assignment: {

//         LLVMValueRef left_value = gen_ir_expr(gen, stmt->Assignment.left_var_expr, state, true);
//         LLVMValueRef value = gen_ir_expr(gen, stmt->Assignment.right_expr, state);

//         // LLVMValueRef *alloca = xp_hash_map_get(gen->locals, stmt->Assignment.left_var_expr->VarExpr.name);
//         // XP_ASSERT_DEFAULT(alloca != nullptr);
//         // LLVMBuildStore(builder, value, *alloca);

//         LLVMBuildStore(builder, value, left_value);
//     } break;

//     case AstType::AstType_ForStmt: {

//         gen->curr_ir_scope = new_ir_scope(gen->curr_ir_scope, stmt);
//         defer(exit_ir_scope(gen));

//         // 1. 创建基本块
//         LLVMBasicBlockRef init_block = LLVMAppendBasicBlockInContext(ctx, state.curr_function, "for.init");
//         LLVMBasicBlockRef cond_block = LLVMAppendBasicBlockInContext(ctx, state.curr_function, "for.cond");
//         LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(ctx, state.curr_function, "for.body");
//         LLVMBasicBlockRef post_block = LLVMAppendBasicBlockInContext(ctx, state.curr_function, "for.post");
//         LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(ctx, state.curr_function, "for.merge");

//         array_push_back(&loop_stack, LLVMLoopBlocks{
//             .post_block = post_block,
//             .merge_block = merge_block
//         });
//         defer(array_pop_back(&loop_stack));

//         // 2. 初始化表达式
//         LLVMBuildBr(builder, init_block);
//         LLVMPositionBuilderAtEnd(builder, init_block);
//         state.curr_block = init_block;

//         if(stmt->ForStmt.init != nullptr) {
//             gen_ir_stmt(gen, stmt->ForStmt.init, state);
//         }
        
//         // 3. 跳转到条件判断
//         LLVMBuildBr(builder, cond_block);


//         // 4. 条件判断
//         LLVMPositionBuilderAtEnd(builder, cond_block);

//         if(stmt->ForStmt.condition != nullptr) {
//             // 有条件循环
//             LLVMValueRef cond_val = gen_ir_expr(gen, stmt->ForStmt.condition, state);
//             LLVMBuildCondBr(builder, cond_val, body_block, merge_block);
//         } else {
//             // 无条件循环
//             LLVMBuildBr(builder, body_block);
//         }
        
        
//         // 5. 循环体
//         LLVMPositionBuilderAtEnd(builder, body_block);
//         state.curr_block = body_block;

//         gen_ir_block(gen, stmt->ForStmt.body, state, false);
//         // LLVMBuildBr(builder, post_block);
//         llvm_build_br_when_no_br(gen, LLVMGetInsertBlock(builder), post_block);

//         // 6. 后置表达式
//         LLVMPositionBuilderAtEnd(builder, post_block);
//         state.curr_block = post_block;

//         if(stmt->ForStmt.post != nullptr) {
//             gen_ir_stmt(gen, stmt->ForStmt.post, state);
//         }
        
//         LLVMBuildBr(builder, cond_block);

//         // 7. 合并块
//         LLVMPositionBuilderAtEnd(builder, merge_block);
//         state.curr_block = merge_block;

//     } break;

//     case AstType_ReturnStmt: {

//         if(stmt->ReturnStmt.expr == nullptr) {
//             // void 返回
//             LLVMBuildRetVoid(builder);
//             break;
//         }

//         LLVMValueRef ret_val = gen_ir_expr(gen, stmt->ReturnStmt.expr, state);
//         LLVMBuildRet(builder, ret_val);
//     } break;

//     case AstType_Break: {
//         LLVMBuildBr(builder, loop_stack[loop_stack.count - 1].merge_block);
//     } break;
//     case AstType_Continue: {
//         LLVMBuildBr(builder, loop_stack[loop_stack.count - 1].post_block);
//     } break;


//     case AstType_Block: {
//         state = gen_ir_block(gen, stmt, state);
//     } break;

//     case AstType_ConstDecl: {
//         // 局部ConstDecl, 不用处理, 因为都已经在语义分析阶段计算好值并存到符号表里了, 直接gen_ir_expr的时候从符号表里取值就行了
//     } break;

//     default: {
//         gen_ir_expr(gen, stmt, state);
//         break;
//     }


//     }

//     return state;
// }

// TODO: 统一数组→切片隐式转换的调用点
LLVMValueRef LLVMGenerator::gen_array_value_to_slice_cast(LLVMValueRef array_value_ptr, TypeRef array_value_type) {


    LLVMTypeRef array_type = get_llvm_type_from_type(array_value_type);
    LLVMTypeRef slice_struct_type = get_llvm_type_from_type(slice_type_as_struct(array_value_type->array_info.element_type));
    
    LLVMValueRef slice_struct_value = LLVMGetUndef(slice_struct_type);

    LLVMValueRef indices[2] = {
        LLVMConstInt(LLVMInt32TypeInContext(ctx), 0, 0),
        LLVMConstInt(LLVMInt32TypeInContext(ctx), 0, 0)
    };
    // 设置数据指针
    LLVMValueRef data_ptr = LLVMBuildGEP2(builder, array_type, array_value_ptr, indices, 2, "arraydataptrtmp");
    slice_struct_value = LLVMBuildInsertValue(builder, slice_struct_value, data_ptr, 0, "insertsliceptrtmp");
    
    // 设置count
    // TODO i64 换成 isize
    LLVMValueRef count_value = LLVMConstInt(LLVMInt64TypeInContext(ctx), array_value_type->array_info.count, 0);

    slice_struct_value = LLVMBuildInsertValue(builder, slice_struct_value, count_value, 1, "insertslicecounttmp");

    return slice_struct_value;
}



// LLVMValueRef gen_ir_cast_expr(LLVMGenerator *gen, Ast *cast_expr, LLVMState state) {
//     return gen_ir_cast(gen, cast_expr->CastExpr.expr->v_type, cast_expr->CastExpr.target_type, gen_ir_expr(gen, cast_expr->CastExpr.expr, state));
// }


void LLVMGenerator::gen_ir_binary_expr(CIRInstructionRef inst) {

    // 如果结果已经在编译期求值得到了, 就不用生成指令了
    if(pkg->cir_package.result_of(inst).type == CIRResultType::WholeValue) {
        return;
    }

    auto binary_info = pkg->cir_package.inst(inst)->binary_info;

    TokenType op = binary_info.op;
    CIRInstructionRef left_inst = binary_info.left_inst;
    CIRInstructionRef right_inst = binary_info.right_inst;


    LLVMValueRef left = get_llvm_value_from_inst_ref(left_inst);
    LLVMValueRef right = get_llvm_value_from_inst_ref(right_inst);

    TypeRef left_type = pkg->cir_package.result_of(left_inst).val.type;
    TypeRef right_type = pkg->cir_package.result_of(right_inst).val.type;

    LLVMValueRef result;

    switch(op) {
        case TokenType::Add: { // +
            if(is_float_type(left_type)) {
                result = LLVMBuildFAdd(builder, left, right, "addtmp");
            } else if(is_pointer_type(left_type) || is_pointer_type(right_type)) {
                LLVMValueRef pointer_val = is_pointer_type(left_type) ? left : right;
                LLVMValueRef index_val  = is_pointer_type(left_type) ? right : left;
                TypeRef pointer_type = is_pointer_type(left_type) ? left_type : right_type;

                LLVMValueRef indices[] = { index_val };
                result = LLVMBuildGEP2(builder, get_llvm_type_from_type(pointer_type->pointed_type), pointer_val, indices, 1, "ptraddtmp");
            } else {
                result = LLVMBuildAdd(builder, left, right, "addtmp");
            }
        } break;
        case TokenType::Minus: { // -
            if(is_float_type(left_type)) {
                result = LLVMBuildFSub(builder, left, right, "subtmp");
            } else if(is_pointer_type(left_type) && is_pointer_type(right_type)) {
                TypeRef pointed_type = left_type->pointed_type;
                LLVMTypeRef int_type = LLVMInt64TypeInContext(ctx);
                LLVMValueRef left_int  = LLVMBuildPtrToInt(builder, left,  int_type, "ptrtointtmp");
                LLVMValueRef right_int = LLVMBuildPtrToInt(builder, right, int_type, "ptrtointtmp");
                LLVMValueRef byte_diff = LLVMBuildSub(builder, left_int, right_int, "ptrdiffbytetmp");
                LLVMValueRef elem_size = LLVMConstInt(int_type, size_of_type(pointed_type), false);
                result = LLVMBuildSDiv(builder, byte_diff, elem_size, "ptrdiffdivtmp");
            } else if(is_pointer_type(left_type)) {
                LLVMValueRef neg_index = LLVMBuildNeg(builder, right, "negtmp");
                LLVMValueRef indices[] = { neg_index };
                result = LLVMBuildGEP2(builder, get_llvm_type_from_type(left_type->pointed_type), left, indices, 1, "ptrsubtmp");
            } else {
                result = LLVMBuildSub(builder, left, right, "subtmp");
            }
        } break;
        case TokenType::Star: { // *
            if(is_float_type(left_type)) {
                result = LLVMBuildFMul(builder, left, right, "multmp");
            } else {
                result = LLVMBuildMul(builder, left, right, "multmp");
            }
        } break;
        case TokenType::ForwardSlash: { // /
            if(is_float_type(left_type)) {
                result = LLVMBuildFDiv(builder, left, right, "divtmp");
            } else if(is_signed_type(left_type)) {
                result = LLVMBuildSDiv(builder, left, right, "divtmp");
            } else {
                result = LLVMBuildUDiv(builder, left, right, "divtmp");
            }
        } break;
        case TokenType::Percent: { // %
            if(is_float_type(left_type)) {
                XP_ASSERT_DEFAULT(0); // 浮点数不支持取模运算
            }

            if(is_signed_type(left_type)) {
                result = LLVMBuildSRem(builder, left, right, "modtmp");
            } else {
                result = LLVMBuildURem(builder, left, right, "modtmp");
            }
        } break;
        case TokenType::GreaterThan: { // >
            if(is_float_type(left_type)) {
                result = LLVMBuildFCmp(builder, LLVMRealOGT, left, right, "gttmp");
            } else if(is_pointer_type(left_type) || is_pointer_type(right_type)) {
                result = LLVMBuildICmp(builder, LLVMIntUGT, left, right, "gttmp");
            } else if(is_signed_type(left_type)) {
                result = LLVMBuildICmp(builder, LLVMIntSGT, left, right, "gttmp");
            } else {
                result = LLVMBuildICmp(builder, LLVMIntUGT, left, right, "gttmp");
            }
        } break;
        case TokenType::GreaterEqual: { // >=
            if(is_float_type(left_type)) {
                result = LLVMBuildFCmp(builder, LLVMRealOGE, left, right, "getmp");
            } else if(is_pointer_type(left_type) || is_pointer_type(right_type)) {
                result = LLVMBuildICmp(builder, LLVMIntUGE, left, right, "getmp");
            } else if(is_signed_type(left_type)) {
                result = LLVMBuildICmp(builder, LLVMIntSGE, left, right, "getmp");
            } else {
                result = LLVMBuildICmp(builder, LLVMIntUGE, left, right, "getmp");
            }
        } break;
        case TokenType::LessThan: { // <
            if(is_float_type(left_type)) {
                result = LLVMBuildFCmp(builder, LLVMRealOLT, left, right, "lttmp");
            } else if(is_pointer_type(left_type) || is_pointer_type(right_type)) {
                result = LLVMBuildICmp(builder, LLVMIntULT, left, right, "lttmp");
            } else if(is_signed_type(left_type)) {
                result = LLVMBuildICmp(builder, LLVMIntSLT, left, right, "lttmp");
            } else {
                result = LLVMBuildICmp(builder, LLVMIntULT, left, right, "lttmp");
            }
        } break;
        case TokenType::LessEqual: { // <=
            if(is_float_type(left_type)) {
                result = LLVMBuildFCmp(builder, LLVMRealOLE, left, right, "letmp");
            } else if(is_pointer_type(left_type) || is_pointer_type(right_type)) {
                result = LLVMBuildICmp(builder, LLVMIntULE, left, right, "letmp");
            } else if(is_signed_type(left_type)) {
                result = LLVMBuildICmp(builder, LLVMIntSLE, left, right, "letmp");
            } else {
                result = LLVMBuildICmp(builder, LLVMIntULE, left, right, "letmp");
            }
        } break;
        case TokenType::DoubleEqual: { // ==
            result = compare_two_values(left, right, left_type, false);
        } break;
        case TokenType::ExclamationEqual: { // !=
            result = compare_two_values(left, right, left_type, true);
        } break;
        case TokenType::DoubleAnd: { // &&
            result = LLVMBuildAnd(builder, left, right, "andtmp");
        } break;
        case TokenType::DoubleOr: { // ||
            result = LLVMBuildOr(builder, left, right, "ortmp");
        } break;


        default: {
            UNREACHABLE();
        } break;
    }

    save_llvm_val_of_inst(inst, result);
}

void LLVMGenerator::gen_ir_unary(CIRInstructionRef inst) {
     // 如果结果已经在编译期求值得到了, 就不用生成指令了
    if(pkg->cir_package.result_of(inst).type == CIRResultType::WholeValue) {
        return;
    }

    auto unary_info = pkg->cir_package.inst(inst)->unary_info;

    auto op = unary_info.op;
    LLVMValueRef operand = get_llvm_value_from_inst_ref(unary_info.operand_inst);
    TypeRef operand_type = pkg->cir_package.result_of(unary_info.operand_inst).val.type;
    LLVMTypeRef llvm_operand_type = get_llvm_type_from_type(operand_type);

    LLVMValueRef result;

    // 一元表达式（如负号）
    if (op == TokenType::Minus) {
        if(is_float_type(operand_type)) {
            LLVMValueRef zero = LLVMConstReal(llvm_operand_type, 0.0);
            result = LLVMBuildFSub(builder, zero, operand, "fnegtmp");
        }

        result = LLVMBuildNeg(builder, operand, "negtmp");
    } else if(op == TokenType::Exclamation) {
        if(is_float_type(operand_type)) {
            XP_ASSERT_DEFAULT(0); // 浮点数不支持逻辑非运算
        }

        result = LLVMBuildNot(builder, operand, "nottmp");
    }
    
  

    save_llvm_val_of_inst(inst, result); 
}


// LLVMValueRef gen_ir_expr(LLVMGenerator *gen, Ast *expr, LLVMState state, bool is_lvalue_expr) {
//     switch (expr->type)
//     {

//     case AstType_FieldAccess: {
//         // parent是 package名(如fmt) 或 结构体变量名(如 person) 
//         Ast *parent = expr->FieldAccess.parent;

//         LLVMValueRef parent_value = gen_ir_expr(gen, parent, state);
//         if(is_package_type(parent->v_type)) {
//             // 包外变量/常量访问

//             // 既然是包名, 那一定只是单个ident
//             XP_ASSERT_DEFAULT(parent->type == AstType_Ident);
//             xpString package_name = parent->Ident.name;
//             SymbolInfo *symbol_info = find_symbol_until(
//                 ScopeType::File,
//                 gen->curr_ir_scope->scope,
//                 package_name
//             );
//             Package *pkg = symbol_info->value.get_package_value();
//             SymbolInfo *outer_val_info = find_symbol_until(
//                 ScopeType::Package,
//                 &pkg->package_scope,
//                 expr->FieldAccess.field_name
//             );

//             return gen_llvm_val_by_value(gen, outer_val_info->value, xpOption<TypeRef>(expr->v_type));
//         }


//         // 枚举类型访问
//         if(is_type_type(parent->v_type) && is_enum_type(parent->v_type->self_type_info)) {
//             // 枚举访问
//             TypeRef enum_type = parent->v_type->self_type_info;

//             Scope &enum_scope = enum_type->enum_info.enum_scope;
//             SymbolInfo *enumerator_info = find_symbol_curr(
//                 &enum_scope,
//                 expr->FieldAccess.field_name
//             );

//             Value enum_value = enumerator_info->value;


//             // TODO: 可以把这个逻辑放到gen_llvm_val_by_value里, 只要传入枚举类型的expected_type就行了, 让gen_llvm_val_by_value来处理这个把枚举值转换成对应整数值的逻辑, 这样就不需要在这里特判枚举类型了, 只要在gen_llvm_val_by_value里处理好枚举类型就行了
//             // NOTE: 把类型改回为原始枚举类型, 以便gen_llvm_val_by_value能正确处理
//             enum_value.set_type(enum_type->enum_info.element_type);
//             return gen_llvm_val_by_value(gen, enum_value, xpOption<TypeRef>::none());
//         }

//         //
//         // 目前这里一定是结构体字段访问, 不用分支
//         //
        
//         Ast *parent_expr = nullptr;
//         if(is_struct_type(expr->FieldAccess.parent->v_type)) {
//             parent_expr = expr->FieldAccess.parent;
//         } else if(is_pointer_type(expr->FieldAccess.parent->v_type) && is_struct_type(get_pointed_type(expr->FieldAccess.parent->v_type))) {
            
//             // 如果是指向结构体的指针，则需要先解引用
//             parent_expr = ast_alloc(AstType_UnaryExpr, stage_allocator());
//             parent_expr->UnaryExpr.op = TokenType::Caret;
//             parent_expr->UnaryExpr.operand = expr->FieldAccess.parent;
//             parent_expr->v_type = get_pointed_type(expr->FieldAccess.parent->v_type);
//         } else {
//             XP_ASSERT_DEFAULT(0);
//         }

//         TypeRef struct_type = parent_expr->v_type;
        
        
//         isize field_index = -1;
//         for(isize i = 0; i < struct_type->struct_info.struct_fields.count; i++) {
//             if(struct_type->struct_info.struct_fields[i].name == expr->FieldAccess.field_name) {
//                 field_index = i;
//                 break;
//             }
//         }
//         XP_ASSERT_DEFAULT(field_index != -1);
        

//         if(is_lvalue_expr) {
//             // 这里正好发现is_lvalue_expr用来获取指针的场景
//             LLVMValueRef struct_ptr = gen_ir_expr(gen, parent_expr, state, true);

//             LLVMValueRef field_ptr = LLVMBuildStructGEP2(builder, get_llvm_type_from_type(struct_type), struct_ptr, field_index, "fieldptrtmp");
//             return field_ptr;
//         } else {
//             LLVMValueRef struct_val = gen_ir_expr(gen, parent_expr, state);
//             LLVMValueRef field_val = LLVMBuildExtractValue(builder, struct_val, field_index, "fieldextractedtmp");
//             return field_val;
//         }

//     } break;

//     case AstType_Ident: {
//         SymbolInfo *info = find_symbol_until_global(gen->curr_ir_scope->scope, expr->Ident.name);
//         XP_ASSERT_DEFAULT(info != nullptr);


//         // 非常量/变量, 
//         if(!is_value_type(info->value.type)) {
//             // 包外函数调用中的包名访问, 直接返回NULL, 在gen_ir_function_call_expr中根据这个名字和函数名构造完整的函数名来获取函数地址
//             return nullptr;
//         }

//         if(info->value.is_runtime_value) {
//             // 变量

//             LLVMValueRef alloca = look_up_local_vals(gen->curr_ir_scope, info);
//             XP_ASSERT_DEFAULT(alloca != nullptr);
    
//             // 数组到切片的隐式转换
//             if(expr->implicit_conversion_tag == ImplicitConversionTag::ArrayToSliceStruct) {
//                 return gen_array_value_to_slice_cast(gen, alloca, expr->v_type);
//             }
    
    
//             if(is_lvalue_expr) {
//                 return alloca;
//             } else {
//                 return LLVMBuildLoad2(builder, get_llvm_type_from_type(expr->v_type), alloca, expr->Ident.name.c_str);
//             }

//         } else if(!info->value.is_runtime_value){
//             // 常量, 直接从scope获取值生成llvmvalue
//             LLVMValueRef const_val = gen_llvm_val_by_value(gen, info->value, xpOption<TypeRef>(expr->v_type));
//             XP_ASSERT_DEFAULT(const_val != nullptr);

//             return const_val;
//         } else {
//             UNREACHABLE();
//         }


//     } break;
//     case AstType_Constant: {
//         // if(is_float_type(expr->v_type)) {
//         //     // 浮点数常量
//         //     double float_value = expr->Constant.float_value;
//         //     return LLVMConstReal(get_llvm_type_from_type( expr->v_type), float_value);
//         // }

//         if(expr->is_null) {
//             // null 常量
//             // 在llvm为*i8类型的指针
//             return LLVMConstNull(LLVMPointerType(LLVMVoidTypeInContext(ctx), 0));
//         }

//         return gen_llvm_val_by_value(gen, expr->Constant.value, xpOption<TypeRef>(expr->v_type));

//         // LLVMBool is_signed = cast(LLVMBool) is_signed_or_bool_type(expr->v_type);
//         // return LLVMConstInt(get_llvm_type_from_type(expr->v_type), expr->Constant.value, is_signed);
//     } break;
//     case AstType_UnaryExpr: {
//         // 一元表达式（如负号）
//         LLVMValueRef operand = gen_ir_expr(gen, expr->UnaryExpr.operand, state);
//         if (expr->UnaryExpr.op == TokenType::Minus) {
//             if(is_float_type(expr->UnaryExpr.operand->v_type)) {
//                 LLVMValueRef zero = LLVMConstReal(get_llvm_type_from_type(expr->UnaryExpr.operand->v_type), 0.0);
//                 return LLVMBuildFSub(builder, zero, operand, "fnegtmp");
//             }

//             return LLVMBuildNeg(builder, operand, "negtmp");
//         } else if(expr->UnaryExpr.op == TokenType::Exclamation) {
//             if(is_float_type(expr->UnaryExpr.operand->v_type)) {
//                 XP_ASSERT_DEFAULT(0); // 浮点数不支持逻辑非运算
//             }

//             return LLVMBuildNot(builder, operand, "nottmp");
//         } else if(expr->UnaryExpr.op == TokenType::And) {
//             // 取地址运算符

//             // TODO 改成通过gen_ir_expr获取
//             // LLVMValueRef *alloca = xp_hash_map_get(gen->locals, expr->UnaryExpr.operand->VarExpr.name);
//             // XP_ASSERT_DEFAULT(alloca != nullptr);
//             // return *alloca;

//             LLVMValueRef ptr = gen_ir_expr(gen, expr->UnaryExpr.operand, state, true);
//             return ptr;

//         } else if(expr->UnaryExpr.op == TokenType::Caret) {
//             // 解引用运算符
//             LLVMValueRef ptr = gen_ir_expr(gen, expr->UnaryExpr.operand, state);
//             if(is_lvalue_expr) {
//                 return ptr;
//             } else {
//                 return LLVMBuildLoad2(builder, get_llvm_type_from_type(expr->v_type), ptr, "loadtmp");
//             }
//         }

//         XP_ASSERT_DEFAULT(0);
//     } break;

//     case AstType_BinaryExpr: {
//         return gen_ir_binary_expr(expr, state);
//     } break;

//     case AstType_FunctionCallExpr: {

//         SymbolInfo *func_symbol = nullptr;
//         if(expr->FunctionCallExpr.func_ident->type == AstType_FieldAccess) {
//             func_symbol = get_extern_func_sym_by_full_ident_ast(expr->FunctionCallExpr.func_ident, gen);
//         } else if(expr->FunctionCallExpr.func_ident->type == AstType_Ident) {
//             func_symbol = find_symbol_until_global(gen->curr_ir_scope->scope, expr->FunctionCallExpr.func_ident->Ident.name);
//         } else {
//             UNREACHABLE();
//         }
//         TypeRef val_type = func_symbol->value.type;

//         // NOTE: 这里应该只会处理别的package的函数
//         // TODO: ABSTRACT: 获取原始函数符号, 并生成LLVM函数(如没生成)
//         SymbolInfo *ori_func_sym = get_ori_func_symbol_by_func_value(func_symbol->value);
//         XP_ASSERT_DEFAULT(ori_func_sym != nullptr);
//         LLVMValueRef ori_func_llvm_val = look_up_local_vals(gen->curr_ir_scope, ori_func_sym);

//         LLVMValueRef func = nullptr;
//         if(ori_func_llvm_val == nullptr) {
//             xpString func_full_name = gen_func_full_name(ori_func_sym->package, ori_func_sym->name, ori_func_sym->value.get_function_is_extern_c(), stage_allocator());
//             LLVMValueRef func_val = LLVMAddFunction(module, func_full_name.c_str, get_llvm_type_from_type(val_type));
//             add_local_val_spec_scope(gen->curr_ir_scope, ScopeType::Package, ori_func_sym, func_val);

//             func = func_val;
//         } else {
//             func = ori_func_llvm_val;
//         }




//         Array<LLVMValueRef> args = make_array_len<LLVMValueRef>(stage_allocator(), expr->FunctionCallExpr.args.count);
//         for(isize i = 0; i < expr->FunctionCallExpr.args.count; i++) {
//             LLVMValueRef arg_val = gen_ir_expr(gen,  expr->FunctionCallExpr.args[i], state);


//             // TODO: 临时兼容c可变参数函数调用
//             if(is_var_arg_function(val_type) && i >= get_fixed_param_count(val_type)) {
//                 // 可变参数, 需要进行类型提升
//                 TypeRef param_type = expr->FunctionCallExpr.args[i]->v_type;

//                 if(param_type == easy_type(Type_f32)) {
//                     // float提升到double
//                     arg_val = gen_ir_cast(gen, param_type, easy_type(Type_f64), arg_val);
//                 } else if(param_type == easy_type(Type_i8) || 
//                           param_type == easy_type(Type_u8) || 
//                           param_type == easy_type(Type_bool)
//                         ) {
                            
//                     // i8/u8/i16/u16/bool提升到i32
//                     arg_val = gen_ir_cast(gen, param_type, easy_type(Type_i32), arg_val);
//                 }
//             }

//             array_push_back(&args, arg_val);
//         }

//         LLVMTypeRef type = LLVMGlobalGetValueType(func);

//         TypeRef return_type = expr->v_type;

//         if(return_type == easy_type(Type_void)) {
//             // 无返回值函数调用

//             return LLVMBuildCall2(builder, type, func, args.data, args.count, "");
//         } else {
//             // 有返回值函数调用

//             return LLVMBuildCall2(builder, type, func, args.data, args.count, "calltmp");
//         }
//     } break;

//     case AstType_CastExpr: {
//         return gen_ir_cast_expr(gen, expr, state);
//     } break;

    
//     case AstType_StructInitExpr: {
//         LLVMTypeRef struct_type = get_llvm_type_from_type(expr->v_type);

//         // 先用 undef 初始化
//         LLVMValueRef struct_val = LLVMGetUndef(struct_type);

//         for(isize i = 0; i < expr->StructInitExpr.field_inits.count; i++) {
//             LLVMValueRef field_value = gen_ir_expr(gen, expr->StructInitExpr.field_inits[i], state);
//             struct_val = LLVMBuildInsertValue(builder, struct_val, field_value, i, "insertvaltmp");
//         }

//         return struct_val;

//     } break;

//     case AstType_ArrayInitExpr: {
//         LLVMTypeRef array_type = get_llvm_type_from_type(expr->v_type);

//         // 先用 undef 初始化
//         LLVMValueRef array_val = LLVMGetUndef(array_type);

//         for(isize i = 0; i < expr->ArrayInitExpr.elements.count; i++) {
//             LLVMValueRef element_value = gen_ir_expr(gen, expr->ArrayInitExpr.elements[i], state);
//             array_val = LLVMBuildInsertValue(builder, array_val, element_value, i, "arrayinsertvaltmp");
//         }

//         if(expr->implicit_conversion_tag == ImplicitConversionTag::ArrayToSliceStruct) {
//             // 数组到切片的隐式转换
//             return gen_array_value_to_slice_cast(gen, get_ptr_of_llvm_value(gen, array_val), expr->v_type);
//         }

//         return array_val;

//     } break;

//     case AstType_IndexExpr: {
//         LLVMValueRef array_or_slice_ptr = gen_ir_expr(gen, expr->IndexExpr.array_var_expr, state, true);
//         LLVMValueRef index_val = gen_ir_expr(gen, expr->IndexExpr.index_expr, state);


//         LLVMValueRef indices[2] = { nullptr };
//         LLVMValueRef elem_ptr = nullptr;
//         if(is_array_type(expr->IndexExpr.array_var_expr->v_type)) {
//             // 数组索引
//             indices[0] = LLVMConstInt(LLVMInt32TypeInContext(ctx), 0, 0); // 取数组指针的第0个元素, 就是数组本身
//             indices[1] = index_val; // 索引值

//             elem_ptr = LLVMBuildGEP2(builder, get_llvm_type_from_type(expr->IndexExpr.array_var_expr->v_type), array_or_slice_ptr, indices, 2, "arrayelementptrtmp");

//         } else if(is_slice_struct_type(expr->IndexExpr.array_var_expr->v_type) || is_string_struct_type(expr->IndexExpr.array_var_expr->v_type)) {
//             // 切片索引 或 字符串索引
            
//             LLVMValueRef slice_val = LLVMBuildLoad2(builder, get_llvm_type_from_type(expr->IndexExpr.array_var_expr->v_type), array_or_slice_ptr, "loadslicetmp");
//             LLVMValueRef data_raw = LLVMBuildExtractValue(builder, slice_val, 0, "slicedataptrtmp");

//             TypeRef data_ptr_type = expr->IndexExpr.array_var_expr->v_type->struct_info.struct_fields[0].type;
//             LLVMTypeRef data_ptr_llvm_type = get_llvm_type_from_type(data_ptr_type);
            


//             LLVMValueRef data_typed_ptr = LLVMBuildBitCast(builder, data_raw, data_ptr_llvm_type, "slicedataptrtypedtmp");
            
//             indices[0] = index_val; // 索引值
//             elem_ptr = LLVMBuildGEP2(builder, get_llvm_type_from_type(data_ptr_type->pointed_type), data_typed_ptr, indices, 1, "sliceelementptrtmp");

//         } else {
//             XP_ASSERT_DEFAULT(0);
//         }


//         if(is_lvalue_expr) {
//             return elem_ptr;
//         } else {
//             return LLVMBuildLoad2(builder, get_llvm_type_from_type(expr->v_type), elem_ptr, "loadelementtmp");
//         }

//     } break;

//     case AstType_StringLiteralExpr: {

//         return gen_ir_string_struct_value(gen, expr->StringLiteralExpr.str);
//     } break;


//     default:
//         UNREACHABLE();
//     }

//     return nullptr;
// }

// 处理 == 和 != 操作符的比较逻辑
LLVMValueRef LLVMGenerator::compare_two_values(LLVMValueRef left, LLVMValueRef right, TypeRef type, bool is_not_equal) {
    if(is_struct_type(type)) {
        // Type type_detail = get_type_detail_if_have(symbol_table(), type);
        
        
        LLVMValueRef result = nullptr;
        for(isize i = 0; i < type->struct_info.struct_fields.count; i++) {
            LLVMValueRef left_field = LLVMBuildExtractValue(builder, left, i, "leftextracttmp");
            LLVMValueRef right_field = LLVMBuildExtractValue(builder, right, i, "rightextracttmp");
            
            LLVMValueRef field_cmp = compare_two_values(left_field, right_field, type->struct_info.struct_fields[i].type, false);
            
            if(result == nullptr) {
                result = field_cmp;
            } else {
                result = LLVMBuildAnd(builder, result, field_cmp, "andeqtmp");
            }
        }

        if(is_not_equal) {
            result = LLVMBuildNot(builder, result, "noteqtmp");
        }

        return result;
    } else if(is_float_type(type)) {

        if(is_not_equal) {
            return LLVMBuildFCmp(builder, LLVMRealONE, left, right, "noteqtmp");
        } 
        return LLVMBuildFCmp(builder, LLVMRealOEQ, left, right, "eqtmp");
    } else {
        // 整数、指针、枚举等类型的比较

        if(is_not_equal) {
            return LLVMBuildICmp(builder, LLVMIntNE, left, right, "noteqtmp");
        }
        return LLVMBuildICmp(builder, LLVMIntEQ, left, right, "eqtmp");
    }

}

// LLVMValueRef LLVMGenerator::gen_ir_compare_expr(CIRInstructionRef inst, LLVMState state, bool is_not_equal) {
//     XP_ASSERT_DEFAULT(pkg->cir_package.inst(inst)->op == CIROperator::Binary);

//     LLVMValueRef left_val = gen_ir_expr(gen, expr->BinaryExpr.left, state);
//     LLVMValueRef right_val = gen_ir_expr(gen, expr->BinaryExpr.right, state);

//     return compare_two_values(gen, left_val, right_val, left_type, is_not_equal);
// }