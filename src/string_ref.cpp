#include "string_ref.hpp"
#include "print.hpp"


StringRef StringRef::init(const char *str) {
    StringRef r;
    r.view = std::string_view(str);
    return r;
}

StringRef StringRef::init(const char *str, size_t count) {
    StringRef r;
    r.view = std::string_view(str, count);
    return r;
}

StringRef StringRef::init(const xpString& str) {
    StringRef r;
    r.view = std::string_view(str.c_str, (size_t)str.length);
    return r;
}


const char *StringRef::data() const {
    return view.data();
}

size_t StringRef::count() const {
    return view.size();
}

const char *StringRef::begin() const {
    return view.data();
}

const char *StringRef::end() const {
    return view.data() + view.size();
}

char StringRef::operator[](size_t index) const {
    ASSERT(index < count());
    return view[index];
}

bool StringRef::operator==(const StringRef& other) const {
    return view == other.view;
}

bool StringRef::starts_with(const StringRef& prefix) const {
    return view.starts_with(prefix.view);
}

bool StringRef::ends_with(const StringRef& suffix) const {
    return view.ends_with(suffix.view);
}

StringRef StringRef::substr(size_t start) const {
    ASSERT(start <= count());
    StringRef r;
    r.view = view.substr(start);
    return r;
}

StringRef StringRef::substr(size_t start, size_t sub_count) const {
    ASSERT(start <= count());
    StringRef r;
    r.view = view.substr(start, sub_count);
    return r;
}

isize StringRef::find(char c) const {
    size_t pos = view.find(c);
    return pos == std::string_view::npos ? -1 : static_cast<isize>(pos);
}

isize StringRef::find(const StringRef& needle) const {
    size_t pos = view.find(needle.view);
    return pos == std::string_view::npos ? -1 : static_cast<isize>(pos);
}



xpString to_xp_str(const StringRef str, xpAllocator allocator) {
    return xp_make_string_count(allocator, str.data(), str.count());
}