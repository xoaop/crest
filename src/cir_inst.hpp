#pragma once

#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>

#include "xoaop.h"
#include "array.hpp"
#include "value.hpp"
#include "span.hpp"
#include "scope.hpp"
#include "error_msg.hpp"

#include "print.hpp"

#include "cir_instruction_ref.hpp"

enum TokenType : u8;  // forward declare, avoid circular include via tokenizer.hpp
struct Ast;


// 递归收集引用字段：单个 ref / ref 数组 / 元素带 refs() 的数组（如 EnumFieldInit）
template<typename F>
static void collect_refs_impl(F& f, Array<CIRInstructionRef>& r, xpAllocator a) {
    using U = std::remove_cvref_t<F>;
    if constexpr (std::is_same_v<U, CIRInstructionRef>) {
        r.push_back(f);
    } else if constexpr (std::is_same_v<U, Array<CIRInstructionRef>>) {
        for (auto& x : f) r.push_back(x);
    } else {
        for (auto& x : f) {
            auto sub = x.refs(a);
            for (auto& ref : sub) r.push_back(ref);
        }
    }
}

#define CIR_COLLECT_REFS(a, ...) \
    do { \
        Array<CIRInstructionRef> r = make_array<CIRInstructionRef>(a); \
        std::apply([&](auto... mp) { \
            ((void)[&](auto mpv) { collect_refs_impl(this->*mpv, r, a); }(mp), ...); \
        }, std::tuple(__VA_ARGS__)); \
        return r; \
    } while(0)


    
// 结构体内声明操作数（refs）——列出引用字段成员指针
#define CIR_REFS(...) \
    Array<CIRInstructionRef> refs(xpAllocator a) const { CIR_COLLECT_REFS(a, __VA_ARGS__); }

// 结构体内声明 target（个别 op）——默认无 target，只个别覆盖
#define CIR_TARGETS(...) \
    Array<CIRInstructionRef> targets(xpAllocator a) const { CIR_COLLECT_REFS(a, __VA_ARGS__); }



//
// operators
//
#define CIR_OPERATORS           \
    X(ConstDecl)                \
    X(FunctionDecl)             \
    X(GetOrInitUnion)           \
    X(FinishUnion)              \
    X(VariableDecl)             \
    X(Binary)                   \
    X(Unary)                    \
    X(FieldAccess)              \
    X(FieldPtr)                 \
    X(Call)                     \
    X(ConstantValue)            \
    X(StringLiteral)            \
    X(Cast)                     \
    X(StructInit)               \
    X(ArrayInit)                \
    X(Index)                    \
    X(IndexPtr)                 \
    X(PointerType)              \
    X(ArrayType)                \
    X(SliceType)                \
    X(EnterScope)               \
    X(ExitScope)                \
    X(CondBr)                   \
    X(Break)                    \
    X(Load)                     \
    X(Deref)                    \
    X(Store)                    \
    X(IdentRef)                 \
    X(IdentVal)                 \
    X(DetermineType)            \
    X(TypeAscribe)              \
    X(GetOrInitStruct)          \
    X(StructField)              \
    X(FinishStruct)             \
    X(EnumDeclInit)             \
    X(AddrOf)                   \
    X(FieldTypeOfStruct)        \
    X(FuncParamType)            \
    X(TypeOfInstResult)         \
    X(FuncType)                 \
    X(BlockRef)                 \
    X(ImportPackage)            \
/**/

enum class CIROperator {
#define X(name) name,
    CIR_OPERATORS
#undef X
};


inline const char *string(CIROperator op) {
    switch(op) {
#define X(name) case CIROperator::name: return #name;
        CIR_OPERATORS
#undef X
        default: return "<unknown operator>";
    }
}


//
// payloads
//

struct CIRConstDeclInfo {
    xpString ident;
    SymbolInfoRef symbol;
    CIRInstructionRef value_inst; // 常量值的指令引用

    CIR_REFS(&CIRConstDeclInfo::value_inst)
};

struct CIRVariableDeclInfo {
    xpString name;
    SymbolInfoRef symbol;
    isize slot;                   // 在栈帧中的槽位（参数 0..N-1，局部变量 N..）
    bool is_var_arg;              // 是否是变长参数（仅函数参数有效）

    bool no_zero_init;
};

struct EnumFieldInit {
    xpString name;
    CIRInstructionRef value_inst;  // INVALID_INST 表示自增

    CIR_REFS(&EnumFieldInit::value_inst)
};

struct CIRFunctionDeclInfo {
    // xpString name;                      // 函数名（symbol_find / call 目标）
    // SymbolInfoRef symbol;

    CIRInstructionRef body_inst;
    CIRInstructionRef return_type_inst; // 返回类型 block 引用, nullopt 表示返回类型由返回值推导
    // Array<CIRVariableDeclInfo> args;           // 参数名 + slot + is_var_arg
    Array<CIRInstructionRef> arg_type_insts; // 每个参数的类型 block 引用（与 args 平行，var_arg 为 INVALID_INST）
    Array<CIRInstructionRef> arg_decl_insts; // 每个参数的 VariableDecl 指令引用
    isize return_count;                    // 返回值数量
    bool is_extern_c;                      // 是否是 extern "C" 函数
    bool is_comptime;                      // 是否是编译期函数
    bool is_builtin;                       // 是否是 #builtin 内置函数


    isize slot_count;               // 局部变量数量（包括参数）

    CIR_REFS(&CIRFunctionDeclInfo::body_inst, &CIRFunctionDeclInfo::return_type_inst,
             &CIRFunctionDeclInfo::arg_type_insts, &CIRFunctionDeclInfo::arg_decl_insts)
};

struct CIRCondBrInfo {
    CIRInstructionRef condition_inst;
    CIRBlockRef true_block;    // 直接持有子 Block（不插入父块，避免双引用）
    CIRBlockRef false_block;

    CIR_REFS(&CIRCondBrInfo::condition_inst)
};

struct CIRBreakInfo {
    CIRInstructionRef break_block;    // 指向 Block 指令
    CIRInstructionRef break_value_inst;

    CIR_REFS(&CIRBreakInfo::break_block, &CIRBreakInfo::break_value_inst)
    CIR_TARGETS(&CIRBreakInfo::break_block)
};

struct CIRLoadInfo {
    CIRInstructionRef ptr_inst;

    CIR_REFS(&CIRLoadInfo::ptr_inst)
};

struct CIRStoreInfo {
    CIRInstructionRef var_inst;
    CIRInstructionRef value_inst;

    CIR_REFS(&CIRStoreInfo::var_inst, &CIRStoreInfo::value_inst)
};

struct CIRTypeAscribeInfo {
    CIRInstructionRef var_inst;
    CIRInstructionRef type_inst;

    CIR_REFS(&CIRTypeAscribeInfo::var_inst, &CIRTypeAscribeInfo::type_inst)
    CIR_TARGETS(&CIRTypeAscribeInfo::var_inst)
};

struct CIRCallInfo {
    CIRInstructionRef called_thing;
    Array<CIRInstructionRef> arg_insts;

    CIR_REFS(&CIRCallInfo::called_thing, &CIRCallInfo::arg_insts)
};

struct CIRBinaryInfo {
    TokenType op;
    CIRInstructionRef left_inst;
    CIRInstructionRef right_inst;

    CIR_REFS(&CIRBinaryInfo::left_inst, &CIRBinaryInfo::right_inst)
};

struct CIRUnaryInfo {
    TokenType op;
    CIRInstructionRef operand_inst;

    CIR_REFS(&CIRUnaryInfo::operand_inst)
};

struct CIRCastInfo {
    CIRInstructionRef expr_inst;
    CIRInstructionRef target_type_inst;

    CIR_REFS(&CIRCastInfo::expr_inst, &CIRCastInfo::target_type_inst)
};

struct CIRFieldAccessInfo {
    CIRInstructionRef parent_inst;
    xpString field_name;

    CIR_REFS(&CIRFieldAccessInfo::parent_inst)
};

struct CIRIndexInfo {
    CIRInstructionRef array_inst;
    CIRInstructionRef index_inst;

    CIR_REFS(&CIRIndexInfo::array_inst, &CIRIndexInfo::index_inst)
};

struct CIRStructInitInfo {
    CIRInstructionRef struct_type_inst;
    Array<CIRInstructionRef> field_init_insts;

    CIR_REFS(&CIRStructInitInfo::struct_type_inst, &CIRStructInitInfo::field_init_insts)
};

struct CIRArrayInitInfo {
    Array<CIRInstructionRef> element_insts;

    CIR_REFS(&CIRArrayInitInfo::element_insts)
    CIR_TARGETS(&CIRArrayInitInfo::element_insts)
};

struct CIRPointerTypeInfo {
    CIRInstructionRef pointed_type_inst;

    CIR_REFS(&CIRPointerTypeInfo::pointed_type_inst)
};

struct CIRArrayTypeInfo {
    CIRInstructionRef element_type_inst;
    CIRInstructionRef count_inst;

    CIR_REFS(&CIRArrayTypeInfo::element_type_inst, &CIRArrayTypeInfo::count_inst)
};

struct CIRSliceTypeInfo {
    CIRInstructionRef element_type_inst;

    CIR_REFS(&CIRSliceTypeInfo::element_type_inst)
};

struct CIRGetOrInitStructInfo {
    Ast *decl_ast;
    SymbolInfoRef symbol;   // 可选的 ConstDecl 绑定符号，未完成类型创建后立即注册（自引用字段）
};

struct CIRStructFieldInfo {
    CIRInstructionRef type_block_inst;
    xpString          name;

    CIR_REFS(&CIRStructFieldInfo::type_block_inst)
};

struct CIRFinishStructInfo {
    CIRInstructionRef struct_decl_inst;
    Array<CIRInstructionRef> field_insts;

    CIR_REFS(&CIRFinishStructInfo::struct_decl_inst, &CIRFinishStructInfo::field_insts)
};

struct CIREnumDeclInitInfo {
    CIRInstructionRef tag_type_inst;
    Ast *decl_ast;
    SymbolInfoRef symbol;   // 可选，ConstDecl绑定的符号，壳子创建后立即注册
    Scope *scope;
    Array<EnumFieldInit> fields;

    CIR_REFS(&CIREnumDeclInitInfo::tag_type_inst, &CIREnumDeclInitInfo::fields)
};

struct CIRGetOrInitUnionInfo {
    Ast *decl_ast;
    SymbolInfoRef symbol;   // 可选的 ConstDecl 绑定符号，未完成类型创建后立即注册（自引用字段）
    Scope *scope;           // resolve 建的 union scope（模板，analysis 派生每实例独立 scope）
};

struct CIRFinishUnionInfo {
    CIRInstructionRef union_decl_inst;
    Array<CIRInstructionRef> field_insts;

    CIR_REFS(&CIRFinishUnionInfo::union_decl_inst, &CIRFinishUnionInfo::field_insts)
};

struct CIRDetermineTypeInfo {
    CIRInstructionRef determining_inst;
    CIRInstructionRef type_inst;  // INVALID_INST 表示"无值"

    CIR_REFS(&CIRDetermineTypeInfo::determining_inst, &CIRDetermineTypeInfo::type_inst)
    CIR_TARGETS(&CIRDetermineTypeInfo::determining_inst)
};

struct CIRAddrOfInfo {
    CIRInstructionRef lval_inst;  // 左值指令（LValue of T）

    CIR_REFS(&CIRAddrOfInfo::lval_inst)
};

struct CIRFuncParamTypeInfo {
    CIRInstructionRef type_of_func_type_inst; // type_type(function_type) 的指令
    isize param_index;

    CIR_REFS(&CIRFuncParamTypeInfo::type_of_func_type_inst)
};

struct CIRFieldTypeOfStructInfo {
    CIRInstructionRef struct_type_inst;
    isize field_index;

    CIR_REFS(&CIRFieldTypeOfStructInfo::struct_type_inst)
};

struct CIRTypeOfInstResultInfo {
    CIRInstructionRef target_inst;  // 要提取类型的指令

    CIR_REFS(&CIRTypeOfInstResultInfo::target_inst)
};

struct CIRFuncTypeInfo {
    Array<CIRInstructionRef> param_type_insts;
    CIRInstructionRef return_type_inst;

    CIR_REFS(&CIRFuncTypeInfo::param_type_insts, &CIRFuncTypeInfo::return_type_inst)
};

struct CIRDerefInfo {
    CIRInstructionRef operand_inst;

    CIR_REFS(&CIRDerefInfo::operand_inst)
};

struct CIRStringLiteralInfo {
    Pointer data;
    isize count;
    xpString str;
    CIRInstructionRef string_type_inst;

    CIR_REFS(&CIRStringLiteralInfo::string_type_inst)
};

// 常量值（ConstantValue op）
struct CIRConstantValueInfo {
    Value value;
};

// BlockRef 指令（父→子 Block 引用）
struct CIRBlockRefInfo {
    CIRBlockRef block_ref;
};

struct CIRIdentRefInfo {
    xpString ident;   // 标识符名（undefined 错误消息用）
};
struct CIRIdentValInfo {
    xpString ident;   // 标识符名（undefined 错误消息用）
};


// 每个 op 独立的 payload 类型（op ↔ payload 一一映射，无共享）
struct CIRFieldPtrInfo {
    CIRInstructionRef parent_inst;
    xpString field_name;

    CIR_REFS(&CIRFieldPtrInfo::parent_inst)
};

struct CIRIndexPtrInfo {
    CIRInstructionRef array_inst;
    CIRInstructionRef index_inst;

    CIR_REFS(&CIRIndexPtrInfo::array_inst, &CIRIndexPtrInfo::index_inst)
};

struct CIREnterScopeInfo {
    Scope *scope;
};

struct CIRExitScopeInfo {
    Scope *scope;
};

struct CIRImportPackageInfo {
    xpString path;
};

//
//
//

// op → payload 类型映射（CIR_OPERATORS 生成：CIR##name##Info，每个 op 独立类型）
template<CIROperator Op> struct info_type;

#define X(name) template<> struct info_type<CIROperator::name> { using type = CIR##name##Info; };
    CIR_OPERATORS
#undef X

// 检测 payload 类型是否声明了 refs()/targets()（CIR_REFS/CIR_TARGETS 生成；
// 无 refs() 的类型无需写 CIR_REFS()，自动按空处理）
template<typename T>
concept HasRefs = requires(T& t, xpAllocator a) { t.refs(a); };

template<typename T>
concept HasTargets = requires(T& t, xpAllocator a) { t.targets(a); };


struct CIRInstruction {

    CIRInstruction() {
        // 清零整个对象：trivial 拷贝（default）会复制 union 未初始化字节（UB），
        // Debug -O0 侥幸不崩，Release -O2 会暴露；op 由 New_Instruction 重设。
        memset(this, 0, sizeof(*this));
        new (&ConstantValue_info) CIRConstantValueInfo();
    }
    CIRInstruction(const CIRInstruction&) = default;
    CIRInstruction& operator=(const CIRInstruction&) = default;

    const char *to_string() const {
        return string(op);
    }
    
    // C++23 deducing this：按 op 取对应 payload（Op = CIROperator 枚举值）
    // if constexpr 展开 CIR_OPERATORS，直接访问对应 union 成员——
    // 标准成员访问（无 reinterpret_cast），const 性由成员访问自动保持
    // auto&& 对左值成员表达式推导为 T&（const 时为 const T&）
    template<CIROperator Op, typename Self>
    auto&& info(this Self&& self) {
        ASSERT(self.op == Op);

#define X(name) if constexpr (Op == CIROperator::name) return self.name##_info;
        CIR_OPERATORS
#undef X

        std::unreachable();
    }


    
    CIROperator       op;
    SymbolInfoRef     symbol;

    SourceLocation src_loc;

private:
    union {
#define X(name) CIR##name##Info name##_info;
        CIR_OPERATORS
#undef X
    };

};
