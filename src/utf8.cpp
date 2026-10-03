#include "utf8.hpp"

bool is_valid_utf8(xpString text) {
    const unsigned char *s = (const unsigned char *)text.c_str;
    const isize n = text.length;

    for(isize i = 0; i < n; ) {
        const unsigned char c = s[i];
        int extra = 0;
        unsigned int cp = 0;

        if(c < 0x80) {
            i += 1;
            continue;
        } else if((c & 0xE0) == 0xC0) {
            extra = 1;
            cp = c & 0x1F;
        } else if((c & 0xF0) == 0xE0) {
            extra = 2;
            cp = c & 0x0F;
        } else if((c & 0xF8) == 0xF0) {
            extra = 3;
            cp = c & 0x07;
        } else {
            return false;
        }

        if(i + extra >= n) {
            return false;
        }
        for(int k = 1; k <= extra; k++) {
            const unsigned char next = s[i + k];
            if((next & 0xC0) != 0x80) {
                return false;
            }
            cp = (cp << 6) | (next & 0x3F);
        }

        // 过长编码（overlong）与代理区都不算合法 UTF-8
        if((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000)) {
            return false;
        }
        if(cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            return false;
        }

        i += extra + 1;
    }
    return true;
}

std::u8string_view as_u8(xpString text) {
    return std::u8string_view((const char8_t *)text.c_str, (size_t)text.length);
}

std::u8string_view as_u8(const std::string& text) {
    return std::u8string_view((const char8_t *)text.data(), text.size());
}
