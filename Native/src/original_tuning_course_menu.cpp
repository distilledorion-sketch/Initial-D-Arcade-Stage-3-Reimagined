#include "original_tuning_course_menu.h"
#include <bit>
#include <stdexcept>
namespace idas3::original {
namespace {std::int32_t sw(std::uint32_t x){return std::bit_cast<std::int32_t>(x);}}
void initializeOriginalTuningCourseMenu(OriginalTuningCourseMenu& s,OriginalBattleProfile& p,const OriginalTuningData& data){
    s.confirmationFrames460=0;s.phase464=0;s.fade472=7;s.fadeMaximum476=7;s.fadeEnabled480=1;
    s.car484=p.u(16);s.selected496=p.byte(152);s.previous500=p.byte(1192)==1?s.selected496:0xffffffffu;
    s.count504=std::uint32_t(data.car(s.car484).packages.size());
    if(!s.count504||s.selected496>=s.count504)throw std::invalid_argument("original tuning-course selection outside source table");
    s.enteredOnLast628=s.selected496==s.count504-1?1:0;
    s.enabled604=0;s.lastLatched629=0;s.leftLast630=0;s.leftLastFrames632=0;s.timedOut636=0;
    s.selectionFrames488=0;s.frame492=0;s.parentEvent64=0;
    p.setu(1176,1679);
}
OriginalTuningCourseMenuEvents tickOriginalTuningCourseMenu(OriginalTuningCourseMenu& s,
        OriginalBattleProfile& p,const OriginalTuningCourseMenuInput& in){
    OriginalTuningCourseMenuEvents out;
    switch(s.phase464){
    case 0:
        --s.fade472;if(sw(s.fade472)<0){s.fade472=0;s.fadeEnabled480=0;++s.phase464;}break;
    case 1:{
        p.setu(1176,sw(p.u(1176))>0?p.u(1176)-1u:0u);
        if(!p.u(1176))s.timedOut636=1;
        ++s.selectionFrames488;
        if(in.selectedIndex>=s.count504)throw std::invalid_argument("original tuning-course selector outside source table");
        if(s.enteredOnLast628){
            if(in.selectorPhase620==4){
                if(in.selectedIndex==s.count504-1)s.lastLatched629=1;
                if(s.lastLatched629&&in.selectedIndex!=s.count504-1){s.lastLatched629=0;s.leftLast630=1;}
                if(s.leftLast630)++s.leftLastFrames632;
                if(sw(s.leftLastFrames632)>9){s.leftLast630=0;s.leftLastFrames632=0;}
            }
        }else s.enabled604=1;
        if(s.selected496!=in.selectedIndex){
            s.selected496=in.selectedIndex;s.selectionFrames488=0;out.selectionChanged=true;out.cueIds.push_back(2);
        }
        if(in.confirmPressed||s.timedOut636){out.cueIds.push_back(3);++s.phase464;}
        break;}
    case 2:
        ++s.confirmationFrames460;
        if(sw(s.confirmationFrames460)>120){++s.phase464;s.fadeEnabled480=1;s.fadeMaximum476=15;}
        break;
    case 3:
        ++s.fade472;
        if(sw(s.fade472)>15){
            s.fade472=15;p.setByte(152,std::uint8_t(s.selected496));
            out.profileCommitted=true;out.previewCommit=true;
            if(p.byte(1192)==1){
                if(s.previous500!=0xffffffffu&&s.previous500!=s.selected496){
                    for(auto offset:{156u,157u,158u,159u,160u,161u,162u,163u,165u})p.setByte(offset,0);
                    if(s.selected496!=s.count504-1){p.setu(1180,p.u(1180)&~0xc00u);p.setByte(153,0);}
                    else if(!(p.u(1180)&0x400u))p.setByte(153,p.byte(164));
                    p.setByte(155,0);p.setByte(154,0);
                }
                const auto target=(p.u(1180)&0x400000u)?s.alternateScreen80:s.previousScreen76;
                s.parentEvent64=(target<<16)|4u;
            }else s.parentEvent64|=1;
            out.parentRequested=true;
        }
        break;
    case 4:
        ++s.fade472;
        if(sw(s.fade472)>15){s.fade472=15;s.parentEvent64=(s.previousScreen76<<16)|4u;out.parentRequested=true;out.previewCommit=true;}
        break;
    default:break;
    }
    ++s.frame492;return out;
}
std::uint32_t originalTuningCourseFadeArgb(const OriginalTuningCourseMenu& s){
    const float x=(float(sw(s.fade472))/float(sw(s.fadeMaximum476)))*255.f;
    return std::uint32_t(std::int32_t(x))<<24;
}
float originalTuningCourseConfirmationPhase(const OriginalTuningCourseMenu& s){return float(sw(s.confirmationFrames460))/36.f;}
}

