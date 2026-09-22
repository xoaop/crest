#pragma once

#include "array.hpp"
#include "cir_instruction_ref.hpp"   // CIRInstructionRef, Ref<CIRResultInstance>, Ref<CIRInstResult>
#include "cir_package.hpp"           // CIRResultContext, CIRPackage
#include "package.hpp"               // Ref<Package>, Package
#include "type.hpp"                  // TypeRef

namespace lcir {


struct FunctionDecl {
    enum class Linkage {
        PureDeclaration,     // 纯声明（无定义）
        Definition,          // 定义（有函数体）
        MergableDefinition,  // 可合并定义（COMDAT）
    } linkage;

    xpString name;

    CIRInstructionRef decl_inst;

    Ref<CIRResultInstance> result_instance;
};


struct Module {

    static Module init(xpAllocator allocator) {
        auto m = Module {
            .functions = make_array<FunctionDecl>(allocator),
        };

        return m;
    }


    Ref<Package> package;

    Array<FunctionDecl> functions;
};


FunctionDecl::Linkage classify_linkage(bool is_extern_c, bool is_builtin, bool has_symbol, bool is_instance);

xpString mangle_name(Ref<CIRInstResult> func_key, bool is_extern_c, TypeRef signature);

template<class F>
void within_instance(CIRResultContext& ctx, const FunctionDecl& fd, F&& body) {
    auto saved_ci = ctx.call_instance();

    if(fd.result_instance != Ref<CIRResultInstance>::INVALID_REF) {
        ctx.enter_call_instance(fd.result_instance);
    }

    body();

    if(saved_ci != Ref<CIRResultInstance>::INVALID_REF) {
        ctx.enter_call_instance(saved_ci);
    } else {
        ctx.exit_call();
    }
}


};
