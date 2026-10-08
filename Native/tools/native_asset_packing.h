#pragma once
#include "native_asset_storage.h"
#include "lz4hc.h"
#include <algorithm>
#include <stdexcept>

namespace idas3 {
inline void appendPackedWord(std::vector<char>& output,std::uint32_t word) {
    for (int shift=0;shift<32;shift+=8) output.push_back(char(word>>shift));
}
// Build-time only. The HC encoder is never linked into the game.
inline std::vector<char> packNativeAsset(std::span<const char> source) {
    if (source.empty() || source.size()>nativeAssetMaxSize) throw std::runtime_error("Invalid native packing size");
    std::vector<char> output(nativePackedMagic.begin(),nativePackedMagic.end());
    appendPackedWord(output,std::uint32_t(source.size()));appendPackedWord(output,nativeAssetBlockSize);
    std::vector<char> encoded(LZ4_compressBound(nativeAssetBlockSize));
    for (std::size_t offset=0;offset<source.size();offset+=nativeAssetBlockSize) {
        auto block=source.subspan(offset,std::min(std::size_t(nativeAssetBlockSize),source.size()-offset));
        int count=LZ4_compress_HC(block.data(),encoded.data(),int(block.size()),int(encoded.size()),9);
        if (count<=0) throw std::runtime_error("Native asset compression failed");
        bool smaller=std::size_t(count)<block.size();
        appendPackedWord(output,smaller?count:std::uint32_t(block.size()));
        appendPackedWord(output,nativeAssetCrc(block));
        auto start=smaller?encoded.data():block.data();
        output.insert(output.end(),start,start+(smaller?count:block.size()));
    }
    if (output.size()>=source.size()) return {source.begin(),source.end()};
    return output;
}
}
