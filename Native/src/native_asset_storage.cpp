#include "native_asset_storage.h"
#include "lz4.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3 {
namespace {
void readExact(std::ifstream& input, void* out, std::size_t size) {
    if (!input.read(static_cast<char*>(out), std::streamsize(size)))
        throw std::runtime_error("Truncated native asset");
}
std::uint32_t readWord(std::ifstream& input) {
    std::array<unsigned char,4> p{}; readExact(input,p.data(),p.size());
    return p[0] | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
constexpr auto makeCrcTable() {
    std::array<std::uint32_t,256> table{};
    for (std::uint32_t i=0;i<256;++i) {
        auto c=i;
        for (int bit=0;bit<8;++bit) c=(c>>1)^((c&1)?0xedb88320u:0);
        table[i]=c;
    }
    return table;
}
constexpr auto crcTable=makeCrcTable();
}
std::uint32_t nativeAssetCrc(std::span<const char> bytes) {
    std::uint32_t crc=0xffffffffu;
    for (unsigned char byte:bytes) crc=(crc>>8)^crcTable[(crc^byte)&255];
    return ~crc;
}
NativeAssetReader::NativeAssetReader(const std::filesystem::path& path):input_(path,std::ios::binary) {
    if (!input_) throw std::runtime_error("Cannot open native asset: " + path.string());
    const auto storedSize=std::filesystem::file_size(path);
    if (storedSize>nativeAssetMaxSize) throw std::runtime_error("Native asset file budget exceeded");
    remaining_=std::uint32_t(storedSize);
    if (storedSize>=nativePackedMagic.size()) {
        std::array<char,8> magic{};readExact(input_,magic.data(),magic.size());
        packed_=magic==nativePackedMagic;
        if (packed_) {
            remaining_=readWord(input_);
            if (!remaining_ || remaining_>nativeAssetMaxSize || readWord(input_)!=nativeAssetBlockSize)
                throw std::runtime_error("Invalid packed native asset header");
        } else input_.seekg(0);
    }
    decoded_.resize(std::min(remaining_,nativeAssetBlockSize));
    if (packed_) encoded_.resize(decoded_.size());
}
void NativeAssetReader::refill() {
    if (!remaining_) throw std::runtime_error("Truncated native asset");
    available_=std::min(remaining_,nativeAssetBlockSize);cursor_=0;
    if (!packed_) {readExact(input_,decoded_.data(),available_);return;}
    auto stored=readWord(input_),crc=readWord(input_);
    if (!stored || stored>available_) throw std::runtime_error("Invalid packed native asset block length");
    if (stored==available_) readExact(input_,decoded_.data(),stored);
    else {
        readExact(input_,encoded_.data(),stored);
        if (LZ4_decompress_safe(encoded_.data(),decoded_.data(),int(stored),int(available_))!=int(available_))
            throw std::runtime_error("Corrupt packed native asset block");
    }
    if (nativeAssetCrc({decoded_.data(),available_})!=crc)
        throw std::runtime_error("Packed native asset checksum mismatch");
}
void NativeAssetReader::bytes(void* out,std::size_t size) {
    if (size>remaining_) throw std::runtime_error("Truncated native asset");
    auto target=static_cast<char*>(out);
    while (size) {
        if (cursor_==available_) refill();
        auto count=std::min(size,available_-cursor_);
        std::memcpy(target,decoded_.data()+cursor_,count);
        target+=count;cursor_+=count;remaining_-=std::uint32_t(count);size-=count;
    }
}
float NativeAssetReader::f32() {
    float value=std::bit_cast<float>(u32());
    if (!std::isfinite(value)) throw std::runtime_error("Non-finite original sprite attribute");
    return value;
}
bool NativeAssetReader::end() {
    return !remaining_ && input_.peek()==std::char_traits<char>::eof();
}
}
