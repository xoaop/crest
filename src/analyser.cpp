#include "analyser.hpp"

#include "common.hpp"

#include "decl_collect.hpp"

xp_internal void analyser_init(Analyser *analyser, xpAllocator allocator);
xp_internal void analyser_free(Analyser *analyser);

void push_symbol_table(Analyser *analyser) {
    SymbolTable table = make_symbol_table(permanent_allocator());
    array_push_back(&analyser->symbol_table_stack, table);
}

void pop_symbol_table(Analyser *analyser) {
    array_pop_back(&analyser->symbol_table_stack);
}

bool at_global_scope(Analyser *analyser) {
    return analyser->symbol_table_stack.count == 1;
}


void semantic_analysis_ast_file(AstFile *ast_file) {

    defer(xp_free_all(temp_allocator()));

    Analyser analyser;
    analyser_init(&analyser, permanent_allocator());

    // TODO: More Stages
    declaration_collect_ast_file(ast_file, &analyser);
    resolve_ast_file(ast_file, &analyser);

    // analyser_free(&analyser);
    *symbol_table() = analyser.symbol_table_stack[0];
    return;
}



xp_internal void resolve_var_decl(Ast *var_decl_ast, Analyser *analyser);
xp_internal void resolve_stmt(Ast *stmt_ast, Analyser *analyser);
xp_internal void resolve_expr(Ast *expr_ast, Analyser *analyser);



void resolve_ast_file(AstFile *ast_file, Analyser *analyser) {

    static AstVisitorFunc resolvers[AstType_COUNT] = {};
    resolvers[AstType_VariableDecl] = resolve_var_decl;
    resolvers[AstType_Break] = resolve_stmt;
    resolvers[AstType_Continue] = resolve_stmt;
    resolvers[AstType_VarExpr] = resolve_expr;
    resolvers[AstType_FunctionCallExpr] = [](Ast *ast, Analyser *analyser) {

        SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, ast->FunctionCallExpr.name);
        XP_ASSERT_MSG(info != NULL && info->type == SymbolType_Function, "function not exist");
        XP_ASSERT_MSG(info->Function.param_types.count == ast->FunctionCallExpr.args.count, "function arg count mismatch");
    };
    

    ast_visitor(ast_file->root, resolvers, analyser);

    return;
}




xp_internal void resolve_var_decl(Ast *var_decl_ast, Analyser *analyser) {
    SymbolInfo *existing = find_symbol(&analyser->symbol_table_stack, var_decl_ast->VariableDecl.var_name);
    if(existing != NULL) {
        // TODO(xoaop): 异常处理, 同一作用域变量重复声明
        printf("---------------------------------");
        print_ast(var_decl_ast);
        XP_ASSERT_MSG(0, "var decl repeat in the same scope");
    }


    SymbolInfo info = make_symbol_info(SymbolType_VariableDecl);
    info.VariableDecl.name = var_decl_ast->VariableDecl.var_name;
    info.VariableDecl.type = var_decl_ast->v_type;
    add_symbol(&analyser->symbol_table_stack, var_decl_ast->VariableDecl.var_name, make_symbol_info(SymbolType_VariableDecl));


    return;
}


xp_internal void resolve_stmt(Ast *stmt_ast, Analyser *analyser) {

    switch (stmt_ast->type)
    {

    case AstType_Break: {
        // TODO
        XP_ASSERT_DEFAULT(analyser->loop_ast_stack.count > 0);
    } break;
    case AstType_Continue: {
        // TODO
        XP_ASSERT_DEFAULT(analyser->loop_ast_stack.count > 0);
    } break;

    default: {
    } break;
    
    }
} 

xp_internal void resolve_expr(Ast *expr_ast, Analyser *analyser) {
    switch (expr_ast->type)
    {
    case AstType_VarExpr:
        SymbolInfo *entry;
        if(!(entry = find_symbol(&analyser->symbol_table_stack, expr_ast->VarExpr.name))) {
            // TODO: 错误处理, 使用未声明变量 
            XP_ASSERT_DEFAULT(0);
        }
        // expr_ast->VarExpr.name = entry->unique_name;
        break;
    default:
        break;
    }
} 




xp_internal void analyser_init(Analyser *analyser, xpAllocator allocator) {
    analyser->loop_ast_stack = make_array<Ast *>(allocator);
    analyser->symbol_table_stack = make_array<SymbolTable>(allocator);

    // NOTE: 这是全局符号表
    push_symbol_table(analyser);
}

xp_internal void analyser_free(Analyser *analyser) {
    array_free(&analyser->loop_ast_stack);
    array_free(&analyser->symbol_table_stack);
}




void ast_visitor(Array<Ast *> ast_array, AstVisitorFunc visit_func[AstType_COUNT], Analyser *analyser) {
    XP_ASSERT_DEFAULT(visit_func != NULL);

    for(isize i = 0; i < ast_array.count; i++) {
        ast_visitor(ast_array[i], visit_func, analyser);
    }
}



void ast_visitor(Ast *ast, AstVisitorFunc visit_func[AstType_COUNT], Analyser *analyser) {
    XP_ASSERT_DEFAULT(visit_func != NULL);

    if(ast == NULL) {
        return;
    }

    if(visit_func[ast->type] != NULL) {
        visit_func[ast->type](ast, analyser);
    }

    switch (ast->type)
    {
        case AstType_Function: {
            new_scope(analyser);
            for(isize i = 0; i < ast->Function.params.count; i++) {
                ast_visitor(ast->Function.params[i], visit_func, analyser);
            }
            ast_visitor(ast->Function.block, visit_func, analyser);
        } break;

        case AstType_Block: {
            if(ast->Block.is_function_body) {
                for(isize i = 0; i < ast->Block.statements.count; i++) {
                    ast_visitor(ast->Block.statements[i], visit_func, analyser);
                }
                break;
            }

            new_scope(analyser);
            for(isize i = 0; i < ast->Block.statements.count; i++) {
                ast_visitor(ast->Block.statements[i], visit_func, analyser);
            }
        } break;

        case AstType_IfStmt: {
            ast_visitor(ast->IfStmt.condition, visit_func, analyser);
            ast_visitor(ast->IfStmt.then_block, visit_func, analyser);
            ast_visitor(ast->IfStmt.else_block, visit_func, analyser);
        } break;

        case AstType_ForStmt: {

            new_scope(analyser);

            array_push_back(&analyser->loop_ast_stack, ast);
            
            ast_visitor(ast->ForStmt.init, visit_func, analyser);
            ast_visitor(ast->ForStmt.condition, visit_func, analyser);
            ast_visitor(ast->ForStmt.post, visit_func, analyser);
            ast_visitor(ast->ForStmt.body, visit_func, analyser);

            array_pop_back(&analyser->loop_ast_stack);
        } break;

        case AstType_ReturnStmt: {
            ast_visitor(ast->ReturnStmt.expr, visit_func, analyser);
        } break;

        case AstType_VariableDecl: {
            ast_visitor(ast->VariableDecl.expr, visit_func, analyser);
        } break;

        case AstType_Assignment: {
            ast_visitor(ast->Assignment.left_var_expr, visit_func, analyser);
            ast_visitor(ast->Assignment.right_expr, visit_func, analyser);
        } break;

        case AstType_BinaryExpr: {
            ast_visitor(ast->BinaryExpr.left, visit_func, analyser);
            ast_visitor(ast->BinaryExpr.right, visit_func, analyser);
        } break;

        case AstType_UnrayExpr: {
            ast_visitor(ast->UnrayExpr.operand, visit_func, analyser);
        } break;

        case AstType_FunctionCallExpr: {
            for(isize i = 0; i < ast->FunctionCallExpr.args.count; i++) {
                ast_visitor(ast->FunctionCallExpr.args[i], visit_func, analyser);
            }
        } break;

        default: {
            // Nope
        } break;
    }
}

