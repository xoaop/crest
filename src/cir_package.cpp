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

    cir_package.string_literals = make_array<xpString>(allocator);
    cir_package.results = xp_hash_map_make<CIRInstructionRef, CIRInstResult>(allocator);
    cir_package.result_instances = make_array<CIRResultInstance>(allocator);
    cir_package.result_instance_map = xp_hash_map_make<FuncCallKey, Ref<CIRResultInstance>>(allocator);
    cir_package.comptime_func_calls = make_array<FuncCallKey>(allocator);
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
    blk.package_ref = package_ref;
    blocks.push_back(blk);
    return ref;
}

Ref<CIRResultInstance> CIRPackage::get_result_instance(FuncCallKey key) {
    Ref<CIRResultInstance> *existing = xp_hash_map_get(result_instance_map, key);
    if(existing != nullptr) {
        return *existing;
    }
    result_instances.push_back(CIRResultInstance::make(permanent_allocator()));

    Ref<CIRResultInstance> r;
    r.cir_package = Ref<CIRPackage>(package_ref.index);
    r.index = (isize)result_instances.count - 1;
    result_instance_map.insert(key, r);
    return r;
}


CIRInstResult& CIRPackage::result_of(CIRInstructionRef ref, Ref<CIRResultInstance> instance) {
    CIRResultInstance* inst = instance.try_get();
    if(inst != nullptr) {
        return inst->result_of_or(ref, [&]{ return result_of(ref); });
    }


    return results.get(ref);
}


template<>
CIRPackage* Ref<CIRPackage>::resolve() const {
    if(this->index < 0) {
        return nullptr;
    }

    return &context()->all_packages[this->index].cir_package;
}




CIRResultInstance* Ref<CIRResultInstance>::resolve() const {
    if(cir_package == Ref<CIRPackage>::INVALID_REF || index < 0) {
        return nullptr;
    }

    return &cir_package->result_instances[index];
}

Ref<CIRInstResult> Ref<CIRInstResult>::init(CIRPackage* pkg, CIRInstructionRef ref,
                                            Ref<CIRResultInstance> ri) {
    return Ref<CIRInstResult>{.cir_package = pkg, .inst_ref = ref, .result_instance = ri};
}

CIRInstResult* Ref<CIRInstResult>::resolve() const {
    if(cir_package == nullptr) {
        return nullptr;
    }
    return get_result();
}

CIRInstResult* Ref<CIRInstResult>::get_result() const {
    CIRResultInstance* inst = result_instance.try_get();
    if(inst != nullptr) {
        return inst->result_ptr_of(inst_ref);
    }
    return &cir_package->result_of(inst_ref);
}

const CIRInstruction& Ref<CIRInstResult>::inst() const {
    return cir_package->inst(inst_ref);
}

u64 FuncCallKey::hash() const {

    // TODO: AI SLOP
    u64 h = xp_hash_combine_u64((u64)func_decl_pc.pkg_index, (u64)func_decl_pc.block_ref);
    h = xp_hash_combine_u64(h, (u64)func_decl_pc.inst_index);
    h = xp_hash_combine_u64(h, (u64)is_generic_instance);

    // $T 实例: 按实参结果的类型去重(TypeRef 是 interned 指针), 与指令身份无关
    if(is_generic_instance) {
        for(isize i = 0; i < comptime_arg_refs.count; i++) {
            auto* res = comptime_arg_refs[i].get_result();
            if(res && res->state >= CIRResultState::OnlyType) {
                h = xp_hash_combine_u64(h, reinterpret_cast<u64>(res->actual_type()));
            }
        }
        return h;
    }

    for(isize i = 0; i < comptime_arg_refs.count; i++) {
        auto* res = comptime_arg_refs[i].get_result();
        if(res && res->state >= CIRResultState::WholeValue) {
            Value v = res->actual_val();
            if(is_type_type(v.type)) {
                h = xp_hash_combine_u64(h, reinterpret_cast<u64>(v.type_val()));
            } else {
                h = xp_hash_combine_u64(h, std::hash<Ref<CIRInstResult>>{}(comptime_arg_refs[i]));
            }
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
        for(isize i = 0; i < comptime_arg_refs.count; i++) {
            auto* ra = comptime_arg_refs[i].get_result();
            auto* rb = other.comptime_arg_refs[i].get_result();
            if(!ra || !rb) return false;
            if(ra->state < CIRResultState::OnlyType || rb->state < CIRResultState::OnlyType) return false;
            if(ra->actual_type() != rb->actual_type()) return false;
        }
        return true;
    }

    for(isize i = 0; i < comptime_arg_refs.count; i++) {
        auto* ra = comptime_arg_refs[i].get_result();
        auto* rb = other.comptime_arg_refs[i].get_result();
        if(!ra || !rb) return false;
        if(ra->state < CIRResultState::WholeValue || rb->state < CIRResultState::WholeValue) return false;
        Value va = ra->actual_val();
        Value vb = rb->actual_val();
        if(is_type_type(va.type) && is_type_type(vb.type)) {
            if(va.type_val() != vb.type_val()) return false;
        } else {
            if(!(comptime_arg_refs[i] == other.comptime_arg_refs[i])) return false;
        }
    }
    return true;
}


isize CIRBlock::push_back_inst(CIRInstruction inst) {
    return insts.push(inst);
}

TypeRef CIRInstResult::type() const {
    ASSERT(state == CIRResultState::OnlyType || state == CIRResultState::WholeValue || state == CIRResultState::InProgress);

    return outstanding_type;
}

TypeRef CIRInstResult::actual_type() const {
    ASSERT(state == CIRResultState::OnlyType || state == CIRResultState::WholeValue || state == CIRResultState::InProgress);

    return val.type;
}


Value CIRInstResult::actual_val() const {
    ASSERT(state == CIRResultState::WholeValue || state == CIRResultState::InProgress);

    return val;
}


void CIRInstResult::set_type(TypeRef new_type) {
    outstanding_type = new_type;
    val.type = new_type;

    if(state == CIRResultState::NothingYet) {
        state = CIRResultState::OnlyType;
    }
}

void CIRInstResult::set_actual_type(TypeRef new_type) {
    val.type = new_type;
}

void CIRInstResult::set_val(Value new_val) {
    val = new_val;

    if(state == CIRResultState::NothingYet || state == CIRResultState::OnlyType || state == CIRResultState::InProgress) {
        state = CIRResultState::WholeValue;
    }
}

void CIRInstResult::set_val_in_progress(Value new_val) {

    // 类型值指向未完成的类型壳子, 函数值指向 FunctionDecl 的结果槽 — 都是引用语义, 补完后登记者可见
    if(!is_type_type(new_val.type) && !is_function_type(new_val.type)) {
        DEBUG_PANIC("set_val_in_progress: 可以提前登记的值的类型必须自带引用语义, 不然无意义");
    }

    outstanding_type = new_val.type;
    val = new_val;
    state = CIRResultState::InProgress;
}


CIRResultInstance CIRResultInstance::make(xpAllocator allocator) {
    CIRResultInstance inst;
    inst.results = xp_hash_map_make<CIRInstructionRef, CIRInstResult>(allocator);
    return inst;
}

CIRInstResult* CIRResultInstance::result_ptr_of(CIRInstructionRef ref) {
    return xp_hash_map_get(results, ref);
}

CIRInstResult& CIRResultInstance::result_of(CIRInstructionRef ref) {
    return results.get(ref);
}


CIRResultContext CIRResultContext::create(CIRPackage *pkg) {
    CIRResultContext ctx;
    ctx._pkg = pkg;
    return ctx;
}

void CIRResultContext::enter_call(FuncCallKey key) {
    _call_key = key;
    _call_instance = _pkg->get_result_instance(key);
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
// 打印单个 block：块头 + 块内每条指令
static void dump_cir_block(CIRPackage *pkg, CIRBlockRef block) {
    CIRBlock& blk = pkg->blocks[block];

    std::string flags;
    if (blk.is_comptime)    flags += " comptime";
    if (blk.immediate_eval) flags += " immediate";
    if (blk.is_loop)        flags += " loop";
    if (blk.yields_value)   flags += " yields";

    println_err("");
    println_err("block #{} {{ {} insts{} }}", block, blk.insts.count(), flags);

    for (auto ref : blk) {
        const CIRInstruction& inst = pkg->inst(ref);

        std::string ref_str = std::format("{}", ref);
        std::string op_name = inst.to_string();

        std::string line;
        line.append(ref_str.size() < 6 ? 6 - ref_str.size() : 0, ' ');
        line += ref_str;
        line += "  ";
        line += op_name;
        line.append(op_name.size() < 15 ? 15 - op_name.size() : 1, ' ');

        template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^CIROperator))) {
            if (inst.op == [:e:]) {
                const auto& payload = inst.info<([:e:])>();
                CIRFormat<std::remove_cvref_t<decltype(payload)>>::write(line, payload);
            }
        }

        auto *entry = pkg->results.get_entry(ref);
        if (entry) {
            CIRInstResult& res = entry->value;

            std::string result;
            switch (res.state) {
                case CIRResultState::NothingYet: break;
                case CIRResultState::OnlyType:   result = std::format("{}", get_type_kind_str(res.type()->kind)); break;
                case CIRResultState::InProgress: result = "<in-progress>"; break;
                case CIRResultState::WholeValue: result = std::format("{} = {}", get_type_kind_str(res.type()->kind), res.actual_val()); break;
                case CIRResultState::Error:      result = "<error>"; break;
            }
            if (res.value_kind == CIRValueKind::LValue) result += " [lvalue]";

            if (!result.empty()) {
                constexpr usize arrow_col = 78;
                line.append(line.size() < arrow_col ? arrow_col - line.size() : 2, ' ');
                line += "-> " + result;
            }
        }

        println_err("{}", line);
    }
}
#endif


void dump_cir_package(CIRPackage *pkg) {
#if defined(CREST_DEBUG)
    const isize block_count = pkg->blocks.count;

    isize total_insts = 0;
    for (isize b = 0; b < block_count; b++) {
        total_insts += pkg->blocks[b].insts.count();
    }

    println_err("CIRPackage {{ {} blocks, {} insts }}", block_count, total_insts);

    for (isize b = 0; b < block_count; b++) {
        dump_cir_block(pkg, b);
    }

    println_err("");
    println_err("--- {} blocks, {} insts ---", block_count, total_insts);
#endif
}
