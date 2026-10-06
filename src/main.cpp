#include <stdio.h>
#include <chrono>

#include "print.hpp"
#include "file.hpp"
#include "path.hpp"
#include "utf8.hpp"

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>

#include "thread_pool/thread_pool.hpp"


#include "xoaop.h"
#include "common.hpp"
#include "context.hpp"
#include "target.hpp"
#include "cir_interpreter.hpp"
#include "compile.hpp"
#include "link.hpp"

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
    println_out("  targets        List supported target platforms");
    println_out("Options:");
    println_out("  -o <path>      Output directory");
    println_out("  -c             Compile only, do not link");
    println_out("  -trace         Enable debug trace output");
    println_out("  -cir_dump      Dump CIR instructions");
    println_out("  -scope_dump    Dump scope tree");
    println_out("  -target <a>-<os>-<env>  Target platform, e.g. x86_64-windows-gnu (default: host, see 'crest targets')");
    println_out("  -cpu <name>    Override target CPU (default: generic)");
    println_out("  -features <f>  Override target features, e.g. \"+avx2,-sse4.2\" (default: from cpu)");
    println_out("  -subsystem <s> Link subsystem: console (default) | windows (Windows targets only)");
    println_out("  -crt <k>       CRT kind: static (default) | dynamic | static-debug | dynamic-debug (MSVC only)");
    println_out("  -linker <arg>  Pass an argument through to the linker, verbatim (repeatable)");
}

static void crest_targets() {
    println_out("Usage: crest build <path> -target <arch>-<os>-<env>");
    println_out("");
    println_out("  arch: {} | {} | {} | {}",
        target_arch_name(TargetArch::X86_64), target_arch_name(TargetArch::X86),
        target_arch_name(TargetArch::AArch64), target_arch_name(TargetArch::Arm));
    println_out("  os:   {} | {} | {} | {}",
        target_os_name(TargetOS::Windows), target_os_name(TargetOS::Linux),
        target_os_name(TargetOS::Darwin), target_os_name(TargetOS::FreeBSD));
    println_out("  env:  {} | {} | {}",
        target_env_name(TargetEnv::MSVC), target_env_name(TargetEnv::GNU), target_env_name(TargetEnv::Musl));
    println_out("");
    println_out("  e.g. x86_64-windows-gnu, x86_64-linux-musl, aarch64-linux-gnu");
}



int main(int argc_raw, char** argv_raw) {
    defer(DEBUG_LOG("\n\nEXIT!"));
    
    
    // 诊断输出不做缓冲
    setvbuf(stderr, nullptr, _IONBF, 0);



    auto start_time = std::chrono::high_resolution_clock::now();
    auto last_time = start_time;

    auto mark_stage = [&](const char* name) {
        auto now = std::chrono::high_resolution_clock::now();
        auto total = std::chrono::duration_cast<std::chrono::duration<double>>(now - start_time).count();
        auto since_last = std::chrono::duration_cast<std::chrono::duration<double>>(now - last_time).count();

        println_out("[phase] {:>10.6f}s (+{:>10.6f}s) {}", total, since_last, name);
        
        last_time = now;
    };


    char const *main_path = nullptr;
    bool compile_only = false;
    TargetInfo target_arg;
    bool target_given = false;
    bool crt_given = false;
    bool subsystem_given = false;

    // 命令行统一成 UTF-8 后再解析
    const std::vector<std::string> args_utf8 = get_utf8_args(argc_raw, argv_raw);
    std::vector<const char*> args_c;
    for(const auto& arg : args_utf8) {
        args_c.push_back(arg.c_str());
    }
    args_c.push_back(nullptr);
    const char **argv = args_c.data();
    const isize argc = (isize)args_utf8.size();

    if(argc < 2) {
        crest_helper();
        return 0;
    }


    context()->current_working_directory = std::filesystem::current_path();
    context()->output_path = context()->current_working_directory; // 默认输出路径为当前工作目录

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

        } else if(strcmp(argv[i], "targets") == 0) {

            crest_targets();
            return 0;

        } else if(strcmp(argv[i], "-trace") == 0) {
#if defined(CREST_DEBUG)
            g_trace_enabled = true;
#endif
        } else if(strcmp(argv[i], "-cir_dump") == 0) {
            context()->cir_dump = true;
        } else if(strcmp(argv[i], "-scope_dump") == 0) {
            context()->scope_dump = true;
        } else if(strcmp(argv[i], "-c") == 0) {
            compile_only = true;
        } else if(strcmp(argv[i], "-o") == 0) {
            // TODO(xoaop): 输出文件路径参数解析
            i += 1; // 跳过 "-o" 参数

            if(i >= argc) {
                err("Missing path argument for -o option");
                return -1;
            }

            context()->output_path = std::filesystem::path(as_u8(std::string(argv[i])));
        } else if(strcmp(argv[i], "-target") == 0) {
            i += 1; // 跳过 "-target" 参数

            if(i >= argc) {
                err("Missing argument for -target option");
                return -1;
            }

            if(!parse_target_spec(argv[i], target_arg)) {
                err("invalid -target: {} (expect <arch>-<os>-<env>, see 'crest targets')", argv[i]);
                return -1;
            }
            target_given = true;
        } else if(strcmp(argv[i], "-cpu") == 0) {
            i += 1; // 跳过 "-cpu" 参数

            if(i >= argc) {
                err("Missing argument for -cpu option");
                return -1;
            }

            context()->target_cpu = argv[i];
        } else if(strcmp(argv[i], "-features") == 0) {
            i += 1; // 跳过 "-features" 参数

            if(i >= argc) {
                err("Missing argument for -features option");
                return -1;
            }

            context()->target_features = argv[i];
        } else if(strcmp(argv[i], "-subsystem") == 0) {
            i += 1; // 跳过 "-subsystem" 参数

            if(i >= argc) {
                err("Missing argument for -subsystem option");
                return -1;
            }

            if(!parse_subsystem_arg(argv[i], context()->link_subsystem)) {
                err("invalid -subsystem: {} (expect console | windows)", argv[i]);
                return -1;
            }
            subsystem_given = true;
        } else if(strcmp(argv[i], "-crt") == 0) {
            i += 1; // 跳过 "-crt" 参数

            if(i >= argc) {
                err("Missing argument for -crt option");
                return -1;
            }

            if(!parse_crt_arg(argv[i], context()->link_crt)) {
                err("invalid -crt: {} (expect static | dynamic | static-debug | dynamic-debug)", argv[i]);
                return -1;
            }
            crt_given = true;
        } else if(strcmp(argv[i], "-linker") == 0) {
            i += 1; // 跳过 "-linker" 参数

            if(i >= argc) {
                err("Missing argument for -linker option");
                return -1;
            }
            if(argv[i][0] == '\0') {
                err("-linker needs a non-empty argument");
                return -1;
            }

            context()->linker_args.push_back(argv[i]);
        }

        else if(main_path == nullptr) {
            main_path = argv[i];
        } else {
            crest_helper();
            return 0;
        }
    }

    if(target_given) {
        context()->target = target_arg;
        context()->target_triple = make_target_triple(target_arg);
    } else {
        // 未显式 -target：沿用宿主 triple
        char *host_triple = LLVMGetDefaultTargetTriple();
        context()->target_triple = host_triple;
        context()->target = parse_target_triple(host_triple);
        LLVMDisposeMessage(host_triple);
    }

    if(compile_only) {
        if(subsystem_given) {
            err("-subsystem has no effect with -c (compile only)");
            return -1;
        }
        if(crt_given) {
            err("-crt has no effect with -c (compile only)");
            return -1;
        }
        if(!context()->linker_args.empty()) {
            err("-linker has no effect with -c (compile only)");
            return -1;
        }
    } else if(!target_is_windows()) {
        if(subsystem_given) {
            err("-subsystem is only supported for Windows targets (target: {})", context()->target_triple);
            return -1;
        }
        if(crt_given) {
            err("-crt is only supported for MSVC targets (target: {})", context()->target_triple);
            return -1;
        }
    } else if(!target_uses_msvc() && crt_given) {
        err("-crt is only supported for MSVC targets (target: {})", context()->target_triple);
        return -1;
    }











    // 内存分配器初始化
    global_allocators_init();
    defer(global_allocators_free());
    

    // 关键字表初始化
    init_keyword_map();

    
    // 初始化context
    const std::string exe_path = get_program_path();
    context()->compiler_path = std::filesystem::path(as_u8(exe_path)).parent_path();

    println_out("Compiler path: {}", (const char *)context()->compiler_path.generic_u8string().c_str());
    println_out("Current working directory: {}", (const char *)context()->current_working_directory.generic_u8string().c_str());



    // 初始化package搜索路径
    context()->package_search_paths = make_array<xpString>(permanent_allocator());
    context()->package_search_paths.push_back(xp_make_string(permanent_allocator(), (const char *)context()->compiler_path.generic_u8string().c_str()));


    context()->reporter = make_error_reporter(permanent_allocator());

    // 类型系统初始化
    init_type_table(context());
    defer(free_type_table(context()));


    // TODO: CIRPackage * has risk
    context()->all_packages = make_array_capacity<Package>(permanent_allocator(), 128);

    context()->all_scopes = make_array<Scope>(permanent_allocator());
    // 闲置：static_mem
    // context()->static_mem.init(MemoryKind::String, permanent_allocator());
    context()->static_values = ValueArray::init(permanent_allocator());
    context()->lcir_modules = xp_hash_map_make<Ref<Package>, lcir::Module>(permanent_allocator());

    // 各包的 stage arena 统一在退出时回收（含以下所有报错早退路径）
    defer({
        for(auto& pkg: context()->all_packages) {
            xp_free_all(pkg.stage_allocator);
        }
    });

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
        main_dir = xp_make_string(permanent_allocator(), (const char *)std::filesystem::path(as_u8(xp_string_c(main_path))).parent_path().generic_u8string().c_str());
    }
    context()->main_src_dir_path = main_dir;
    context()->package_search_paths.push_back(main_dir);


    auto main_pkg_opt = compile_package_from_path(xp_string_c(main_path));
    if(main_pkg_opt.is_none()) {
        err("main package path '{}' is not a valid directory or file", main_path);
        return 1;
    }

    mark_stage("analyze packages");

    
    if(context()->scope_dump) {
        print_scope_tree(&context()->global_blank_package.unwrap().package_scope.unwrap());
        return 0;
    }
    
    

    if(context()->reporter.error_count > 0) {
        context()->reporter.print_msg();
        return 1;
    }

    // TODO: move to a function
    // std::for_each(context()->lcir_modules.begin(), context()->lcir_modules.end(), [](const auto& entry) {
    //     auto& module = entry.value;

    //     println_out("Module for package: {}", entry.key.unwrap().path);

    //     std::for_each(module.functions.begin(), module.functions.end(), [](const auto& func) {
    //         println_out("Function: {} (linkage: {})", func.value.raw_name, static_cast<int>(func.value.linkage));
    //     });

    //     println_out("");
    // });


    init_llvm();

    std::filesystem::create_directories(context()->output_path);

    LLVMIRGenerateConfig llvm_config;
    Array<xpString> obj_paths = gen_ir_all_packages(context()->lcir_modules, llvm_config);
    
    mark_stage("generate LLVM IR");

    // 链接：把各包的 .o 链成可执行文件（-c 时只编译不链接）
    if(!compile_only && obj_paths.count > 0) {
        const std::filesystem::path main_p(as_u8(xp_string_c(main_path)));
        std::string binary_stem = (const char *)(main_p.has_extension() ? main_p.stem() : main_p.filename()).generic_u8string().c_str();
        if(binary_stem.empty()) {
            binary_stem = "output";
        }

        LinkRequest link_req;
        link_req.obj_paths = obj_paths;
        link_req.output_binary_path = (const char *)(context()->output_path / (binary_stem + target_exe_suffix())).generic_u8string().c_str();

        if(!link_objects(link_req)) {
            return 1;
        }

        mark_stage("link");
    }

    return 0;
}
