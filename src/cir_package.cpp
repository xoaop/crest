#include "cir_package.hpp"
#include "common.hpp"
#include "error_msg.hpp"

#include <print>
#include "tokenizer.hpp"
#include "type.hpp"


CIRPackage make_cir_package(xpAllocator allocator) {
    CIRPackage cir_package = {};
    cir_package.instructions = StableOrderedArray<CIRInstruction>::make(allocator);

    cir_package.blocks = make_array<CIRBlock2>(allocator);

    cir_package.string_literals = make_array<xpString>(allocator);
    cir_package.results = xp_hash_map_make<CIRInstructionRef, CIRInstResult>(allocator);
    cir_package.result_instances = xp_hash_map_make<FuncCallKey, CIRResultInstanceRef>(allocator);
    cir_package.comptime_func_calls = make_array<FuncCallKey>(allocator);
    return cir_package;
}


CIRInstruction* CIRPackage::inst(CIRInstructionRef ref) {
    XP_ASSERT(ref.inst_index >= 0);   // ref 恒为真实指令（INVALID_INST={-1,-1} 除外）
    return &blocks[ref.block_ref].insts[ref.inst_index];
}

const CIRInstruction* CIRPackage::inst(CIRInstructionRef ref) const {
    XP_ASSERT(ref.inst_index >= 0);   // ref 恒为真实指令（INVALID_INST={-1,-1} 除外）
    return &blocks[ref.block_ref].insts[ref.inst_index];
}

CIRBlock2* CIRPackage::block(CIRBlockRef ref) {
    return &blocks[ref];
}

const CIRBlock2* CIRPackage::block(CIRBlockRef ref) const {
    return &blocks[ref];
}

CIRBlockRef CIRPackage::create_block(bool is_comptime, bool immediate_eval, bool is_loop) {
    CIRBlock2 blk = {};
    blk.insts = StableOrderedArray<CIRInstruction>::make(permanent_allocator());
    blk.is_comptime = is_comptime;
    blk.immediate_eval = immediate_eval;
    blk.is_loop = is_loop;
    CIRBlockRef ref = blocks.count;
    blocks.push_back(blk);
    return ref;
}

CIRResultInstanceRef CIRPackage::get_result_instance(FuncCallKey key) {
    return result_instances.get_or_insert(key, [&]{
        return CIRResultInstance::make(permanent_allocator());
    });
}


CIRInstResult& CIRPackage::result_of(CIRInstructionRef ref, std::optional<CIRResultInstanceRef> instance) {
    if(instance.has_value()) {
        auto inst = instance.value();

        return inst->result_of_or(ref, [&]{ return result_of(ref); });
    }


    return results.get(ref);
}





CIRInstResultRef CIRInstResultRef::make(CIRPackage* pkg, CIRInstructionRef ref,
                                        std::optional<CIRResultInstanceRef> ri) {
    return {pkg, ref, ri};
}

const CIRInstResult* CIRInstResultRef::get_result() const {
    if(result_instance.has_value() && result_instance.value()) {
        return result_instance.value()->result_ptr_of(inst_ref);
    }
    return &cir_package->result_of(inst_ref);
}

const CIRInstruction* CIRInstResultRef::inst() const {
    return cir_package->inst(inst_ref);
}

u64 FuncCallKey::hash() const {
    u64 h = xp_hash_combine_u64((u64)func_decl_pc.block_ref, (u64)func_decl_pc.inst_index);
    h = xp_hash_combine_u64(h, (u64)(usize)(func_instance ? *func_instance : nullptr));
    for(isize i = 0; i < comptime_arg_refs.count; i++) {
        auto* res = comptime_arg_refs[i].get_result();
        if(res && res->state >= CIRResultState::WholeValue) {
            Value v = res->actual_val();
            if(is_type_type(v.type)) {
                h = xp_hash_combine_u64(h, reinterpret_cast<u64>(v.type_val()));
            } else {
                h = xp_hash_combine_u64(h, std::hash<CIRInstResultRef>{}(comptime_arg_refs[i]));
            }
        }
    }
    return h;
}

bool FuncCallKey::operator==(const FuncCallKey& other) const {
    if(func_decl_pc != other.func_decl_pc) return false;
    if(func_instance.has_value() != other.func_instance.has_value()) return false;
    if(func_instance.has_value() && func_instance.value() != other.func_instance.value()) return false;
    if(comptime_arg_refs.count != other.comptime_arg_refs.count) return false;
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


isize CIRBlock2::push_back_inst(CIRInstruction inst) {
    return insts.push(inst);
}

TypeRef CIRInstResult::type() const {
    ASSERT(state == CIRResultState::OnlyType || state == CIRResultState::WholeValue);

    return outstanding_type;
}

TypeRef CIRInstResult::actual_type() const {
    ASSERT(state == CIRResultState::OnlyType || state == CIRResultState::WholeValue);

    return val.type;
}


Value CIRInstResult::actual_val() const {
    ASSERT(state == CIRResultState::WholeValue);

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


    if(state == CIRResultState::NothingYet || state == CIRResultState::OnlyType) {
        state = CIRResultState::WholeValue;
    }
}


CIRResultInstanceRef CIRResultInstance::make(xpAllocator allocator) {
    auto ptr = xp_alloc<CIRResultInstance>(allocator);
    ptr->results = xp_hash_map_make<CIRInstructionRef, CIRInstResult>(allocator);
    return ptr;
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

void CIRResultContext::enter_call_instance(CIRResultInstanceRef instance) {
    _call_key = std::nullopt;
    _call_instance = instance;
}

void CIRResultContext::exit_call() {
    _call_key = std::nullopt;
    _call_instance = nullptr;
}

const FuncCallKey &CIRResultContext::call_key() const {
    XP_ASSERT(in_call());
    return _call_key.value();
}

CIRInstResult &CIRResultContext::result_of(CIRInstructionRef ref) const {
    if (_call_instance) {
        return _pkg->result_of(ref, _call_instance);
    }
    return _pkg->result_of(ref);
}


// ─── is_pure_comptime_func ────────────────────────────────────────

bool is_pure_comptime_func(CIRFunctionDeclInfo& func, const CIRResultContext& ctx) {
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


//
// debug
//

static void dump_result(CIRInstResult& res) {
    switch(res.state) {
        case CIRResultState::NothingYet:
            break;
        case CIRResultState::OnlyType:
            std::print(stderr," -> {}", get_type_kind_str(res.type()->kind));
            break;
        case CIRResultState::WholeValue:
            std::print(stderr," -> {} = {}", get_type_kind_str(res.type()->kind), res.actual_val());
            break;
        case CIRResultState::Error:
            std::print(stderr," -> <error>");
            break;
    }
    if(res.value_kind == CIRValueKind::LValue) {
        std::print(stderr," [lvalue]");
    }
}

static void dump_inst_compact(CIRPackage *pkg, CIRInstructionRef ref, bool show_result) {
    auto& inst = *pkg->inst(ref);

    switch (inst.op) {
    case CIROperator::VariableDecl:
        std::print(stderr,"VariableDecl({}, slot={})", inst.info<CIROperator::VariableDecl>().name, inst.info<CIROperator::VariableDecl>().slot);
        break;
    case CIROperator::ConstDecl:
        std::print(stderr,"ConstDecl({}, value=%{})", inst.info<CIROperator::ConstDecl>().ident, inst.info<CIROperator::ConstDecl>().value_inst);
        break;
    case CIROperator::FunctionDecl: {
        auto& f = inst.info<CIROperator::FunctionDecl>() ;
        std::print(stderr,"FunctionDecl(return_type=%{}, return_count={}, slot_count={}, is_extern_c={}, is_comptime={}",
            f.return_type_inst, f.return_count, f.slot_count, f.is_extern_c, f.is_comptime);
        if(f.arg_decl_insts.count > 0) {
            std::print(stderr,", params=[");
            for (isize i = 0; i < f.arg_decl_insts.count; i++) {
                auto& var = pkg->inst(f.arg_decl_insts[i])->info<CIROperator::VariableDecl>();
                if(i > 0) std::print(stderr,", ");
                std::print(stderr,"{} slot={} type=%{}", var.name, var.slot, f.arg_type_insts[i]);
            }
            std::print(stderr,"]");
        }
        std::print(stderr,", body=%{})", f.body_inst);
        break;
    }
    case CIROperator::Break:
        if(inst.info<CIROperator::Break>().break_value_inst != INVALID_INST) {
            std::print(stderr,"Break(%{}, %{})", inst.info<CIROperator::Break>().break_block, inst.info<CIROperator::Break>().break_value_inst);
        } else {
            std::print(stderr,"Break(%{}, void)", inst.info<CIROperator::Break>().break_block);
        }
        break;
    case CIROperator::Store:
        std::print(stderr,"Store(%{}, %{})", inst.info<CIROperator::Store>().var_inst, inst.info<CIROperator::Store>().value_inst);
        break;
    case CIROperator::Load:
        std::print(stderr,"Load(%{})", inst.info<CIROperator::Load>().ptr_inst);
        break;
    case CIROperator::TypeAscribe:
        std::print(stderr,"TypeAscribe(%{}, %{})", inst.info<CIROperator::TypeAscribe>().var_inst, inst.info<CIROperator::TypeAscribe>().type_inst);
        break;
    case CIROperator::Binary:
        std::print(stderr,"Binary({}, %{}, %{})",
            token_strings[(int)inst.info<CIROperator::Binary>().op],
            inst.info<CIROperator::Binary>().left_inst, inst.info<CIROperator::Binary>().right_inst);
        break;
    case CIROperator::Unary:
        std::print(stderr,"Unary({}, %{})", token_strings[(int)inst.info<CIROperator::Unary>().op], inst.info<CIROperator::Unary>().operand_inst);
        break;
    case CIROperator::Call:
        std::print(stderr,"Call(%{}, [", inst.info<CIROperator::Call>().called_thing);
        for (isize i = 0; i < inst.info<CIROperator::Call>().arg_insts.count; i++) {
            if(i > 0) std::print(stderr,", ");
            std::print(stderr,"%{}", inst.info<CIROperator::Call>().arg_insts[i]);
        }
        std::print(stderr,"])");
        break;
    case CIROperator::Cast:
        std::print(stderr,"Cast(%{}, %{})", inst.info<CIROperator::Cast>().expr_inst, inst.info<CIROperator::Cast>().target_type_inst);
        break;
    case CIROperator::FieldAccess:
        std::print(stderr,"FieldAccess({}, %{})", inst.info<CIROperator::FieldAccess>().field_name, inst.info<CIROperator::FieldAccess>().parent_inst);
        break;
    case CIROperator::FieldPtr:
        std::print(stderr,"FieldPtr({}, %{})", inst.info<CIROperator::FieldPtr>().field_name, inst.info<CIROperator::FieldPtr>().parent_inst);
        break;
    case CIROperator::Index:
        std::print(stderr,"Index(%{}, %{})", inst.info<CIROperator::Index>().array_inst, inst.info<CIROperator::Index>().index_inst);
        break;
    case CIROperator::IndexPtr:
        std::print(stderr,"IndexPtr(%{}, %{})", inst.info<CIROperator::IndexPtr>().array_inst, inst.info<CIROperator::IndexPtr>().index_inst);
        break;
    case CIROperator::StructInit:
        std::print(stderr,"StructInit(%{}, [", inst.info<CIROperator::StructInit>().struct_type_inst);
        for (isize i = 0; i < inst.info<CIROperator::StructInit>().field_init_insts.count; i++) {
            if(i > 0) std::print(stderr,", ");
            std::print(stderr,"%{}", inst.info<CIROperator::StructInit>().field_init_insts[i]);
        }
        std::print(stderr,"])");
        break;
    case CIROperator::ArrayInit:
        std::print(stderr,"ArrayInit([");
        for (isize i = 0; i < inst.info<CIROperator::ArrayInit>().element_insts.count; i++) {
            if(i > 0) std::print(stderr,", ");
            std::print(stderr,"%{}", inst.info<CIROperator::ArrayInit>().element_insts[i]);
        }
        std::print(stderr,"])");
        break;
    case CIROperator::ConstantValue:
        std::print(stderr,"Const({})", inst.info<CIROperator::ConstantValue>().value);
        break;
    case CIROperator::StringLiteral:
        std::print(stderr,"StringLiteral(\"{}\")", inst.info<CIROperator::StringLiteral>().str);
        break;
    case CIROperator::IdentRef: {
        SymbolInfo *sym = (inst.symbol)();
        std::print(stderr,"IdentRef({})", sym ? sym->name : xpString{});
        break;
    }
    case CIROperator::IdentVal: {
        SymbolInfo *sym = (inst.symbol)();
        std::print(stderr,"IdentVal({})", sym ? sym->name : xpString{});
    }
        break;
    case CIROperator::DetermineType:
        std::print(stderr,"DetermineType(%{}", inst.info<CIROperator::DetermineType>().determining_inst);
        if(inst.info<CIROperator::DetermineType>().type_inst != INVALID_INST) {
            std::print(stderr,", %{}", inst.info<CIROperator::DetermineType>().type_inst);
        }
        std::print(stderr,")");
        break;
    case CIROperator::PointerType:
        std::print(stderr,"PointerType(%{})", inst.info<CIROperator::PointerType>().pointed_type_inst);
        break;
    case CIROperator::ArrayType:
        std::print(stderr,"ArrayType(%{}, %{})", inst.info<CIROperator::ArrayType>().element_type_inst, inst.info<CIROperator::ArrayType>().count_inst);
        break;
    case CIROperator::SliceType:
        std::print(stderr,"SliceType(%{})", inst.info<CIROperator::SliceType>().element_type_inst);
        break;
    case CIROperator::GetOrInitStruct:
        std::print(stderr,"GetOrInitStruct");
        break;
    case CIROperator::StructField:
        std::print(stderr,"StructField({}, type=%{})", inst.info<CIROperator::StructField>().name, inst.info<CIROperator::StructField>().type_block_inst);
        break;
    case CIROperator::FinishStruct: {
        auto& fs = inst.info<CIROperator::FinishStruct>() ;
        std::print(stderr,"FinishStruct(struct=%{}, fields=[", fs.struct_decl_inst);
        for (isize i = 0; i < fs.field_insts.count; i++) {
            if (i > 0) std::print(stderr,", ");
            std::print(stderr,"%{}", fs.field_insts[i]);
        }
        std::print(stderr,"])");
        break;
    }
    case CIROperator::EnumDeclInit: {
        auto& ed = inst.info<CIROperator::EnumDeclInit>() ;
        std::print(stderr,"EnumDeclInit(tag=%{}, fields=[", ed.tag_type_inst);
        for (isize i = 0; i < ed.fields.count; i++) {
            if(i > 0) std::print(stderr,", ");
            auto& ef = ed.fields[i];
            if(ef.value_inst != INVALID_INST)
                std::print(stderr,"{}: %{}", ef.name, ef.value_inst);
            else
                std::print(stderr,"{}: auto", ef.name);
        }
        std::print(stderr,"])");
        break;
    }
    case CIROperator::UnionDecl:
        std::print(stderr,"UnionDecl");
        break;
    case CIROperator::AddrOf:
        std::print(stderr,"AddrOf(%{})", inst.info<CIROperator::AddrOf>().lval_inst);
        break;
    case CIROperator::TypeOfInstResult:
        std::print(stderr,"TypeOfInstResult(%{})", inst.info<CIROperator::TypeOfInstResult>().target_inst);
        break;
    case CIROperator::FieldTypeOfStruct:
        std::print(stderr,"FieldTypeOfStruct(struct=%{}, field={})",
            inst.info<CIROperator::FieldTypeOfStruct>().struct_type_inst,
            inst.info<CIROperator::FieldTypeOfStruct>().field_index);
        break;
    case CIROperator::FuncParamType:
        std::print(stderr,"FuncParamType(type_func=%{}, param_index={})",
            inst.info<CIROperator::FuncParamType>().type_of_func_type_inst,
            inst.info<CIROperator::FuncParamType>().param_index);
        break;
    case CIROperator::CondBr:
        std::print(stderr,"CondBr(cond=%{}, then=blk#{}, else={})",
            inst.info<CIROperator::CondBr>().condition_inst, inst.info<CIROperator::CondBr>().true_block,
            inst.info<CIROperator::CondBr>().false_block == INVALID_BLOCK ? "none" : std::format("blk#{}", inst.info<CIROperator::CondBr>().false_block));
        break;
    case CIROperator::EnterScope:
        std::print(stderr,"EnterScope {}", to_string(inst.info<CIROperator::EnterScope>().scope->scope_type));
        break;
    case CIROperator::ExitScope:
        std::print(stderr,"ExitScope {}", to_string(inst.info<CIROperator::ExitScope>().scope->scope_type));
        break;
    default:
        std::print(stderr,"{}", string(inst.op));
        break;
    }

    if(show_result) {
        auto *entry = pkg->results.get_entry(ref);
        if(entry) dump_result(entry->value);
    }
    std::println(stderr,"");
}

void dump_cir_package(CIRPackage *pkg) {
    std::println(stderr,"CIRPackage {{");
    for (CIRBlockRef b = 0; b < pkg->blocks.count; b++) {
        auto& blk = pkg->blocks[b];
        for (auto it = blk.insts.begin(); it != blk.insts.end(); ++it) {
            dump_inst_compact(pkg, CIRInstructionRef{b, it.subscript()}, true);
        }
    }
    std::println(stderr,"}}");

    std::println(stderr,"\n--- total blocks: {} ---", pkg->blocks.count);



}
