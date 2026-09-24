#pragma once

#include "array.hpp"
#include "cir_instruction_ref.hpp"   // CIRInstructionRef, Ref<CIRResultInstance>, Ref<CIRInstResult>
#include "cir_package.hpp"           // CIRResultContext, CIRPackage
#include "package.hpp"               // Ref<Package>, Package
#include "type.hpp"                  // TypeRef

namespace lcir {


struct FunctionDecl {
    enum class Linkage {
        PureDeclaration = 0,     // 纯声明（无定义）
        Definition,          // 定义（有函数体）
        MergableDefinition,  // 可合并定义（COMDAT）
    } linkage;

    xpString raw_name;
    Ref<CIRInstResult> decl_result;
};


struct Module {

    static Module init(xpAllocator allocator) {
        auto m = Module {
            .functions = xp_hash_map_make<Ref<CIRInstResult>, FunctionDecl>(allocator),
        };

        return m;
    }

    xpHashMap<Ref<CIRInstResult>, FunctionDecl> functions;   // func_key → 条目，插入即去重
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
