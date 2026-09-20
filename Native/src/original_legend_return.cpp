#include "original_legend_return.h"
#include "original_legend_progress.h"
#include <array>
#include <bit>
#include <stdexcept>

namespace idas3::original {
namespace {
constexpr std::array<unsigned,9> first{0,3,6,9,18,14,21,24,17};
constexpr std::array<unsigned,9> count{3,3,3,5,3,3,3,6,1};
constexpr std::array<unsigned,31> lossMusic{36,36,35,35,35,36,36,35,35,36,35,38,35,35,35,35,35,38,35,35,35,35,35,35,36,35,38,38,35,35,35};
constexpr std::array<unsigned,31> lastCourse{0,0,0,1,1,1,2,2,2,3,3,3,4,4,5,5,5,4,6,6,6,7,7,7,8,8,9,9,10,10,4};
std::int32_t si(std::uint32_t x){return std::bit_cast<std::int32_t>(x);}
void check(const OriginalBattleProfile& p){if(p.u(0)!=0||p.u(4)>8||p.u(24)>30)throw std::invalid_argument("Legend return requires original mode0/course0..8/rival0..30");}
unsigned wins(const OriginalBattleProfile& p,unsigned e){return p.byte(116+e)>>4;}
unsigned losses(const OriginalBattleProfile& p,unsigned e){return p.byte(116+e)&15;}
bool cleared(const OriginalBattleProfile& p,unsigned course,int excludeFirstWin=-1){
    for(unsigned e=first[course];e<first[course]+count[course];++e)
        if(!wins(p,e)||(int(e)==excludeFirstWin&&wins(p,e)==1))return false;
    return true;
}
void event(OriginalLegendReturnFrame& f,OriginalLegendReturnCommand c,unsigned a=0,unsigned b=0){f.events.push_back({c,a,b});}
//1341A0: the continue timer saturates at zero.
void timer(OriginalBattleProfile& p){p.setu(1176,si(p.u(1176))>0?p.u(1176)-1:0);}
OriginalLegendReturnFrame frame(const OriginalLegendReturnState& s){
    return {s.phase108,s.selected248,s.confirmFrame504,s.choiceFrame520,{},s.finished,
        originalLegendReturnDestination(s.continue92,s.action96)};
}
void accept(OriginalLegendReturnState& s,OriginalBattleProfile& p,OriginalLegendReturnFrame& f){
    s.continue92=true;event(f,OriginalLegendReturnCommand::ContinueAccepted);
    p.setByte(1191,2);if(p.u(1180)&2)p.setu(1180,(p.u(1180)&~2u)|1u);
}
void finish(OriginalLegendReturnState& s,OriginalBattleProfile& p,OriginalLegendReturnFrame& f){
    //0F0760: 31CE18 -64 +52 = profile1136.
    if(!(p.u(1180)&0x2000)&&!p.byte(1184))p.setu(1136,lastCourse[s.enemy112]);
    s.finished=true;f.finished=true;f.destination=originalLegendReturnDestination(s.continue92,s.action96);
    event(f,OriginalLegendReturnCommand::Finish,unsigned(f.destination));
}
// The next-rival painters (0F0F20/0F1EC0) tick the continue timer and advance
// the owner's own animation counter once per update, before the phase checks.
void drawNextRival(OriginalLegendReturnState& s,OriginalBattleProfile& p,OriginalLegendReturnFrame& f){
    event(f,OriginalLegendReturnCommand::DrawNextRival,s.selected248,s.confirmFrame504);timer(p);++s.choiceFrame520;f.choiceFrame=s.choiceFrame520;
}
// After a plain dialog close the common objects are released first (0F07C0);
// after the course-clear movie they were released when the movie began, and
// sound set2 is re-selected explicitly before0EF620 rebuilds everything.
void nextRival(OriginalLegendReturnState& s,OriginalBattleProfile& p,OriginalLegendReturnFrame& f,bool continuationEnabled,bool afterCourseClear){
    using C=OriginalLegendReturnCommand;
    if(afterCourseClear)event(f,C::SoundSet,2);else event(f,C::DestroyCommon);
    //0EF620 re-creates the selector and resets the fields at0EFAF0/0EFDE4.
    event(f,C::InitializeCommon);p.setu(1176,879);
    s.selected248=0;s.choiceFrame520=0;
    s.enemy112=originalLegendNextRival(p,s.enemy112);
    event(f,C::MusicRequest,s.enemy112+3,continuationEnabled);
    const unsigned kind=wins(p,s.enemy112)?14:losses(p,s.enemy112)?7:0;
    selectOriginalRival(p,s.enemy112);
    event(f,C::ConfigureDialog,s.enemy112,kind);s.phase108=202;
}
void choose(OriginalLegendReturnState& s,OriginalLegendReturnFrame& f,const OriginalLegendReturnInput& in){
    if(in.selectedIndex>1)throw std::invalid_argument("Original binary return selector");
    if(s.selected248!=in.selectedIndex){s.selected248=in.selectedIndex;event(f,OriginalLegendReturnCommand::Cue,2);}
    f.selectedIndex=s.selected248;
}
//0F232A: common objects released, fade quad and course movie constructed,
// then the movie audio stream16 under sound set1.
void startCourseClear(OriginalLegendReturnState& s,const OriginalBattleProfile& p,OriginalLegendReturnFrame& f){
    using C=OriginalLegendReturnCommand;
    event(f,C::DestroyCommon);event(f,C::StartCourseClear,p.u(4));s.fadeFrame104=0;
    event(f,C::SoundSet,1);event(f,C::StreamVolume,127);event(f,C::StreamStart,16);event(f,C::StreamVolume,127);event(f,C::StreamPlay);
    s.phase108=211;
}
}
OriginalLegendReturnDestination originalLegendPostResultChild(std::uint32_t status){
    return status?OriginalLegendReturnDestination::RivalLost:OriginalLegendReturnDestination::RivalWon;
}
OriginalLegendReturnDestination originalLegendReturnDestination(bool cont,std::uint32_t action){
    if(action==3)return OriginalLegendReturnDestination::Ending;
    if(!cont)return OriginalLegendReturnDestination::EjectCard;
    return action==1?OriginalLegendReturnDestination::CourseLoad:OriginalLegendReturnDestination::Select;
}
std::uint32_t originalLegendNextRival(const OriginalBattleProfile& p,std::uint32_t enemy){
    if(enemy>30)throw std::out_of_range("Original next rival");const unsigned next=enemy==30?0:enemy+1;
    const auto flags=p.u(1180);const unsigned mask=next==30?0x8000u:next==28?0x08000000u:next==29?0x4000u:0;
    return mask&&!(flags&mask)?0:next;
}
unsigned originalLegendLossMusic(std::uint32_t enemy){return lossMusic.at(enemy);}
OriginalLegendReturnFrame initializeOriginalLegendReturn(OriginalLegendReturnState& s,OriginalBattleProfile& p,const OriginalLegendReturnSetup& setup){
    if(s.initialized)return frame(s);check(p);s={};s.initialized=true;s.won=setup.resultStatus80==0;
    s.resultStatus80=setup.resultStatus80;s.continue92=setup.continue92;s.action96=setup.action96;
    s.enemy112=p.u(24);s.phase108=s.won?200:100;p.setu(1176,879);
    auto f=frame(s);event(f,OriginalLegendReturnCommand::InitializeCommon);
    unsigned kind=0,music=0;
    if(s.won){
        kind=wins(p,s.enemy112)!=1?17:losses(p,s.enemy112)?10:3;
        music=s.enemy112==11||s.enemy112==17||s.enemy112==26?37:34;
        const bool alreadyCleared=cleared(p,p.u(4),int(s.enemy112));
        refreshOriginalLegendCourseProgress(p);
        s.courseClear528=!alreadyCleared&&cleared(p,p.u(4));
    }else{
        kind=s.resultStatus80==2?21:wins(p,s.enemy112)?18:losses(p,s.enemy112)==1?4:11;
        music=lossMusic[s.enemy112];
    }
    event(f,OriginalLegendReturnCommand::ConfigureDialog,s.enemy112,kind);
    event(f,OriginalLegendReturnCommand::MusicRequest,music,1);return f;
}
OriginalLegendReturnFrame advanceOriginalLegendReturn(OriginalLegendReturnState& s,OriginalBattleProfile& p,const OriginalLegendReturnInput& in){
    if(!s.initialized)throw std::logic_error("Legend return not initialized");
    auto f=frame(s);if(s.finished)return f;
    using C=OriginalLegendReturnCommand;
    const unsigned base=s.won?200:100;
    //Shared continuation/credit/retry phases have the same source bodies, but
    //different phase numbers. Normal dialog Main precedes this owner dispatch.
    const unsigned continueChoice=base+(s.won?3:1),continueConfirm=continueChoice+1;
    const unsigned credit=continueChoice+2,creditDelay=continueChoice+3;
    const unsigned nextChoice=continueChoice+4,nextConfirm=continueChoice+5;
    const unsigned reply=continueChoice+6,closing=continueChoice+7;
    if(s.phase108==continueChoice){
        timer(p);choose(s,f,in);event(f,C::DrawContinue,s.selected248,s.confirmFrame504);
        if(in.confirm||p.u(1176)==0){event(f,C::Cue,3);s.phase108=continueConfirm;s.confirmFrame504=0;if(!p.u(1176))s.selected248=1;}
    }else if(s.phase108==continueConfirm){
        ++s.confirmFrame504;event(f,C::DrawContinue,s.selected248,s.confirmFrame504);f.confirmFrame=s.confirmFrame504;
        if(si(s.confirmFrame504)>40){
            if(s.selected248==0&&si(p.u(1176))>0){
                if(in.freePlay||in.creditReady){s.phase108=creditDelay;s.creditFrame492=0;}
                else{s.phase108=credit;s.creditFrame492=0;p.setu(1176,879);}
            }else{s.phase108=creditDelay;s.continue92=false;s.creditFrame492=0;}
        }
    }else if(s.phase108==credit){
        event(f,C::DrawCredit,s.creditFrame492,p.u(1176));
        if(in.coinEvent==1||in.coinEvent==2)p.setu(1176,879);
        if(si(s.creditFrame492)>180&&in.debugSkip)p.setu(1176,si(p.u(1176))>60?(p.u(1176)/60)*60-1:0);
        else timer(p);
        ++s.creditFrame492;
        if(in.creditReady){s.phase108=creditDelay;s.creditFrame492=0;}
        else if(!p.u(1176)){s.continue92=false;s.phase108=creditDelay;s.creditFrame492=0;}
    }else if(s.phase108==creditDelay){
        if(si(++s.creditFrame492)>3){
            if(s.continue92){accept(s,p,f);s.phase108=nextChoice;p.setu(1176,879);}
            else{event(f,C::AdvanceDialog,2);s.phase108=reply;}
        }
    }else if(s.phase108==nextChoice){
        choose(s,f,in);drawNextRival(s,p,f);
        if(in.confirm||!p.u(1176)){event(f,C::Cue,3);s.phase108=nextConfirm;s.confirmFrame504=0;}
    }else if(s.phase108==nextConfirm){
        ++s.confirmFrame504;drawNextRival(s,p,f);f.confirmFrame=s.confirmFrame504;
        if(si(s.confirmFrame504)>40){
            if(s.selected248==0){
                s.action96=1;selectOriginalRival(p,s.enemy112);
                if(!s.won||in.nextDialogPageExists){event(f,C::AdvanceDialog,1);s.phase108=reply;}
                else{event(f,C::CloseDialog);event(f,C::MusicFade);s.phase108=closing;}
            }else{s.action96=2;event(f,C::AdvanceDialog,2);s.phase108=reply;}
        }
    }else if(s.phase108==reply){
        if(in.debugSkip)event(f,C::SkipDialog);
        if(in.dialogReady){event(f,C::CloseDialog);s.phase108=closing;event(f,C::MusicFade);}
    }else if(s.phase108==closing){
        if(in.dialogClosed)finish(s,p,f);
    }else if(s.phase108==base){
        if(in.debugSkip)event(f,C::SkipDialog);
        if(in.dialogReady){
            if(s.won){s.phase108=201;event(f,C::CloseDialog);event(f,C::MusicFade);}
            else if(s.enemy112==30){
                if(s.resultStatus80<=1)s.action96=3;
                s.continue92=false;event(f,C::MusicFade);event(f,C::CloseDialog);s.phase108=108;
            }else if(in.continuationEnabled){s.phase108=101;s.confirmFrame504=0;}
            else{s.continue92=false;event(f,C::AdvanceDialog,2);s.phase108=107;}
        }
    }else if(s.won&&s.phase108==201){
        if(in.dialogClosed){
            if(s.courseClear528)startCourseClear(s,p,f);
            else if(s.enemy112==30){event(f,C::MusicFade);s.continue92=false;s.action96=3;s.phase108=210;}
            else if(!in.continuationEnabled){event(f,C::MusicFade);s.continue92=false;event(f,C::CloseDialog);s.phase108=210;}
            else nextRival(s,p,f,true,false);
        }
    }else if(s.won&&s.phase108==202){
        if(in.debugSkip)event(f,C::SkipDialog);
        if(in.dialogReady){
            if(s.enemy112==30&&!p.u(148)){
                s.action96=1;selectOriginalRival(p,s.enemy112);s.continue92=true;
                event(f,C::CloseDialog);event(f,C::MusicFade);s.phase108=210;
            }else if(in.continuationEnabled)s.phase108=203;
            else{event(f,C::AdvanceDialog,2);s.continue92=false;s.phase108=209;}
        }
    }else if(s.won&&s.phase108==211){
        event(f,C::StepCourseClear,s.courseClearFrame532);
        if(si(s.courseClearFrame532)>237){
            ++s.fadeFrame104;float alpha=float(s.fadeFrame104)/60.f;alpha*=255.f;
            event(f,C::CourseClearFade,unsigned(alpha)>255?255:unsigned(alpha));
        }
        const unsigned old=s.courseClearFrame532++;
        if(si(old)>300){
            event(f,C::StreamStop);event(f,C::FinishCourseClear);
            nextRival(s,p,f,in.continuationEnabled,true);
            if(!in.continuationEnabled){s.continue92=false;event(f,C::CloseDialog);s.phase108=210;}
        }else if(s.courseClearFrame532==150)event(f,C::StreamFade,8);
    }else throw std::logic_error("Unknown original Legend return phase");
    return f;
}
}
