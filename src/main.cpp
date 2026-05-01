#include <stdio.h>
#include <print>
#include <format>
#include <chrono>

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>

#include "thread_pool/thread_pool.hpp"
#include "file.hpp"


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

static void crest_helper() {
    printf("Usage: crest <command> [options]\n");
    printf("Commands:\n");
    printf("  build <path>   Build the project at the specified path\n");
    printf("  help           Show this help message\n");
}



int main(int argc, char** argv) {
    defer(printf("\n\nEXIT!"));


    auto start_time = std::chrono::high_resolution_clock::now();
    auto last_time = start_time;
    auto end_time = start_time;
    
    
    char const *main_path = NULL;

    if(argc < 2) {
        crest_helper();
        return 0;
    }


    context()->output_path = std::filesystem::current_path(); // 默认输出路径为当前工作目录

    // TODO(xoaop): 参数解析
    for(isize i = 1; i < argc; i++) {
        if(strcmp(argv[i], "build") == 0) {
            // Build

            i += 1; // 跳过 "build" 参数

            if(i >= argc) {
                printf("Error: Missing path argument for build command\n");
                return -1;
            }

            main_path = argv[i];
            
        } else if(strcmp(argv[i], "help") == 0) {
            crest_helper();
            return 0;
        } else if(strcmp(argv[i], "-o") == 0) {
            // TODO(xoaop): 输出文件路径参数解析
            i += 1; // 跳过 "-o" 参数

            if(i >= argc) {
                printf("Error: Missing path argument for -o option\n");
                return -1;
            }

            context()->output_path = std::filesystem::path(argv[i]);
        }
        
        else {
            crest_helper();
            return 0;
        }
    }





    





    // 内存分配器初始化
    global_allocators_init();
    defer(global_allocators_free());
    

    // 关键字表初始化和测试
    init_keyword_map();
    // test_keyword_map();



    
    // 初始化context
    std::string exe_path = get_program_path();
    context()->compiler_path = std::filesystem::absolute(std::filesystem::path(exe_path)).parent_path();
    context()->current_working_directory = std::filesystem::current_path();

    std::println("Compiler path: {}", context()->compiler_path.string());
    std::println("Current working directory: {}", context()->current_working_directory.string());


    context()->thread_pool = new ThreadPool(std::thread::hardware_concurrency());
    defer(delete context()->thread_pool);


    context()->global_blank_package = make_package(xp_make_string(permanent_allocator(), "<global_blank_package>"), permanent_allocator());
    context()->global_blank_package.package_scope = make_scope(NULL, ScopeType::Global, permanent_allocator());
    context()->ast_scope_map = xp_hash_map_make<Ast *, Scope *>(permanent_allocator());

    context()->reporter = make_error_reporter(permanent_allocator());

    // 类型系统初始化
    init_type_table(context());


    end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    std::println("Initialization time: {:.5f} s", duration.count());
    last_time = end_time;



    Array<Package> all_packages = resolve_dependencies(xp_string_c(main_path));
    std::reverse(all_packages.begin(), all_packages.end()); // 反转包的顺序, 让被依赖的包在前面, 这样后续分析的时候就不需要担心包的依赖顺序了

    context()->all_packages = all_packages;

    
    end_time = std::chrono::high_resolution_clock::now();
    duration = end_time - last_time;
    printf("Dependency resolution time: %.5f s\n", duration.count());
    last_time = end_time;
    
    if(context()->reporter.error_count > 0) {
        // 如果有错误信息, 就不继续进行语义分析了
        context()->reporter.print_msg();
        return -1;
    }

    sema_analysis_all_packages(&context()->all_packages);

    end_time = std::chrono::high_resolution_clock::now();
    duration = end_time - last_time;
    printf("Semantic analysis time: %.5f s\n", duration.count());
    last_time = end_time;

    
    if(context()->reporter.error_count > 0) {
        // 如果有错误信息, 就不继续生成IR了
        context()->reporter.print_msg();
        return -1;
    }

    
    init_llvm();
    Array<xpString> obj_paths = gen_ir_all_packages(all_packages, LLVMIRGenerateConfig{.optimization_level = LLVMIROptimizationLevel::O3});
    
    end_time = std::chrono::high_resolution_clock::now();
    duration = end_time - last_time;
    printf("LLVM IR generation time: %.5f s\n", duration.count());


    // 链接
    


    duration = end_time - start_time;
    printf("Total compilation time: %.5f s\n", duration.count());

    
    return 0;
}