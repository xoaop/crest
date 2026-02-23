#include "analyser.hpp"


struct Valuee {
    enum class Type {
        Integer,
        Float,
    } type;

    static Valuee make(i128 val);
    static Valuee make(double val);

    Valuee() = default;

    Type get_type() const;
    i128 get_as_integer() const;
    double get_as_float() const;
    void set(i128 val);
    void set(double val);

    // TODO: 实现运算符重载, 直接用表达式的方式计算, 这样就不需要写operation_integer_ast和operation_float_ast了

private:
    union {
        i128 int_value;
        double float_value;
    };

    bool has_error = false;
};



i128 operation_integer_ast(TokenType op_type, Ast *left_c, Ast *right_c);
double operation_float_ast(TokenType op_type, Ast *left_c, Ast *right_c);

bool try_constant_expr_folding(Ast *const_expr);
