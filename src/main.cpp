#include <stdio.h>
#include <chrono>

#include "print.hpp"
#include "file.hpp"
#include "path.hpp"

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>

#include "thread_pool/thread_pool.hpp"


#include "xoaop.h"
#include "common.hpp"
#include "context.hpp"
#include "cir_interpreter.hpp"
#include "compile.hpp"

#include "tokenizer.hpp"
#include "parser.hpp"
#include "analyser.hpp"
#include "cir_builder.hpp"
#include "llvm_generate_ir.hpp"
#include "stable_ordered_array.hpp"






static void crest_helper() {
    println_out("Usage: crest <command> [options]");
    println_out("Commands:");
    println_out("  build <path>   Build the project at the specified path");
    println_out("  crest <path>   Same as 'crest build <path>'");
    println_out("  help           Show this help message");
    println_out("Options:");
    println_out("  -o <path>      Output directory");
    println_out("  -trace         Enable debug trace output");
    println_out("  -cir_dump      Dump CIR instructions");
    println_out("  -scope_dump    Dump scope tree");
}



int main(int argc, char** argv) {
    
    defer(DEBUG_LOG("\n\nEXIT!"));

    auto start_time = std::chrono::high_resolution_clock::now();
    auto last_time = start_time;

    auto mark_stage = [&](const char* name) {
        auto now = std::chrono::high_resolution_clock::now();
        using Sec = std::chrono::duration<double>;
        auto total = std::chrono::duration_cast<Sec>(now - start_time).count();
        auto since_last = std::chrono::duration_cast<Sec>(now - last_time).count();
        println_out("[phase] {:>10.6f}s (+{:>10.6f}s) {}", total, since_last, name);
        last_time = now;
    };
    
    #ifdef CREST_DEBUG
    // test_stable_ordered_array();
    // test_array_perf();
    #endif

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
                err("Missing path argument for build command");
                return -1;
            }

            main_path = argv[i];
            
        } else if(strcmp(argv[i], "help") == 0) {
            
            crest_helper();
            return 0;

        } else if(strcmp(argv[i], "-trace") == 0) {
#if defined(CREST_DEBUG)
            g_trace_enabled = true;
#endif
        } else if(strcmp(argv[i], "-cir_dump") == 0) {
            context()->cir_dump = true;
        } else if(strcmp(argv[i], "-scope_dump") == 0) {
            context()->scope_dump = true;
        } else if(strcmp(argv[i], "-o") == 0) {
            // TODO(xoaop): 输出文件路径参数解析
            i += 1; // 跳过 "-o" 参数

            if(i >= argc) {
                err("Missing path argument for -o option");
                return -1;
            }

            context()->output_path = std::filesystem::path(argv[i]);
        }
        
        else if(main_path == NULL) {
            main_path = argv[i];
        } else {
            crest_helper();
            return 0;
        }
    }





    





    // 内存分配器初始化
    global_allocators_init();
    defer(global_allocators_free());
    

    // 关键字表初始化
    init_keyword_map();

    
    // 初始化context
    std::string exe_path = get_program_path();
    context()->compiler_path = std::filesystem::absolute(std::filesystem::path(exe_path)).parent_path();
    context()->current_working_directory = std::filesystem::current_path();

    println_out("Compiler path: {}", context()->compiler_path.string());
    println_out("Current working directory: {}", context()->current_working_directory.string());



    // 初始化package搜索路径
    context()->package_search_paths = make_array<xpString>(permanent_allocator());
    context()->package_search_paths.push_back(xp_make_string(permanent_allocator(), context()->compiler_path.string().c_str()));


    context()->reporter = make_error_reporter(permanent_allocator());

    // 类型系统初始化
    init_type_table(context());
    defer(free_type_table(context()));


    context()->all_packages = make_array<Package>(permanent_allocator());
    context()->all_scopes = make_array<Scope>(permanent_allocator());
    context()->static_mem.init(MemoryKind::String, permanent_allocator());



    // builtin 完整构建+分析（删全局循环后没有 import 会触发它，必须显式做，且在 main 之前）。
    auto builtin_pkg_opt = compile_package_from_import(xp_string_c("std/builtin"));
    if(builtin_pkg_opt.is_none()) {
        err("std/builtin package not found");
        return 1;
    }

    xpString main_dir;
    if(is_existing_directory(xp_string_c(main_path))) {
        main_dir = xp_string_c(main_path);
    } else {
        std::filesystem::path p{std::string(main_path)};
        main_dir = xp_make_string(permanent_allocator(), p.parent_path().string().c_str());
    }
    context()->package_search_paths.push_back(main_dir);
    context()->main_src_dir_path = main_dir;


    auto main_pkg_opt = compile_package_from_path(xp_string_c(main_path));
    if(main_pkg_opt.is_none()) {
        context()->reporter.report_error("main package path '{}' is not a valid directory or file", main_path);
        context()->reporter.print_msg();
        return 0;
    }

    mark_stage("analyze packages");

    if(context()->scope_dump) {
        print_scope_tree(&context()->global_blank_package.unwrap().package_scope.unwrap());
    }

    if(context()->cir_dump) {
        for(auto& pkg : context()->all_packages) {
            dump_cir_package(&pkg.cir_package);
        }
    }

    if(context()->reporter.error_count > 0) {
        context()->reporter.print_msg();
        return 0;
    }


    init_llvm();

    std::filesystem::create_directories(context()->output_path);

    LLVMIRGenerateConfig llvm_config = {};
    Array<xpString> obj_paths = gen_ir_all_packages(&context()->all_packages, llvm_config);
    
    mark_stage("generate LLVM IR");

    return 0;
}