#include "cir_inst_result_ref.hpp"
#include "cir_package.hpp"
#include "cir_package_ref.hpp"


CIRInstResultRef CIRInstResultRef::init(CIRInstructionRef ref, CIRResultInstanceRef ri) {
    return CIRInstResultRef{
        .inst_ref = ref,
        .result_instance = ri
    };
}

CIRInstResult *CIRInstResultRef::operator->() const {
    ASSERT(inst_ref.pkg != CIRPackageRef::INVALID_REF);
    return &inst_ref.pkg->result_of(inst_ref, result_instance);
}

const CIRInstResult* CIRInstResultRef::get_result() const {
    return inst_ref.pkg->find_result(inst_ref, result_instance);
}

const CIRInstruction& CIRInstResultRef::inst() const {
    return inst_ref.pkg->inst(inst_ref);
}
