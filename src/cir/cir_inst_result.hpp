#pragma once

#include <optional>

#include "xoaop.h"
#include "value.hpp"


enum class CIRResultState: u32 {
    NothingYet,  // 还没有计算结果
    OnlyType,   // 仅仅只有类型推导
    InProgress,  // 值已提前登记, 求值未完成（递归调用命中此态中断）
    WholeValue,  // 计算出实际值了
    Error        // 本指令分析出错，结果不可用，下游应跳过
};

enum class CIRValueKind : u8 {
    RValue,  // 实际值（参与运算、传参）
    LValue,  // 位置句柄（可被 Load/Store/FieldPtr/IndexPtr 操作）
};

struct CIRInstResult {
    CIRResultState state = CIRResultState::NothingYet;
    CIRValueKind value_kind = CIRValueKind::RValue;


    std::optional<TypeRef> implicit_type = std::nullopt;

    TypeRef type() const;
    TypeRef actual_type() const;
    Value actual_val() const;

    void set_type(TypeRef new_type);
    void set_actual_type(TypeRef new_type);
    void set_val(Value new_val);
    void set_val_in_progress(Value new_val);

private:
    Value val;
    TypeRef outstanding_type = nullptr;
};
