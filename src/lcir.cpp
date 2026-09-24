#include "lcir.hpp"

#include "symbol.hpp"     // SymbolInfo, try_access_val
#include "cir_inst.hpp"   // CIROperator, CIRFunctionDeclInfo

namespace lcir {


static xpHashMap<Ref<CIRInstResult>, xpString>& name_cache() {
    static xpHashMap<Ref<CIRInstResult>, xpString> map = xp_hash_map_make<Ref<CIRInstResult>, xpString>(permanent_allocator());
    return map;
}


xpString mangle_name(Ref<CIRInstResult> key, std::optional<xpString> base_name_opt, bool is_extern_c, Ref<Package> package) {
    auto& map = name_cache();

    return map.get_or_insert(key, [&]{
        // const auto& inst = key.cir_package->inst(key.inst_ref);
        // ASSERT(inst.op == CIROperator::FunctionDecl);

        // SymbolInfo* sym = try_access_val(inst.symbol);
        xpString base_name = base_name_opt.value_or([]{
            static isize anon_counter = 0;
            xpString name = xp_string_copy(permanent_allocator(), xp_string_c("__anon_"));
            xp_string_append(&name, xp_isize_to_string(anon_counter++, permanent_allocator()));
            return name;
        }());

        // TODO: 不急
        // $T 实例：名字里带上形参类型。否则各实例同名，只靠 LLVMAddFunction 自动加的
        // .1/.2 后缀区分 —— 那个编号跨模块不保证一致，COMDAT 会把不同签名误当同名去重
        // if(key.result_instance != Ref<CIRResultInstance>::INVALID_REF
        //    && inst.info<CIROperator::FunctionDecl>().has_generic_param_type
        //    && func_type != nullptr && is_function_type(func_type)) {
        //     base_name = xp_string_copy(permanent_allocator(), base_name);
        //     for(isize i = 0; i < func_type->function_info.param_types.count; i++) {
        //         xp_string_append(&base_name, xp_string_c("$"));
        //         xp_string_append(&base_name, func_type->function_info.param_types[i]->t_name());
        //     }
        // }

        if(xp_string_equal(base_name, xp_string_c("main")) || is_extern_c) {
            return base_name;
        }

        return xp_string_concat_mid(
            package != Ref<Package>::INVALID_REF ? package->path : xp_string_c(""),
            base_name,
            xpOption<xpString>(xp_string_c(".")),
            permanent_allocator()
        );
    });
}


};
