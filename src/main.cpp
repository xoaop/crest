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
#include "cir_interpreter.hpp"

#include "tokenizer.hpp"
#include "parser.hpp"
#include "resolve_depend.hpp"
#include "analyser.hpp"
#include "cir_builder.hpp"
#include "llvm_generate_ir.hpp"
#include "stable_ordered_array.hpp"






static void crest_helper() {
    printf("Usage: crest <command> [options]\n");
    printf("Commands:\n");
    printf("  build <path>   Build the project at the specified path\n");
    printf("  crest <path>   Same as 'crest build <path>'\n");
    printf("  help           Show this help message\n");
    printf("Options:\n");
    printf("  -o <path>      Output directory\n");
    printf("  -trace         Enable debug trace output\n");
    printf("  -cir_dump      Dump CIR instructions\n");
    printf("  -scope_dump    Dump scope tree\n");
}



int main(int argc, char** argv) {
    
    defer(printf("\n\nEXIT!"));

    auto start_time = std::chrono::high_resolution_clock::now();
    auto last_time = start_time;

    auto mark_phase = [&](const char* name) {
        auto now = std::chrono::high_resolution_clock::now();
        using Sec = std::chrono::duration<double>;
        auto total = std::chrono::duration_cast<Sec>(now - start_time).count();
        auto since_last = std::chrono::duration_cast<Sec>(now - last_time).count();
        std::println(stderr, "[phase] {:>10.6f}s (+{:>10.6f}s) {}", total, since_last, name);
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
                printf("Error: Missing path argument for build command\n");
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
                printf("Error: Missing path argument for -o option\n");
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
    

    // 关键字表初始化和测试
    init_keyword_map();
    // test_keyword_map();



    
    // 初始化context
    std::string exe_path = get_program_path();
    context()->compiler_path = std::filesystem::absolute(std::filesystem::path(exe_path)).parent_path();
    context()->current_working_directory = std::filesystem::current_path();

    std::println("Compiler path: {}", context()->compiler_path.string());
    std::println("Current working directory: {}", context()->current_working_directory.string());

    // 初始化package搜索路径
    context()->package_search_paths = make_array<xpString>(permanent_allocator());
    context()->package_search_paths.push_back(xp_make_string(permanent_allocator(), context()->compiler_path.string().c_str()));


    context()->reporter = make_error_reporter(permanent_allocator());

    // 类型系统初始化
    init_type_table(context());
    defer(free_type_table(context()));


    context()->all_packages = make_array<Package>(permanent_allocator());

    // std/builtin 是必需包：提供全局 scope 与基础类型（string 等），必须存在
    auto builtin_path_opt = resolve_package_path(xp_string_c("std/builtin"), permanent_allocator());
    if (!builtin_path_opt.has_value()) {
        std::println("Error: std/builtin package not found (defines base types)");
        return 1;
    }
    Package builtin_pkg = tokenize_and_parse_package(builtin_path_opt.unwrap().as_c_str());
    builtin_pkg.package_scope = make_scope(NULL, ScopeType::Global, permanent_allocator());
    context()->all_packages.push_back(builtin_pkg);

    resolve_dependencies(xp_string_c(main_path), context()->all_packages);

    // 在所有 push_back 完成后设置指针，避免 realloc 导致指针失效
    context()->global_blank_package = &context()->all_packages[0];


    DEBUG_TRACE("All packages resolved:");
    for(auto& pkg : context()->all_packages) {
        DEBUG_TRACE("Package: {}, ast_files: {}", pkg.path, pkg.ast_files.count);
    }

    
    mark_phase("resolve dependencies");
    xp_arena_allocator_clear(stage_allocator());


    if(context()->reporter.error_count > 0) {
        context()->reporter.print_msg();
        return 0;
    }

    resolve_ast_all_packages(&context()->all_packages);
    mark_phase("tokenize + parse");
    xp_arena_allocator_clear(stage_allocator());

    if(context()->reporter.error_count > 0) {
        context()->reporter.print_msg();
        return 0;
    }

    if(context()->scope_dump) {
        print_scope_tree(&context()->global_blank_package->package_scope);
    }


    context()->static_mem.init(MemoryKind::String, permanent_allocator());
    for(auto& pkg : context()->all_packages) {
        CIRBuilder builder{stage_allocator()};
        builder.build_cir_package(&pkg);
    }
    mark_phase("build CIR");
    xp_arena_allocator_clear(stage_allocator());

    if(context()->cir_dump) {
        for(auto& pkg : context()->all_packages) {
            dump_cir_package(&pkg.cir_package);
        }
    }

    if(context()->reporter.error_count > 0) {
        context()->reporter.print_msg();
        return 0;
    }

    for(auto& pkg : context()->all_packages) {
        analyze_package(&pkg);
    }
    mark_phase("analyze CIR");
    xp_arena_allocator_clear(stage_allocator());

    if(context()->reporter.error_count > 0) {
        context()->reporter.print_msg();
        return 0;
    }

    init_llvm();

    std::filesystem::create_directories(context()->output_path);

    LLVMIRGenerateConfig llvm_config = {};
    Array<xpString> obj_paths = gen_ir_all_packages(&context()->all_packages, llvm_config);
    mark_phase("generate LLVM IR");
    xp_arena_allocator_clear(stage_allocator());

    return 0;
}