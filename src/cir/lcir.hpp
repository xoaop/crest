#pragma once

#include "array.hpp"
#include "cir_instruction_ref.hpp"
#include "cir_package.hpp"
#include "package.hpp"
#include "type.hpp"

namespace lcir {


struct FunctionDecl {
    enum class Linkage {
        PureDeclaration = 0,     // 纯声明（无定义）
        Definition,          // 定义（有函数体）
        MergableDefinition,  // 可合并定义（COMDAT）
        InternalDefinition,  // 内部链接定义（文件作用域，符号不出包）
    } linkage;

    xpString raw_name;
    Ref<CIRInstResult> decl_result;
};


struct GlobalVariableDecl {
    xpString raw_name;
    Ref<CIRInstResult> init_result;   // 初值指令的结果（编译期常量）
    Ref<CIRInstResult> decl_result;
};


struct Module {

    static Module init(xpAllocator allocator) {
        auto m = Module {
            .functions = xp_hash_map_make<Ref<CIRInstResult>, FunctionDecl>(allocator),
            .globals = xp_hash_map_make<Ref<CIRInstResult>, GlobalVariableDecl>(allocator),
        };

        return m;
    }

    xpHashMap<Ref<CIRInstResult>, FunctionDecl> functions;   // func_key → 条目，插入即去重
    xpHashMap<Ref<CIRInstResult>, GlobalVariableDecl> globals;   // 顶层变量 → 条目（文件作用域，包内 internal）
};


xpString mangle_name(Ref<CIRInstResult> key, std::optional<xpString> base_name_opt, bool is_extern_c, Ref<Package> package);

template<class F>
void within_instance(CIRResultContext& ctx, const FunctionDecl& fd, F&& body) {
    auto saved_ci = ctx.call_instance();

    const auto result_instance = fd.decl_result.result_instance;
    if(result_instance != Ref<CIRResultInstance>::INVALID_REF) {
        ctx.enter_call_instance(result_instance);
    }

    body();

    if(saved_ci != Ref<CIRResultInstance>::INVALID_REF) {
        ctx.enter_call_instance(saved_ci);
    } else {
        ctx.exit_call();
    }
}


};
