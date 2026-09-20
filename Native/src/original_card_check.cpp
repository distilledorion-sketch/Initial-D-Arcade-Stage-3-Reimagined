#include "original_card_check.h"
#include <bit>
#include <stdexcept>

namespace idas3::original {
namespace {
constexpr std::array<unsigned,5> optionOffsets{1184,1186,1187,1188,1189};
std::int32_t signedWord(std::uint32_t value){return std::bit_cast<std::int32_t>(value);}
void commit(OriginalCardCheckState& s,OriginalBattleProfile& p,OriginalCardCheckEvents& out){
    s.committed1524.at(s.row1460)=s.draft1544.at(s.row1460);
    for(unsigned i=0;i<optionOffsets.size();++i)p.setByte(optionOffsets[i],std::uint8_t(s.committed1524[i]));
    out.profileWritten=true;
}
}
void loadOriginalCardCheckOptions(OriginalCardCheckState& s,const OriginalBattleProfile& p){
    for(unsigned i=0;i<optionOffsets.size();++i)s.committed1524[i]=p.byte(optionOffsets[i]);
    s.draft1544=s.committed1524;
}
OriginalCardCheckEvents tickOriginalCardCheckTransition(OriginalCardCheckState& s,OriginalBattleProfile& p){
    OriginalCardCheckEvents out;
    if(s.page492==7&&s.editing496)s.confirm516=0;
    if(!s.canConfirm524)s.confirm516=0;
    switch(s.phase452){
    case 0:
        --s.fade464;
        if(signedWord(s.fade464)<0){s.fade464=0;s.overlay460=0;s.phase452=1;}
        break;
    case 1:{
        const auto remaining=p.u(1176);p.setu(1176,signedWord(remaining)>0?remaining-1:0);
        if(s.confirm516||!p.u(1176)){
            if(s.confirm516)out.soundGroups.push_back(7);
            s.overlay460=1;++s.phase452;
        }else if(s.view520){
            ++s.page492;
            if(signedWord(s.page492)>7)s.page492=0;
        }
        break;
    }
    case 2:
        ++s.fade464;
        if(signedWord(s.fade464)>15){s.fade464=15;s.parentEvent64=8;out.parentRequested=true;}
        break;
    default:break;
    }
    return out;
}
OriginalCardCheckEvents tickOriginalCardCheck(OriginalCardCheckState& s,OriginalBattleProfile& p,const OriginalCardCheckInput& input){
    if(input.rowDirection< -1||input.rowDirection>1||s.row1460>=5)throw std::out_of_range("Original card configuration row/input");
    OriginalCardCheckEvents out;
    s.confirm516=input.confirm?1:0;s.view520=input.nextPage?16:0;
    if(input.nextPage){commit(s,p,out);s.row1460=0;s.editPhase1456=0;}
    if(s.page492==7){
        if(input.rowDirection){
            if(s.editPhase1456==0){
                s.editPhase1456=1;
                if(input.rowDirection==1)s.row1460=4;
            }else if(s.editPhase1456==1||s.editPhase1456==2){
                out.soundGroups.push_back(7);
                if(s.editPhase1456==2){commit(s,p,out);s.editPhase1456=1;}
                s.row1460+=input.rowDirection==1?std::uint32_t(-1):1u;
                if(signedWord(s.row1460)<0||signedWord(s.row1460)>4){s.row1460=0;s.editPhase1456=0;}
            }
        }
        if(s.editPhase1456==1){
            if(s.confirm516)s.editPhase1456=2;
        }else if(s.editPhase1456==2){
            if(s.selectedValue1452!=input.selectorValue)out.soundGroups.push_back(2);
            s.selectedValue1452=input.selectorValue;s.draft1544[s.row1460]=input.selectorValue;
            if(s.confirm516){s.editPhase1456=1;commit(s,p,out);}
            if(input.cancel){s.editPhase1456=1;s.draft1544=s.committed1524;}
        }
    }
    ++s.frame448;s.editing496=s.editPhase1456;
    const auto transition=tickOriginalCardCheckTransition(s,p);
    out.soundGroups.insert(out.soundGroups.end(),transition.soundGroups.begin(),transition.soundGroups.end());
    out.parentRequested=transition.parentRequested;
    return out;
}
}
