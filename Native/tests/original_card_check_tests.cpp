#include "original_card_check.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <string>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0x0d000000,stack=0x0d040000,profile=0x0c31c99c;
constexpr unsigned vtable=0x0d003000,callback=0x0d003100,stop=0x0d003200;
unsigned cases=0;std::uint64_t instructions=0,comparisons=0;std::string scenario;
void eq(unsigned a,unsigned b,const std::string& label){++comparisons;if(a!=b)throw std::runtime_error(scenario+" "+label+": "+hex(a)+" != "+hex(b));}
void seed(RefMemory& m,const OriginalCardCheckState& s,const OriginalBattleProfile& p,const OriginalCardCheckInput& in){
    m.clear();m.zeroRegion(owner,0x50000);m.zeroRegion(profile,0x1000);m.zeroRegion(0x0c92ed00,0x80);
    for(unsigned j=0;j<p.words.size();++j)m.write32(profile+4*j,p.words[j]);
    for(auto [offset,value]:std::initializer_list<std::pair<unsigned,unsigned>>{
        {64,s.parentEvent64},{448,s.frame448},{452,s.phase452},{460,s.overlay460},{464,s.fade464},
        {492,s.page492},{496,s.editing496},{516,s.confirm516},{520,s.view520},
        {1452,s.selectedValue1452},{1456,s.editPhase1456},{1460,s.row1460}})m.write32(owner+offset,value);
    m.write8(owner+524,s.canConfirm524);
    for(unsigned j=0;j<5;++j){m.write32(owner+1524+4*j,s.committed1524[j]);m.write32(owner+1544+4*j,s.draft1544[j]);}
    m.write32(owner+432,vtable);m.write32(vtable+48,0);m.write32(vtable+52,callback);
    m.write8(0x0c92ed00,in.nextPage?16:0);
    m.write8(0x0c92ed40,in.rowDirection==1?16:in.rowDirection==-1?32:0);
}
void compare(RefMemory& m,const OriginalCardCheckState& s,const OriginalBattleProfile& p){
    for(auto [offset,value]:std::initializer_list<std::pair<unsigned,unsigned>>{
        {64,s.parentEvent64},{448,s.frame448},{452,s.phase452},{460,s.overlay460},{464,s.fade464},
        {492,s.page492},{496,s.editing496},{516,s.confirm516},{520,s.view520},
        {1452,s.selectedValue1452},{1456,s.editPhase1456},{1460,s.row1460}})eq(m.read32(owner+offset),value,"owner+"+std::to_string(offset));
    for(unsigned j=0;j<5;++j){eq(m.read32(owner+1524+4*j),s.committed1524[j],"committed option");eq(m.read32(owner+1544+4*j),s.draft1544[j],"draft option");}
    for(unsigned j=0;j<p.words.size();++j)eq(m.read32(profile+4*j),p.words[j],"profile+"+std::to_string(4*j));
}
OriginalCardCheckEvents tick(RefMemory& m,OriginalCardCheckState& s,OriginalBattleProfile& p,const OriginalCardCheckInput& in){
    seed(m,s,p,in);RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;unsigned callbacks=0;std::vector<unsigned> sounds;
    // Only hardware sampling, the pre-existing analog selector, sound output
    // and the base virtual page callback are bounded. Configuration commits,
    // cancel, page gates, fades and timer execute from the original image.
    c.callHooks[0x0c0d4300]=[&](auto& cpu){cpu.r[0]=cpu.r[5]==1?in.confirm:in.cancel;};
    c.callHooks[0x0c0d4340]=[](auto& cpu){cpu.setFloat(0,0.f);};
    c.callHooks[0x0c1f6ad0]=[](auto&){};
    c.callHooks[0x0c09bc40]=[&](auto& cpu){cpu.r[0]=in.selectorValue;};
    c.callHooks[0x0c141f80]=[&](auto& cpu){eq(cpu.r[5],1,"sound command");sounds.push_back(cpu.r[4]);};
    c.callHooks[callback]=[&](auto&){++callbacks;};
    instructions+=c.run(0x0c10c6c0,stop,10000);
    const auto out=tickOriginalCardCheck(s,p,in);compare(m,s,p);eq(unsigned(sounds.size()),unsigned(out.soundGroups.size()),"sound count");
    for(unsigned i=0;i<sounds.size();++i)eq(sounds[i],out.soundGroups[i],"sound group");
    eq(callbacks,1,"base callback count");++cases;return out;
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Usage: original_card_check_tests canonical_image");
    RefMemory memory(argv[1]);
    for(unsigned page:{0u,6u,7u})for(unsigned phase=0;phase<4;++phase)
    for(unsigned edit=0;edit<3;++edit)for(unsigned row=0;row<5;++row)
    for(unsigned mask=0;mask<8;++mask)for(int direction:{-1,0,1})
    for(unsigned countdown:{0u,1u,17u})for(unsigned enabled:{0u,1u}){
        OriginalCardCheckState s;s.page492=page;s.phase452=phase;s.fade464=phase==0?1:phase==2?15:0;
        s.overlay460=phase!=1;s.editPhase1456=edit;s.editing496=edit;s.row1460=row;s.selectedValue1452=2;s.canConfirm524=std::uint8_t(enabled);
        s.committed1524={0,2,4,1,0};s.draft1544={1,1,6,0,1};
        auto p=makeOriginalFreshBattleProfile();p.setu(1176,countdown);p.setu(1184,0xabcdef01);p.setu(1188,0x12345678);
        scenario="page="+std::to_string(page)+" phase="+std::to_string(phase)+" edit="+std::to_string(edit)+" row="+std::to_string(row)+" mask="+std::to_string(mask)+" direction="+std::to_string(direction);
        tick(memory,s,p,{bool(mask&1),bool(mask&2),bool(mask&4),direction,3});
    }
    // Connected sequence: enter Configuration, edit, cancel, edit/confirm,
    // change page, and leave. This catches cross-tick state interactions.
    OriginalCardCheckState s;s.page492=7;s.phase452=1;auto p=makeOriginalFreshBattleProfile();loadOriginalCardCheckOptions(s,p);
    scenario="connected options flow";
    tick(memory,s,p,{false,false,false,-1,0});
    tick(memory,s,p,{true,false,false,0,0});
    const auto original=p.words;
    tick(memory,s,p,{false,false,false,0,1});
    tick(memory,s,p,{false,true,false,0,1});
    eq(p.byte(1184),original[1184/4]&255,"cancel preserves stored setting");
    tick(memory,s,p,{true,false,false,0,1});
    tick(memory,s,p,{true,false,false,0,1});
    eq(p.byte(1184),1,"confirmed setting committed");
    tick(memory,s,p,{false,false,true,0,1});eq(s.page492,0,"last page wraps");
    tick(memory,s,p,{true,false,false,0,1});
    for(unsigned i=0;i<16;++i)tick(memory,s,p,{});
    eq(s.parentEvent64,8,"confirmed exit after source fade");
    std::cout<<"PASS original card-check input/transition: "<<cases<<" cases, "<<comparisons<<" comparisons, "<<instructions<<" source instructions. Rendering and analog interpolation excluded.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
