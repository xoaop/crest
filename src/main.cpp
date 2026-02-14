#include <stdio.h>
#include <format>

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>

#include "xoaop.h"
#include "common.hpp"
#include "context.hpp"
#include "string_map.hpp"
#include "tokenizer.hpp"
#include "symbol.hpp"
#include "parser.hpp"
#include "analyser.hpp"
#include "resolve_depend.hpp"
#include "llvm_generate_ir.hpp"



int main(int argc, char** argv) {
    
    
    defer(printf("\n\nEXIT!"));

    // LLVM初始化
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeDisassembler();


    // 内存分配器初始化
    global_allocators_init();
    defer(global_allocators_free());
    

    // 关键字表初始化和测试
    init_keyword_map();
    test_keyword_map();



    
    // 初始化context
    context()->global_blank_package = make_package(xp_make_string(permanent_allocator(), "<global_blank_package>"), permanent_allocator());
    context()->global_blank_package.package_scope = make_scope(NULL, ScopeType::Global, permanent_allocator());
    
    context()->reporter = make_error_reporter(permanent_allocator());

    // 类型系统初始化
    init_type_table(context());



    // TODO(xoaop): 参数解析
    if(argc < 2) {
        printf("usage: crest <path>\n");
        return -1;
    }

    char const *path_of_main_dir = argv[1];

    Array<Package> all_packages = resolve_dependencies(xp_make_string(permanent_allocator(), path_of_main_dir));

    sema_analysis_all_packages(all_packages);
    
    context()->reporter.print_msg();

    if(context()->reporter.error_msgs.count > 0) {
        // 如果有错误信息, 就不继续生成IR了
        return -1;
    }

    gen_ir_all_packages(all_packages);

    
    
    return 0;
}