#pragma once

#include <cstring>
#include <tuple>
#include <vector>
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

// 字段标记：dump 遇到带这个注解的字段，就把它指向的块就地展开（见 dump_cir_block）
enum class CIRFieldTag {
    ChildBlock
};

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

    [[ =CIRFieldTag::ChildBlock ]]
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
    [[ =CIRFieldTag::ChildBlock ]]
    CIRBlockRef true_block;    // 直接持有子 Block（不插入父块，避免双引用）
    [[ =CIRFieldTag::ChildBlock ]]
    CIRBlockRef false_block;
    bool is_short_circuit = false;   // &&/|| 短路 CondBr：cond 已知时死分支（右操作数）整体跳过

    CIR_REFS(&CIRCondBrInfo::condition_inst)
};

// if 表达式：自身即结果载体，两臂是两个 Block（臂块以 Break 结尾，break 到本指令所在块）
struct CIRIfExprInfo {
    CIRInstructionRef condition_inst;
    [[ =CIRFieldTag::ChildBlock ]]
    CIRBlockRef true_block;
    [[ =CIRFieldTag::ChildBlock ]]
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
    // 闲置：ValueMemory 体系的字节指针
    // MemPointer data;
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
    [[ =CIRFieldTag::ChildBlock ]]
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
// 值 → 文本
//
// 每个值类型一个 CIRFormat 特化：只回答"这个值写成什么"，不管字段怎么摆。
// 不占用 std::formatter，只给 CIR dump 用。
//
template <typename T>
struct CIRFormat {
    static std::string as_string(const T& v) {
        return std::format("{}", v);
    }
};

// 闲置：改成 field_strings 之后，布局不在这层拼了
//
// template <typename T>
// void write_field(std::string& out, std::string_view name, const T& v) {
//     std::format_to(std::back_inserter(out), " {}=", name);
//     CIRFormat<T>::write(out, v);
// }


// ── 值类型 ──────────────────────────────────────────────

template<> struct CIRFormat<CIRInstructionRef> {
    static std::string as_string(const CIRInstructionRef& v) {
        if (v.block_ref < 0)  { return "-"; }
        if (v.inst_index < 0) { return std::format("block#{}", v.block_ref); }
        return std::format("{}.{}", v.block_ref, v.inst_index);
    }
};

template<> struct CIRFormat<EnumFieldInit> {
    static std::string as_string(const EnumFieldInit& v) {
        return std::format("name={} value_inst={}",
            CIRFormat<xpString>::as_string(v.name),
            CIRFormat<CIRInstructionRef>::as_string(v.value_inst));
    }
};

template <typename T> struct CIRFormat<Ref<T>> {
    static std::string as_string(const Ref<T>& v) {
        return std::format("{}", v.index);
    }
};

template<> struct CIRFormat<Ref<SymbolInfo>> {
    static std::string as_string(const Ref<SymbolInfo>& v) {
        return CIRFormat<xpString>::as_string(v.name);
    }
};

template <typename T> struct CIRFormat<Array<T>> {
    static std::string as_string(const Array<T>& v) {
        std::string out = "[";
        for (isize i = 0; i < v.count; i++) {
            if (i > 0) { out += ", "; }
            out += CIRFormat<T>::as_string(v[i]);
        }
        out += "]";
        return out;
    }
};

template<> struct CIRFormat<Ast *> {
    static std::string as_string(Ast * v) {
        return std::format("{}", (const void *)v);
    }
};

// 闲置：ValueMemory 体系的字节指针
// template<> struct CIRFormat<MemPointer> {
//     static std::string as_string(const MemPointer& v) {
//         return std::format("{}:{}", ::to_string(v.kind), v.offset);
//     }
// };

template<> struct CIRFormat<TokenType> {
    static std::string as_string(TokenType v) {
        return std::format("{}", ::to_string(v));
    }
};


// ── payload ────────────────────────────────────────────
//
// 每个类型给出自己的字段：(名字, 值的文本)，顺序即字段顺序。
// 怎么摆（` name=`、分隔、缩进）由上层 dump 定。新增 op 不写特化就编不过。
//
template <typename T> struct CIRFields;

template<> struct CIRFields<CIRConstDeclInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRConstDeclInfo& p) {
        return {
            { "ident", CIRFormat<decltype(p.ident)>::as_string(p.ident) },
            { "symbol", CIRFormat<decltype(p.symbol)>::as_string(p.symbol) },
            { "value_inst", CIRFormat<decltype(p.value_inst)>::as_string(p.value_inst) },
        };
    }
};


template<> struct CIRFields<CIRVariableDeclInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRVariableDeclInfo& p) {
        return {
            { "name", CIRFormat<decltype(p.name)>::as_string(p.name) },
            { "symbol", CIRFormat<decltype(p.symbol)>::as_string(p.symbol) },
            { "slot", CIRFormat<decltype(p.slot)>::as_string(p.slot) },
            { "is_var_arg", CIRFormat<decltype(p.is_var_arg)>::as_string(p.is_var_arg) },
            { "is_param", CIRFormat<decltype(p.is_param)>::as_string(p.is_param) },
            { "no_zero_init", CIRFormat<decltype(p.no_zero_init)>::as_string(p.no_zero_init) },
        };
    }
};


template<> struct CIRFields<CIRFunctionDeclInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRFunctionDeclInfo& p) {
        return {
            { "body_inst", CIRFormat<decltype(p.body_inst)>::as_string(p.body_inst) },
            { "return_type_inst", CIRFormat<decltype(p.return_type_inst)>::as_string(p.return_type_inst) },
            { "arg_type_insts", CIRFormat<decltype(p.arg_type_insts)>::as_string(p.arg_type_insts) },
            { "arg_decl_insts", CIRFormat<decltype(p.arg_decl_insts)>::as_string(p.arg_decl_insts) },
            { "return_count", CIRFormat<decltype(p.return_count)>::as_string(p.return_count) },
            { "is_extern_c", CIRFormat<decltype(p.is_extern_c)>::as_string(p.is_extern_c) },
            { "is_comptime", CIRFormat<decltype(p.is_comptime)>::as_string(p.is_comptime) },
            { "is_builtin", CIRFormat<decltype(p.is_builtin)>::as_string(p.is_builtin) },
            { "slot_count", CIRFormat<decltype(p.slot_count)>::as_string(p.slot_count) },
            { "has_generic_param_type", CIRFormat<decltype(p.has_generic_param_type)>::as_string(p.has_generic_param_type) },
            { "generic_param_type_var_insts", CIRFormat<decltype(p.generic_param_type_var_insts)>::as_string(p.generic_param_type_var_insts) },
            { "generic_param_type_var_param_indices", CIRFormat<decltype(p.generic_param_type_var_param_indices)>::as_string(p.generic_param_type_var_param_indices) },
            { "all_param_type_and_return_type_inst_blk_ref", CIRFormat<decltype(p.all_param_type_and_return_type_inst_blk_ref)>::as_string(p.all_param_type_and_return_type_inst_blk_ref) },
        };
    }
};


template<> struct CIRFields<CIRCondBrInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRCondBrInfo& p) {
        return {
            { "condition_inst", CIRFormat<decltype(p.condition_inst)>::as_string(p.condition_inst) },
            { "true_block", CIRFormat<decltype(p.true_block)>::as_string(p.true_block) },
            { "false_block", CIRFormat<decltype(p.false_block)>::as_string(p.false_block) },
            { "is_short_circuit", CIRFormat<decltype(p.is_short_circuit)>::as_string(p.is_short_circuit) },
        };
    }
};


template<> struct CIRFields<CIRIfExprInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRIfExprInfo& p) {
        return {
            { "condition_inst", CIRFormat<decltype(p.condition_inst)>::as_string(p.condition_inst) },
            { "true_block", CIRFormat<decltype(p.true_block)>::as_string(p.true_block) },
            { "false_block", CIRFormat<decltype(p.false_block)>::as_string(p.false_block) },
        };
    }
};


template<> struct CIRFields<CIRBreakInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRBreakInfo& p) {
        return {
            { "break_block", CIRFormat<decltype(p.break_block)>::as_string(p.break_block) },
            { "break_value_inst", CIRFormat<decltype(p.break_value_inst)>::as_string(p.break_value_inst) },
        };
    }
};


template<> struct CIRFields<CIRLoadInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRLoadInfo& p) {
        return {
            { "ptr_inst", CIRFormat<decltype(p.ptr_inst)>::as_string(p.ptr_inst) },
        };
    }
};


template<> struct CIRFields<CIRStoreInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRStoreInfo& p) {
        return {
            { "var_inst", CIRFormat<decltype(p.var_inst)>::as_string(p.var_inst) },
            { "value_inst", CIRFormat<decltype(p.value_inst)>::as_string(p.value_inst) },
        };
    }
};


template<> struct CIRFields<CIRTypeAscribeInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRTypeAscribeInfo& p) {
        return {
            { "var_inst", CIRFormat<decltype(p.var_inst)>::as_string(p.var_inst) },
            { "type_inst", CIRFormat<decltype(p.type_inst)>::as_string(p.type_inst) },
        };
    }
};


template<> struct CIRFields<CIRCallInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRCallInfo& p) {
        return {
            { "called_thing", CIRFormat<decltype(p.called_thing)>::as_string(p.called_thing) },
            { "arg_insts", CIRFormat<decltype(p.arg_insts)>::as_string(p.arg_insts) },
        };
    }
};


template<> struct CIRFields<CIRHookInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRHookInfo& p) {
        return {
            { "name", CIRFormat<decltype(p.name)>::as_string(p.name) },
            { "arg_insts", CIRFormat<decltype(p.arg_insts)>::as_string(p.arg_insts) },
        };
    }
};


template<> struct CIRFields<CIRInstantiateFuncInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRInstantiateFuncInfo& p) {
        return {
            { "called_thing", CIRFormat<decltype(p.called_thing)>::as_string(p.called_thing) },
            { "arg_insts", CIRFormat<decltype(p.arg_insts)>::as_string(p.arg_insts) },
        };
    }
};


template<> struct CIRFields<CIRBinaryInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRBinaryInfo& p) {
        return {
            { "op", CIRFormat<decltype(p.op)>::as_string(p.op) },
            { "left_inst", CIRFormat<decltype(p.left_inst)>::as_string(p.left_inst) },
            { "right_inst", CIRFormat<decltype(p.right_inst)>::as_string(p.right_inst) },
        };
    }
};


template<> struct CIRFields<CIRUnaryInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRUnaryInfo& p) {
        return {
            { "op", CIRFormat<decltype(p.op)>::as_string(p.op) },
            { "operand_inst", CIRFormat<decltype(p.operand_inst)>::as_string(p.operand_inst) },
        };
    }
};


template<> struct CIRFields<CIRCastInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRCastInfo& p) {
        return {
            { "expr_inst", CIRFormat<decltype(p.expr_inst)>::as_string(p.expr_inst) },
            { "target_type_inst", CIRFormat<decltype(p.target_type_inst)>::as_string(p.target_type_inst) },
        };
    }
};


template<> struct CIRFields<CIRFieldAccessInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRFieldAccessInfo& p) {
        return {
            { "parent_inst", CIRFormat<decltype(p.parent_inst)>::as_string(p.parent_inst) },
            { "field_name", CIRFormat<decltype(p.field_name)>::as_string(p.field_name) },
        };
    }
};


template<> struct CIRFields<CIRIndexInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRIndexInfo& p) {
        return {
            { "array_inst", CIRFormat<decltype(p.array_inst)>::as_string(p.array_inst) },
            { "index_inst", CIRFormat<decltype(p.index_inst)>::as_string(p.index_inst) },
        };
    }
};


template<> struct CIRFields<CIRStructInitInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRStructInitInfo& p) {
        return {
            { "struct_type_inst", CIRFormat<decltype(p.struct_type_inst)>::as_string(p.struct_type_inst) },
            { "field_init_insts", CIRFormat<decltype(p.field_init_insts)>::as_string(p.field_init_insts) },
        };
    }
};


template<> struct CIRFields<CIRArrayInitInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRArrayInitInfo& p) {
        return {
            { "element_insts", CIRFormat<decltype(p.element_insts)>::as_string(p.element_insts) },
        };
    }
};


template<> struct CIRFields<CIRPointerTypeInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRPointerTypeInfo& p) {
        return {
            { "pointed_type_inst", CIRFormat<decltype(p.pointed_type_inst)>::as_string(p.pointed_type_inst) },
        };
    }
};


template<> struct CIRFields<CIRArrayTypeInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRArrayTypeInfo& p) {
        return {
            { "element_type_inst", CIRFormat<decltype(p.element_type_inst)>::as_string(p.element_type_inst) },
            { "count_inst", CIRFormat<decltype(p.count_inst)>::as_string(p.count_inst) },
        };
    }
};


template<> struct CIRFields<CIRSliceTypeInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRSliceTypeInfo& p) {
        return {
            { "element_type_inst", CIRFormat<decltype(p.element_type_inst)>::as_string(p.element_type_inst) },
        };
    }
};


template<> struct CIRFields<CIRGetOrInitStructInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRGetOrInitStructInfo& p) {
        return {
            { "decl_ast", CIRFormat<decltype(p.decl_ast)>::as_string(p.decl_ast) },
            { "symbol", CIRFormat<decltype(p.symbol)>::as_string(p.symbol) },
        };
    }
};


template<> struct CIRFields<CIRStructFieldInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRStructFieldInfo& p) {
        return {
            { "type_block_inst", CIRFormat<decltype(p.type_block_inst)>::as_string(p.type_block_inst) },
            { "name", CIRFormat<decltype(p.name)>::as_string(p.name) },
        };
    }
};


template<> struct CIRFields<CIRFinishStructInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRFinishStructInfo& p) {
        return {
            { "struct_decl_inst", CIRFormat<decltype(p.struct_decl_inst)>::as_string(p.struct_decl_inst) },
            { "field_insts", CIRFormat<decltype(p.field_insts)>::as_string(p.field_insts) },
        };
    }
};


template<> struct CIRFields<CIREnumDeclInitInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIREnumDeclInitInfo& p) {
        return {
            { "tag_type_inst", CIRFormat<decltype(p.tag_type_inst)>::as_string(p.tag_type_inst) },
            { "decl_ast", CIRFormat<decltype(p.decl_ast)>::as_string(p.decl_ast) },
            { "symbol", CIRFormat<decltype(p.symbol)>::as_string(p.symbol) },
            { "scope", CIRFormat<decltype(p.scope)>::as_string(p.scope) },
            { "fields", CIRFormat<decltype(p.fields)>::as_string(p.fields) },
        };
    }
};


template<> struct CIRFields<CIRGetOrInitUnionInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRGetOrInitUnionInfo& p) {
        return {
            { "decl_ast", CIRFormat<decltype(p.decl_ast)>::as_string(p.decl_ast) },
            { "symbol", CIRFormat<decltype(p.symbol)>::as_string(p.symbol) },
            { "scope", CIRFormat<decltype(p.scope)>::as_string(p.scope) },
        };
    }
};


template<> struct CIRFields<CIRFinishUnionInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRFinishUnionInfo& p) {
        return {
            { "union_decl_inst", CIRFormat<decltype(p.union_decl_inst)>::as_string(p.union_decl_inst) },
            { "field_insts", CIRFormat<decltype(p.field_insts)>::as_string(p.field_insts) },
        };
    }
};


template<> struct CIRFields<CIRDetermineTypeInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRDetermineTypeInfo& p) {
        return {
            { "determining_inst", CIRFormat<decltype(p.determining_inst)>::as_string(p.determining_inst) },
            { "type_inst", CIRFormat<decltype(p.type_inst)>::as_string(p.type_inst) },
        };
    }
};


template<> struct CIRFields<CIRAddrOfInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRAddrOfInfo& p) {
        return {
            { "lval_inst", CIRFormat<decltype(p.lval_inst)>::as_string(p.lval_inst) },
        };
    }
};


template<> struct CIRFields<CIRFuncParamTypeInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRFuncParamTypeInfo& p) {
        return {
            { "type_of_func_type_inst", CIRFormat<decltype(p.type_of_func_type_inst)>::as_string(p.type_of_func_type_inst) },
            { "param_index", CIRFormat<decltype(p.param_index)>::as_string(p.param_index) },
        };
    }
};


template<> struct CIRFields<CIRFieldTypeOfStructInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRFieldTypeOfStructInfo& p) {
        return {
            { "struct_type_inst", CIRFormat<decltype(p.struct_type_inst)>::as_string(p.struct_type_inst) },
            { "field_index", CIRFormat<decltype(p.field_index)>::as_string(p.field_index) },
        };
    }
};


template<> struct CIRFields<CIRTypeOfInstResultInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRTypeOfInstResultInfo& p) {
        return {
            { "target_inst", CIRFormat<decltype(p.target_inst)>::as_string(p.target_inst) },
        };
    }
};


template<> struct CIRFields<CIRFuncTypeInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRFuncTypeInfo& p) {
        return {
            { "param_type_insts", CIRFormat<decltype(p.param_type_insts)>::as_string(p.param_type_insts) },
            { "return_type_inst", CIRFormat<decltype(p.return_type_inst)>::as_string(p.return_type_inst) },
        };
    }
};


template<> struct CIRFields<CIRDerefInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRDerefInfo& p) {
        return {
            { "operand_inst", CIRFormat<decltype(p.operand_inst)>::as_string(p.operand_inst) },
        };
    }
};


template<> struct CIRFields<CIRStringLiteralInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRStringLiteralInfo& p) {
        return {
            { "count", CIRFormat<decltype(p.count)>::as_string(p.count) },
            { "str", CIRFormat<decltype(p.str)>::as_string(p.str) },
            { "string_type_inst", CIRFormat<decltype(p.string_type_inst)>::as_string(p.string_type_inst) },
        };
    }
};


template<> struct CIRFields<CIRConstantValueInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRConstantValueInfo& p) {
        return {
            { "value", CIRFormat<decltype(p.value)>::as_string(p.value) },
        };
    }
};


template<> struct CIRFields<CIRBlockRefInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRBlockRefInfo& p) {
        return {
            { "block_ref", CIRFormat<decltype(p.block_ref)>::as_string(p.block_ref) },
            { "in_which_block", CIRFormat<decltype(p.in_which_block)>::as_string(p.in_which_block) },
        };
    }
};


template<> struct CIRFields<CIRIdentRefInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRIdentRefInfo& p) {
        return {
            { "ident", CIRFormat<decltype(p.ident)>::as_string(p.ident) },
        };
    }
};


template<> struct CIRFields<CIRIdentValInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRIdentValInfo& p) {
        return {
            { "ident", CIRFormat<decltype(p.ident)>::as_string(p.ident) },
        };
    }
};


template<> struct CIRFields<CIRFieldPtrInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRFieldPtrInfo& p) {
        return {
            { "parent_inst", CIRFormat<decltype(p.parent_inst)>::as_string(p.parent_inst) },
            { "field_name", CIRFormat<decltype(p.field_name)>::as_string(p.field_name) },
        };
    }
};


template<> struct CIRFields<CIRIndexPtrInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRIndexPtrInfo& p) {
        return {
            { "array_inst", CIRFormat<decltype(p.array_inst)>::as_string(p.array_inst) },
            { "index_inst", CIRFormat<decltype(p.index_inst)>::as_string(p.index_inst) },
        };
    }
};


template<> struct CIRFields<CIREnterScopeInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIREnterScopeInfo& p) {
        return {
            { "scope", CIRFormat<decltype(p.scope)>::as_string(p.scope) },
        };
    }
};


template<> struct CIRFields<CIRExitScopeInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRExitScopeInfo& p) {
        return {
            { "scope", CIRFormat<decltype(p.scope)>::as_string(p.scope) },
        };
    }
};


template<> struct CIRFields<CIRImportPackageInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRImportPackageInfo& p) {
        return {
            { "path", CIRFormat<decltype(p.path)>::as_string(p.path) },
        };
    }
};


template<> struct CIRFields<CIRPublishReturnValueInfo> {
    static std::vector<std::tuple<std::string, std::string>> field_strings(const CIRPublishReturnValueInfo& p) {
        return {
            { "target_block", CIRFormat<decltype(p.target_block)>::as_string(p.target_block) },
            { "value_inst", CIRFormat<decltype(p.value_inst)>::as_string(p.value_inst) },
        };
    }
};

