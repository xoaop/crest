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

        if(StringRef::init(base_name) == get_global_string(context()->known_global_strings.main) || is_extern_c) {
            return base_name;
        }

        return xp_string_concat_mid(
            package != Ref<Package>::INVALID_REF ? to_xp_str(get_global_string(package->path), permanent_allocator()) : xp_string_c(""),
            base_name,
            xpOption<xpString>(xp_string_c(".")),
            permanent_allocator()
        );
    });
}


void check_extern_c_symbols() {
    struct Entry {
        TypeRef type;
        bool is_definition;
        Ref<Package> owner;
    };

    xpHashMap<xpString, Entry> seen = xp_hash_map_make<xpString, Entry>(permanent_allocator());

    for(auto&& mod_entry : context()->lcir_modules) {
        Ref<Package> pkg = mod_entry.key;

        for(auto&& fn_entry : mod_entry.value.functions) {
            Ref<CIRInstResult> fk = fn_entry.key;
            const FunctionDecl& fd = fn_entry.value;

            // 调用方侧为跨包引用补的声明：fk 属于被调包，签名必然与 owner 一致
            if(fk.cir_package->package_ref != pkg) {
                continue;
            }

            auto& inst = fk.cir_package->inst(fk.inst_ref);
            if(!inst.info<CIROperator::FunctionDecl>().is_extern_c) {
                continue;
            }

            TypeRef sig = fk.cir_package->result_of(fk.inst_ref, fk.result_instance).actual_val().type;
            const bool is_definition = fd.linkage != FunctionDecl::Linkage::PureDeclaration;

            Entry* prev = xp_hash_map_get(seen, fd.raw_name);
            if(prev == nullptr) {
                xp_hash_map_insert(&seen, fd.raw_name, Entry { sig, is_definition, pkg });
                continue;
            }

            if(!is_equal_type(*prev->type, *sig)) {
                context()->reporter.report_error(
                    inst.src_loc,
                    "extern \"C\" 符号 '{}' 的签名与另一处不一致：'{}' 与 '{}'",
                    fd.raw_name, prev->type->name(), sig->name()
                );
            } else if(prev->is_definition && is_definition) {
                context()->reporter.report_error(
                    inst.src_loc,
                    "extern \"C\" 符号 '{}' 重复定义（另一处在包 '{}'）",
                    fd.raw_name, get_global_string(prev->owner.unwrap().path)
                );
            }

            // 后来的定义替换声明作为代表，后续比对都以它的签名为准
            if(is_definition) {
                prev->is_definition = true;
                prev->owner = pkg;
            }
        }
    }
}


};
