#pragma once
#include <array>
#include <bit>
#include <cstdint>
inline std::array<std::int32_t,16> originalDspTestInput(std::uint32_t& random,unsigned frame){
    std::array<std::int32_t,16> result{};
    for(unsigned bus=0;bus<16;++bus){
        random^=random<<13;random^=random>>17;random^=random<<5;
        if(frame>=128&&frame<4096)result[bus]=std::bit_cast<std::int32_t>(random)>>14;
        if(frame==bus*3)result[bus]=(bus&1)?-0x30000:0x30000;
        if(frame>=8192&&frame<9216)result[bus]=(int((frame*37+bus*97)&1023)-512)*128;
    }
    return result;
}
inline void originalDspTestHash(std::uint64_t& hash,std::uint32_t value){
    for(unsigned i=0;i<4;++i){hash^=(value>>(8*i))&255;hash*=1099511628211ull;}
}
inline constexpr std::uint64_t originalDspHashBasis=14695981039346656037ull;
