#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

namespace idas3 {
// Same asset names and decoded formats as before; only the storage wrapper is
// new. Independently compressed blocks bound scratch memory to 512 KiB per
// reader, including for the largest course banks. No temporary extraction.
inline constexpr std::array<char,8> nativePackedMagic{'I','D','A','S','L','Z','4','1'};
inline constexpr std::uint32_t nativeAssetBlockSize = 256 * 1024;
inline constexpr std::uint32_t nativeAssetMaxSize = 512 * 1024 * 1024;
std::uint32_t nativeAssetCrc(std::span<const char> bytes);

class NativeAssetReader {
    std::ifstream input_;
    std::vector<char> decoded_, encoded_;
    std::size_t cursor_ = 0, available_ = 0;
    std::uint32_t remaining_ = 0;
    bool packed_ = false;
    void refill();
public:
    explicit NativeAssetReader(const std::filesystem::path& path);
    void bytes(void* out, std::size_t size);
    std::uint32_t u32() {
        // Most reads are texture pixels or vertex words inside one block.
        if (available_ - cursor_ >= 4) {
            auto p = reinterpret_cast<const unsigned char*>(decoded_.data() + cursor_);
            cursor_ += 4; remaining_ -= 4;
            return p[0] | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
        }
        std::array<unsigned char,4> p{}; bytes(p.data(), p.size());
        return p[0] | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
    }
    float f32();
    bool end();
};
}
