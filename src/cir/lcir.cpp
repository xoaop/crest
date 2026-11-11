#include "lcir.hpp"

#include "symbol.hpp"
#include "cir_inst.hpp"
#include "context.hpp"

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

        // SymbolInfo* sym = inst.symbol.try_get();
        xpString base_name = base_name_opt.value_or([]{
            static isize anon_counter = 0;
            xpString name = xp_string_copy(permanent_allocator(), xp_string_c("__anon_"));
            xp_string_append(&name, xp_isize_to_string(anon_counter++, permanent_allocator()));
            return name;
        }());

        if(StringRef::init(base_name) == get_global_string(context()->known_global_strings.main) || is_extern_c) {
            return base_name;
        }

        // TODO: EXAM
        // 泛型实例：名字里带实例下标。否则各实例同名，只靠 LLVMAddFunction 自动加的
        // .1/.2 后缀区分 —— 那个编号跨模块按插入顺序生成，不保证一致
        if(key.result_instance != Ref<CIRResultInstance>::INVALID_REF) {
            xpString uniq = xp_string_copy(permanent_allocator(), base_name);
            xp_string_append(&uniq, xp_string_c("$"));
            xp_string_append(&uniq, xp_isize_to_string(key.result_instance.index, permanent_allocator()));
            base_name = uniq;
        }

        return xp_string_concat_mid(
            package != Ref<Package>::INVALID_REF ? to_xp_str(get_global_string(package->path), permanent_allocator()) : xp_string_c(""),
            base_name,
            xpOption<xpString>(xp_string_c(".")),
            permanent_allocator()
        );
    });
}


};
