#ifndef CREST_LLVM_BACKEND_INTERNAL_LLVM_GENERATOR_HPP
#define CREST_LLVM_BACKEND_INTERNAL_LLVM_GENERATOR_HPP

// 内部实现头文件：LLVM 后端的结构体/类定义。
// 仅供 src/llvm_backend/ 下的 .cpp 使用，其他模块不要 include。

#include "llvm_generate_ir.hpp"      // LLVMIROptimizationLevel / LLVMIRGenerateConfig

#include "xoaop.h"
#include "array.hpp"
#include "type.hpp"                  // TypeRef / TypeHashKey
#include "cir_instruction_ref.hpp"   // CIRInstructionRef / CIRBlockRef / CIRInstResultRef
#include "cir_inst.hpp"              // CIRFunctionDeclInfo / CIRInstruction
#include "cir_package.hpp"           // CIRResultContext

#include "llvm-c/Core.h"
#include "llvm-c/Comdat.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Analysis.h"
#include "llvm-c/BitWriter.h"
#include "llvm-c/Transforms/PassBuilder.h"
#include "llvm-c/Linker.h"


struct SymbolInfo;
struct Package;
struct Value;
struct CIRPackage;


struct IRSymbolTable {
    xpHashMap<SymbolInfo *, LLVMValueRef> local_vals;
};


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

#endif // CREST_LLVM_BACKEND_INTERNAL_LLVM_GENERATOR_HPP
