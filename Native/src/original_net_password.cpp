#include "original_net_password.h"
#include "original_net_password_data.h"
#include <span>
#include <stdexcept>
namespace idas3::original {
namespace {
using namespace net_password_data;
std::uint32_t checksum(std::span<const std::uint8_t> bytes){
    std::uint32_t value=0x57377534;
    for(unsigned i=0;i<bytes.size();++i)value+=(bytes[i]*13u+71u*i+1u)*0x19660du+0x3c6ef35fu;
    return value;
}
std::uint32_t permute(std::uint32_t value,const std::array<unsigned,32>& table){
    std::uint32_t out=0;for(unsigned i=0;i<32;++i)if(value&(1u<<(31-i)))out|=1u<<(31-table[i]);return out;
}
std::uint32_t substitute(std::uint32_t value){return (value&0xffff0000u)|(substitution[(value>>8)&255]<<8)|substitution[value&255];}
}
std::array<unsigned,14> originalNetPassword(const OriginalNetPasswordInput& in){
    if(in.course>8||in.direction>1||in.weather>1||in.night>1||in.car>34||in.manual>1||!in.ticks6000)
        throw std::invalid_argument("Invalid original NetRankings record");
    std::uint32_t a=(in.course==8?128u:(in.course&15)<<5)|(in.car&31)|((in.salt&1023)<<8);
    std::uint32_t b=((baseTimes[in.weather*10+in.course]-in.ticks6000)&0xfffff)<<5;
    b|=(in.course==8?0u:in.night<<2)|(in.direction<<3)|(in.weather<<4)|(in.manual<<1)|((in.car>>5)&1);
    std::array<std::uint8_t,7> name{};for(unsigned i=0;i<5;++i)name[i]=in.name[i]==202?55:in.name[i];
    name[5]=std::uint8_t(in.salt);name[6]=std::uint8_t(in.salt>>16);
    const auto key=checksum(name)&0xfffff;
    const std::array<std::uint8_t,7> bytes{std::uint8_t((b>>24)&1),std::uint8_t(b>>16),std::uint8_t(b>>8),std::uint8_t(b),std::uint8_t(a),std::uint8_t(a>>8),std::uint8_t((a>>16)&3)};
    a|=(checksum(bytes)<<18)&0xfc0000u;
    for(unsigned round=0;round<14;++round){
        const auto aa=(a&0xffffce74u)|(b&0x318bu),bb=(b&0xffffce74u)|(a&0x318bu);
        a=substitute(permute(aa,permutationA)^key);b=substitute(permute(bb,permutationB)^key);
        a=((a&0x7fffff)<<1)|((a>>23)&1);b=((b&0xffffff)<<1)|((b>>24)&1);
    }
    std::array<unsigned,14> digits{},out{};
    for(int i=6;i>=0;--i){digits[i]=a%12;a/=12;digits[i+7]=b%12;b/=12;}
    for(unsigned i=0;i<14;++i)out[i]=symbols[digits[order[i]]];return out;
}
std::string originalNetPasswordText(const OriginalNetPasswordInput& in){
    std::string out;const auto code=originalNetPassword(in);
    for(unsigned i=0;i<code.size();++i){if(i==3||i==7||i==11)out+=' ';out+=char(characters[code[i]]&255);}return out;
}
unsigned originalNetPasswordGlyph(unsigned symbol){
    if(symbol>=characters.size())throw std::out_of_range("Password symbol");
    for(unsigned i=0;i<glyphCharacters.size();++i)if(glyphCharacters[i]==characters[symbol])return i;return 0;
}
}
