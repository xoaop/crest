#include "analyser.hpp"

#include "common.hpp"


xp_internal void analyser_init(Analyser *analyser, xpAllocator allocator);
xp_internal void analyser_free(Analyser *analyser);

void push_symbol_table(Analyser *analyser) {
    array_push_back(&analyser->symbol_table_stack, SymbolTable{});
}

void pop_symbol_table(Analyser *analyser) {
    array_pop_back(&analyser->symbol_table_stack);
}

bool at_global_scope(Analyser *analyser) {
    return analyser->symbol_table_stack.count == 1;
}



xp_internal void resolve_function(Analyser *analyser, Ast *function_ast);
xp_internal void resolve_var_decl(Analyser *analyser, Ast *var_decl_ast);
xp_internal void resolve_block(Analyser *analyser, Ast *block_ast);
xp_internal void resolve_stmt(Analyser *analyser, Ast *stmt_ast);
xp_internal void resolve_expr(Analyser *analyser, Ast *expr_ast);


// #define NEW_SCOPE                                                                          \
//     xpHashMap<xpString, MapEntry> *old_idents = analyser->identifier_map;                  \
//     defer(analyser->identifier_map = old_idents);                                          \
//     xpHashMap<xpString, MapEntry> idents;                                                  \
//     if(old_idents != NULL) {                                                               \
//         idents = xp_hash_map_copy(old_idents, temp_allocator());                           \
//     } else {                                                                               \
//         idents = xp_hash_map_make<xpString, MapEntry>(temp_allocator());                   \
//     }                                                                                      \
//     false_curr_scope(&idents);                                                             \
//     analyser->identifier_map = &idents                                                     \
// /**/


void resolve_ast_file_new(AstFile *ast_file) {
    defer(xp_free_all(temp_allocator()));

    // TODO(xoaop): 实现语义分析
    Analyser analyser;
    analyser_init(&analyser, permanent_allocator());

    static AstVisitorFunc resolvers[AstType_COUNT] = {};
    resolvers[AstType_VariableDecl] = resolve_var_decl;
    resolvers[AstType_IfStmt] = resolve_stmt;
    resolvers[AstType_ForStmt] = resolve_stmt;
    resolvers[AstType_Break] = resolve_stmt;
    resolvers[AstType_Continue] = resolve_stmt;
    resolvers[AstType_VarExpr] = resolve_expr;
    


    analyser_free(&analyser);
    return;
}

void resolve_ast_file(AstFile *ast_file) {
    defer(xp_free_all(temp_allocator()));

    // TODO(xoaop): 实现语义分析
    Analyser analyser;
    analyser_init(&analyser, permanent_allocator());

    for(isize i = 0; i < ast_file->root.count; ++i) {
        Ast *ast = ast_file->root[i];
        resolve_function(&analyser, ast);
    }

    analyser_free(&analyser);
    return;
}


xp_internal void resolve_function(Analyser *analyser, Ast *function_ast) {
    NEW_SCOPE;
    
    resolve_block(analyser, function_ast->Function.block);

    xp_free_all(temp_allocator());
    return;
}

xp_internal void resolve_var_decl(Analyser *analyser, Ast *var_decl_ast) {
    MapEntry *existing = xp_hash_map_get(*analyser->identifier_map, var_decl_ast->VariableDecl.var_name);
    if(existing && existing->curr_scope == true) {
        // TODO(xoaop): 异常处理, 同一作用域变量重复声明
        printf("---------------------------------");
        print_ast(var_decl_ast);
        XP_ASSERT_MSG(0, "var decl repeat in the same scope");
    }

    xpString unique_name = rename_ident(&analyser->name_map, var_decl_ast->VariableDecl.var_name);

    MapEntry entry;
    entry.unique_name = unique_name;
    entry.curr_scope = true;

    xp_hash_map_insert(analyser->identifier_map, var_decl_ast->VariableDecl.var_name, entry);
    var_decl_ast->VariableDecl.var_name = unique_name;

    resolve_expr(analyser, var_decl_ast->VariableDecl.expr);

    return;
}

xp_internal void resolve_block(Analyser *analyser, Ast *block_ast) {
    NEW_SCOPE;

    for(isize i = 0; i < block_ast->Block.statements.count; i++) {
        resolve_stmt(analyser, block_ast->Block.statements[i]);
    }

    return;
}

xp_internal void resolve_stmt(Analyser *analyser, Ast *stmt_ast) {

    switch (stmt_ast->type)
    {
    case AstType_VariableDecl:
        resolve_var_decl(analyser, stmt_ast);
        break;
    case AstType_ForStmt: {
        NEW_SCOPE;

        array_push_back(&analyser->loop_ast_stack, stmt_ast);
        defer(array_pop_back(&analyser->loop_ast_stack));

        // TODO
        if(stmt_ast->ForStmt.init != NULL && stmt_ast->ForStmt.init->type == AstType_VariableDecl) {
            resolve_var_decl(analyser, stmt_ast->ForStmt.init);
        } else if(stmt_ast->ForStmt.init != NULL && stmt_ast->ForStmt.init->type == AstType_Assignment) {
            resolve_expr(analyser, stmt_ast->ForStmt.init);
        } else {
            // TODO
            XP_ASSERT_DEFAULT(0);
        }

        if(stmt_ast->ForStmt.condition != NULL) {
            resolve_expr(analyser, stmt_ast->ForStmt.condition);
        }

        if(stmt_ast->ForStmt.post != NULL) {
            resolve_expr(analyser, stmt_ast->ForStmt.post);
        }

        resolve_block(analyser, stmt_ast->ForStmt.body);
        break;
    }
    case AstType_IfStmt: {
        resolve_expr(analyser, stmt_ast->IfStmt.condition);
        resolve_block(analyser, stmt_ast->IfStmt.then_block);
        if(stmt_ast->IfStmt.else_block != NULL) {
            resolve_block(analyser, stmt_ast->IfStmt.else_block);
        }
        break;
    }
    case AstType_Block: {
        resolve_block(analyser, stmt_ast);
    } break;
    case AstType_ReturnStmt: {
        resolve_expr(analyser, stmt_ast->ReturnStmt.expr);
    } break;

    case AstType_Break: {
        // TODO
        XP_ASSERT_DEFAULT(analyser->loop_ast_stack.count > 0);
    } break;
    case AstType_Continue: {
        // TODO
        XP_ASSERT_DEFAULT(analyser->loop_ast_stack.count > 0);
    } break;

    default: {
        resolve_expr(analyser, stmt_ast);
    } break;
    
    }
} 

xp_internal void resolve_expr(Analyser *analyser, Ast *expr_ast) {
    switch (expr_ast->type)
    {
    case AstType_Assignment:

        resolve_expr(analyser, expr_ast->Assignment.left_var_expr);
        resolve_expr(analyser, expr_ast->Assignment.right_expr);
        break;

    case AstType_VarExpr:
        MapEntry *entry;
        if(!(entry = xp_hash_map_get(*analyser->identifier_map, expr_ast->VarExpr.name))) {
            // TODO: 错误处理, 使用未声明变量 
            XP_ASSERT_DEFAULT(0);
        }
        expr_ast->VarExpr.name = entry->unique_name;

        break;
    case AstType_UnrayExpr:
        resolve_expr(analyser, expr_ast->UnrayExpr.operand);
        break;
    case AstType_BinaryExpr:
        resolve_expr(analyser, expr_ast->BinaryExpr.left);
        resolve_expr(analyser, expr_ast->BinaryExpr.right);
        break;

    case AstType_FunctionCallExpr:
        for(isize i = 0; i < expr_ast->FunctionCallExpr.args.count; i++) {
            resolve_expr(analyser, expr_ast->FunctionCallExpr.args[i]);
        }
        break;

    case AstType_Constant:
        // Nope
        break;
    default:
        break;
    }
} 




xp_internal void analyser_init(Analyser *analyser, xpAllocator allocator) {
    analyser->identifier_map = NULL;
    analyser->name_map = xp_hash_map_make<xpString, isize>(allocator);
    analyser->loop_ast_stack =make_array<Ast *>(allocator);
    analyser->symbol_table_stack = make_array<SymbolTable>(allocator);

    // NOTE: 这是全局符号表
    array_push_back(&analyser->symbol_table_stack, make_symbol_table(allocator));
}

xp_internal void analyser_free(Analyser *analyser) {
    xp_hash_map_free(analyser->name_map);
    array_free(&analyser->loop_ast_stack);
    array_free(&analyser->symbol_table_stack);
}











void false_curr_scope(xpHashMap<xpString, MapEntry> *identifier_map) {
    xpHashMapEntry<xpString, MapEntry> *entry;
    for(isize i = xp_hash_map_first_entry(identifier_map, &entry); i != -1; i = xp_hash_map_next_entry(identifier_map, i, &entry)) {
        entry->value.curr_scope = false;
    }
}





void ast_visitor(Array<Ast *> ast_array, AstVisitorFunc visit_func[AstType_COUNT], Analyser *analyser) {
    XP_ASSERT_DEFAULT(visit_func != NULL);

    for(isize i = 0; i < ast_array.count; i++) {
        ast_visitor(ast_array[i], visit_func, analyser);
    }
}

void ast_visitor(Ast *ast, AstVisitorFunc visit_func[AstType_COUNT], Analyser *analyser) {
    XP_ASSERT_DEFAULT(ast != NULL && visit_func != NULL);

    if(visit_func[ast->type] == NULL) {
        return;
    }

    visit_func[ast->type](ast, analyser);

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
            ast_visitor(ast->ForStmt.init, visit_func, analyser);
            
            ast_visitor(ast->ForStmt.condition, visit_func, analyser);
            ast_visitor(ast->ForStmt.post, visit_func, analyser);
            ast_visitor(ast->ForStmt.body, visit_func, analyser);
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

        default: {
            // Nope
        } break;
    }
}


// void set_visit_function_for_stmt(AstVisitorFunc visit_func[AstType_COUNT], AstVisitorFunc func) {
//     visit_func[AstType_IfStmt] = func;
//     visit_func[AstType_ForStmt] = func;
//     visit_func[AstType_ReturnStmt] = func;
// }