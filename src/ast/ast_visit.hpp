#pragma once

#include <meta>
#include <type_traits>

#include "ast.hpp"
#include "common.hpp"


// 前序遍历 AST 时，回调对每个节点的处置
enum class AstVisit {
    Continue,       // 继续下探子节点
    SkipChildren,   // 当前节点处理完，不进子节点
    Stop,           // 整棵遍历结束
};


template<typename F>
AstVisit visit_ast(Ast *ast, F &fn);


// 反射遍历 payload 里的 Ast* / Array<Ast*> 成员
template<typename Payload, typename F>
AstVisit visit_ast_children(Payload &payload, F &fn) {
    AstVisit result = AstVisit::Continue;

    for_each_field(payload, [&](auto &field) {
        if(result != AstVisit::Continue) {
            return;
        }

        using Member = std::remove_cvref_t<decltype(field)>;

        if constexpr(std::is_same_v<Member, Ast *>) {
            result = visit_ast(field, fn);
        } else if constexpr(std::is_same_v<Member, Array<Ast *>>) {
            for(Ast *child : field) {
                result = visit_ast(child, fn);
                if(result != AstVisit::Continue) {
                    break;
                }
            }
        }
    });

    return result;
}


// 前序：先跑 fn(ast)，再按 fn 的处置下探子节点
template<typename F>
AstVisit visit_ast(Ast *ast, F &fn) {
    if(ast == nullptr) {
        return AstVisit::Continue;
    }

    const AstVisit result = fn(ast);
    if(result != AstVisit::Continue) {
        return result;
    }

    switch(ast->type) {
        #define AST_INFO(type_name, ...) \
            case AstType_##type_name: \
                return visit_ast_children(ast->type_name, fn);
        AST_INFOS
        #undef AST_INFO

        default:
            return AstVisit::Continue;
    }
}
