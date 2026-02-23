#ifndef CREST_LLVM_GENERATE_IR_HPP
#define CREST_LLVM_GENERATE_IR_HPP

#include "parser.hpp"
#include "package.hpp"

#include "llvm-c/Core.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Analysis.h"
#include "llvm-c/BitWriter.h"






void gen_ir_all_packages(Array<Package> all_packages);





#endif