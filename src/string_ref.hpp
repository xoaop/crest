#pragma once

#include "xoaop.h"
#include <string_view>
#include <format>

struct StringRef {

    static StringRef init(const char *str);
    static StringRef init(const char *str, size_t count);
    static StringRef init(const xpString& str);

    const char *data() const;
    size_t count() const;

    const char *begin() const;
    const char *end() const;

    char operator[](size_t index) const;
    bool operator==(const StringRef& other) const;

    bool starts_with(const StringRef& prefix) const;
    bool ends_with(const StringRef& suffix) const;

    StringRef substr(size_t start) const;
    StringRef substr(size_t start, size_t sub_count) const;

    isize find(char c) const;
    isize find(const StringRef& needle) const;


private:
    std::string_view view;

    friend struct std::hash<StringRef>;
    friend struct std::formatter<StringRef>;
};


template<>
struct std::hash<StringRef> {
    usize operator()(const StringRef& r) const {
        return std::hash<std::string_view>{}(r.view);
    }
};


template<>
struct std::formatter<StringRef> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const StringRef& r, std::format_context& ctx) const {
        return std::formatter<std::string_view>{}.format(r.view, ctx);
    }
};


xpString to_xp_str(const StringRef str, xpAllocator allocator);