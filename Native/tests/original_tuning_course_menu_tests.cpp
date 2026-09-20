#include "original_tuning_course_menu.h"
#include "sh4_scalar_reference.h"
#include <bit>
#include <iostream>
using namespace idas3::original;using namespace idas3::reference;
namespace {
std::size_t checks{},instructions{};constexpr auto owner=0x0d000000u,stack=0x0d004000u,pbase=0x0c31c99cu;
void eq(std::uint32_t a,std::uint32_t b,const char* n){++checks;if(a!=b)throw std::runtime_error(std::string(n)+":"+hex(a)+" != "+hex(b));}
using Field=std::pair<unsigned,std::uint32_t OriginalTuningCourseMenu::*>;
const Field fields[]={{460,&OriginalTuningCourseMenu::confirmationFrames460},{464,&OriginalTuningCourseMenu::phase464},
 {472,&OriginalTuningCourseMenu::fade472},{476,&OriginalTuningCourseMenu::fadeMaximum476},{480,&OriginalTuningCourseMenu::fadeEnabled480},
 {484,&OriginalTuningCourseMenu::car484},{488,&OriginalTuningCourseMenu::selectionFrames488},{496,&OriginalTuningCourseMenu::selected496},
 {500,&OriginalTuningCourseMenu::previous500},{504,&OriginalTuningCourseMenu::count504},{64,&OriginalTuningCourseMenu::parentEvent64},
 {76,&OriginalTuningCourseMenu::previousScreen76},{80,&OriginalTuningCourseMenu::alternateScreen80},{632,&OriginalTuningCourseMenu::leftLastFrames632}};
using ByteField=std::pair<unsigned,std::uint8_t OriginalTuningCourseMenu::*>;
const ByteField byteFields[]={{604,&OriginalTuningCourseMenu::enabled604},{628,&OriginalTuningCourseMenu::enteredOnLast628},
 {629,&OriginalTuningCourseMenu::lastLatched629},{630,&OriginalTuningCourseMenu::leftLast630},{636,&OriginalTuningCourseMenu::timedOut636}};
void compareTick(RefMemory& m,OriginalTuningCourseMenu& s,OriginalBattleProfile& p,OriginalTuningCourseMenuInput in){
    m.clear();m.zeroRegion(owner,0x6000);
    for(auto [offset,member]:fields)m.write32(owner+offset,s.*member);
    for(auto [offset,member]:byteFields)m.write8(owner+offset,s.*member);
    for(unsigned j=0;j<p.words.size();++j)m.write32(pbase+j*4,p.words[j]);
    m.write32(owner+620,in.selectorPhase620);
    std::vector<unsigned> cues;unsigned preview=0,changed=0;
    RefCpu c(m);c.r[11]=owner;c.r[14]=stack;c.r[15]=stack+0x1000;c.r[8]=owner+96;c.r[13]=in.confirmPressed;
    c.callHooks[0x0c1b39c0]=[&](auto& cpu){cpu.r[0]=in.selectedIndex;}; // actual input-selector boundary
    c.callHooks[0x0c141f80]=[&](auto& cpu){eq(cpu.r[5],1,"cue argument");cues.push_back(cpu.r[4]);};
    c.callHooks[0x0c0ddaa0]=[&](auto& cpu){++changed;cpu.r[0]=0;};
    c.callHooks[0x0c12a820]=[](auto& cpu){cpu.r[0]=0;}; // preview geometry reconstruction
    c.callHooks[0x0c12b520]=[](auto&){}; // preview-only appearance calls
    c.callHooks[0x0c12b440]=[&](auto& cpu){eq(cpu.r[5],1,"preview commit");++preview;};
    c.callHooks[0x0c055d60]=[](auto&){};
    instructions+=c.run(0x0c12addc,0x0c12b14a,3000);
    // The non-controller tail increments492 after reconstructing the preview
    // list. Execute that actual increment independently; no rendering hooks.
    c.r[11]=owner;m.write32(owner+492,s.frame492);
    instructions+=c.run(0x0c12b298,0x0c12b2a6,30);
    const auto event=tickOriginalTuningCourseMenu(s,p,in);
    for(auto [offset,member]:fields)eq(s.*member,m.read32(owner+offset),"menu field");
    for(auto [offset,member]:byteFields)eq(s.*member,m.read8(owner+offset),"menu byte");
    eq(s.frame492,m.read32(owner+492),"menu frame");
    for(unsigned j=0;j<p.words.size();++j)eq(p.words[j],m.read32(pbase+j*4),"full profile mutation");
    eq(event.previewCommit,preview!=0,"preview event");eq(event.selectionChanged,changed!=0,"selected event");
    eq(event.cueIds.size(),cues.size(),"cue count");for(unsigned j=0;j<cues.size();++j)eq(event.cueIds[j],cues[j],"cue order");
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("Usage: original_tuning_course_menu_tests canonical_image game_root");
    RefMemory m(argv[1]);const auto data=OriginalTuningData::load(argv[2]);
    // Actual Init's phase/fade/selected/previous block, including imported
    // byte values. This is independent of seeding Main from native state.
    for(unsigned kind=0;kind<256;++kind){
        m.clear();m.zeroRegion(owner,0x6000);
        auto p=makeOriginalFreshBattleProfile();p.setByte(1192,std::uint8_t(kind));p.setByte(152,1);
        for(unsigned j=0;j<p.words.size();++j)m.write32(pbase+j*4,p.words[j]);
        m.write32(stack+216,owner);
        RefCpu c(m);c.r[14]=stack;c.r[0]=304;
        instructions+=c.run(0x0c129dd4,0x0c129e64,100);
        OriginalTuningCourseMenu native;initializeOriginalTuningCourseMenu(native,p,data);
        for(auto [offset,member]:fields)if(offset==460||offset==464||offset==472||offset==476||offset==480||offset==496||offset==500)
            eq(native.*member,m.read32(owner+offset),"actual initialization");
        RefCpu timer(m);timer.r[15]=stack+0x1000;timer.pr=0x0f000000;
        instructions+=timer.run(0x0c192840,0x0f000000,30);eq(p.u(1176),timer.r[0],"actual init timer");
    }
    for(unsigned car=0;car<35;++car){
        RefCpu table(m);table.r[4]=car;table.r[15]=stack;table.pr=0x0f000000;
        instructions+=table.run(0x0c0dda80,0x0f000000,100);
        eq(data.car(car).packages.size(),table.r[0],"source package count");
        for(auto kind:{0u,1u,2u})for(unsigned phase=0;phase<5;++phase)for(unsigned variation=0;variation<10;++variation){
            auto p=makeOriginalFreshBattleProfile();p.setu(16,car);p.setByte(1192,std::uint8_t(kind));
            for(unsigned j=152;j<172;++j)p.setByte(j,std::uint8_t(11+j));
            p.setByte(152,std::uint8_t(variation%data.car(car).packages.size()));
            p.setu(1180,0x81u|((variation&1)?0x400000u:0)|((variation&2)?0x400u:0)|((variation&4)?0x800u:0));
            p.setu(72,134321);p.setu(1176,variation%3);
            OriginalTuningCourseMenu s;initializeOriginalTuningCourseMenu(s,p,data);p.setu(1176,variation%3);
            s.phase464=phase;s.fade472=variation%2?15:0;s.confirmationFrames460=variation%2?120:119;
            s.selected496=variation%s.count504;s.previous500=variation%3==0?0xffffffffu:(variation+1)%s.count504;
            s.enteredOnLast628=variation&1;s.lastLatched629=variation&2;s.leftLast630=variation&4;s.leftLastFrames632=variation;
            compareTick(m,s,p,{(variation+1)%s.count504,bool(variation&1),variation%2?4u:0u});
        }
    }
    // Sequential migrated driver: original kind2 preserves all earned tuning
    // and points while permitting the player to choose the package and name.
    auto p=makeOriginalFreshBattleProfile();p.setu(72,234567);p.setByte(1192,2);p.setByte(164,7);p.setByte(161,2);
    OriginalTuningCourseMenu s;initializeOriginalTuningCourseMenu(s,p,data);
    unsigned ticks=0;while(!s.parentEvent64&&ticks<500){++ticks;compareTick(m,s,p,{1,ticks==9,0});}
    eq(ticks,146,"eight entry + confirm +121 hold +16 fade");eq(s.parentEvent64,1,"advance to name");
    eq(p.u(72),234567,"migration retains points");eq(p.byte(164),7,"migration retains performance stage");eq(p.byte(161),2,"migration retains wheel");
    p=makeOriginalFreshBattleProfile();p.setByte(1192,0);initializeOriginalTuningCourseMenu(s,p,data);ticks=0;
    while(!s.parentEvent64&&ticks<2000){++ticks;compareTick(m,s,p,{0,false,0});}
    eq(ticks,1824,"timeout source duration");eq(p.u(1176),0,"shared timer exhausted");
    std::cout<<"PASS tuning course controller: "<<checks<<" comparisons, "<<instructions<<" actual instructions; analog selector, preview resources and platform cues are explicit boundaries.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
