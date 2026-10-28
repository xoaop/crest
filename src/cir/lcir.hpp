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

// extern_C 用裸符号名，跨包同名就是同一个 C 符号。
// Crest 一次编译所有包，所以在链接前统一检查：同名最多一个定义，且所有
// 声明/定义的签名必须一致。否则链接器只会报重复定义，签名不同时更是静默 UB。
void check_extern_c_symbols();

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
