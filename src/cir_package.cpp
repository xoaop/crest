#include "cir_package.hpp"
#include "common.hpp"
#include "error_msg.hpp"

#include <print>
#include "tokenizer.hpp"
#include "type.hpp"


CIRPackage make_cir_package(xpAllocator allocator) {
    CIRPackage cir_package = {};
    cir_package.instructions = StableOrderedArray<CIRInstruction>::make(allocator);
    cir_package.top_level_insts = make_array<CIRInstructionRef>(allocator);
    cir_package.file_ranges = make_array<CIRFileRange>(allocator);
    cir_package.string_literals = make_array<xpString>(allocator);
    cir_package.results = xp_hash_map_make<CIRInstructionRef, CIRInstResult>(allocator);
    cir_package.result_instances = xp_hash_map_make<FuncCallKey, CIRResultInstanceRef>(allocator);
    cir_package.comptime_func_calls = make_array<FuncCallKey>(allocator);
    return cir_package;
}


CIRInstruction* CIRPackage::inst(CIRInstructionRef ref) {
    return &instructions[ref];
}

const CIRInstruction* CIRPackage::inst(CIRInstructionRef ref) const {
    return &instructions[ref];
}

Scope *CIRPackage::scope_for_pc(CIRInstructionRef pc) const {
    XP_ASSERT(file_ranges.count > 0);
    isize lo = 0, hi = file_ranges.count - 1;
    while (lo < hi) {
        isize mid = (lo + hi + 1) / 2;
        if (file_ranges[mid].start <= pc) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return file_ranges[lo].file_scope;
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
    u64 h = (u64)(usize)func_decl_pc;
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


isize CIRInstruction::len() const {
    switch(op) {
        case CIROperator::Block: return block_info.body_len + 1;
        case CIROperator::Loop: return loop_info.body_len + 1;
        default: return 1;
    }
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

bool is_pure_comptime_func(CIRFunction& func, const CIRResultContext& ctx) {
    if(func.is_comptime) {
        return true;
    }

    auto *cir_pkg = ctx.pkg();

    for(auto& arg_inst_ref: func.arg_decl_insts) {
        auto& var_info = cir_pkg->inst(arg_inst_ref)->var_decl;
        if(var_info.is_comptime) {
            return true;
        }
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
    auto& inst = pkg->instructions[ref];

    switch (inst.op) {
    case CIROperator::VariableDecl:
        std::print(stderr,"VariableDecl({}, slot={})", inst.var_decl.name, inst.var_decl.slot);
        break;
    case CIROperator::ConstDecl:
        std::print(stderr,"ConstDecl({}, value=%{})", inst.const_decl.ident, inst.const_decl.value_inst);
        break;
    case CIROperator::FunctionDecl: {
        auto& f = inst.func_decl;
        std::print(stderr,"FunctionDecl(return_type=%{}, return_count={}, slot_count={}, is_extern_c={}, is_comptime={}",
            f.return_type_inst, f.return_count, f.slot_count, f.is_extern_c, f.is_comptime);
        if(f.arg_decl_insts.count > 0) {
            std::print(stderr,", params=[");
            for (isize i = 0; i < f.arg_decl_insts.count; i++) {
                auto& var = pkg->inst(f.arg_decl_insts[i])->var_decl;
                if(i > 0) std::print(stderr,", ");
                std::print(stderr,"{} slot={} type=%{}", var.name, var.slot, f.arg_type_insts[i]);
                if(var.is_comptime) std::print(stderr," comptime");
            }
            std::print(stderr,"]");
        }
        std::print(stderr,", body=%{})", f.body_inst);
        break;
    }
    case CIROperator::Block:
        std::print(stderr,"Block(body_len={}, from {} to {}, comptime={}, immediate_eval={})",
            inst.block_info.body_len, ref + 1, ref + inst.block_info.body_len, inst.block_info.is_comptime, inst.block_info.immediate_eval);
        break;
    case CIROperator::Break:
        if(inst.break_info.break_value_inst != INVALID_INST) {
            std::print(stderr,"Break(%{}, %{})", inst.break_info.break_block, inst.break_info.break_value_inst);
        } else {
            std::print(stderr,"Break(%{}, void)", inst.break_info.break_block);
        }
        break;
    case CIROperator::Store:
        std::print(stderr,"Store(%{}, %{})", inst.store_info.var_inst, inst.store_info.value_inst);
        break;
    case CIROperator::Load:
        std::print(stderr,"Load(%{})", inst.load_info.ptr_inst);
        break;
    case CIROperator::TypeAscribe:
        std::print(stderr,"TypeAscribe(%{}, %{})", inst.type_ascribe_info.var_inst, inst.type_ascribe_info.type_inst);
        break;
    case CIROperator::Binary:
        std::print(stderr,"Binary({}, %{}, %{})",
            token_strings[(int)inst.binary_info.op],
            inst.binary_info.left_inst, inst.binary_info.right_inst);
        break;
    case CIROperator::Unary:
        std::print(stderr,"Unary({}, %{})", token_strings[(int)inst.unary_info.op], inst.unary_info.operand_inst);
        break;
    case CIROperator::Call:
        std::print(stderr,"Call(%{}, [", inst.call_info.called_thing);
        for (isize i = 0; i < inst.call_info.arg_insts.count; i++) {
            if(i > 0) std::print(stderr,", ");
            std::print(stderr,"%{}", inst.call_info.arg_insts[i]);
        }
        std::print(stderr,"])");
        break;
    case CIROperator::Cast:
        std::print(stderr,"Cast(%{}, %{})", inst.cast_info.expr_inst, inst.cast_info.target_type_inst);
        break;
    case CIROperator::FieldAccess:
        std::print(stderr,"FieldAccess({}, %{})", inst.field_access_info.field_name, inst.field_access_info.parent_inst);
        break;
    case CIROperator::FieldPtr:
        std::print(stderr,"FieldPtr({}, %{})", inst.field_access_info.field_name, inst.field_access_info.parent_inst);
        break;
    case CIROperator::Index:
        std::print(stderr,"Index(%{}, %{})", inst.index_info.array_inst, inst.index_info.index_inst);
        break;
    case CIROperator::IndexPtr:
        std::print(stderr,"IndexPtr(%{}, %{})", inst.index_info.array_inst, inst.index_info.index_inst);
        break;
    case CIROperator::StructInit:
        std::print(stderr,"StructInit(%{}, [", inst.struct_init_info.struct_type_inst);
        for (isize i = 0; i < inst.struct_init_info.field_init_insts.count; i++) {
            if(i > 0) std::print(stderr,", ");
            std::print(stderr,"%{}", inst.struct_init_info.field_init_insts[i]);
        }
        std::print(stderr,"])");
        break;
    case CIROperator::ArrayInit:
        std::print(stderr,"ArrayInit([");
        for (isize i = 0; i < inst.array_init_info.element_insts.count; i++) {
            if(i > 0) std::print(stderr,", ");
            std::print(stderr,"%{}", inst.array_init_info.element_insts[i]);
        }
        std::print(stderr,"])");
        break;
    case CIROperator::ConstantValue:
        std::print(stderr,"Const({})", inst.imm_val);
        break;
    case CIROperator::StringLiteral:
        std::print(stderr,"StringLiteral(\"{}\")", inst.string_literal_info.str);
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
        std::print(stderr,"DetermineType(%{}", inst.determine_type_info.determining_inst);
        if(inst.determine_type_info.type_inst != INVALID_INST) {
            std::print(stderr,", %{}", inst.determine_type_info.type_inst);
        }
        std::print(stderr,")");
        break;
    case CIROperator::PointerType:
        std::print(stderr,"PointerType(%{})", inst.pointer_type_info.pointed_type_inst);
        break;
    case CIROperator::ArrayType:
        std::print(stderr,"ArrayType(%{}, %{})", inst.array_type_info.element_type_inst, inst.array_type_info.count_inst);
        break;
    case CIROperator::SliceType:
        std::print(stderr,"SliceType(%{})", inst.slice_type_info.element_type_inst);
        break;
    case CIROperator::GetOrInitStruct:
        std::print(stderr,"GetOrInitStruct");
        break;
    case CIROperator::StructField:
        std::print(stderr,"StructField({}, type=%{})", inst.struct_field_info.name, inst.struct_field_info.type_block_inst);
        break;
    case CIROperator::FinishStruct: {
        auto& fs = inst.finish_struct_info;
        std::print(stderr,"FinishStruct(struct=%{}, fields=[", fs.struct_decl_inst);
        for (isize i = 0; i < fs.field_insts.count; i++) {
            if (i > 0) std::print(stderr,", ");
            std::print(stderr,"%{}", fs.field_insts[i]);
        }
        std::print(stderr,"])");
        break;
    }
    case CIROperator::EnumDeclInit: {
        auto& ed = inst.enum_decl_init_info;
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
        std::print(stderr,"AddrOf(%{})", inst.addr_of_info.lval_inst);
        break;
    case CIROperator::TypeOfInstResult:
        std::print(stderr,"TypeOfInstResult(%{})", inst.type_of_inst_result_info.target_inst);
        break;
    case CIROperator::FieldTypeOfStruct:
        std::print(stderr,"FieldTypeOfStruct(struct=%{}, field={})",
            inst.field_type_of_struct_info.struct_type_inst,
            inst.field_type_of_struct_info.field_index);
        break;
    case CIROperator::FuncParamType:
        std::print(stderr,"FuncParamType(type_func=%{}, param_index={})",
            inst.func_param_type_info.type_of_func_type_inst,
            inst.func_param_type_info.param_index);
        break;
    case CIROperator::CondBr:
        std::print(stderr,"CondBr(cond=%{}, then=%{}, else={})",
            inst.condbr_info.condition_inst, inst.condbr_info.true_block_inst,
            inst.condbr_info.false_block_inst == INVALID_INST ? "none" : std::format("%{}", inst.condbr_info.false_block_inst));
        break;
    case CIROperator::Loop:
        std::print(stderr,"Loop(body_len={})",
            inst.loop_info.body_len);
        break;
    case CIROperator::EnterScope:
        std::print(stderr,"EnterScope {}", to_string(inst.scope_info.scope->scope_type));
        break;
    case CIROperator::ExitScope:
        std::print(stderr,"ExitScope {}", to_string(inst.scope_info.scope->scope_type));
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
    for (auto it = pkg->instructions.begin(); it != pkg->instructions.end(); ++it) {
        dump_inst_compact(pkg, it.subscript(), true);
    }
    std::println(stderr,"}}");

    std::println(stderr,"\n--- total instructions: {} ---", pkg->instructions.count());

    // file ranges
    if(pkg->file_ranges.count > 0) {
        std::println(stderr,"\n--- file ranges ---");
        isize fi = 0;
        for (auto& fr : pkg->file_ranges) {
            std::println(stderr,"  [{}] start=%{} scope={}", fi++, fr.start, to_string(fr.file_scope->scope_type));
        }
    }

    // top-level
    if(pkg->top_level_insts.count > 0) {
        std::println(stderr,"\n--- top-level insts ---");
        isize ti = 0;
        for (auto ref : pkg->top_level_insts) {
            auto& inst = pkg->instructions[ref];
            std::println(stderr,"  [{}] %{} = ConstDecl({})", ti++, ref, inst.const_decl.ident);
        }
    }

}
