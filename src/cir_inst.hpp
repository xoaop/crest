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

enum TokenType : u8;
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
    X(IfExpr)                   \
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
    X(InstantiateFunc)          \
    X(TypeOfInstResult)         \
    X(FuncType)                 \
    X(BlockRef)                 \
    X(ImportPackage)            \
    X(PublishReturnValue)       \
    X(Hook)                     \
/**/

enum class CIROperator {
#define X(name) name,
    CIR_OPERATORS
#undef X
};

// 名字字符串池只生成一份，实例化在 cir_inst.cpp
extern template const char *to_string<CIROperator>(CIROperator);


//
// payloads
//

struct CIRConstDeclInfo {
    xpString ident;
    Ref<SymbolInfo> symbol;
    CIRInstructionRef value_inst; // 常量值的指令引用

    CIR_REFS(&CIRConstDeclInfo::value_inst)
};

struct CIRVariableDeclInfo {
    xpString name;
    Ref<SymbolInfo> symbol;
    isize slot;                   // 在栈帧中的槽位（参数 0..N-1，局部变量 N..）
    bool is_var_arg;              // 是否是变长参数（仅函数参数有效）
    bool is_param;                // 是否函数形参（含类型形参：其类型可以是 type）

    bool no_zero_init;
};

struct EnumFieldInit {
    xpString name;
    CIRInstructionRef value_inst;  // INVALID_INST 表示自增

    CIR_REFS(&EnumFieldInit::value_inst)
};

struct CIRFunctionDeclInfo {
    // xpString name;                      // 函数名（symbol_find / call 目标）
    // Ref<SymbolInfo> symbol;

    CIRInstructionRef body_inst;
    CIRInstructionRef return_type_inst; // 返回类型 block 引用, nullopt 表示返回类型由返回值推导
    Array<CIRInstructionRef> arg_type_insts; // 每个参数的类型 block 引用（与 args 平行，var_arg 为 INVALID_INST）
    Array<CIRInstructionRef> arg_decl_insts; // 每个参数的 VariableDecl 指令引用
    isize return_count;                    // 返回值数量
    bool is_extern_c;                      // 是否是 extern "C" 函数
    bool is_comptime;                      // 是否是编译期函数
    bool is_builtin;                       // 是否是 #builtin 内置函数


    isize slot_count;               // 局部变量数量（包括参数）

    // $T 泛型: 类型变量是函数自己的槽, 值由调用点按实参类型填, 每个实例一份
    bool has_generic_param_type;
    Array<CIRInstructionRef> generic_param_type_var_insts;   // 各类型变量的 VariableDecl
    Array<isize> generic_param_type_var_param_indices;       // 各类型变量从第几个实参推导
    CIRInstructionRef all_param_type_and_return_type_inst_blk_ref;   // 签名块(形参+返回类型), 实例化只重跑它

    CIR_REFS(&CIRFunctionDeclInfo::body_inst, &CIRFunctionDeclInfo::return_type_inst,
             &CIRFunctionDeclInfo::arg_type_insts, &CIRFunctionDeclInfo::arg_decl_insts)
};

struct CIRCondBrInfo {
    CIRInstructionRef condition_inst;
    CIRBlockRef true_block;    // 直接持有子 Block（不插入父块，避免双引用）
    CIRBlockRef false_block;
    bool is_short_circuit = false;   // &&/|| 短路 CondBr：cond 已知时死分支（右操作数）整体跳过

    CIR_REFS(&CIRCondBrInfo::condition_inst)
};

// if 表达式：自身即结果载体，两臂是两个 Block（臂块以 Break 结尾，break 到本指令所在块）
struct CIRIfExprInfo {
    CIRInstructionRef condition_inst;
    CIRBlockRef true_block;
    CIRBlockRef false_block;

    CIR_REFS(&CIRIfExprInfo::condition_inst)
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

// 内建 hook：编译器开的口子，name 决定语义，interp 里硬编码 switch 处理
struct CIRHookInfo {
    xpString name;
    Array<CIRInstructionRef> arg_insts;

    CIR_REFS(&CIRHookInfo::arg_insts)
};

// $T 实例化：把 called_thing 的未实例化模板按实参类型具化，结果写回 called_thing 的结果槽。
// 与 DetermineType 同模式（不写自身结果，靠 CIR_TARGETS 传播）。排在
// TypeOfInstResult/FuncParamType/DetermineType 之前，使后续环节拿到的都是真签名，无需推迟或重跑
struct CIRInstantiateFuncInfo {
    CIRInstructionRef called_thing;
    Array<CIRInstructionRef> arg_insts;

    CIR_REFS(&CIRInstantiateFuncInfo::called_thing, &CIRInstantiateFuncInfo::arg_insts)
    CIR_TARGETS(&CIRInstantiateFuncInfo::called_thing)
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
    Ref<SymbolInfo> symbol;   // 可选的 ConstDecl 绑定符号，未完成类型创建后立即注册（自引用字段）
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
    Ref<SymbolInfo> symbol;   // 可选，ConstDecl绑定的符号，壳子创建后立即注册
    Ref<Scope> scope;
    Array<EnumFieldInit> fields;

    CIR_REFS(&CIREnumDeclInitInfo::tag_type_inst, &CIREnumDeclInitInfo::fields)
};

struct CIRGetOrInitUnionInfo {
    Ast *decl_ast;
    Ref<SymbolInfo> symbol;   // 可选的 ConstDecl 绑定符号，未完成类型创建后立即注册（自引用字段）
    Ref<Scope> scope;      // resolve 建的 union scope（模板，analysis 派生每实例独立 scope）
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
    CIRBlockRef in_which_block;
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
    Ref<Scope> scope;
};

struct CIRExitScopeInfo {
    Ref<Scope> scope;
};

struct CIRImportPackageInfo {
    xpString path;
};

// 提前登记返回值（return <type-decl> 用）
struct CIRPublishReturnValueInfo {
    CIRInstructionRef target_block;
    CIRInstructionRef value_inst;

    CIR_REFS(&CIRPublishReturnValueInfo::value_inst)
    CIR_TARGETS(&CIRPublishReturnValueInfo::target_block)
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
        return ::to_string(op);
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

        UNREACHABLE();
    }


    
    CIROperator       op;
    Ref<SymbolInfo>     symbol;

    SourceLocation src_loc;

private:
    union {
#define X(name) CIR##name##Info name##_info;
        CIR_OPERATORS
#undef X
    };

};


//
// payload → 文本
//
// 集中一处，每个类型一个 CIRFormat 特化：格式显式写死，输出不随结构体成员名 / 顺序漂移。
// 不占用 std::formatter，只给 CIR dump 用。新增 op 不写特化就编不过。
//
template <typename T>
struct CIRFormat {
    static void write(std::string& out, const T& v) {
        std::format_to(std::back_inserter(out), "{}", v);
    }
};

template <typename T>
void write_field(std::string& out, std::string_view name, const T& v) {
    std::format_to(std::back_inserter(out), " {}=", name);
    CIRFormat<T>::write(out, v);
}


// ── 值类型 ──────────────────────────────────────────────

template<> struct CIRFormat<CIRInstructionRef> {
    static void write(std::string& out, const CIRInstructionRef& v) {
        if (v.block_ref < 0)  { out += "-"; return; }
        if (v.inst_index < 0) { std::format_to(std::back_inserter(out), "block#{}", v.block_ref); return; }
        std::format_to(std::back_inserter(out), "{}.{}", v.block_ref, v.inst_index);
    }
};

template<> struct CIRFormat<EnumFieldInit> {
    static void write(std::string& out, const EnumFieldInit& v) {
        write_field(out, "name", v.name);
        write_field(out, "value_inst", v.value_inst);
    }
};

template <typename T> struct CIRFormat<Ref<T>> {
    static void write(std::string& out, const Ref<T>& v) {
        std::format_to(std::back_inserter(out), "{}", v.index);
    }
};

template<> struct CIRFormat<Ref<SymbolInfo>> {
    static void write(std::string& out, const Ref<SymbolInfo>& v) {
        CIRFormat<xpString>::write(out, v.name);
    }
};

template <typename T> struct CIRFormat<Array<T>> {
    static void write(std::string& out, const Array<T>& v) {
        out += "[";
        for (isize i = 0; i < v.count; i++) {
            if (i > 0) out += ", ";
            CIRFormat<T>::write(out, v[i]);
        }
        out += "]";
    }
};

template<> struct CIRFormat<Ast *> {
    static void write(std::string& out, Ast * v) {
        std::format_to(std::back_inserter(out), "{}", (const void *)v);
    }
};

template<> struct CIRFormat<Pointer> {
    static void write(std::string& out, const Pointer& v) {
        std::format_to(std::back_inserter(out), "{}:{}", ::to_string(v.kind), v.offset);
    }
};

template<> struct CIRFormat<TokenType> {
    static void write(std::string& out, TokenType v) {
        std::format_to(std::back_inserter(out), "{}", ::to_string(v));
    }
};


// ── payload ────────────────────────────────────────────

template<> struct CIRFormat<CIRConstDeclInfo> {
    static void write(std::string& out, const CIRConstDeclInfo& p) {
        write_field(out, "ident", p.ident);
        write_field(out, "symbol", p.symbol);
        write_field(out, "value_inst", p.value_inst);
    }
};

template<> struct CIRFormat<CIRVariableDeclInfo> {
    static void write(std::string& out, const CIRVariableDeclInfo& p) {
        write_field(out, "name", p.name);
        write_field(out, "symbol", p.symbol);
        write_field(out, "slot", p.slot);
        write_field(out, "is_var_arg", p.is_var_arg);
        write_field(out, "is_param", p.is_param);
        write_field(out, "no_zero_init", p.no_zero_init);
    }
};

template<> struct CIRFormat<CIRFunctionDeclInfo> {
    static void write(std::string& out, const CIRFunctionDeclInfo& p) {
        write_field(out, "body_inst", p.body_inst);
        write_field(out, "return_type_inst", p.return_type_inst);
        write_field(out, "arg_type_insts", p.arg_type_insts);
        write_field(out, "arg_decl_insts", p.arg_decl_insts);
        write_field(out, "return_count", p.return_count);
        write_field(out, "is_extern_c", p.is_extern_c);
        write_field(out, "is_comptime", p.is_comptime);
        write_field(out, "is_builtin", p.is_builtin);
        write_field(out, "slot_count", p.slot_count);
        write_field(out, "has_generic_param_type", p.has_generic_param_type);
        write_field(out, "generic_param_type_var_insts", p.generic_param_type_var_insts);
        write_field(out, "generic_param_type_var_param_indices", p.generic_param_type_var_param_indices);
        write_field(out, "all_param_type_and_return_type_inst_blk_ref", p.all_param_type_and_return_type_inst_blk_ref);
    }
};

template<> struct CIRFormat<CIRCondBrInfo> {
    static void write(std::string& out, const CIRCondBrInfo& p) {
        write_field(out, "condition_inst", p.condition_inst);
        write_field(out, "true_block", p.true_block);
        write_field(out, "false_block", p.false_block);
        write_field(out, "is_short_circuit", p.is_short_circuit);
    }
};

template<> struct CIRFormat<CIRIfExprInfo> {
    static void write(std::string& out, const CIRIfExprInfo& p) {
        write_field(out, "condition_inst", p.condition_inst);
        write_field(out, "true_block", p.true_block);
        write_field(out, "false_block", p.false_block);
    }
};

template<> struct CIRFormat<CIRBreakInfo> {
    static void write(std::string& out, const CIRBreakInfo& p) {
        write_field(out, "break_block", p.break_block);
        write_field(out, "break_value_inst", p.break_value_inst);
    }
};

template<> struct CIRFormat<CIRLoadInfo> {
    static void write(std::string& out, const CIRLoadInfo& p) {
        write_field(out, "ptr_inst", p.ptr_inst);
    }
};

template<> struct CIRFormat<CIRStoreInfo> {
    static void write(std::string& out, const CIRStoreInfo& p) {
        write_field(out, "var_inst", p.var_inst);
        write_field(out, "value_inst", p.value_inst);
    }
};

template<> struct CIRFormat<CIRTypeAscribeInfo> {
    static void write(std::string& out, const CIRTypeAscribeInfo& p) {
        write_field(out, "var_inst", p.var_inst);
        write_field(out, "type_inst", p.type_inst);
    }
};

template<> struct CIRFormat<CIRCallInfo> {
    static void write(std::string& out, const CIRCallInfo& p) {
        write_field(out, "called_thing", p.called_thing);
        write_field(out, "arg_insts", p.arg_insts);
    }
};

template<> struct CIRFormat<CIRHookInfo> {
    static void write(std::string& out, const CIRHookInfo& p) {
        write_field(out, "name", p.name);
        write_field(out, "arg_insts", p.arg_insts);
    }
};

template<> struct CIRFormat<CIRInstantiateFuncInfo> {
    static void write(std::string& out, const CIRInstantiateFuncInfo& p) {
        write_field(out, "called_thing", p.called_thing);
        write_field(out, "arg_insts", p.arg_insts);
    }
};

template<> struct CIRFormat<CIRBinaryInfo> {
    static void write(std::string& out, const CIRBinaryInfo& p) {
        write_field(out, "op", p.op);
        write_field(out, "left_inst", p.left_inst);
        write_field(out, "right_inst", p.right_inst);
    }
};

template<> struct CIRFormat<CIRUnaryInfo> {
    static void write(std::string& out, const CIRUnaryInfo& p) {
        write_field(out, "op", p.op);
        write_field(out, "operand_inst", p.operand_inst);
    }
};

template<> struct CIRFormat<CIRCastInfo> {
    static void write(std::string& out, const CIRCastInfo& p) {
        write_field(out, "expr_inst", p.expr_inst);
        write_field(out, "target_type_inst", p.target_type_inst);
    }
};

template<> struct CIRFormat<CIRFieldAccessInfo> {
    static void write(std::string& out, const CIRFieldAccessInfo& p) {
        write_field(out, "parent_inst", p.parent_inst);
        write_field(out, "field_name", p.field_name);
    }
};

template<> struct CIRFormat<CIRIndexInfo> {
    static void write(std::string& out, const CIRIndexInfo& p) {
        write_field(out, "array_inst", p.array_inst);
        write_field(out, "index_inst", p.index_inst);
    }
};

template<> struct CIRFormat<CIRStructInitInfo> {
    static void write(std::string& out, const CIRStructInitInfo& p) {
        write_field(out, "struct_type_inst", p.struct_type_inst);
        write_field(out, "field_init_insts", p.field_init_insts);
    }
};

template<> struct CIRFormat<CIRArrayInitInfo> {
    static void write(std::string& out, const CIRArrayInitInfo& p) {
        write_field(out, "element_insts", p.element_insts);
    }
};

template<> struct CIRFormat<CIRPointerTypeInfo> {
    static void write(std::string& out, const CIRPointerTypeInfo& p) {
        write_field(out, "pointed_type_inst", p.pointed_type_inst);
    }
};

template<> struct CIRFormat<CIRArrayTypeInfo> {
    static void write(std::string& out, const CIRArrayTypeInfo& p) {
        write_field(out, "element_type_inst", p.element_type_inst);
        write_field(out, "count_inst", p.count_inst);
    }
};

template<> struct CIRFormat<CIRSliceTypeInfo> {
    static void write(std::string& out, const CIRSliceTypeInfo& p) {
        write_field(out, "element_type_inst", p.element_type_inst);
    }
};

template<> struct CIRFormat<CIRGetOrInitStructInfo> {
    static void write(std::string& out, const CIRGetOrInitStructInfo& p) {
        write_field(out, "decl_ast", p.decl_ast);
        write_field(out, "symbol", p.symbol);
    }
};

template<> struct CIRFormat<CIRStructFieldInfo> {
    static void write(std::string& out, const CIRStructFieldInfo& p) {
        write_field(out, "type_block_inst", p.type_block_inst);
        write_field(out, "name", p.name);
    }
};

template<> struct CIRFormat<CIRFinishStructInfo> {
    static void write(std::string& out, const CIRFinishStructInfo& p) {
        write_field(out, "struct_decl_inst", p.struct_decl_inst);
        write_field(out, "field_insts", p.field_insts);
    }
};

template<> struct CIRFormat<CIREnumDeclInitInfo> {
    static void write(std::string& out, const CIREnumDeclInitInfo& p) {
        write_field(out, "tag_type_inst", p.tag_type_inst);
        write_field(out, "decl_ast", p.decl_ast);
        write_field(out, "symbol", p.symbol);
        write_field(out, "scope", p.scope);
        write_field(out, "fields", p.fields);
    }
};

template<> struct CIRFormat<CIRGetOrInitUnionInfo> {
    static void write(std::string& out, const CIRGetOrInitUnionInfo& p) {
        write_field(out, "decl_ast", p.decl_ast);
        write_field(out, "symbol", p.symbol);
        write_field(out, "scope", p.scope);
    }
};

template<> struct CIRFormat<CIRFinishUnionInfo> {
    static void write(std::string& out, const CIRFinishUnionInfo& p) {
        write_field(out, "union_decl_inst", p.union_decl_inst);
        write_field(out, "field_insts", p.field_insts);
    }
};

template<> struct CIRFormat<CIRDetermineTypeInfo> {
    static void write(std::string& out, const CIRDetermineTypeInfo& p) {
        write_field(out, "determining_inst", p.determining_inst);
        write_field(out, "type_inst", p.type_inst);
    }
};

template<> struct CIRFormat<CIRAddrOfInfo> {
    static void write(std::string& out, const CIRAddrOfInfo& p) {
        write_field(out, "lval_inst", p.lval_inst);
    }
};

template<> struct CIRFormat<CIRFuncParamTypeInfo> {
    static void write(std::string& out, const CIRFuncParamTypeInfo& p) {
        write_field(out, "type_of_func_type_inst", p.type_of_func_type_inst);
        write_field(out, "param_index", p.param_index);
    }
};

template<> struct CIRFormat<CIRFieldTypeOfStructInfo> {
    static void write(std::string& out, const CIRFieldTypeOfStructInfo& p) {
        write_field(out, "struct_type_inst", p.struct_type_inst);
        write_field(out, "field_index", p.field_index);
    }
};

template<> struct CIRFormat<CIRTypeOfInstResultInfo> {
    static void write(std::string& out, const CIRTypeOfInstResultInfo& p) {
        write_field(out, "target_inst", p.target_inst);
    }
};

template<> struct CIRFormat<CIRFuncTypeInfo> {
    static void write(std::string& out, const CIRFuncTypeInfo& p) {
        write_field(out, "param_type_insts", p.param_type_insts);
        write_field(out, "return_type_inst", p.return_type_inst);
    }
};

template<> struct CIRFormat<CIRDerefInfo> {
    static void write(std::string& out, const CIRDerefInfo& p) {
        write_field(out, "operand_inst", p.operand_inst);
    }
};

template<> struct CIRFormat<CIRStringLiteralInfo> {
    static void write(std::string& out, const CIRStringLiteralInfo& p) {
        write_field(out, "data", p.data);
        write_field(out, "count", p.count);
        write_field(out, "str", p.str);
        write_field(out, "string_type_inst", p.string_type_inst);
    }
};

template<> struct CIRFormat<CIRConstantValueInfo> {
    static void write(std::string& out, const CIRConstantValueInfo& p) {
        write_field(out, "value", p.value);
    }
};

template<> struct CIRFormat<CIRBlockRefInfo> {
    static void write(std::string& out, const CIRBlockRefInfo& p) {
        write_field(out, "block_ref", p.block_ref);
        write_field(out, "in_which_block", p.in_which_block);
    }
};

template<> struct CIRFormat<CIRIdentRefInfo> {
    static void write(std::string& out, const CIRIdentRefInfo& p) {
        write_field(out, "ident", p.ident);
    }
};

template<> struct CIRFormat<CIRIdentValInfo> {
    static void write(std::string& out, const CIRIdentValInfo& p) {
        write_field(out, "ident", p.ident);
    }
};

template<> struct CIRFormat<CIRFieldPtrInfo> {
    static void write(std::string& out, const CIRFieldPtrInfo& p) {
        write_field(out, "parent_inst", p.parent_inst);
        write_field(out, "field_name", p.field_name);
    }
};

template<> struct CIRFormat<CIRIndexPtrInfo> {
    static void write(std::string& out, const CIRIndexPtrInfo& p) {
        write_field(out, "array_inst", p.array_inst);
        write_field(out, "index_inst", p.index_inst);
    }
};

template<> struct CIRFormat<CIREnterScopeInfo> {
    static void write(std::string& out, const CIREnterScopeInfo& p) {
        write_field(out, "scope", p.scope);
    }
};

template<> struct CIRFormat<CIRExitScopeInfo> {
    static void write(std::string& out, const CIRExitScopeInfo& p) {
        write_field(out, "scope", p.scope);
    }
};

template<> struct CIRFormat<CIRImportPackageInfo> {
    static void write(std::string& out, const CIRImportPackageInfo& p) {
        write_field(out, "path", p.path);
    }
};

template<> struct CIRFormat<CIRPublishReturnValueInfo> {
    static void write(std::string& out, const CIRPublishReturnValueInfo& p) {
        write_field(out, "target_block", p.target_block);
        write_field(out, "value_inst", p.value_inst);
    }
};
