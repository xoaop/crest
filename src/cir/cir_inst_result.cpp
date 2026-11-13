#include "cir_inst_result.hpp"

#include "print.hpp"
#include "type.hpp"


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
