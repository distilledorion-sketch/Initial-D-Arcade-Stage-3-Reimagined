#include "original_tuning_child.h"
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
float f(std::uint32_t bits){return std::bit_cast<float>(bits);}
bool after(std::uint32_t frame,int limit){return std::int32_t(frame)>limit;}
void selectRecords(OriginalTuningChild& s,const OriginalBattleProfile& p,const OriginalCarTuningData& d){
    s.skip=1;
    if(s.selected>=s.count){s.current=-1;return;}
    s.current=std::int32_t(s.selected);
    while(s.selected+s.skip<s.count){
        const unsigned index=s.selected+s.skip;
        s.next=std::int32_t(index);
        const bool attained=s.kind==OriginalTuningChildKind::basic?
            d.packages.at(s.package).steps[index].words[0]==8&&p.byte(164)>=d.packages.at(s.package).steps[index].words[1]:
            p.byte(164)>=d.performance[index].words[0];
        if(!attained)return;
        ++s.skip;
    }
    s.next=-1;
}
void basicDescription(OriginalTuningChild& s,const OriginalCarTuningData& d,OriginalTuningChildFrame& out,bool chained){
    if(s.current<0)return;
    const auto& r=d.packages.at(s.package).steps.at(s.current).words;
    out.descriptionChanged=true;out.descriptionAddress=r[3];out.descriptionX=r[0]==8?100.0f:chained?240.0f:250.0f;
    s.picture=r[1];s.extraIndex=r[4];
}
void basicFlags(OriginalTuningChild& s,const OriginalCarTuningData& d,bool chained){
    const auto& r=d.packages.at(s.package).steps.at(s.current).words;
    if(chained){const unsigned before=s.flags;s.flags|=4;if(r[0]!=8)s.flags=before|12;}
    else if(r[0]!=8)s.flags|=8;
    const unsigned before=s.flags;s.flags|=16;if(std::int32_t(s.extraIndex)>=0)s.flags=before|18;
}
unsigned optionalCursor(unsigned cursor,float axis){
    //09C200 initializes the two-choice selector at1. Preserve the original
    // separately rounded arithmetic and FMAC for its hysteresis thresholds.
    const float fifth=f(0x3e4ccccd),half=0.5f;
    const float upperStep=fifth/(1.0f-fifth),lowerStep=fifth/(1.0f+fifth),gap=f(0x3dcccccd)/2.0f;
    float lower1=half-lowerStep*half;lower1-=gap;
    const float upper1=std::fma(upperStep,half,half)+gap;
    float center0=half-lowerStep*half;center0-=lowerStep;
    const float lower0=center0-gap,upper0=(center0+lowerStep)+gap;
    if((cursor?lower1:lower0)>axis&&cursor>0)return cursor-1;
    if(axis>(cursor?upper1:upper0)&&cursor<1)return cursor+1;
    return cursor;
}
}
OriginalTuningChild beginOriginalTuningChild(const OriginalBattleProfile& p,const OriginalTuningData& data,OriginalTuningChildKind kind){
    OriginalTuningChild s;s.kind=kind;s.car=p.u(16);s.package=p.byte(152);s.selected=p.byte(153);s.balance=p.u(72);
    const auto& d=data.car(s.car);
    if(kind==OriginalTuningChildKind::basic)s.count=unsigned(d.packages.at(s.package).steps.size());
    else if(kind==OriginalTuningChildKind::performance)s.count=unsigned(d.performance.size());
    else if(kind==OriginalTuningChildKind::optionalPart){s.optionalIndex=p.byte(154);if(s.optionalIndex>=d.optional.size())throw std::invalid_argument("Original optional tuning child has no part");}
    return s;
}
OriginalTuningChildFrame advanceOriginalTuningChild(OriginalTuningChild& s,OriginalBattleProfile& p,const OriginalTuningData& data,const OriginalTuningChildInput& input){
    OriginalTuningChildFrame out;const auto& d=data.car(s.car);
    const auto transition=[&](unsigned phase){s.phase=phase;s.frame=0;};
    if(s.kind==OriginalTuningChildKind::basic){
        const auto& rows=d.packages.at(s.package).steps;
        switch(s.phase){
        case 0:
            if(s.frame==40)out.command=11;
            if(after(s.frame,60)){s.selected=p.byte(153);selectRecords(s,p,d);basicDescription(s,d,out,false);if(s.current>=0)out.command=12;transition(1);}break;
        case 1:
            s.flags|=4;if(s.frame==40)out.command=11;
            if(after(s.frame,60)){transition(2);basicFlags(s,d,false);out.command=12;}break;
        case 2:if(after(s.frame,60)){transition(3);out.command=1;}break;
        case 3:if(after(s.frame,30)){
            transition(4);s.nextThreshold=0xffffffffu;
            if(s.next>=0){s.nextThreshold=rows[s.next].words[2];if(rows.at(s.current).words[2]==s.nextThreshold)s.phase=9;else{s.flags|=32;out.command=13;}}
        }break;
        case 4:if(after(s.frame,30))transition(5);break;
        case 5:if(after(s.frame,120)){
            if(p.u(1180)&0x400){transition(7);s.completionX=f(0xc099999a);s.completionY=s.completionZ=0;s.flags=65;out.command=8;}
            else{transition(6);if(s.next>=0&&std::int32_t(s.nextThreshold)<=std::int32_t(s.balance)&&s.current>=0&&rows[s.current].words[2]==rows[s.next].words[2]){s.phase=0;s.flags=1;out.command=7;}}
        }break;
        case 6:out.command=14;break;
        case 7:{float x=float(std::int32_t(30-s.frame));x*=f(0xc099999a);x/=20.0f;s.completionX=x;if(s.frame==1)out.command=11;
            if(after(s.frame,20)){s.completionX=0;transition(8);out.command=12;}break;}
        case 8:if(after(s.frame,240))transition(6);break;
        case 9:
            if(s.frame==130)out.command=11;
            if(after(s.frame,150)){s.selected=p.byte(153);selectRecords(s,p,d);s.flags=1;basicDescription(s,d,out,true);basicFlags(s,d,true);transition(2);out.command=12;}break;
        default:break;
        }
    }else if(s.kind==OriginalTuningChildKind::performance){
        switch(s.phase){
        case 0:
            if(s.frame==40)out.command=11;
            if(after(s.frame,60)){s.selected=p.byte(153);selectRecords(s,p,d);if(s.current>=0){out.descriptionChanged=true;out.descriptionAddress=d.performance[s.current].words[2];out.descriptionX=90;}transition(1);out.command=12;}break;
        case 1:s.flags|=4;if(s.frame==40)out.command=11;if(after(s.frame,60)){transition(2);s.flags|=16;out.command=12;}break;
        case 2:if(after(s.frame,60)){transition(3);out.command=2;}break;
        case 3:if(after(s.frame,30)){transition(4);s.nextThreshold=0xffffffffu;if(s.next>=0){s.nextThreshold=d.performance[s.next].words[1];s.flags|=32;out.command=13;}}break;
        case 4:if(after(s.frame,30))transition(5);break;
        case 5:if(after(s.frame,120)){transition(6);if(p.u(1180)&0x800)out.command=9;}break;
        case 6:out.command=14;break;
        default:break;
        }
    }else if(s.kind==OriginalTuningChildKind::optionalPart){
        const auto& row=d.optional.at(s.optionalIndex).words;
        switch(s.phase){
        case 0:{
            p.setu(1176,std::int32_t(p.u(1176))>0?p.u(1176)-1:0);
            s.choiceCursor=optionalCursor(s.choiceCursor,input.selectionAxis);
            if(s.choice!=s.choiceCursor){s.choice=s.choiceCursor;out.directCueIds.push_back(2);out.command=s.choice?6:5;}
            if(input.confirm){if(s.choice==0){s.balanceBeforeSpend=s.balance;transition(1);out.command=11;}else{transition(5);out.command=4;}}
            else if(!p.u(1176)){transition(5);out.command=4;}break;}
        case 1:if(after(s.frame,60)){transition(2);out.command=3;}break;
        case 2:case 4:if(after(s.frame,60))transition(s.phase+1);break;
        case 3:s.balance=s.balanceBeforeSpend-(row[3]*s.frame)/60;out.command=10;if(std::int32_t(s.frame)>=60){transition(4);s.balance=p.u(72);}break;
        case 5:out.command=14;break;
        default:break;
        }
    }else{out.command=14;}
    ++s.frame;out.finished=out.command==14;return out;
}
}
