#include "cir_inst_result_ref.hpp"
#include "cir_package.hpp"
#include "cir_package_ref.hpp"


Ref<CIRInstResult> Ref<CIRInstResult>::init(CIRPackage* pkg, CIRInstructionRef ref,
                                            Ref<CIRResultInstance> ri) {
    return Ref<CIRInstResult>{
        .cir_package = Ref<CIRPackage>(pkg->package_ref.index),
        .inst_ref = ref,
        .result_instance = ri
    };
}

CIRInstResult& Ref<CIRInstResult>::resolve() const {
    ASSERT(cir_package != Ref<CIRPackage>::INVALID_REF);
    return cir_package->result_of(inst_ref, result_instance);
}

CIRInstResult* Ref<CIRInstResult>::get_result() const {
    if(result_instance != Ref<CIRResultInstance>::INVALID_REF) {
        return result_instance->result_ptr_of(inst_ref);
    }
    return &cir_package->result_of(inst_ref);
}

const CIRInstruction& Ref<CIRInstResult>::inst() const {
    return cir_package->inst(inst_ref);
}
