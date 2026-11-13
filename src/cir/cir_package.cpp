#include "cir_package.hpp"
#include "common.hpp"
#include "context.hpp"
#include "error_msg.hpp"

#include "print.hpp"
#include "tokenizer.hpp"
#include "type.hpp"


CIRPackage make_cir_package(xpAllocator allocator) {
    CIRPackage cir_package = {};

    cir_package.blocks = make_array<CIRBlock>(allocator);

    cir_package.string_literals = make_array<GlobalStringIndex>(allocator);
    cir_package.result_instances = make_array<CIRResultInstance>(allocator);
    cir_package.result_instances.push_back(CIRResultInstance::init_root(allocator));
    cir_package.result_instance_map = xp_hash_map_make<FuncCallKey, Ref<CIRResultInstance>>(allocator);
    return cir_package;
}


CIRInstruction& CIRPackage::inst_mut(CIRInstructionRef ref) {
    ASSERT((ref.inst_index != INVALID_INST_INDEX && ref.block_ref != INVALID_BLOCK) || ref.block_ref != INVALID_BLOCK);   // ref 恒为真实指令（INVALID_INST={-1,-1} 除外）
    return blocks[ref.block_ref].insts[ref.inst_index];
}

const CIRInstruction& CIRPackage::inst(CIRInstructionRef ref) const {
    ASSERT((ref.inst_index != INVALID_INST_INDEX && ref.block_ref != INVALID_BLOCK) || ref.block_ref != INVALID_BLOCK);
    return blocks[ref.block_ref].insts[ref.inst_index];
}

CIRBlock& CIRPackage::block_mut(CIRBlockRef ref) {
    return blocks[ref];
}

const CIRBlock& CIRPackage::block(CIRBlockRef ref) const {
    return blocks[ref];
}

CIRBlockRef CIRPackage::create_block(bool is_comptime, bool immediate_eval, bool is_loop, bool yields_value) {
    CIRBlock blk = {};
    blk.insts = StableOrderedArray<CIRInstruction>::make(permanent_allocator());
    blk.is_comptime = is_comptime;
    blk.immediate_eval = immediate_eval;
    blk.is_loop = is_loop;
    blk.yields_value = yields_value;
    CIRBlockRef ref = blocks.count;
    blk.self = ref;
    blk.cir_package = self_ref;
    blocks.push_back(blk);
    return ref;
}

Ref<CIRResultInstance> CIRPackage::root_instance() const {
    Ref<CIRResultInstance> r;
    r.cir_package = self_ref;
    r.index = 0;
    return r;
}


Ref<CIRResultInstance> CIRPackage::get_or_init_result_instance(FuncCallKey key, Ref<CIRResultInstance> parent) {
    Ref<CIRResultInstance> *existing = xp_hash_map_get(result_instance_map, key);
    if(existing != nullptr) {
        return *existing;
    }
    result_instances.push_back(CIRResultInstance::init(permanent_allocator(), parent));

    Ref<CIRResultInstance> r;
    r.cir_package = self_ref;
    r.index = (isize)result_instances.count - 1;
    result_instance_map.insert(key, r);
    return r;
}


CIRInstResult& CIRPackage::result_of(CIRInstructionRef ref, Ref<CIRResultInstance> instance) {
    if(instance != Ref<CIRResultInstance>::INVALID_REF) {
        return instance->result_of_or(ref, [&]{ return root_instance()->result_of(ref); });
    }


    return root_instance()->result_of(ref);
}


const CIRInstResult* CIRPackage::find_result(CIRInstructionRef ref, Ref<CIRResultInstance> instance) const {
    if(instance != Ref<CIRResultInstance>::INVALID_REF) {
        return instance->find_along_chain(ref);
    }
    return root_instance()->find_along_chain(ref);
}


u64 FuncCallKey::hash() const {

    // TODO: AI SLOP
    u64 h = xp_hash_combine_u64((u64)std::hash<Ref<CIRPackage>>{}(func_decl_pc.pkg), (u64)func_decl_pc.block_ref);
    h = xp_hash_combine_u64(h, (u64)func_decl_pc.inst_index);
    h = xp_hash_combine_u64(h, (u64)is_generic_instance);

    // $T 实例: 按实参结果的类型去重(TypeRef 是 interned 指针), 与指令身份无关
    if(is_generic_instance) {
        for(auto&& it : comptime_arg_refs) {
            auto* res = it.get_result();
            if(res && res->state >= CIRResultState::OnlyType) {
                h = xp_hash_combine_u64(h, reinterpret_cast<u64>(res->actual_type()));
            }
        }
        return h;
    }

    FOR(comptime_arg_refs) {
        auto* res = it.get_result();
        if(res && res->state >= CIRResultState::WholeValue) {
            h = xp_hash_combine_u64(h, std::hash<Value>{}(res->actual_val()));
        }
    }
    return h;
}

bool FuncCallKey::operator==(const FuncCallKey& other) const {

    // TODO: AI SLOP
    if(func_decl_pc != other.func_decl_pc) {
        return false;
    }
    if(is_generic_instance != other.is_generic_instance) {
        return false;
    }
    if(comptime_arg_refs.count != other.comptime_arg_refs.count) {
        return false;
    }

    // $T 实例: 同上, 比类型不比指令身份
    if(is_generic_instance) {
        FOR(comptime_arg_refs) {
            auto* ra = it.get_result();
            auto* rb = other.comptime_arg_refs[it_index].get_result();
            if(!ra || !rb) {
                return false;
            }

            if(ra->state < CIRResultState::OnlyType || rb->state < CIRResultState::OnlyType) {
                return false;
            }

            if(ra->actual_type() != rb->actual_type()) {
                return false;
            }
        }
        return true;
    }

    FOR(comptime_arg_refs) {
        auto* ra = it.get_result();
        auto* rb = other.comptime_arg_refs[it_index].get_result();
        if(!ra || !rb) {
            return false;
        }

        if(ra->state < CIRResultState::WholeValue || rb->state < CIRResultState::WholeValue) {
            return false;
        }

        if(!ra->actual_val().equal(rb->actual_val())) {
            return false;
        }
    }
    return true;
}


isize CIRBlock::push_back_inst(CIRInstruction inst) {
    return insts.push(inst);
}


CIRResultContext CIRResultContext::create(Ref<CIRPackage> pkg) {
    CIRResultContext ctx;
    ctx._pkg = pkg;
    return ctx;
}

void CIRResultContext::enter_call(FuncCallKey key, Ref<CIRResultInstance> parent) {
    _call_key = key;
    _call_instance = _pkg->get_or_init_result_instance(key, parent);
}

void CIRResultContext::enter_call_instance(Ref<CIRResultInstance> instance) {
    _call_key = std::nullopt;
    _call_instance = instance;
}

void CIRResultContext::exit_call() {
    _call_key = std::nullopt;
    _call_instance = {};
}

const FuncCallKey &CIRResultContext::call_key() const {
    XP_ASSERT(in_call());
    return _call_key.value();
}

CIRInstResult &CIRResultContext::result_of(CIRInstructionRef ref) const {
    if (_call_instance != Ref<CIRResultInstance>::INVALID_REF) {
        return _pkg->result_of(ref, _call_instance);
    }
    return _pkg->result_of(ref);
}

const CIRInstResult *CIRResultContext::result_ptr_of(CIRInstructionRef ref) const {
    return _pkg->find_result(ref, _call_instance);
}


// ─── is_pure_comptime_func ────────────────────────────────────────

bool is_pure_comptime_func(const CIRFunctionDeclInfo& func, const CIRResultContext& ctx) {
    if(func.is_comptime) {
        return true;
    }

    if(func.return_type_inst == INVALID_INST) {
        return true;
    }

    auto& res = ctx.result_of(func.return_type_inst);
    if(res.state != CIRResultState::WholeValue) {
        return false;
    }

    TypeRef return_type = res.actual_val().type_val();
    if(is_type_type(return_type)) {
        return true;
    }

    return false;
}

#if defined(CREST_DEBUG)

// 子块字段取值：CIRBlockRef 直接用；裸块句柄（CIRInstructionRef）取它的块号
static CIRBlockRef child_block_of(CIRBlockRef block) {
    return block;
}

static CIRBlockRef child_block_of(CIRInstructionRef ref) {
    return ref.block_ref;
}


// 打印单个 block：块头 + 块内每条指令；带 ChildBlock 注解的字段指到的子块就地展开
static void dump_cir_block(CIRPackage *pkg, CIRBlockRef block, isize depth) {
    if(block == INVALID_BLOCK) {
        return;
    }

    const auto& blk = pkg->blocks[block];

    // 块头在 depth 层，块内容（指令 + 子块头）再进一层
    const std::string indent(depth * 2, ' ');
    const std::string body_indent((depth + 1) * 2, ' ');

    std::string flags;
    if(blk.is_comptime) {
        flags += " comptime";
    }
    if(blk.immediate_eval) {
        flags += " immediate";
    }
    if(blk.is_loop) {
        flags += " loop";
    }
    if(blk.yields_value) {
        flags += " yields";
    }

    if(depth == 0) {
        println_err("");
    }
    println_err("{}block #{} ({} insts{}) {{", indent, block, blk.insts.count(), flags);

    for(auto ref : blk) {
        const auto& inst = pkg->inst(ref);

        const auto ref_str = std::format("{}", ref);
        const std::string op_name = inst.to_string();

        std::string line = body_indent;
        line += ref_str;
        line.append(ref_str.size() < 6 ? 6 - ref_str.size() : 0, ' ');
        line += "  ";
        line += op_name;
        line.append(op_name.size() < 15 ? 15 - op_name.size() : 1, ' ');

        template for(constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^CIROperator))) {
            if(inst.op == [:e:]) {
                const auto& payload = inst.info<([:e:])>();

                // 布局在这一层定：每字段一格 " 名字=值"
                for(auto& [name, value] : CIRFields<std::remove_cvref_t<decltype(payload)>>::field_strings(inst, payload)) {
                    line += ' ';
                    line += name;
                    line += '=';
                    line += value;
                }
            }
        }

        const auto *res_ptr = pkg->root_instance()->result_ptr_of(ref);
        if(res_ptr) {
            const auto& res = *res_ptr;

            std::string result;
            switch (res.state) {
                case CIRResultState::NothingYet: break;
                case CIRResultState::OnlyType:   result = std::format("{}", get_global_string(get_type_kind_str(res.type()->kind))); break;
                case CIRResultState::InProgress: result = "<in-progress>"; break;
                case CIRResultState::WholeValue: result = std::format("{} = {}", get_global_string(get_type_kind_str(res.type()->kind)), res.actual_val()); break;
                case CIRResultState::Error:      result = "<error>"; break;
            }
            if(res.value_kind == CIRValueKind::LValue) {
                result += " [lvalue]";
            }

            if(!result.empty()) {
                constexpr usize arrow_col = 78;
                line.append(line.size() < arrow_col ? arrow_col - line.size() : 2, ' ');
                line += "-> " + result;
            }
        }

        println_err("{}", line);

        // 子块：payload 里带 CIRFieldTag::ChildBlock 注解的字段，一律就地展开（层层包裹）
        template for(constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^CIROperator))) {
            if(inst.op == [:e:]) {
                const auto& payload = inst.info<([:e:])>();
                using Payload = std::remove_cvref_t<decltype(payload)>;

                template for(constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^Payload, std::meta::access_context::current()))) {
                    if constexpr(has_annotation(m, CIRFieldTag::ChildBlock)) {
                        dump_cir_block(pkg, child_block_of(payload.[:m:]), depth + 1);
                    }
                }
            }
        }
    }

    println_err("{}", indent + "}");
}
#endif


void dump_cir_package(CIRPackage *pkg) {
#if defined(CREST_DEBUG)
    const isize block_count = pkg->blocks.count;

    isize total_insts = 0;
    for(isize b = 0; b < block_count; b++) {
        total_insts += pkg->blocks[b].insts.count();
    }

    println_err("CIRPackage {{ {} blocks, {} insts }}", block_count, total_insts);

    // 从 top_blk 递归进去，子块在各自父块里就地展开
    dump_cir_block(pkg, pkg->top_blk, 0);

    println_err("");
    println_err("--- {} blocks, {} insts ---", block_count, total_insts);
#endif
}
