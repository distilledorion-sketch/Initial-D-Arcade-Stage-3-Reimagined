#pragma once
#include <cstddef>
#include <span>
#include <string_view>

namespace idas3::original {
inline bool supportedProgramDigest(std::span<const std::byte> bytes) {
    if (bytes.size() != 32) return false;
    constexpr std::string_view hex = "0123456789abcdef";
    for (std::string_view source : {
        "efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335",
        "62e23a4213a65b677ad9ba46a2659ad5777bf7e87485bd6f8021146197821593"
    }) {
        std::size_t i = 0;
        for (; i < bytes.size(); ++i) {
            auto byte = std::to_integer<unsigned>(bytes[i]);
            if (hex[byte >> 4] != source[i * 2] || hex[byte & 15] != source[i * 2 + 1]) break;
        }
        if (i == bytes.size()) return true;
    }
    return false;
}
}
