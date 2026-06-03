#pragma once

#include "array.hpp"
#include "value.hpp"
#include "cir_builder.hpp"

struct CIRFile;

struct Interpreter {
    Interpreter(xpAllocator allocator);
    ~Interpreter();

    void analyze_cir_file(CIRFile* cir_file);
    void eval_instruction(CIRFile* cir_file, CIRInstruction* inst_ref);


public:
    // state
    Array<Value> local_variables; 

};
