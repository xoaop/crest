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
    CIRInstResultRef decl_result;
};


struct GlobalVariableDecl {
    xpString raw_name;
    CIRInstResultRef init_result;   // 初值指令的结果（编译期常量）
    CIRInstResultRef decl_result;
};


struct Module {

    static Module init(xpAllocator allocator) {
        auto m = Module {
            .functions = xp_hash_map_make<CIRInstResultRef, FunctionDecl>(allocator),
            .globals = xp_hash_map_make<CIRInstResultRef, GlobalVariableDecl>(allocator),
        };

        return m;
    }

    xpHashMap<CIRInstResultRef, FunctionDecl> functions;   // func_key → 条目，插入即去重
    xpHashMap<CIRInstResultRef, GlobalVariableDecl> globals;   // 顶层变量 → 条目（文件作用域，包内 internal）
};


xpString mangle_name(CIRInstResultRef key, std::optional<xpString> base_name_opt, bool is_extern_c, PackageRef package);

template<class F>
void within_instance(CIRResultContext& ctx, const FunctionDecl& fd, F&& body) {
    auto saved_ci = ctx.call_instance();

    const auto result_instance = fd.decl_result.result_instance;
    if(result_instance != CIRResultInstanceRef::INVALID_REF) {
        ctx.enter_call_instance(result_instance);
    }

    body();

    if(saved_ci != CIRResultInstanceRef::INVALID_REF) {
        ctx.enter_call_instance(saved_ci);
    } else {
        ctx.exit_call();
    }
}


};
