#include "cir_result_instance_ref.hpp"
#include "cir_package.hpp"


CIRResultInstance& Ref<CIRResultInstance>::resolve() const {
    ASSERT(cir_package.is_valid() && index >= 0);
    ASSERT(index < cir_package->result_instances.count);

    return cir_package->result_instances[index];
}
