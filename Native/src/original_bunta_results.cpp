#include "original_bunta_results.h"
#include <array>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
std::int32_t signedWord(std::uint32_t v){return std::bit_cast<std::int32_t>(v);}
unsigned courseIndex(const OriginalBattleProfile& p){
    if(p.u(0)!=2||p.u(4)>8)throw std::invalid_argument("Original Bunta result requires selected mode2/course0..8");
    return p.u(4)==8?3:p.u(4);
}
std::uint32_t truncateOriginal(float f){
    if(std::isnan(f)||f<=-2147483648.f)return 0x80000000u;
    if(f>=2147483648.f)return 0x7fffffffu;
    return std::uint32_t(std::int32_t(f));
}
}
bool recordOriginalBuntaResult(OriginalBattleProfile& p,std::uint8_t finished,std::uint32_t outcome){
    const auto course=courseIndex(p);
    p.setu(1180,p.u(1180)&0xffcfffffu); //05CEB4..05CEC6
    if(!finished||outcome!=0)return false;
    const auto offset=1080+course*4,level=p.u(offset);
    if(signedWord(level)<=14){p.setu(offset,level+1u);p.setu(1180,p.u(1180)|0x00100000u);}
    else if(level==15)p.setu(offset,16);
    return true;
}
OriginalBuntaPoints calculateOriginalBuntaPoints(const OriginalBattleProfile& p,std::uint32_t status,float advantage){
    const auto level=signedWord(p.u(1080+courseIndex(p)*4));
    const unsigned tier=level>9?2:level>4?1:0;
    OriginalBuntaPoints out;
    const auto balance=p.u(72);
    if(status==0){
        constexpr std::array<unsigned,3> awards={3000,6000,12000}; //28FD78
        out.win=awards[tier];
        if(advantage>0.f){
            // These are the exact reads made at1881DC..1881F2. The source
            // indexes by raw course8 for Snow, past its eight-row factor
            // table. Retain those adjacent literal bits rather than invent
            // a Snow bonus or silently applying the course3 multiplier.
            const auto course=p.u(4);
            const float factor=std::bit_cast<float>(course==8?0x000005dcu:0x41200000u);
            const auto cap=course==8?0x4f43205bu:1500u;
            out.advantage=truncateOriginal(advantage*factor);
            if(signedWord(out.advantage)>signedWord(cap))out.advantage=cap;
        }
        out.total=out.win+out.advantage;
        out.balanceBeforeCap=balance+out.total;
    }else{
        constexpr std::array<unsigned,3> charges={1000,2000,4000}; //28FD84, negated
        auto delta=0u-charges[tier];
        if(signedWord(balance)<=signedWord(0u-delta))delta=0u-balance;
        out.total=signedWord(delta)<0?0u-delta:delta;
        out.balanceBeforeCap=balance+delta;out.deduction=true;
    }
    return out;
}
OriginalBuntaPoints awardOriginalBuntaPoints(OriginalBattleProfile& p,std::uint32_t status,float advantage){
    const auto out=calculateOriginalBuntaPoints(p,status,advantage);
    p.setu(72,out.balanceBeforeCap>999999998u?999999999u:out.balanceBeforeCap);
    return out;
}
}
