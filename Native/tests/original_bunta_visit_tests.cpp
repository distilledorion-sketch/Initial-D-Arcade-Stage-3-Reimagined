#include "original_bunta_visit.h"
#include "sh4_scalar_reference.h"
#include <fstream>
#include <iostream>

using namespace idas3::original;
using namespace idas3::reference;
namespace {
std::uint64_t checks=0,instructions=0;
void check(bool yes,const char* label){++checks;if(!yes)throw std::runtime_error(label);}
std::string token(const RefMemory&m,unsigned p){std::string s;for(unsigned i=0;i<4096;++i){auto c=m.read8(p+i);if(!c)return s;s+=char(c);}throw std::runtime_error("token bound");}
void savePpm(const std::filesystem::path&p,const std::vector<std::uint32_t>&pixels){std::ofstream o(p,std::ios::binary);o<<"P6\n640 480\n255\n";for(auto c:pixels){o.put(char(c>>16));o.put(char(c>>8));o.put(char(c));}}
}
int main(int argc,char**argv)try{
    if(argc!=4)throw std::runtime_error("usage: original_bunta_visit_tests native-root canonical-image proof-directory");
    const auto root=std::filesystem::path(argv[1]),proof=std::filesystem::path(argv[3]);std::filesystem::create_directories(proof);
    const auto data=OriginalRivalDialogData::load(root);check(data.hasBunta(),"Bunta extension present");
    RefMemory m(argv[2]);
    // Independent original record/token verification; raw Bunta rows need no startup emulation.
    for(unsigned course=0;course<9;++course)for(unsigned kind=0;kind<51;++kind){
        const auto&r=data.buntaRecord(course,kind);const auto a=0x0c363100+(course==8?3:course)*0x660+kind*32;
        check(r.sourceRecord==a&&r.sourceScript==m.read32(a),"Bunta record selector");
        for(unsigned i=0;i<6;++i)check(std::bit_cast<unsigned>(r.portraitCoordinates[i])==m.read32(a+4+i*4),"Bunta coordinates");
        for(unsigned i=0;i<r.tokens.size();++i){check(r.tokens[i].sourceAddress==m.read32(r.sourceScript+i*4),"Bunta token pointer");check(r.tokens[i].bytes==token(m,r.tokens[i].sourceAddress),"Bunta original text");}
    }
    constexpr unsigned paddr=0x0c31c99c,owner=0x0cd00000,dialog=0x0cd02000,stack=0x0cfff000,frame=0x0cfe0000,stop=0x00ff0000;
    // Run only Bunta scalar Init selection blocks, hooking graphics/dialog/audio constructors.
    for(unsigned course=0;course<9;++course)for(unsigned level:{0u,5u,10u,11u,15u,16u})
    for(unsigned akina:{0u,10u,11u,15u,16u})for(unsigned phase=0;phase<3;++phase){
        auto p=makeOriginalFreshBattleProfile();p.setu(0,2);p.setu(4,course);p.setu(1092,akina);p.setu(1080+course*4,level);
        m.clear();m.zeroRegion(owner,0x4000);m.zeroRegion(frame,0x20000);
        for(unsigned i=0;i<p.words.size();++i)m.write32(paddr+i*4,p.words[i]);
        m.write32(owner+80,dialog);m.write32(frame+52,course);m.write32(frame+72,owner+64);
        unsigned captured=999;RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.r[14]=frame;c.pr=stop;
        c.callHooks[0x0c0ef620]=[](auto&){};c.callHooks[0x0c055d60]=[](auto&){};
        c.callHooks[0x0c141ec0]=[](auto& c){c.r[0]=0;};
        c.callHooks[0x0c0f88c0]=[&](auto& c){captured=c.r[5];check(c.r[6]==course,"Bunta original course argument");};
        if(phase==0)instructions+=c.run(0x0c182cd0,0x0c182d0a,200);
        else instructions+=c.run(phase==1?0x0c182760:0x0c182a60,stop,300);
        const auto actual=originalBuntaDialogKind(p,phase==0,phase==1?0:1);
        check(actual==captured,"Bunta script kind equals original Init");
        for(unsigned i=0;i<p.words.size();++i)check(p.words[i]==m.read32(paddr+i*4),"Bunta Init profile mutation");
    }
    // Every authored course/level record must naturally finish, using its own
    // script and original interpreter. Frame bounds catch missing command wiring.
    auto p=makeOriginalFreshBattleProfile();p.setu(0,2);p.setu(72,100000);p.setu(1180,1);
    unsigned maxFrames=0;std::ofstream csv(proof/"bunta-dialog-lifetimes.csv");csv<<"course,kind,frames,portrait,background\n";
    for(unsigned course=0;course<9;++course)for(unsigned kind=0;kind<51;++kind){
        p.setu(4,course);OriginalRivalDialogSetup setup;setup.enemy=30;setup.kind=kind;setup.buntaChallenge=true;setup.buntaCourse=course;
        OriginalRivalDialogState state;resetOriginalRivalDialog(state,data,p,setup);unsigned frames=0;
        for(;frames<20000&&!originalRivalDialogReady(state);++frames)stepOriginalRivalDialog(state,data,p);
        check(frames<20000,"Bunta script terminates");check(state.character==31&&state.variant==34+course,"Bunta distinct scene");
        check(state.portraitChunk<12,"Bunta portrait valid");
        maxFrames=std::max(maxFrames,frames);csv<<course<<','<<kind<<','<<frames<<','<<state.portraitChunk<<','<<state.backgroundChunk<<'\n';
    }
    OriginalBuntaVisit visit;check(OriginalBuntaVisit::available(root),"Bunta artwork available");visit.load(root);
    for(unsigned course=0;course<9;++course)for(unsigned phase=0;phase<3;++phase){
        p.setu(4,course);p.setu(1080+course*4,phase==1?6:5);OriginalBuntaVisit::Setup setup;setup.beforeRace=phase==0;setup.resultStatus=phase==2?1:0;
        visit.begin(p,setup);auto events=visit.takeEvents();check(events.size()==2&&events[1].a==33+phase,"Bunta cue33/34/35");
        for(unsigned f=0;f<70;++f)visit.advance(p,{});
        std::vector<std::uint32_t> pixels(640*480,0xff000000);visit.paint(pixels,640,480);
        check(std::count_if(pixels.begin(),pixels.end(),[](auto c){return (c&0xffffff)!=0;})>10000,"Bunta art painted");
        if(course==3)savePpm(proof/(std::string("bunta-")+(phase==0?"before":phase==1?"win":"loss")+".ppm"),pixels);
        for(unsigned f=0;f<60&&!visit.finished();++f)visit.advance(p,{true});
        check(visit.finished(),"Bunta skip closes within source fades");
        events=visit.takeEvents();check(std::count_if(events.begin(),events.end(),[](auto e){return e.command==OriginalLegendReturnCommand::MusicFade;})==1,"Bunta fade once");
    }
    std::cout<<"PASS "<<checks<<" Bunta checks, "<<instructions<<" bounded original scalar instructions;459 authored-course script runs,max "<<maxFrames<<" frames;27 scene/cue/skip/paint cases.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
