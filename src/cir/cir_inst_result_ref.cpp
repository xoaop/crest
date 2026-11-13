#include "cir_inst_result_ref.hpp"
#include "cir_package.hpp"
#include "cir_package_ref.hpp"


Ref<CIRInstResult> Ref<CIRInstResult>::init(CIRInstructionRef ref, Ref<CIRResultInstance> ri) {
    return Ref<CIRInstResult>{
        .inst_ref = ref,
        .result_instance = ri
    };
}

CIRInstResult& Ref<CIRInstResult>::resolve() const {
    ASSERT(inst_ref.pkg != Ref<CIRPackage>::INVALID_REF);
    return inst_ref.pkg->result_of(inst_ref, result_instance);
}

const CIRInstResult* Ref<CIRInstResult>::get_result() const {
    return inst_ref.pkg->find_result(inst_ref, result_instance);
}

const CIRInstruction& Ref<CIRInstResult>::inst() const {
    return inst_ref.pkg->inst(inst_ref);
}
