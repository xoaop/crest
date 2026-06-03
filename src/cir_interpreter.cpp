#include "cir_interpreter.hpp"

Interpreter::Interpreter(xpAllocator allocator) {
    local_variables = make_array<Value>(allocator);
}

Interpreter::~Interpreter() {
    array_free(&local_variables);
}


void Interpreter::analyze_cir_file(CIRFile* cir_file) {
    for(CIRInstructionRef top: cir_file->top_level_insts) {
        CIRInstruction *inst = &cir_file->instructions[top];
        eval_instruction(cir_file, inst);
    }
}


void Interpreter::eval_instruction(CIRFile* cir_file, CIRInstruction* inst_ref) {
    switch(inst_ref->op) {
        case CIROperator::ConstDecl: {

        } break;

        case CIROperator::Block: {
        } break;

        default: {
            XP_ASSERT_MSG(false, "Unsupported CIR instruction for interpretation)");
        } break;

    }
}