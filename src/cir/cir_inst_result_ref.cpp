#include "cir_inst_result_ref.hpp"
#include "cir_package.hpp"


Ref<CIRInstResult> Ref<CIRInstResult>::init(CIRPackage* pkg, CIRInstructionRef ref,
                                            Ref<CIRResultInstance> ri) {
    return Ref<CIRInstResult>{.cir_package = pkg, .inst_ref = ref, .result_instance = ri};
}

CIRInstResult& Ref<CIRInstResult>::resolve() const {
    ASSERT(cir_package != nullptr);
    return cir_package->result_of(inst_ref, result_instance);
}

CIRInstResult* Ref<CIRInstResult>::get_result() const {
    if(result_instance.is_valid()) {
        return result_instance->result_ptr_of(inst_ref);
    }
    return &cir_package->result_of(inst_ref);
}

const CIRInstruction& Ref<CIRInstResult>::inst() const {
    return cir_package->inst(inst_ref);
}
