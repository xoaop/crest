#include <stdarg.h>
#include <cstring>

#include "print.hpp"

#include "ast.hpp"


// Ast字符串列表定义
const char *ast_strs[] = {
    #define AST_INFO(type_name, type_str, ...) type_str,
    AST_INFOS
    #undef AST_INFO
};

Ast::Ast() {
    memset((void*)this, 0, sizeof(*this));
}

Ast::Ast(const Ast& other) {
    if(this == &other) return;
    memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(*this));
}

Ast& Ast::operator=(const Ast& other) {
    if (this == &other) return *this;
    memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(*this));
    return *this;
}


Ast ast_make(AstType type) {
    Ast ast = {};
    
    ast.ast_symbol = Ref<SymbolInfo>::INVALID_REF;
    ast.is_const_expr = false;
    ast.is_lvalue = false;
    ast.is_null = false;
    ast.implicit_conversion_tag = ImplicitConversionTag::None;


    ast.type = type;
    return ast;
}

Ast *ast_alloc(AstType type, xpAllocator allocator) {
    Ast *a = cast(Ast *) xp_alloc(allocator, sizeof(Ast));
    *a = ast_make(type);

    return a;
}

Ast *ast_alloc(AstType type) {
    return ast_alloc(type, ast_allocator());
}

Ast *ast_alloc(AstType type, Token token) {
    Ast *a = ast_alloc(type);
    a->token = token;
    a->src_loc = token.src_loc;
    
    return a;
}

Ast *ast_alloc(AstType type, Token token, SourceLocation src_loc) {
    Ast *a = ast_alloc(type);
    a->token = token;
    a->src_loc = src_loc;

    return a;
}


bool is_binary_op(TokenType type) {
    switch (type)
    {
    case Add:
    case Minus:
    case Star:
    case ForwardSlash:
    case Percent:
    case GreaterThan:
    case GreaterEqual:
    case LessThan:
    case LessEqual:
    case DoubleEqual:
    case ExclamationEqual:
    case DoubleAnd:
    case DoubleOr:
        return true;
    default:
        return false;
    }

    return false;
}

bool is_unary_op(TokenType type) {
    switch (type)
    {
    case TokenType::Minus: // -
    case TokenType::Exclamation: // !
    
    // 指针运算
    case TokenType::And: // &
    case TokenType::Caret: // ^
        return true;
    default:
        return false;
    }
    return false;
}



bool is_add_sub_operator(TokenType t) {
    return t == TokenType::Add || t == TokenType::Minus;
}


bool is_equal_compare_operator(TokenType t) {
    return t == TokenType::DoubleEqual || 
           t == TokenType::ExclamationEqual;
}

bool is_compare_operator(TokenType t) {
    return t == TokenType::LessThan ||
           t == TokenType::LessEqual ||
           t == TokenType::GreaterThan ||
           t == TokenType::GreaterEqual ||
           is_equal_compare_operator(t);
}

bool is_operator_for_bool(TokenType t) {
    return is_equal_compare_operator(t) || is_logic_operator(t);
}

bool is_logic_operator(TokenType t) {
    return t == TokenType::DoubleAnd ||
           t == TokenType::DoubleOr  ||
           t == TokenType::Exclamation;
}

bool is_return_bool_operator(TokenType t) {
    return is_compare_operator(t) || is_logic_operator(t);
}

const char *ast_string(AstType type) {
    return ast_strs[static_cast<i32>(type)];
}

#if defined(CREST_DEBUG)

xp_internal void print_indent(i32 depth) {
    for (i32 i = 0; i < depth; i++) {
        print_err("│   ");
    }
}

xp_internal void print_branch(i32 depth, bool is_last) {
    if (depth == 0) return;
    print_indent(depth - 1);
    if (is_last) {
        print_err("└── ");
    } else {
        print_err("├── ");
    }
}

// 新增：打印带分支前缀的一行
template <typename... Args>
xp_internal void print_line(i32 depth, bool is_last, std::format_string<Args...> fmt, Args&&... args) {
    print_branch(depth, is_last);
    println_err(fmt, std::forward<Args>(args)...);
}


// 主函数：打印单个 Ast 节点
void print_ast(Ast *a, i32 depth = 0, bool is_last = true) {
    if (a == NULL) {
        print_line(depth, is_last, "(null)");
        return;
    }

    // 打印当前节点类型
    print_line(depth, is_last, "{}", ast_strs[static_cast<i32>(a->type)]);

    // 根据类型打印子内容（全部在 depth+1 层级）
    switch (a->type) {
        case AstType_Undefined:
            break;

        case AstType_VariableDecl: {
            print_line(depth + 1, false, "name: {}", a->VariableDecl.var_name.c_str);
            print_ast(a->VariableDecl.expr, depth + 1, true);
            print_ast(a->VariableDecl.type_ast, depth + 1, true);
            print_line(depth + 1, true, "no_zero_init: {}", a->VariableDecl.no_zero_init ? "true" : "false");
            break;
        }

        case AstType_Constant: {
            // TODO: 更正确地打印常量值, 目前只处理了整数和浮点数, 其他类型的常量还没有处理

            print_branch(depth + 1, false);
            print_err("value: ");
            TypeRef t = a->Constant.value.type;
            if(is_integer_or_untyped_type(t)) {
                print_i128(a->Constant.value.integer_val());
            } else if(is_float_or_untyped_type(t)) {
                print_err("{}", a->Constant.value.float_val());
            } else if(t->kind == Type_bool) {
                print_err("{}", a->Constant.value.bool_val() ? "true" : "false");
            }
            println_err("");
            break;
        }

        case AstType_Block: {
            print_ast(a->Block.statements, depth + 1, true);
            break;
        }

        case AstType_BinaryExpr: {
            print_line(depth + 1, false, "op: {}", token_strings[a->BinaryExpr.op]);
            print_line(depth + 1, false, "left:");
            print_ast(a->BinaryExpr.left, depth + 2, false);
            print_line(depth + 1, true, "right:");
            print_ast(a->BinaryExpr.right, depth + 2, true);
            break;
        }

        case AstType_UnaryExpr: {
            print_line(depth + 1, false, "op: {}", token_strings[a->UnaryExpr.op]);
            print_line(depth + 1, true, "operand:");
            print_ast(a->UnaryExpr.operand, depth + 2, true);
            break;
        }

        case AstType_Assignment: {
            print_line(depth + 1, false, "left:");
            print_ast(a->Assignment.left_var_expr, depth + 2, false);
            print_line(depth + 1, true, "right:");
            print_ast(a->Assignment.right_expr, depth + 2, true);
            break;
        }

        case AstType_IfStmt: {
            print_line(depth + 1, false, "condition:");
            print_ast(a->IfStmt.condition, depth + 2, false);
            print_line(depth + 1, false, "then:");
            print_ast(a->IfStmt.then_block, depth + 2, a->IfStmt.else_block == nullptr);
            if (a->IfStmt.else_block) {
                print_line(depth + 1, true, "else:");
                print_ast(a->IfStmt.else_block, depth + 2, true);
            }
            break;
        }

        case AstType_ForStmt: {
            i32 child_count = 0;
            if (a->ForStmt.iter_var) child_count++;
            if (a->ForStmt.index_var) child_count++;
            if (a->ForStmt.iterable) child_count++;
            if (a->ForStmt.iterable_end) child_count++;
            if (a->ForStmt.condition) child_count++;
            child_count++; // body always exists

            i32 idx = 0;
            if (a->ForStmt.iter_var) {
                print_line(depth + 1, (++idx == child_count), "iter_var:");
                print_ast(a->ForStmt.iter_var, depth + 2, true);
            }
            if (a->ForStmt.index_var) {
                print_line(depth + 1, (++idx == child_count), "index_var:");
                print_ast(a->ForStmt.index_var, depth + 2, true);
            }
            if (a->ForStmt.iterable) {
                print_line(depth + 1, (++idx == child_count), "iterable:");
                print_ast(a->ForStmt.iterable, depth + 2, true);
            }
            if (a->ForStmt.iterable_end) {
                print_line(depth + 1, (++idx == child_count), "iterable_end:");
                print_ast(a->ForStmt.iterable_end, depth + 2, true);
            }
            if (a->ForStmt.condition) {
                print_line(depth + 1, (++idx == child_count), "condition:");
                print_ast(a->ForStmt.condition, depth + 2, true);
            }
            print_line(depth + 1, true, "body:");
            print_ast(a->ForStmt.body, depth + 2, true);
            break;
        }

        case AstType_ReturnStmt: {
            if (a->ReturnStmt.expr) {
                print_line(depth + 1, true, "expr:");
                print_ast(a->ReturnStmt.expr, depth + 2, true);
            } else {
                print_line(depth + 1, true, "(no expr)");
            }
            break;
        }

        case AstType_CastExpr: {
            print_line(depth + 1, false, "target_type:");
            print_indent(depth + 2);
            print_line(depth + 1, true, "expr:");
            print_ast(a->CastExpr.expr, depth + 2, true);
            break;
        }

        case AstType_FunctionCallExpr: {
            print_line(depth + 1, true, "args:");
            print_ast(a->FunctionCallExpr.args, depth + 2, true);
            break;
        }

        case AstType_FieldAccess: {
            print_line(depth + 1, false, "parent:");
            print_ast(a->FieldAccess.parent, depth + 2, false);
            print_line(depth + 1, true, "field: {}", a->FieldAccess.field_name.c_str);
            break;
        }

        case AstType_StructField: {
            print_line(depth + 1, false, "name: {}", a->StructField.name.c_str);
            print_line(depth + 1, true, "type:");
            print_ast(a->StructField.type_ast, depth + 2, true);
            break;
        }

        case AstType_StructInitExpr: {
            print_line(depth + 1, true, "field_inits:");
            print_ast(a->StructInitExpr.field_inits, depth + 2, true);
            break;
        }

        case AstType_ArrayInitExpr: {
            print_line(depth + 1, true, "elements:");
            print_ast(a->ArrayInitExpr.elements, depth + 2, true);
            break;
        }

        case AstType_IndexExpr: {
            print_line(depth + 1, false, "array:");
            print_ast(a->IndexExpr.array_var_expr, depth + 2, false);
            print_line(depth + 1, true, "index:");
            print_ast(a->IndexExpr.index_expr, depth + 2, true);
            break;
        }

        case AstType_EasyType: {
            print_line(depth + 1, true, "type: {}", get_type_kind_str(a->EasyType.kind).c_str);

            break;
        }
        case AstType_SliceType: {
            print_line(depth + 1, false, "element_type:");
            print_ast(a->SliceType.element_type_ast, depth + 2, true);
            break;
        }
        case AstType_PointerType: {
            print_line(depth + 1, true, "pointed_type:");
            print_ast(a->PointerType.pointed_type_ast, depth + 2, true);
            break;
        }
        case AstType_ArrayType: {
            print_line(depth + 1, false, "element_type:");
            print_ast(a->ArrayType.element_type_ast, depth + 2, false);
            print_line(depth + 1, true, "count:");
            print_ast(a->ArrayType.count_expr, depth + 2, true);
            break;
        }

        case AstType_FunctionType: {
            print_line(depth + 1, false, "param_types:");
            print_ast(a->FunctionType.param_types, depth + 2, false);
            print_line(depth + 1, true, "return_type:");
            print_ast(a->FunctionType.return_type_ast, depth + 2, true);
            break;
        }

        case AstType_Ident: {
            print_line(depth + 1, true, "name: {}", a->Ident.name.c_str);
            break;
        }

        case AstType_ConstDecl: {
            print_line(depth + 1, false, "name: {}", a->ConstDecl.name.c_str);
            print_line(depth + 1, true, "value:");
            print_ast(a->ConstDecl.value_ast, depth + 2, true);
            break;
        }

        case AstType_ParamDecl: {
            print_line(depth + 1, false, "name: {}", a->ParamDecl.name.c_str);
            print_line(depth + 1, false, "is_var_arg: {}", a->ParamDecl.is_var_arg ? "true" : "false");
            print_line(depth + 1, true, "type:");
            print_ast(a->ParamDecl.type_ast, depth + 2, true);
            break;
        }

        case AstType_FunctionDeclValue: {
            print_line(depth + 1, false, "params:");
            print_ast(a->FunctionDeclValue.params, depth + 2, true);
            print_line(depth + 1, true, "body:");
            print_ast(a->FunctionDeclValue.block, depth + 2, true);
            break;
        }

        case AstType_StructDeclValue: {
            print_line(depth + 1, true, "fields:");
            print_ast(a->StructDeclValue.fields, depth + 2, true);
            break;
        }

        case AstType_EnumDecl: {
            print_line(depth + 1, false, "type:");
            print_ast(a->EnumDecl.type_ast, depth + 2, false);
            print_line(depth + 1, true, "fields:");
            print_ast(a->EnumDecl.fields, depth + 2, true);
            break;
        }

        case AstType_UnionDecl: {
            print_line(depth + 1, true, "fields:");
            print_ast(a->UnionDecl.fields, depth + 2, true);
            break;
        }

        case AstType_Import: {
            print_line(depth + 1, true, "path: {}", a->Import.path.c_str);
            break;
        }

        case AstType_StringLiteralExpr: {
            print_line(depth + 1, true, "value: {}", a->StringLiteralExpr.str.c_str);
            break;
        }


        case AstType_Break:
        case AstType_Continue:
            break;

        

        case AstType_BadDecl: 
        case AstType_BadStmt:
        case AstType_BadExpr:
        case AstType_BadType:
            print_line(depth + 1, true, "(bad node)");
            break;

        default:
            // 未实现就报错
            std::unreachable();
            break;
    }
}

// 重载：打印 Ast 数组
void print_ast(Array<Ast *> a_arr, i32 depth, bool is_last) {
    if (a_arr.count == 0) {
        print_line(depth, is_last, "(empty list)");
        return;
    }

    for (isize i = 0; i < a_arr.count; i++) {
        bool last = (i == a_arr.count - 1);
        print_ast(a_arr[i], depth, last);
    }
}
#endif // CREST_DEBUG





xpAllocator ast_allocator() {
    return permanent_allocator();
}
