#include <stdio.h>
#include <format>

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>

#include "xoaop.h"
#include "common.hpp"
#include "tokenizer.hpp"
#include "string_map.hpp"
#include "symbol.hpp"
#include "parser.hpp"
#include "analyser.hpp"
#include "llvm_generate_ir.hpp"


#include "resolve_depend.hpp"
#include "context.hpp"

int main(int argc, char** argv) {
    
    // 内存分配器初始化
    global_allocators_init();
    defer(global_allocators_free());
    

    // 关键字表初始化和测试
    init_keyword_map();
    test_keyword_map();



    
    // 初始化context
    context()->global_scope = make_scope(NULL, ScopeType::Global, permanent_allocator());
    


    // 类型系统初始化
    init_type_table(context());



    // TODO NEW:
    if(argc < 2) {
        printf("usage: crest <path>\n");
        return -1;
    }

    char const *path_of_main_dir = argv[1];
    context()->main_src_dir_path = path_of_main_dir;

    Array<Package> all_packages = resolve_dependencies(xp_make_string(permanent_allocator(), path_of_main_dir));


    sema_analysis_all_packages(all_packages);

    gen_ir_all_packages(all_packages);

    // TODO OLD:
    // TODO(xoaop): 参数解析
    // if(argc < 2) {
    //     printf("usage: crest <path>");
    //     return -1;
    // }

    

    // char const *path = argv[1];

    // xpString code_str = file_to_string(path, permanent_allocator());
    
    // Tokenizer tokenizer;
    // tokenizer_init(&tokenizer, code_str);
    // tokenize(&tokenizer);

    
    // #if 1
    // for(isize i = 0; i < tokenizer.token_array.count; i++) {
    //     Token token = tokenizer.token_array[i];
    //     extern const char* token_strings[];
    //     printf("line %lld column %lld: %s  type: %s\n", token.line_index, token.column_index, token.token_str.c_str, token_strings[(usize)token.type]);
    // }
    // #endif

    // #if 1
    // AstFile f = parse_file(tokenizer.token_array);
    // #endif

    // print_ast(f.top_levels);
    // printf("\n=========================================\n");

    // semantic_analysis_ast_file(&f);
    
    // print_ast(f.top_levels);
    // printf("\n=========================================\n");

    // gen_ir_astfile(f);
    // print_ast(f.top_levels);

    
    
    printf("\n\nEXIT!");
    return 0;
}