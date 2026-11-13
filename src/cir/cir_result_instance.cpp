#include "cir_result_instance.hpp"

#include "print.hpp"


CIRResultInstance CIRResultInstance::init(xpAllocator allocator, Ref<CIRResultInstance> parent) {
    ASSERT(parent != Ref<CIRResultInstance>::INVALID_REF);

    CIRResultInstance inst;
    inst.results = xp_hash_map_make<CIRInstructionRef, CIRInstResult>(allocator);
    inst.parent = parent;
    return inst;
}

CIRResultInstance CIRResultInstance::init_root(xpAllocator allocator) {
    CIRResultInstance inst;
    inst.results = xp_hash_map_make<CIRInstructionRef, CIRInstResult>(allocator);
    return inst;
}

const CIRInstResult* CIRResultInstance::result_ptr_of(CIRInstructionRef ref) const {
    return xp_hash_map_get(results, ref);
}

CIRInstResult& CIRResultInstance::result_of(CIRInstructionRef ref) {
    return results.get(ref);
}

const CIRInstResult* CIRResultInstance::find_along_chain(CIRInstructionRef ref) const {
    if(const CIRInstResult *r = result_ptr_of(ref)) {
        return r;
    }
    for(Ref<CIRResultInstance> it = parent; it != Ref<CIRResultInstance>::INVALID_REF; it = it->parent) {
        if(const CIRInstResult *r = it->result_ptr_of(ref)) {
            return r;
        }
    }
    return nullptr;
}
