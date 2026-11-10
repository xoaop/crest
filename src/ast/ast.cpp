#include <stdarg.h>
#include <cstring>
#include <string>

#include "common.hpp"
#include "print.hpp"

#include "ast.hpp"
#include "context.hpp"


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
    ast.is_null = false;


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
    case Pipe:        // |
    case Caret:       // ^ (二元：按位异或；一元时是解引用)
    case And:         // & (二元：按位与；一元时是取地址)
    case ShiftLeft:   // <<
    case ShiftRight:  // >>
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
    case TokenType::Tilde: // ~ 按位取反

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

// 按位/移位：只接受整数操作数，结果为操作数类型（不返回 bool）
bool is_bitwise_operator(TokenType t) {
    return t == TokenType::And ||        // & (二元)
           t == TokenType::Pipe ||       // |
           t == TokenType::Caret ||      // ^ (二元)
           t == TokenType::Tilde ||      // ~ (一元)
           t == TokenType::ShiftLeft ||  // <<
           t == TokenType::ShiftRight;   // >>
}

bool is_return_bool_operator(TokenType t) {
    return is_compare_operator(t) || is_logic_operator(t);
}

const char *ast_string(AstType type) {
    return ast_strs[static_cast<i32>(type)];
}


// 新增：打印带分支前缀的一行
xp_internal void print_connector(const std::string &prefix, bool is_last) {
    print_err("{}{}", prefix, is_last ? "└── " : "├── ");
}

// 顶层列表的第一项
xp_internal void print_connector_first(const std::string &prefix) {
    print_err("{}┌── ", prefix);
}

template <typename... Args>
xp_internal void print_line(const std::string &prefix, bool is_last, std::format_string<Args...> fmt, Args&&... args) {
    print_connector(prefix, is_last);
    println_err(fmt, std::forward<Args>(args)...);
}

// 子节点用的前缀: 父节点是最后一个就留空, 否则续 │
xp_internal std::string child_prefix(const std::string &prefix, bool is_last, bool is_first = false) {
    return prefix + ((is_last && !is_first) ? "    " : "│   ");
}


void print_ast(Array<Ast *> a_arr);


xp_internal void print_field_value(GlobalStringIndex v) {
    print_err("{}", get_global_string(v));
}

template<typename T> requires std::is_enum_v<T>
xp_internal void print_field_value(const T &v) {
    print_err("{}", to_string(v));
}

template<typename T>
xp_internal void print_field_value(const T &v) {
    print_err("{}", v);
}


// 这个成员会不会被当字段打出来
template<typename Member>
consteval bool is_printable_field() {
    if constexpr(std::is_same_v<Member, Ast *> || std::is_same_v<Member, Array<Ast *>>) {
        return false;
    } else if constexpr(std::is_same_v<Member, GlobalStringIndex> || std::is_enum_v<Member>) {
        return true;
    } else {
        return requires(Member &v) { println_err("{}", v); };
    }
}


xp_internal void print_ast_node(Ast *node, const std::string &prefix, bool is_last, bool is_first = false);


// 按成员声明顺序打印, 子节点就地递归
template<typename Payload>
xp_internal void print_ast_members(Payload &payload, const std::string &prefix) {
    using Plain = std::remove_cv_t<Payload>;

    static constexpr auto members = std::define_static_array(
            std::meta::nonstatic_data_members_of(^^Plain, std::meta::access_context::current()));

    isize render_count = 0;

    template for(constexpr auto m : members) {
        const auto &field = payload.[:m:];
        using Member = std::remove_cvref_t<decltype(field)>;

        if constexpr(std::is_same_v<Member, Ast *>) {
            render_count += 1;
        } else if constexpr(std::is_same_v<Member, Array<Ast *>>) {
            if(field.count > 0) {
                render_count += 1;
            }
        } else if constexpr(is_printable_field<Member>()) {
            render_count += 1;
        }
    }

    isize rendered = 0;

    template for(constexpr auto m : members) {
        const auto &field = payload.[:m:];
        using Member = std::remove_cvref_t<decltype(field)>;

        if constexpr(std::is_same_v<Member, Ast *>) {
            rendered += 1;
            const bool is_last = rendered == render_count;

            print_line(prefix, is_last, "{}:", std::meta::identifier_of(m));
            print_ast_node(field, child_prefix(prefix, is_last), true);
        } else if constexpr(std::is_same_v<Member, Array<Ast *>>) {
            if(field.count > 0) {
                rendered += 1;
                const bool is_last = rendered == render_count;
                const std::string child_indent = child_prefix(prefix, is_last);

                print_line(prefix, is_last, "{}:", std::meta::identifier_of(m));

                isize index = 0;
                for(Ast *element : field) {
                    index += 1;
                    print_ast_node(element, child_indent, index == field.count);
                }
            }
        } else if constexpr(is_printable_field<Member>()) {
            rendered += 1;

            print_connector(prefix, rendered == render_count);
            print_err("{}: ", std::meta::identifier_of(m));
            print_field_value(field);
            println_err("");
        }
    }
}


xp_internal void print_ast_node(Ast *node, const std::string &prefix, bool is_last, bool is_first) {
    if(node == nullptr) {
        print_line(prefix, is_last, "(null)");
        return;
    }

    if(is_first) {
        print_connector_first(prefix);
        println_err("{}", ast_string(node->type));
    } else {
        print_line(prefix, is_last, "{}", ast_string(node->type));
    }

    const std::string members_prefix = child_prefix(prefix, is_last, is_first);

    switch(node->type) {
        #define AST_INFO(type_name, ...) \
            case AstType_##type_name: \
                print_ast_members(node->type_name, members_prefix); \
                break;

        AST_INFOS
        #undef AST_INFO

        default:
            break;
    }
}

// 主函数：打印单个 Ast 节点
void print_ast(Ast *a) {
    print_ast_node(a, "", true, true);
}

// 重载：打印 Ast 数组
void print_ast(Array<Ast *> a_arr) {
    isize index = 0;
    for(Ast *child : a_arr) {
        index += 1;
        print_ast_node(child, "", index == a_arr.count, index == 1);
    }
}





xpAllocator ast_allocator() {
    return permanent_allocator();
}
