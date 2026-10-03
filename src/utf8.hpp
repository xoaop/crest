#pragma once

#include <string>
#include <string_view>

#include "xoaop.h"

// 字符串是不是合法 UTF-8。标准库没有这个；非法序列喂给 std::filesystem::path 的构造会抛异常，
// 而本项目是 -fno-exceptions。
bool is_valid_utf8(xpString text);

// char → char8_t 的类型搬运，供 std::filesystem::path 的 u8 构造使用（不涉及编码转换）
std::u8string_view as_u8(xpString text);
std::u8string_view as_u8(const std::string& text);
