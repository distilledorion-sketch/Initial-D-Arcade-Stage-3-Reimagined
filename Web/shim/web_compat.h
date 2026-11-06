#pragma once
// Force-included (-include) when building the core for non-Windows targets.
// It only adds the few Win32 names the portable scene path still references;
// nothing here changes behaviour on Windows, where this header is not used.
#include "host_platform.h"
#include <cstring>
#include <cstdint>

#if !defined(_WIN32)
constexpr WORD XINPUT_GAMEPAD_LEFT_SHOULDER = 0x0100, XINPUT_GAMEPAD_RIGHT_SHOULDER = 0x0200;

#define strnlen_s(s, n) strnlen((s), (n))

// Only the "is this valid UTF-8" use of MultiByteToWideChar is needed: it must
// return > 0 for valid input and 0 for invalid input (MB_ERR_INVALID_CHARS).
constexpr unsigned CP_UTF8 = 65001, MB_ERR_INVALID_CHARS = 8;
inline int MultiByteToWideChar(unsigned, unsigned, const char* text, int length, wchar_t*, int) {
    const auto* s = reinterpret_cast<const unsigned char*>(text);
    int units = 0;
    for (int i = 0; i < length;) {
        const unsigned c = s[i];
        int extra = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : -1;
        if (extra < 0 || i + extra >= length) return 0;  // bad lead byte or truncated sequence
        std::uint32_t cp = extra == 0 ? c : c & (0x3f >> extra);
        for (int k = 1; k <= extra; ++k) {
            if ((s[i + k] & 0xc0) != 0x80) return 0;
            cp = (cp << 6) | (s[i + k] & 0x3f);
        }
        if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000) ||
            cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return 0;
        units += cp > 0xffff ? 2 : 1;
        i += extra + 1;
    }
    return units;
}
#endif
