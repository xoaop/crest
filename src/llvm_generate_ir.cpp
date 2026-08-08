#include "llvm_generate_ir.hpp"

// #include "common.hpp"

// #include "symbol.hpp"

// #include "ast.hpp"

// #include "analyser.hpp"

#include "context.hpp"

#include "path.hpp"

#include <print>



/*
============================================================================
LLVM IR 生成 —— 核心概念语义定义
============================================================================

【Region】
  拥有自己 mapper 的 CIR 指令：Block、Loop。
  特征：有 mapper（fragments + exit_blk）、有 body（CIR 子指令流）、
  占多个 CIR slot（gen_ir_inst 返回 1 + body_len）。

【Mapper (LLVMBasicBlockMapper)】
  Region 拥有的 BB 集合，两部分：
    fragments  — add_frag_blk() 添加的运行时可达 BB 序列
    exit_block — 构造时预创建的独立 BB，不在 fragments 里，是 Region 的"统一出口"
  创建时机：get_or_create_mapper(ref) 首次调用时。

【exit_blk】
  Region 的统一出口 BB。不变量：
  - 任何离开 Region 的控制流（Break、自然落出）都经过 exit_blk
  - exit_blk 不含业务代码，只做跳转中转
  - 父级（CondBr 或 Block 自连接）负责把 exit_blk 接线到父 mapper 的 merge_bb

【connect_to_parent】
  gen_ir_block_in_func_block(blk_ref, connect_to_parent) 的第二参数：
    true  — Block 自己衔接父 BB：parent_bb → br first_bb，exit_blk → br 父.merge。
            由 gen_ir_inst Block case 调用（父迭代器路径）。
    false — Block 不处理外部衔接，调用者负责。由 CondBr 调用（then/else 路径）。

【curr_blk】
  当前正在生成代码的 Region 的 CIR Block 引用。
  用途：CondBr / Block 自连接通过 get_or_create_mapper(curr_blk) 找到父 mapper，
  把 merge_bb 加入其中。
  生命周期：gen_ir_block_in_func_block / Loop 入口 save → set → defer restore。

【Block (gen_ir_block_in_func_block)】
  1. 保存 parent_bb，创建/获取 mapper，添加 first_bb
  2. (connect_to_parent) parent_bb 未终止 → br first_bb
  3. position 到 first_bb，遍历 body 指令
  4. 落出：last_bb 未终止 → br exit_blk（func_body → ret void）
  5. (connect_to_parent 且非 func_body) exit_blk 未终止 → br 父.block.merge，position 过去
  6. (!connect_to_parent) 恢复 builder 到 parent_bb

【Loop】
  1. 保存 parent_bb，创建 mapper，添加 first_bb，position 到 first_bb
  2. 遍历 body
  3. 回边：last_bb 未终止 → br first_bb
  4. 入口接线：parent_bb 未终止 → br first_bb
  5. position 到 exit_blk（break 目标）
  Loop 的衔接是内建的，无开关——Loop 总是自己接线。

【CondBr】
  不是 Region，没有 mapper。语义：
  1. 取 cond_val；若 cond 是 Block 且其 exit_blk 未终止 → position 过去
  2. 直接调 gen_ir_block_in_func_block(then, false)、gen_ir_block_in_func_block(else, false)
  3. 在当前位置建 condbr cond_val ? then.first_bb : else.first_bb
  4. 在父 mapper (curr_blk) 建 condbr.merge
  5. 接线：then.exit_blk → merge、else.exit_blk → merge
  6. position 到 merge

【Break】
  跳转到 target_block 的 exit_blk。
  target == func_body → ret；否则 → br target.exit_blk。
  Break 后在当前 mapper 建 after_break BB（死代码隔离），position 过去。

【父迭代器（while body_pc < body_end）】
  调用 gen_ir_inst(body_pc)，用返回值 skip 推进 body_pc。
  不感知子指令是否是 Region——统一用 skip 跳过。
  Region 通过自连接/回边/入口接线自己管理 BB 衔接。

【关联关系】
  父迭代器 ──gen_ir_inst(skip)──→ 子指令

  子是 Block(connect=true): 父不知 Block 存在，Block 自己 parent→br first, exit→br parent.merge
  子是 Loop:              父不知 Loop 存在，Loop 自己 parent→br first, 回边→br first
  子是 CondBr:            CondBr 自己 gen then/else(connect=false)，建 condbr+parent.merge
  子是 Break:             br target.exit_blk，由 target 父级把 exit_blk 接到 merge

  核心原则：父迭代器不穿透抽象。Region 自己管理 BB 衔接。CondBr 作为 then/else
  的"所有者"，显式管理它们的衔接。
============================================================================
*/

struct LLVMGenerator;

static xpHashMap<CIRInstResultRef, xpString>& get_global_func_names() {
    static xpHashMap<CIRInstResultRef, xpString> map = xp_hash_map_make<CIRInstResultRef, xpString>(permanent_allocator());
    return map;
}

xpString register_func_name(CIRInstResultRef key, bool is_extern_c) {
    auto& map = get_global_func_names();
    return map.get_or_insert(key, [&]{
        CIRInstruction* inst = key.cir_package->inst(key.inst_ref);
        ASSERT(inst->op == CIROperator::FunctionDecl);

        SymbolInfo* sym = (inst->symbol)();
        xpString base_name;
        if (sym) {
            base_name = sym->name;
        } else {
            static isize anon_counter = 0;
            base_name = xp_string_copy(permanent_allocator(), xp_string_c("__anon_"));
            xp_string_append(&base_name, xp_isize_to_string(anon_counter++, permanent_allocator()));
        }

        if (xp_string_equal(base_name, xp_string_c("main")) || is_extern_c) {
            return base_name;
        }

        Package* pkg = sym ? sym->package : nullptr;
        return xp_string_concat_mid(
            pkg ? pkg->path : xp_string_c(""),
            base_name,
            xpOption<xpString>(xp_string_c(".")),
            permanent_allocator()
        );
    });
}


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
    LLVMBasicBlockRef entry;
};






struct LLVMBasicBlockMapper {
    LLVMBasicBlockMapper() = default;
    LLVMBasicBlockMapper(xpAllocator allocator, LLVMContextRef ctx, LLVMValueRef curr_func, bool create_exit_block = true);

    LLVMBasicBlockRef add_frag_blk(LLVMContextRef ctx, LLVMValueRef func, const char *name);
    LLVMBasicBlockRef first_frag_blk();
    LLVMBasicBlockRef last_frag_blk();
    LLVMBasicBlockRef exit_blk();
    void create_exit(LLVMContextRef ctx, LLVMValueRef func);
    LLVMBasicBlockRef frag_at(isize i);
    isize frag_count();

    LLVMValueRef owner_func;  // 该 mapper 所属的 LLVM 函数

private:
    Array<LLVMBasicBlockRef> fragments;
    LLVMBasicBlockRef exit_block;
};


LLVMBasicBlockMapper::LLVMBasicBlockMapper(xpAllocator allocator, LLVMContextRef ctx, LLVMValueRef curr_func, bool create_exit_block) {
    fragments = make_array<LLVMBasicBlockRef>(allocator);
    owner_func = curr_func;
    if(create_exit_block)
        exit_block = LLVMAppendBasicBlockInContext(ctx, curr_func, "block.exit");
    else
        exit_block = nullptr;
}


LLVMBasicBlockRef LLVMBasicBlockMapper::add_frag_blk(LLVMContextRef ctx, LLVMValueRef func, const char *name) {
    auto bb = LLVMAppendBasicBlockInContext(ctx, func, name);
    fragments.push_back(bb);
    return bb;
}

LLVMBasicBlockRef LLVMBasicBlockMapper::first_frag_blk() {
    XP_ASSERT_MSG(fragments.count > 0, "Remember to add at least one frag for block before getting first frag blk");
    return fragments[0];
}

LLVMBasicBlockRef LLVMBasicBlockMapper::last_frag_blk() {
    XP_ASSERT_MSG(fragments.count > 0, "Remember to add at least one frag for block before getting last frag blk");
    return fragments.back();
}

LLVMBasicBlockRef LLVMBasicBlockMapper::exit_blk() {
    return exit_block;
}

void LLVMBasicBlockMapper::create_exit(LLVMContextRef ctx, LLVMValueRef func) {
    if(!exit_block) {
        exit_block = LLVMAppendBasicBlockInContext(ctx, func, "block.exit");
    }
}

LLVMBasicBlockRef LLVMBasicBlockMapper::frag_at(isize i) {
    XP_ASSERT_MSG(i >= 0 && i < fragments.count, "frag_at index out of range");
    return fragments[i];
}

isize LLVMBasicBlockMapper::frag_count() {
    return fragments.count;
}


// LLVM IR 生成器, 保存生成一个Module所需的状态
// 目前一个Module就代表一个package
struct LLVMGenerator {

    void init(Package *pkg, xpAllocator allocator);
    void deinit();

    int size_of_type(TypeRef type);
    LLVMValueRef insert_alloca_before_last_inst_which_is_br(LLVMBasicBlockRef target_block, const char *var_name, LLVMTypeRef type);
    LLVMTypeRef get_llvm_type_from_type(TypeRef type);
    void gen_ir_function(CIRInstructionRef func_ref, CIRPackage *target_cir_pkg = nullptr);
    void gen_func_body(CIRInstResultRef key, LLVMValueRef llvm_func);

    void gen_ir_inst(CIRInstructionRef ref);
    void gen_ir_block_in_func_block(CIRBlockRef blk_ref, bool connect_to_parent = false);
    void gen_ir_loop(LLVMBasicBlockRef last_bb, LLVMBasicBlockRef first_bb, LLVMBasicBlockMapper& blk_mapper, CIRBlockRef parent_blk_ref);   // 循环收尾接线（回边 + break 目标 → 父 merge）
    void gen_ir_variable_decl(CIRInstructionRef ref, CIRInstruction* inst);
    void gen_ir_binary_expr(CIRInstructionRef inst);
    void gen_ir_unary(CIRInstructionRef inst);
    void llvm_build_br_when_no_br(LLVMBasicBlockRef from, LLVMBasicBlockRef to);
    xpString gen_ir_package(LLVMIRGenerateConfig config);

    // LLVMValueRef gen_ir_string_struct_value(xpString str);

    LLVMValueRef gen_ir_cast(TypeRef from_type, TypeRef to_type, LLVMValueRef value);
    LLVMValueRef gen_array_value_to_slice_cast(LLVMValueRef array_value_ptr, TypeRef array_value_type);

    LLVMValueRef get_ptr_of_llvm_value(LLVMValueRef value, bool allow_alloc_value_to_get_ptr = false);
    LLVMValueRef gen_llvm_val_by_value(Value& value, std::optional<TypeRef> expected_type = std::nullopt);

    LLVMValueRef get_llvm_val_from_inst_ref(CIRInstructionRef ref);
    void save_llvm_val_of_inst(CIRInstructionRef ref, LLVMValueRef llvm_val);

    LLVMValueRef compare_two_values(LLVMValueRef left, LLVMValueRef right, TypeRef type, bool is_not_equal);


    LLVMBasicBlockMapper& add_mapper_for_block(CIRBlockRef blk, bool create_exit = true);
    LLVMBasicBlockMapper* mapper(CIRBlockRef blk);
    LLVMBasicBlockMapper& get_or_create_mapper(CIRBlockRef blk);

    LLVMBasicBlockRef curr_bb();
public:
    LLVMContextRef ctx;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMTargetMachineRef target_machine;
    LLVMTargetDataRef target_data;


    Array<LLVMLoopBlocks> loop_stack;

    xpHashMap<TypeHashKey, LLVMTypeRef> struct_types;


    Package *pkg;


    IRSymbolTable syms;

    CIRFunctionDeclInfo curr_func_info;

    xpHashMap<CIRInstResultRef, LLVMValueRef> inst_vals;

    xpHashMap<CIRBlockRef, Array<LLVMBasicBlockMapper>> block_to_bbs;
    xpHashMap<isize, LLVMValueRef> string_globals;  // static_mem offset → @str_N

    LLVMState curr_state;

    CIRBlockRef curr_blk = INVALID_BLOCK;

    CIRResultContext result_ctx;
    CIRInstructionRef debug_curr_gen_ref = INVALID_INST;

};



void LLVMGenerator::init(Package *pkg, xpAllocator allocator) {
    ctx = LLVMContextCreate();
    module = LLVMModuleCreateWithNameInContext(pkg->path.c_str, ctx);
    builder = LLVMCreateBuilderInContext(ctx);

    LLVMTargetRef target;
    char *error = nullptr;
    if(LLVMGetTargetFromTriple(LLVMGetDefaultTargetTriple(), &target, &error)) {
        std::println(stderr, "Error getting target: {}", error);
        LLVMDisposeMessage(error);
        XP_ASSERT_DEFAULT(0);
    }

    target_machine = LLVMCreateTargetMachine(
        target,
        LLVMGetDefaultTargetTriple(),
        "x86-64",
        "",
        LLVMCodeGenLevelDefault,
        LLVMRelocPIC,
        LLVMCodeModelDefault
    );
    target_data = LLVMCreateTargetDataLayout(target_machine);

    loop_stack = make_array<LLVMLoopBlocks>(allocator);
    struct_types = xp_hash_map_make<TypeHashKey, LLVMTypeRef>(allocator);
    this->pkg = pkg;
    result_ctx = CIRResultContext::create(&this->pkg->cir_package);
    this->curr_state = {nullptr, nullptr};
    curr_blk = INVALID_BLOCK;
    syms = IRSymbolTable{
        .local_vals = xp_hash_map_make<SymbolInfo *, LLVMValueRef>(allocator)
    };
    inst_vals = xp_hash_map_make<CIRInstResultRef, LLVMValueRef>(allocator);
    block_to_bbs = xp_hash_map_make<CIRBlockRef, Array<LLVMBasicBlockMapper>>(allocator);
    string_globals = xp_hash_map_make<isize, LLVMValueRef>(allocator);
}


void LLVMGenerator::deinit() {
    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(module);
    LLVMContextDispose(ctx);
    LLVMDisposeTargetData(target_data);
    LLVMDisposeTargetMachine(target_machine);

    array_free(&loop_stack);
    xp_hash_map_free(struct_types);
    xp_hash_map_free(block_to_bbs);
}

















void init_llvm() {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeDisassembler();
}


LLVMBasicBlockMapper& LLVMGenerator::add_mapper_for_block(CIRBlockRef blk, bool create_exit) {
    LLVMBasicBlockMapper mapper = LLVMBasicBlockMapper(stage_allocator(), ctx, curr_state.curr_function, create_exit);
    auto* arr = xp_hash_map_get(block_to_bbs, blk);
    if(!arr) {
        auto new_arr = make_array<LLVMBasicBlockMapper>(permanent_allocator());
        new_arr.push_back(mapper);
        xp_hash_map_insert(&block_to_bbs, blk, new_arr);
        return xp_hash_map_get(block_to_bbs, blk)->back();
    }
    arr->push_back(mapper);
    return arr->back();
}


LLVMBasicBlockMapper* LLVMGenerator::mapper(CIRBlockRef blk) {
    auto arr = xp_hash_map_get(block_to_bbs, blk);
    if(!arr || arr->count == 0) return nullptr;
    // 返回最新添加的 mapper（对应当前 LLVM 函数）
    return &arr->back();
}

LLVMBasicBlockMapper& LLVMGenerator::get_or_create_mapper(CIRBlockRef blk) {
    auto* m = mapper(blk);
    // mapper() 返回最新 mapper；若属于当前函数则复用，否则新建
    if(m && m->owner_func == curr_state.curr_function) return *m;
    return add_mapper_for_block(blk);
}

LLVMBasicBlockRef LLVMGenerator::curr_bb() {
    return mapper(curr_blk)->last_frag_blk();
}



int LLVMGenerator::size_of_type(TypeRef type) {
    return (int)LLVMStoreSizeOfType(target_data, get_llvm_type_from_type(type));
}





LLVMValueRef LLVMGenerator::insert_alloca_before_last_inst_which_is_br(LLVMBasicBlockRef target_block, const char *var_name, LLVMTypeRef type) {
    LLVMBasicBlockRef curr_block = curr_bb();
    

    LLVMPositionBuilderAtEnd(builder,target_block);

    // 如果有末尾指令且还是分支指令，就把 builder 定位到它前面，保证新 alloca 在分支指令前面
    LLVMValueRef last_instr = LLVMGetLastInstruction(target_block);
    if (last_instr && LLVMIsABranchInst(last_instr)) {
        LLVMPositionBuilderBefore(builder, last_instr);
    }

    DEBUG_LOG("BuildAlloca: var={}, type_kind={}", var_name, (int)LLVMGetTypeKind(type));
    XP_ASSERT_DEFAULT(type != nullptr);
    LLVMValueRef alloca = LLVMBuildAlloca(builder, type, var_name);
    DEBUG_LOG("BuildAlloca OK");

    LLVMPositionBuilderAtEnd(builder,curr_block);

    return alloca;
}


void LLVMGenerator::llvm_build_br_when_no_br(LLVMBasicBlockRef from, LLVMBasicBlockRef to) {
    LLVMPositionBuilderAtEnd(builder, from);
    if(!LLVMGetBasicBlockTerminator(from)) {
        LLVMBuildBr(builder, to);
    }
    LLVMPositionBuilderAtEnd(builder, curr_bb());
}




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
        LLVMValueRef temp_alloca = insert_alloca_before_last_inst_which_is_br(curr_bb(), "temp", value_type);
        LLVMBuildStore(builder, value, temp_alloca);
        return temp_alloca;
    } else {
        XP_ASSERT_DEFAULT(0 && "Compiler Internal Error: Cannot get pointer of value, and not allowed to alloc value to get pointer");
    }

    return nullptr;
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
            
        case Type_isize:
        case Type_usize:
            return LLVMIntPtrTypeForASInContext(ctx, target_data, 0);

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
            
            
            Array<LLVMTypeRef> field_types = make_array_capacity<LLVMTypeRef>(stage_allocator(), type->struct_info.struct_fields.count);
            
            for(isize i = 0; i < type->struct_info.struct_fields.count; i++) {
                StructField field = type->struct_info.struct_fields[i];
                LLVMTypeRef field_llvm_type = get_llvm_type_from_type(field.type);
                field_types.push_back(field_llvm_type);
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
            Array<LLVMTypeRef> params = make_array_capacity<LLVMTypeRef>(stage_allocator(), type->function_info.param_types.count);


            isize fix_param_count = type->function_info.param_types.count - (is_var_arg_function(type) ? 1 : 0);
            for(isize i = 0; i < fix_param_count; i++) {
                params.push_back(get_llvm_type_from_type(type->function_info.param_types[i]));
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
            DEBUG_TRACE("get_llvm_type_from_type UNREACHABLE: type kind={}", (int)type->kind);
            std::unreachable();
    }

    return nullptr;
}


Array<xpString> gen_ir_all_packages(Array<Package>* all_packages, LLVMIRGenerateConfig config) {
    Array<xpString> obj_paths = make_array<xpString>(permanent_allocator());
    for(Package& pkg: *all_packages) {
        // 跳过空包（如全局 scope 容器 <global_blank_package>）：没有源码可生成，
        // 且其路径含 Windows 非法字符（<>），写 .ll 会 EINVAL 报错
        if(pkg.ast_files.count == 0) {
            continue;
        }
        LLVMGenerator gen;
        gen.init(&pkg, stage_allocator());
        defer(gen.deinit());
        obj_paths.push_back(gen.gen_ir_package(config));
    }

    return obj_paths;
}



xpString LLVMGenerator::gen_ir_package(LLVMIRGenerateConfig config) {
    // @new — 用迭代器遍历根 Block。每个指令占一个 slot，嵌套 Block（BlockRef/FunctionDecl）
    // 由 gen_ir_inst 内部递归生成，父级只需顺序推进迭代器。
    auto& root_blk = *pkg->cir_package.block(pkg->cir_package.top_blk);
    for(auto it = root_blk.insts.begin(); it != root_blk.insts.end(); ++it) {
        gen_ir_inst(CIRInstructionRef{pkg->cir_package.top_blk, it.subscript()});
    }



    //
    // 生成目标文件
    //


    // 输出 .ll 文件
    char *error = nullptr;
    std::string file_name = to_path(pkg->path).generic_string();
    std::replace(file_name.begin(), file_name.end(), '/', '_');
    std::replace(file_name.begin(), file_name.end(), ':', '_');

    std::filesystem::path &output_path = context()->output_path;

    std::filesystem::path obj_file_path = output_path / (file_name + ".o");
    std::filesystem::path ll_file_path = output_path / (file_name + ".ll");
    

    if(LLVMPrintModuleToFile(module, ll_file_path.generic_string().c_str(), &error)) {
        std::println(stderr, "Error writing .ll file: {}", error);
        LLVMDisposeMessage(error);
    }
    
    if(LLVMVerifyModule(module, LLVMReturnStatusAction, &error)) {
        std::println(stderr, "Module verification failed: {}", error);
        LLVMDisposeMessage(error);
        // XP_ASSERT_DEFAULT(0);
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

    


    // 确保目标文件夹存在
    std::filesystem::create_directories(obj_file_path.parent_path());

    // 生成.o文件
    if(LLVMTargetMachineEmitToFile(
        target_machine,
        module,
        obj_file_path.generic_string().c_str(),
        LLVMObjectFile,
        &error
    )) {
        std::println(stderr, "Error writing object file: {}", error);
        LLVMDisposeMessage(error);
    }
    
    std::string obj_file_path_str = obj_file_path.generic_string();
    xpString obj_file_path_str_permanent = xp_make_string_count(permanent_allocator(), obj_file_path_str.c_str(), obj_file_path_str.size());

    return obj_file_path_str_permanent;
}


LLVMValueRef LLVMGenerator::get_llvm_val_from_inst_ref(CIRInstructionRef ref) {
    DEBUG_TRACE("get_llvm_val_from_inst_ref ENTER: ref={} pkg={} call_inst={} gen_ref={}" , ref, (void*)result_ctx.pkg(), (void*)result_ctx.call_instance(), debug_curr_gen_ref);
    CIRInstResultRef key{result_ctx.pkg(), ref, result_ctx.call_instance() ? std::optional<CIRResultInstanceRef>(result_ctx.call_instance()) : std::nullopt};
    LLVMValueRef *val = xp_hash_map_get(inst_vals, key);
    if(val != nullptr) {
        return *val;
    }

    auto result = result_ctx.result_of(ref);
    if(result.state == CIRResultState::WholeValue) {
        Value v = result.actual_val();
        DEBUG_TRACE("get_llvm_val_from_inst_ref WholeValue: ref={}, op={}, v.type kind={}",
            ref, result_ctx.pkg()->inst(ref)->to_string(), (int)(v.type ? v.type->kind : -1));
        LLVMValueRef llvm_val;
        if(is_function_type(v.type)) {
            // 函数值：走原子入口（声明 + body 一体），inst_vals 命中即完整
            const auto& fk = v.func_val().func_key;
            auto *saved_ci = result_ctx.call_instance();
            if(fk.result_instance.has_value()) {
                result_ctx.enter_call_instance(fk.result_instance.value());
            }
            gen_ir_function(fk.inst_ref, fk.cir_package);
            if(fk.result_instance.has_value()) {
                result_ctx.exit_call();
                if(saved_ci) result_ctx.enter_call_instance(saved_ci);
            }
            llvm_val = *xp_hash_map_get(inst_vals, fk);
        } else {
            llvm_val = gen_llvm_val_by_value(v);
            save_llvm_val_of_inst(ref, llvm_val);
        }
        return llvm_val;
    }

    LLVMValueRef *cached = xp_hash_map_get(inst_vals, key);
    if(cached) return *cached;

    // 当前 call_instance 未命中 → 尝试 null call_instance
    if(result_ctx.call_instance() != nullptr) {
        CIRInstResultRef null_key{result_ctx.pkg(), ref, std::nullopt};
        LLVMValueRef *null_cached = xp_hash_map_get(inst_vals, null_key);
        if(null_cached) return *null_cached;
    }

    DEBUG_TRACE("get_llvm_val_from_inst_ref UNREACHABLE: ref={} op={} state={} call_instance={} curr_gen_ref={} curr_gen_op={}", ref, (int)result_ctx.pkg()->inst(ref)->op, (int)result.state, (void*)result_ctx.call_instance(), debug_curr_gen_ref, result_ctx.pkg()->inst(debug_curr_gen_ref) ? (int)result_ctx.pkg()->inst(debug_curr_gen_ref)->op : -1);
    std::unreachable();
    return nullptr;
}

void LLVMGenerator::save_llvm_val_of_inst(CIRInstructionRef ref, LLVMValueRef llvm_val) {
    CIRInstResultRef key{result_ctx.pkg(), ref, result_ctx.call_instance() ? std::optional<CIRResultInstanceRef>(result_ctx.call_instance()) : std::nullopt};
    DEBUG_TRACE("save_llvm_val_of_inst: ref={} pkg={} call_inst={} op={}", ref, (void*)result_ctx.pkg(), (void*)result_ctx.call_instance(), (int)result_ctx.pkg()->inst(ref)->op);
    xp_hash_map_insert(&inst_vals, key, llvm_val);
    // 同时以 null call_instance 保存，使不同调用上下文均能命中缓存
    if(result_ctx.call_instance() != nullptr) {
        CIRInstResultRef null_key{result_ctx.pkg(), ref, std::nullopt};
        xp_hash_map_insert(&inst_vals, null_key, llvm_val);
    }
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

    // bool ⇄ integer (i1)
    if(to_type == easy_type(Type_bool) && is_integer_or_untyped_type(from_type)) {
        return LLVMBuildTrunc(builder, value, LLVMInt1TypeInContext(ctx), "booltrunctmp");
    }
    if(from_type == easy_type(Type_bool) && is_integer_or_untyped_type(to_type)) {
        return LLVMBuildZExt(builder, value, get_llvm_type_from_type(to_type), "boolzexttmp");
    }
    if(from_type == easy_type(Type_bool) && to_type == easy_type(Type_bool)) {
        return value;
    }

    std::unreachable();

    return nullptr;
}




// LLVMValueRef LLVMGenerator::gen_ir_string_struct_value(xpString str) {
//     LLVMValueRef str_const = LLVMBuildGlobalString(
//         builder, 

//         // 这里是为了让字符串能由token里的带有""而无\0结尾的字符串转换而来, 使得llvm能正确识别, 
//         // 不把""当成字符串内容
//         xp_string_to_c_style(str, stage_allocator()).c_str,
//         "stringliteraltmp"
//     );

//     LLVMValueRef str_struct = LLVMGetUndef(get_llvm_type_from_type(string_type_as_struct()));
    
//     // TODO: 现在是硬编码
//     str_struct = LLVMBuildInsertValue(builder, str_struct, str_const, 0, "insertstrptrtmp");

//     str_struct = LLVMBuildInsertValue(builder, str_struct, LLVMConstInt(LLVMInt64TypeInContext(ctx), str.length, 0), 1, "insertstrlenntmp");

//     return str_struct;
// }


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
        case Type_isize:
        case Type_usize:
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
                element_values.push_back(gen_llvm_val_by_value(val));
            }

            llvm_val = LLVMConstArray2(get_llvm_type_from_type(value.type->array_info.element_type), element_values.data, element_values.count);
        } break;

        case Type_struct: {
            if(is_struct_type(value.type)) {
                Array<LLVMValueRef> field_values = make_array<LLVMValueRef>(stage_allocator());

                Array<Value> field_vals = value.struct_fields_val();
                for(isize i = 0; i < field_vals.count; i++) {
                    LLVMValueRef field_llvm_val = gen_llvm_val_by_value(field_vals[i]);
                    field_values.push_back(field_llvm_val);
                }

                llvm_val = LLVMConstNamedStruct(get_llvm_type_from_type(value.type), field_values.data, cast(unsigned)field_values.count);
            } else {
                std::unreachable();
            }

        } break;

        case Type_pointer: {
            if(value.is_null) {
                llvm_val = LLVMConstNull(get_llvm_type_from_type(value.type));
                break;
            }

            // TODO: HARDCODE, 这里的情况只处理字符串, 别的类型数据不能处理
            // 非空 comptime 指针：从 static_mem 读取字符串数据，创建 LLVM 全局
            if(value.actual_type() == ActualValueType::Pointer) {
                Pointer ptr = value.pointer_val();

                if(LLVMValueRef* cached = xp_hash_map_get(string_globals, ptr.offset); cached != nullptr) {
                    llvm_val = *cached;
                    break;
                }

                if(ptr.kind == MemoryKind::String) {
                    // 扫描 null 结尾确定长度
                    auto& bytes = ptr.mem->bytes;
                    isize max_len = bytes.count - ptr.offset;
                    isize str_len = 0;
                    for(isize i = 0; i < max_len; i++) {
                        if(bytes[ptr.offset + i] == 0) { str_len = i; break; }
                    }

                    xpAutoArenaRestore t{temp_allocator()};
                    char *str = static_cast<char *>(xp_alloc(temp_allocator(), str_len + 1));
                    ptr.load_bytes(0, str, str_len);
                    str[str_len] = '\0';

                    auto str_val = LLVMBuildGlobalString(builder, str, "strptr");
                    xp_hash_map_insert(&string_globals, ptr.offset, str_val);
                    llvm_val = str_val;
                    break;
                }
            }

            std::unreachable();
        } break;

        case Type_enum: {
            TypeRef actual_elem_type = value.type->enum_info.element_type;
            Value actual_elem_val = make_value(actual_elem_type);
            actual_elem_val.integer_val(value.integer_val());
            llvm_val = gen_llvm_val_by_value(actual_elem_val);
        } break;

        default: {
            // TODO 其他类型的常量
            std::unreachable();
        } break;
    }

    if(expected_type.has_value()) {
        return gen_ir_cast(value.type, expected_type.value(), llvm_val);
    } else {
        return llvm_val;
    }

}


void LLVMGenerator::gen_ir_function(CIRInstructionRef func_ref, CIRPackage *target_cir_pkg) {
    bool is_cross_pkg = (target_cir_pkg != nullptr && target_cir_pkg != &pkg->cir_package);
    if (target_cir_pkg == nullptr) target_cir_pkg = &pkg->cir_package;

    CIRInstruction *func_inst = target_cir_pkg->inst(func_ref);

    XP_ASSERT_DEFAULT(func_inst->op == CIROperator::FunctionDecl);
    auto& fd = func_inst->info<CIROperator::FunctionDecl>();

    auto *saved_ctx_pkg = result_ctx.pkg();
    result_ctx.set_pkg(target_cir_pkg);
    defer({
        result_ctx.set_pkg(saved_ctx_pkg);
    });

    if (is_pure_comptime_func(fd, result_ctx)) return;

    SymbolInfo *func_sym = (func_inst->symbol)();

    auto& res = result_ctx.result_of(func_ref);
    if (res.state != CIRResultState::WholeValue) return;
    CIRInstResultRef fk = res.actual_val().func_val().func_key;

    // 无状态：inst_vals 命中即已完整处理（声明 + body 原子完成）
    if (xp_hash_map_get(inst_vals, fk)) return;

    // 创建声明 + 落库（生成即稳定）
    LLVMTypeRef fn_type = get_llvm_type_from_type(res.actual_val().type);
    xpString func_full_name = register_func_name(fk, fd.is_extern_c);
    const char *c_name = xp_string_to_c_style(func_full_name, stage_allocator()).c_str;
    LLVMValueRef func = LLVMAddFunction(module, c_name, fn_type);
    xp_hash_map_insert(&inst_vals, fk, func);

    // extern C / builtin：只声明（无 body，值由外部符号提供）
    if (fd.is_extern_c || fd.is_builtin) return;

    // 跨包命名函数：只声明（body 由所属包生成），匿名函数即使跨包也生成（COMDAT 去重）
    if (is_cross_pkg && func_sym) return;

    gen_func_body(fk, func);
}

void LLVMGenerator::gen_func_body(CIRInstResultRef key, LLVMValueRef llvm_func) {
    auto* func_inst = key.cir_package->inst(key.inst_ref);
    auto& fd = func_inst->info<CIROperator::FunctionDecl>();
    SymbolInfo *func_sym = (func_inst->symbol)();

    // 上下文切到函数所在包/实例（生成完恢复）
    auto *saved_pkg = result_ctx.pkg();
    auto saved_call_instance = result_ctx.call_instance();
    result_ctx.set_pkg(key.cir_package);
    if (key.result_instance.has_value()) {
        result_ctx.enter_call_instance(key.result_instance.value());
    }
    defer({
        result_ctx.set_pkg(saved_pkg);
        if (saved_call_instance) {
            result_ctx.enter_call_instance(saved_call_instance);
        } else {
            result_ctx.exit_call();
        }
    });

    // 保存调用者生成状态（函数体生成完恢复）
    auto saved_state = curr_state;
    auto saved_func_info = curr_func_info;
    auto saved_blk = curr_blk;
    defer({
        curr_state = saved_state;
        curr_func_info = saved_func_info;
        curr_blk = saved_blk;
        if (curr_state.curr_function != nullptr) {
            LLVMPositionBuilderAtEnd(builder, curr_bb());
        }
    });

    add_local_val(&syms, func_sym, llvm_func);

    curr_state.curr_function = llvm_func;
    save_llvm_val_of_inst(key.inst_ref, llvm_func);
    CIRBlockRef body_blk_ref = result_ctx.pkg()->inst(fd.body_inst)->info<CIROperator::BlockRef>().block_ref;   // body_inst 是 handle（BlockRef 指令），经 inst() 取子块
    auto& body_mapper = add_mapper_for_block(body_blk_ref, false);
    LLVMBasicBlockRef entry = body_mapper.add_frag_blk(ctx, llvm_func, "entry");
    body_mapper.create_exit(ctx, llvm_func);
    LLVMPositionBuilderAtEnd(builder, entry);

    curr_state = {llvm_func, entry};
    curr_func_info = fd;

    curr_blk = body_blk_ref;
    gen_ir_block_in_func_block(body_blk_ref, true);

    {
        auto* mapper_ptr = mapper(body_blk_ref);
        LLVMBasicBlockRef merge_bb = mapper_ptr->last_frag_blk();
        LLVMPositionBuilderAtEnd(builder, merge_bb);
        if (!LLVMGetBasicBlockTerminator(merge_bb)) {
            LLVMBuildUnreachable(builder);
        }
    }

    // 匿名函数可能被多个模块引用，用 COMDAT (any) 去重，避免 COFF 弱外部 .default. 冲突
    if (!func_sym) {
        LLVMComdatRef comdat = LLVMGetOrInsertComdat(module, LLVMGetValueName(llvm_func));
        LLVMSetComdatSelectionKind(comdat, LLVMAnyComdatSelectionKind);
        LLVMSetComdat(llvm_func, comdat);
    }
}


void LLVMGenerator::gen_ir_block_in_func_block(CIRBlockRef blk_ref, bool connect_to_parent) {
    auto& block_info = *result_ctx.pkg()->block(blk_ref);
    bool is_loop = block_info.is_loop;

    // 不在函数内，或 comptime Block：只扫描 FunctionDecl 和 BlockRef
    if(curr_state.curr_function == nullptr || block_info.is_comptime) {
        for(auto it = block_info.insts.begin(); it != block_info.insts.end(); ++it) {
            CIRInstructionRef pc{blk_ref, it.subscript()};
            CIRInstruction *body_inst = result_ctx.pkg()->inst(pc);
            if(body_inst->op == CIROperator::FunctionDecl || body_inst->op == CIROperator::BlockRef) {
                gen_ir_inst(pc);
            }
        }
        return;
    }

    auto old_curr_blk = curr_blk;
    defer(curr_blk = old_curr_blk);

    LLVMBasicBlockRef parent_bb = curr_bb();

    auto& blk_mapper = get_or_create_mapper(blk_ref);
    auto first_bb = blk_mapper.add_frag_blk(ctx, curr_state.curr_function, is_loop ? "loop" : "block");

    // 入口接线：Block(connect_to_parent) 与 Loop（内建接线）都从 parent_bb 进入 first_bb
    if(connect_to_parent || is_loop) llvm_build_br_when_no_br(parent_bb, first_bb);

    LLVMPositionBuilderAtEnd(builder, first_bb);
    curr_blk = blk_ref;

    for(auto it = block_info.insts.begin(); it != block_info.insts.end(); ++it) {
        gen_ir_inst(CIRInstructionRef{blk_ref, it.subscript()});
    }

    LLVMBasicBlockRef last_bb = curr_bb();

    if(is_loop) {
        gen_ir_loop(last_bb, first_bb, blk_mapper, old_curr_blk);
    } else if(connect_to_parent) {
        llvm_build_br_when_no_br(last_bb, blk_mapper.exit_blk());
        auto& parent_mapper = get_or_create_mapper(old_curr_blk);
        auto merge_bb = parent_mapper.add_frag_blk(ctx, curr_state.curr_function, "block.merge");
        llvm_build_br_when_no_br(blk_mapper.exit_blk(), merge_bb);
        LLVMPositionBuilderAtEnd(builder, merge_bb);
    } else {
        llvm_build_br_when_no_br(last_bb, blk_mapper.exit_blk());
        LLVMPositionBuilderAtEnd(builder, parent_bb);
    }
}

// 循环收尾接线（相对独立）：回边到循环头 + exit_blk（break 目标）→ loop.cont → 父.merge
void LLVMGenerator::gen_ir_loop(LLVMBasicBlockRef last_bb, LLVMBasicBlockRef first_bb,
                                LLVMBasicBlockMapper& blk_mapper, CIRBlockRef parent_blk_ref) {
    // 回边 → 循环头
    llvm_build_br_when_no_br(last_bb, first_bb);

    // exit_blk（break 目标）→ loop.cont → 父.merge
    auto cont_bb = blk_mapper.add_frag_blk(ctx, curr_state.curr_function, "loop.cont");
    llvm_build_br_when_no_br(blk_mapper.exit_blk(), cont_bb);
    auto& parent_mapper = get_or_create_mapper(parent_blk_ref);
    auto loop_merge = parent_mapper.add_frag_blk(ctx, curr_state.curr_function, "loop.merge");
    LLVMPositionBuilderAtEnd(builder, cont_bb);
    LLVMBuildBr(builder, loop_merge);
    LLVMPositionBuilderAtEnd(builder, loop_merge);
}


void LLVMGenerator::gen_ir_inst(CIRInstructionRef ref) {
    debug_curr_gen_ref = ref;
    CIRInstruction *inst = result_ctx.pkg()->inst(ref);
    DEBUG_TRACE("gen_ir_inst ref=%{} op={}, loc= {}", ref, string(inst->op), inst->src_loc);

    auto op = inst->op;

    // FunctionDecl 始终生成（无论是否在函数内）
    // BlockRef 始终进入（内部根据是否在函数中决定行为）
    // 不在函数内 → 跳过（FunctionDecl 和 BlockRef 已在上面处理）
    if(op != CIROperator::FunctionDecl && op != CIROperator::BlockRef) {
        if(curr_state.curr_function == nullptr) {
            return;
        }

        // 在函数内但编译期已求值 → 跳过
        if(result_ctx.result_of(ref).state == CIRResultState::WholeValue) {
            return;
        }
    }

    switch(inst->op) {

        // FunctionDecl 和 BlockRef 在 switch 前已处理，这里仅消除编译警告
        case CIROperator::FunctionDecl: {
            if(inst->info<CIROperator::FunctionDecl>().is_extern_c || inst->info<CIROperator::FunctionDecl>().is_builtin) return;

            // 纯编译期函数（返回 type 或有 comptime 参数）→ 跳过，不生成 LLVM IR
            if(is_pure_comptime_func(inst->info<CIROperator::FunctionDecl>(), result_ctx)) {
                return;
            }

            gen_ir_function(ref);   // 原子入口：生成状态由 gen_func_body 管理
        } break;

        case CIROperator::BlockRef: {
            gen_ir_block_in_func_block(inst->info<CIROperator::BlockRef>().block_ref, true);
        } break;


        // 编译期指令，不生成IR
        case CIROperator::ConstDecl:
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
        case CIROperator::FieldTypeOfStruct:
        case CIROperator::FuncParamType:
        case CIROperator::TypeOfInstResult:
        case CIROperator::FuncType: {

        } break;
        
        
        
        
        // 这些与状态相关的指令, 需要更新状态, 但不生成IR
        case CIROperator::EnterScope:
            break;
        case CIROperator::ExitScope:
            break;
        
        
        // 不需要生成IR
        case CIROperator::ConstantValue:
        case CIROperator::StringLiteral:
            break;
        // 这些是运行时指令, 可能需要生成IR
        case CIROperator::StructInit: {
            auto &info = inst->info<CIROperator::StructInit>();
            TypeRef struct_type = result_ctx.result_of(info.struct_type_inst).actual_val().type_val();
            LLVMTypeRef llvm_struct_type = get_llvm_type_from_type(struct_type);
            LLVMValueRef struct_val = LLVMGetUndef(llvm_struct_type);

            for (isize i = 0; i < info.field_init_insts.count; i++) {
                LLVMValueRef field_val = get_llvm_val_from_inst_ref(info.field_init_insts[i]);
                struct_val = LLVMBuildInsertValue(builder, struct_val, field_val, (unsigned)i, "insertfieldtmp");
            }
            save_llvm_val_of_inst(ref, struct_val);
        } break;
        case CIROperator::ArrayInit: {
            auto &info = inst->info<CIROperator::ArrayInit>();
            TypeRef array_type = result_ctx.result_of(ref).actual_type();
            LLVMTypeRef llvm_array_type = get_llvm_type_from_type(array_type);
            LLVMValueRef array_val = LLVMGetUndef(llvm_array_type);

            for (isize i = 0; i < info.element_insts.count; i++) {
                LLVMValueRef elem_val = get_llvm_val_from_inst_ref(info.element_insts[i]);
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

        case CIROperator::AddrOf: {
            auto info = inst->info<CIROperator::AddrOf>();
            LLVMValueRef operand = get_llvm_val_from_inst_ref(info.lval_inst);
    
            LLVMValueRef result = get_ptr_of_llvm_value(operand);
            save_llvm_val_of_inst(ref, result);
        } break;

        case CIROperator::FieldAccess: {
            auto &info = inst->info<CIROperator::FieldAccess>();

            LLVMValueRef parent_val = get_llvm_val_from_inst_ref(info.parent_inst);
            auto &parent_res = result_ctx.result_of(info.parent_inst);
            TypeRef logical_type = parent_res.type();
            bool is_lval = parent_res.value_kind == CIRValueKind::LValue;

            // ① LValue → load 一层得到逻辑值
            if (is_lval) {
                parent_val = LLVMBuildLoad2(builder, get_llvm_type_from_type(logical_type), parent_val, "loadtmp");
            }

            // ② 如果逻辑类型是 *StructType → 再 load 得到结构体值
            TypeRef struct_type = nullptr;
            if (is_pointer_type(logical_type) && is_struct_type(logical_type->pointed_type)) {
                struct_type = logical_type->pointed_type;
                parent_val = LLVMBuildLoad2(builder, get_llvm_type_from_type(struct_type), parent_val, "loadtmp");
            } else {
                struct_type = logical_type;
            }
            XP_ASSERT_DEFAULT(is_struct_type(struct_type));

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
        } break;

        case CIROperator::FieldPtr: {
            auto &info = inst->info<CIROperator::FieldPtr>();
            LLVMValueRef struct_ptr = get_llvm_val_from_inst_ref(info.parent_inst);
            auto &parent_res = result_ctx.result_of(info.parent_inst);
            TypeRef actual_type = parent_res.actual_type();
            bool is_lval = parent_res.value_kind == CIRValueKind::LValue;

            // ① LValue of *StructType（actual=**StructType）→ load 出 *StructType 指针值
            if (is_lval && is_pointer_type(actual_type) && is_pointer_type(actual_type->pointed_type)) {
                struct_ptr = LLVMBuildLoad2(builder, get_llvm_type_from_type(actual_type), struct_ptr, "loaddblptr");
                actual_type = actual_type->pointed_type;
            }

            // ② 确定结构体类型并 GEP
            TypeRef struct_type = nullptr;
            if (is_pointer_type(actual_type) && is_struct_type(actual_type->pointed_type)) {
                struct_type = actual_type->pointed_type;
            } else if (is_lval && is_struct_type(actual_type)) {
                // LValue 但 actual_type 不是指针（actual_type 未按 *T 规范），
                // LLVM 值隐含是指向 actual_type 的指针
                struct_type = actual_type;
            }
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
            auto &info = inst->info<CIROperator::Index>();
            LLVMValueRef array_val = get_llvm_val_from_inst_ref(info.array_inst);
            LLVMValueRef index_val = get_llvm_val_from_inst_ref(info.index_inst);
            // LValue 操作数的 actual_type 是指针（*[]u8），类型分派用逻辑类型 type()
            TypeRef array_type = result_ctx.result_of(info.array_inst).type();

            LLVMValueRef indices[2] = { nullptr };
            LLVMValueRef elem_ptr = nullptr;
            bool array_is_ptr = LLVMGetTypeKind(LLVMTypeOf(array_val)) == LLVMPointerTypeKind;

            if(is_array_type(array_type)) {
                LLVMValueRef array_ptr = array_is_ptr ? array_val : get_ptr_of_llvm_value(array_val, true);
                indices[0] = LLVMConstInt(LLVMInt32TypeInContext(ctx), 0, 0);
                indices[1] = index_val;
                elem_ptr = LLVMBuildGEP2(builder, get_llvm_type_from_type(array_type), array_ptr, indices, 2, "arrayelemptrtmp");
            } else if(is_slice_struct_type(array_type)) {
                LLVMValueRef slice_val = array_is_ptr ? LLVMBuildLoad2(builder, get_llvm_type_from_type(array_type), array_val, "loadslicetmp") : array_val;
                LLVMValueRef data_raw = LLVMBuildExtractValue(builder, slice_val, 0, "slicedataptrtmp");
                TypeRef data_ptr_type = array_type->struct_info.struct_fields[0].type;
                LLVMTypeRef data_ptr_llvm_type = get_llvm_type_from_type(data_ptr_type);
                LLVMValueRef data_typed_ptr = LLVMBuildBitCast(builder, data_raw, data_ptr_llvm_type, "slicedatatypedptrtmp");
                indices[0] = index_val;
                elem_ptr = LLVMBuildGEP2(builder, get_llvm_type_from_type(data_ptr_type->pointed_type), data_typed_ptr, indices, 1, "sliceelemptrtmp");
            } else {
                std::unreachable();
            }

            TypeRef val_type = result_ctx.result_of(ref).actual_type();
            LLVMValueRef loaded = LLVMBuildLoad2(builder, get_llvm_type_from_type(val_type), elem_ptr, "loadindextmp");
            save_llvm_val_of_inst(ref, loaded);
        } break;

        case CIROperator::IndexPtr: {
            // 老代码:
            //     LLVMValueRef elem_ptr = ...GEP...;
            //     return elem_ptr;  // is_lvalue_expr 分支

            auto &info = inst->info<CIROperator::IndexPtr>();
            LLVMValueRef array_val = get_llvm_val_from_inst_ref(info.array_inst);
            LLVMValueRef index_val = get_llvm_val_from_inst_ref(info.index_inst);
            // LValue 操作数的 actual_type 是指针（*[]u8），类型分派用逻辑类型 type()
            TypeRef array_type = result_ctx.result_of(info.array_inst).type();

            LLVMValueRef indices[2] = { nullptr };
            LLVMValueRef elem_ptr = nullptr;
            bool array_is_ptr = LLVMGetTypeKind(LLVMTypeOf(array_val)) == LLVMPointerTypeKind;

            if(is_array_type(array_type)) {
                LLVMValueRef array_ptr = array_is_ptr ? array_val : get_ptr_of_llvm_value(array_val, true);
                indices[0] = LLVMConstInt(LLVMInt32TypeInContext(ctx), 0, 0);
                indices[1] = index_val;
                elem_ptr = LLVMBuildGEP2(builder, get_llvm_type_from_type(array_type), array_ptr, indices, 2, "arrayelemptrtmp");
            } else if(is_slice_struct_type(array_type)) {
                LLVMValueRef slice_val = array_is_ptr ? LLVMBuildLoad2(builder, get_llvm_type_from_type(array_type), array_val, "loadslicetmp") : array_val;
                LLVMValueRef data_raw = LLVMBuildExtractValue(builder, slice_val, 0, "slicedataptrtmp");
                TypeRef data_ptr_type = array_type->struct_info.struct_fields[0].type;
                LLVMTypeRef data_ptr_llvm_type = get_llvm_type_from_type(data_ptr_type);
                LLVMValueRef data_typed_ptr = LLVMBuildBitCast(builder, data_raw, data_ptr_llvm_type, "slicedatatypedptrtmp");
                indices[0] = index_val;
                elem_ptr = LLVMBuildGEP2(builder, get_llvm_type_from_type(data_ptr_type->pointed_type), data_typed_ptr, indices, 1, "sliceelemptrtmp");
            } else {
                std::unreachable();
            }

            save_llvm_val_of_inst(ref, elem_ptr);
        } break;

        case CIROperator::Call: {
            auto &info = inst->info<CIROperator::Call>();

            TypeRef called_type = result_ctx.result_of(info.called_thing).actual_type();
            TypeRef func_type = is_pointer_type(called_type) ? called_type->pointed_type : called_type;
            XP_ASSERT_DEFAULT(is_function_type(func_type));

            // 调用处确保被调函数存在（原子：声明 + body 一体，生成状态由 gen_func_body 管理）
            auto& callee_result = result_ctx.result_of(info.called_thing);
            if(callee_result.state == CIRResultState::WholeValue) {
                Value v = callee_result.actual_val();
                if(is_function_type(v.type)) {
                    auto fk = v.func_val().func_key;
                    auto *saved_ci = result_ctx.call_instance();
                    if(fk.result_instance.has_value()) {
                        result_ctx.enter_call_instance(fk.result_instance.value());
                    }
                    gen_ir_function(fk.inst_ref, fk.cir_package);
                    if(fk.result_instance.has_value()) {
                        result_ctx.exit_call();
                        if(saved_ci) result_ctx.enter_call_instance(saved_ci);
                    }
                }
            }

            LLVMTypeRef fn_type = get_llvm_type_from_type(func_type);

            LLVMValueRef callee = get_llvm_val_from_inst_ref(info.called_thing);


            Array<LLVMValueRef> args = make_array_capacity<LLVMValueRef>(stage_allocator(), info.arg_insts.count);
            for(isize i = 0; i < info.arg_insts.count; i++) {
                LLVMValueRef arg_val = get_llvm_val_from_inst_ref(info.arg_insts[i]);

                TypeRef arg_type = result_ctx.result_of(info.arg_insts[i]).actual_type();
                TypeRef param_type = i < func_type->function_info.param_types.count ? func_type->function_info.param_types[i] : nullptr;
                // TODO: 统一数组→切片隐式转换的调用点
                if(is_array_type(arg_type) && is_slice_struct_type(param_type)) {
                    arg_val = gen_ir_cast(arg_type, param_type, arg_val);
                }

                if(is_var_arg_function(func_type) && i >= get_fixed_param_count(func_type)) {
                    if(arg_type == easy_type(Type_f32)) {
                        arg_val = gen_ir_cast(arg_type, easy_type(Type_f64), arg_val);
                    } else if(
                       arg_type == easy_type(Type_i8) ||
                       arg_type == easy_type(Type_u8) ||
                       arg_type == easy_type(Type_bool)
                    ) {
                        arg_val = gen_ir_cast(arg_type, easy_type(Type_i32), arg_val);
                    }
                }

                args.push_back(arg_val);
            }

            TypeRef return_type = result_ctx.result_of(ref).actual_type();
            char const *name = (return_type && return_type->kind == Type_void) ? "" : "calltmp";
            LLVMValueRef result = LLVMBuildCall2(builder, fn_type, callee, args.data, (unsigned)args.count, name);
            save_llvm_val_of_inst(ref, result);
        } break;
        
        case CIROperator::Cast: {

            if(result_ctx.result_of(ref).state == CIRResultState::WholeValue) {
                return;
            }

            auto &info = inst->info<CIROperator::Cast>();

            LLVMValueRef expr_val = get_llvm_val_from_inst_ref(info.expr_inst);
            TypeRef from_type = result_ctx.result_of(info.expr_inst).actual_type();

            // TODO: 可以直接获取result.actual_type()也行, 不用现在那么麻烦
            TypeRef to_type = result_ctx.result_of(info.target_type_inst).actual_val().type_val();


            LLVMValueRef result = gen_ir_cast(from_type, to_type, expr_val);
            save_llvm_val_of_inst(ref, result);
        } break;

        case CIROperator::CondBr: {
            auto &info = inst->info<CIROperator::CondBr>();

            LLVMValueRef cond_val = get_llvm_val_from_inst_ref(info.condition_inst);

            // then/else 是独立 Block（各占一个 slot），自身接线由 connect_to_parent=false 处理
            gen_ir_block_in_func_block(info.true_block, false);
            gen_ir_block_in_func_block(info.false_block, false);

            auto* true_mapper = mapper(info.true_block);
            auto* false_mapper = mapper(info.false_block);

            LLVMBasicBlockRef true_bb = true_mapper->first_frag_blk();
            LLVMBasicBlockRef false_bb = false_mapper->first_frag_blk();

            LLVMBuildCondBr(builder, cond_val, true_bb, false_bb);

            auto& parent_mapper = get_or_create_mapper(curr_blk);
            auto merge_bb = parent_mapper.add_frag_blk(ctx, curr_state.curr_function, "condbr.merge");

            LLVMBasicBlockRef true_exit = true_mapper->exit_blk();
            llvm_build_br_when_no_br(true_exit, merge_bb);

            LLVMBasicBlockRef false_exit = false_mapper->exit_blk();
            llvm_build_br_when_no_br(false_exit, merge_bb);

            LLVMPositionBuilderAtEnd(builder,merge_bb);
        } break;

        case CIROperator::Break: {
            auto &info = inst->info<CIROperator::Break>();

            auto target_block = info.break_block;
            auto break_val_ref = info.break_value_inst;

            if(target_block == curr_func_info.body_inst) {
                // 函数 return：直接生成 ret
                if(break_val_ref != INVALID_INST) {
                    LLVMValueRef ret_val = get_llvm_val_from_inst_ref(break_val_ref);
                    LLVMBuildRet(builder, ret_val);
                } else {
                    LLVMBuildRetVoid(builder);
                }
            } else {
                // block break：保存值到目标 block，跳转到 exit_block
                auto target_blk_ref = result_ctx.pkg()->inst(target_block)->info<CIROperator::BlockRef>().block_ref;   // target_block 是 handle（BlockRef 指令），经 inst() 取子块
                auto target_block_mapper = mapper(target_blk_ref);
                LLVMBasicBlockRef target_block_exit = target_block_mapper->exit_blk();

                if(break_val_ref != INVALID_INST) {
                    if(result_ctx.result_of(target_block).state != CIRResultState::WholeValue) {
                        LLVMValueRef break_val = get_llvm_val_from_inst_ref(break_val_ref);
                        save_llvm_val_of_inst(info.break_block, break_val);
                    }
                }
                LLVMBuildBr(builder, target_block_exit);
            }

            // ret 和 br 都是 terminator，后续指令不可达：统一创建新 fragment block 承接死代码
            auto curr_mapper = mapper(curr_blk);
            auto new_frag_blk = curr_mapper->add_frag_blk(ctx, curr_state.curr_function, "after_break");
            LLVMPositionBuilderAtEnd(builder, new_frag_blk);
        } break;

        case CIROperator::Load: {
            auto &info = inst->info<CIROperator::Load>();
            LLVMValueRef ptr = get_llvm_val_from_inst_ref(info.ptr_inst);
            auto& ptr_result = result_ctx.result_of(info.ptr_inst);

            // ^func_ptr: 函数指针无法在 LLVM 层解引用（opaque pointer），直接透传
            if(ptr_result.value_kind == CIRValueKind::RValue &&
               is_pointer_type(ptr_result.actual_type()) &&
               is_function_type(ptr_result.actual_type()->pointed_type)) {
                save_llvm_val_of_inst(ref, ptr);
            } else {
                auto &load_result = result_ctx.result_of(ref);
                if(load_result.state == CIRResultState::NothingYet) {
                    DEBUG_TRACE("Load NothingYet: ref={} ptr_inst={}", ref, info.ptr_inst);
                }
                TypeRef val_type = load_result.actual_type();
                LLVMValueRef loaded = LLVMBuildLoad2(builder, get_llvm_type_from_type(val_type), ptr, "loadtmp");
                save_llvm_val_of_inst(ref, loaded);
            }
        } break;

        case CIROperator::Deref: {
            auto &info = inst->info<CIROperator::Deref>();
            LLVMValueRef ptr = get_llvm_val_from_inst_ref(info.operand_inst);
            // 从 alloca 加载完整的指针值（*T），作为 Store 的目标地址
            auto result = result_ctx.result_of(info.operand_inst);
            TypeRef stored_type = result.actual_type();
            TypeRef pointed_type = stored_type->pointed_type;
            LLVMValueRef addr = LLVMBuildLoad2(builder, get_llvm_type_from_type(pointed_type), ptr, "derefptr");
            save_llvm_val_of_inst(ref, addr);
        } break;

        case CIROperator::Store: {
            auto &info = inst->info<CIROperator::Store>();

            // 泛型运行时调用：comptime 结果是 type value，实际运行时值由 LLVM 单态化提供
            auto& val_result = result_ctx.result_of(info.value_inst);
            if(val_result.state == CIRResultState::WholeValue) {
                Value v = val_result.actual_val();
                if(v.type == type_type()) {
                    break;  // 跳过 comptime Store，由单态化生成
                }
            }

            LLVMValueRef ptr = get_llvm_val_from_inst_ref(info.var_inst);
            LLVMValueRef val = get_llvm_val_from_inst_ref(info.value_inst);

            TypeRef from_type = result_ctx.result_of(info.value_inst).actual_type();
            TypeRef to_type   = result_ctx.result_of(info.var_inst).actual_type();

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

            SymbolInfo *sym = (inst->symbol)();
            XP_ASSERT_DEFAULT(sym != nullptr);
            LLVMValueRef alloca = look_up_local_vals(&syms, sym);
            XP_ASSERT_DEFAULT(alloca != nullptr);

            save_llvm_val_of_inst(ref, alloca);
        } break;


        case CIROperator::IdentVal: {
            SymbolInfo *sym = (inst->symbol)();
            XP_ASSERT_DEFAULT(sym != nullptr);

            auto r = sym->result({});
            if(r.state >= CIRResultState::OnlyType && is_function_type(r.type())) {
                LLVMValueRef func_val = look_up_local_vals(&syms, sym);
                XP_ASSERT_DEFAULT(func_val != nullptr);
                save_llvm_val_of_inst(ref, func_val);
            }
        } break;


    }
}


void LLVMGenerator::gen_ir_variable_decl(CIRInstructionRef ref, CIRInstruction* inst) {
    XP_ASSERT_DEFAULT(inst->op == CIROperator::VariableDecl);

    SymbolInfo *var_info = (inst->info<CIROperator::VariableDecl>().symbol)();

    // 1. 分配空间
    LLVMValueRef alloca = insert_alloca_before_last_inst_which_is_br(curr_state.entry, xp_string_to_c_style(inst->info<CIROperator::VariableDecl>().name, stage_allocator()).c_str, get_llvm_type_from_type(result_ctx.result_of(ref).actual_type()));


    // 2. 如果有初始值，生成初始值 IR 并存储

    if(inst->info<CIROperator::VariableDecl>().no_zero_init) {
        // 无初始化, 不做处理

        // 无事发生
    } else {
        // 零初始化

        LLVMTypeRef var_type = get_llvm_type_from_type(result_ctx.result_of(ref).actual_type());
        LLVMValueRef zero_value = LLVMConstNull(var_type);
        LLVMBuildStore(builder, zero_value, alloca);
    }


    auto arg_slot_count = curr_func_info.arg_decl_insts.count;

    // 如果这是函数的参数 (slot >= 0), 则应把函数入参存入该 alloca 而不是保持零初始化。
    // Alloc_Var 在为函数参数分配时会给出 slot (0,1,2...)。此处尝试从当前函数中读取对应参数。
    if(inst->info<CIROperator::VariableDecl>().slot < arg_slot_count && curr_state.curr_function != nullptr) {
        unsigned param_idx = (unsigned)inst->info<CIROperator::VariableDecl>().slot;
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
    if(result_ctx.result_of(inst).state == CIRResultState::WholeValue) {
        return;
    }

    auto binary_info = result_ctx.pkg()->inst(inst)->info<CIROperator::Binary>();

    TokenType op = binary_info.op;
    CIRInstructionRef left_inst = binary_info.left_inst;
    CIRInstructionRef right_inst = binary_info.right_inst;


    LLVMValueRef left = get_llvm_val_from_inst_ref(left_inst);
    LLVMValueRef right = get_llvm_val_from_inst_ref(right_inst);

    TypeRef left_type = result_ctx.result_of(left_inst).actual_type();
    TypeRef right_type = result_ctx.result_of(right_inst).actual_type();

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
            std::unreachable();
        } break;
    }

    save_llvm_val_of_inst(inst, result);
}

void LLVMGenerator::gen_ir_unary(CIRInstructionRef inst) {
     // 如果结果已经在编译期求值得到了, 就不用生成指令了
    if(result_ctx.result_of(inst).state == CIRResultState::WholeValue) {
        return;
    }

    auto unary_info = result_ctx.pkg()->inst(inst)->info<CIROperator::Unary>();

    auto op = unary_info.op;
    LLVMValueRef operand = get_llvm_val_from_inst_ref(unary_info.operand_inst);
    TypeRef operand_type = result_ctx.result_of(unary_info.operand_inst).actual_type();
    LLVMTypeRef llvm_operand_type = get_llvm_type_from_type(operand_type);

    LLVMValueRef result;

    // 一元表达式（如负号）
    if (op == TokenType::Minus) {
        if(is_float_type(operand_type)) {
            LLVMValueRef zero = LLVMConstReal(llvm_operand_type, 0.0);
            result = LLVMBuildFSub(builder, zero, operand, "fnegtmp");
        } else {
            result = LLVMBuildNeg(builder, operand, "negtmp");
        }
    } else if(op == TokenType::Exclamation) {
        if(is_float_type(operand_type)) {
            DEBUG_PANIC("不支持对浮点数使用逻辑非运算符");
        }

        result = LLVMBuildNot(builder, operand, "nottmp");
    }
    
  

    save_llvm_val_of_inst(inst, result); 
}


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