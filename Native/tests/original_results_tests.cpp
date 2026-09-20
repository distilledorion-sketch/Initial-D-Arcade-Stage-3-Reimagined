#include "original_results.h"
#include "sh4_scalar_reference.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace idas3;
using namespace idas3::reference;
namespace {
void require(bool b,const std::string& message){if(!b)throw std::runtime_error(message);}
void bitmap(const std::filesystem::path& path,const std::vector<std::uint32_t>& pixels,unsigned w,unsigned h){
    std::ofstream f(path,std::ios::binary);auto u16=[&](unsigned v){for(int i=0;i<2;i++)f.put(char(v>>(8*i)));};auto u32=[&](unsigned v){for(int i=0;i<4;i++)f.put(char(v>>(8*i)));};
    u16(0x4d42);u32(54+w*h*4);u32(0);u32(54);u32(40);u32(w);u32(0u-h);u16(1);u16(32);u32(0);u32(w*h*4);u32(2835);u32(2835);u32(0);u32(0);for(auto p:pixels)u32(p);require(bool(f),"results preview write failed");
}
}
int main(int argc,char**argv){try{
    if(argc<3)throw std::runtime_error("canonical-image game-root [preview-directory]");
    const auto native=OriginalTimeAttackResults::load(argv[2]);RefMemory m(argv[1]);
    constexpr unsigned hud=0x0d000000,state=0x0d001000,mainBank=0x0d002000,taBank=0x0d003000,stack=0x0d010000,stop=0x00ff0000;
    std::size_t cases=0,instructions=0,comparisons=0,draws=0;
    for(unsigned flags=0;flags<16;flags++)for(unsigned available=0;available<4;available++)for(unsigned time:{0u,6u,5999u,6000u,3599999u,3600000u,0xffffffffu}){
        OriginalResultsState s;s.recordFlags=flags<<27;s.bestTimes6000={time,time,available&2?time:0};s.modelBestAvailable=(available&1)!=0;
        const auto expected=native.drawList(s);std::size_t next=0;
        m.clear();m.zeroRegion(hud,0x4000);m.zeroRegion(stack,0x10000);m.zeroRegion(0x0c98ad0c,12);m.zeroRegion(0x0ce00000,0x10000);m.zeroRegion(0x0c31ce18,40);
        m.write16(0x0c98ad0e,32);m.write32(0x0c98ad10,0x0ce00000);m.write32(0x0c98ad14,0x0ce00000);
        m.write32(hud+104,mainBank);m.write32(hud+220,taBank);m.write32(state+104,0x8004|s.recordFlags);
        for(unsigned i=0;i<3;i++)m.write32(state+48+4*i,s.bestTimes6000[i]);m.write32(state+60,s.modelBestAvailable?1:0);
        RefCpu c(m);c.r[4]=hud;c.r[5]=state;c.r[15]=stack+0xf000;c.pr=stop;
        for(unsigned i=0;i<16;i++)c.xf[i]=std::bit_cast<unsigned>(i%5==0?1.f:0.f);
        // Explicit composition/renderer boundaries. The base HUD is separately
        // verified; all new TA selection, formatting and matrices run actual bytes.
        c.callHooks[0x0c0c96a0]=[](auto&){};
        c.callHooks[0x0c145a00]=[](auto&){};
        c.callHooks[0x0c145ac0]=[](auto&){};
        // These two source helpers enter PR1 FPU mode, unsupported by this oracle.
        c.callHooks[0x0c2223e0]=[](auto& cpu){require(cpu.r[5]!=0,"zero clock divisor");cpu.fpul=cpu.r[4]/cpu.r[5];};
        c.callHooks[0x0c2223b8]=[](auto& cpu){require(cpu.r[5]!=0,"zero glyph divisor");cpu.fpul=unsigned(signed32(cpu.r[4])/signed32(cpu.r[5]));};
        c.callHooks[0x0c145ae0]=[&](auto& cpu){
            require(next<expected.size(),"extra original results draw");const auto& e=expected[next++];
            require(cpu.r[4]==(e.bank==OriginalResultsDraw::Bank::race?mainBank:taBank),"results bank mismatch");
            require(cpu.r[5]==e.index,"results chunk mismatch draw "+std::to_string(next)+" native "+std::to_string(e.index)+" original "+std::to_string(cpu.r[5]));comparisons+=2;
            for(unsigned i=0;i<16;i++){++comparisons;require(std::bit_cast<unsigned>(e.matrix.elements[i])==cpu.xf[i],"results matrix mismatch draw "+std::to_string(next)+" chunk "+std::to_string(e.index)+" word "+std::to_string(i));}
            ++draws;
        };
        instructions+=c.run(0x0c0ceb20,stop,200000);require(next==expected.size(),"missing original results draw");require(m.read16(0x0c98ad0c)==0,"unbalanced original results matrix stack");++cases;
    }
    if(argc>3){
        const std::filesystem::path directory=argv[3];std::filesystem::create_directories(directory);
        for(unsigned status=0;status<4;status++){
            OriginalResultsState s;s.carId=0;s.condition=6;s.totalTicks6000=1024566;s.remainingTicks6000=180000;
            s.sectionCount=3;s.sectionTimes6000={240000,510000,780000,0};s.bestTimes6000={1024566,1040000,1050000};s.modelBestAvailable=true;
            s.recordFlags=status?OriginalResultsState::newRecord|(OriginalResultsState::courseRecord<<(status-1)):0;s.edgeAnchored=true;
            std::vector<std::uint32_t> pixels(1280*720,0xff34485b);native.paint(pixels,1280,720,s);
            require(std::count_if(pixels.begin(),pixels.end(),[](auto p){return p!=0xff34485b;})>20000,"results artwork missing");
            bitmap(directory/("time-attack-results-"+std::to_string(status)+".bmp"),pixels,1280,720);
        }
    }
    for(unsigned capacity:{2u,3u,4u}){
        OriginalBattleResultsState result;result.sectionCapacity=capacity;result.sectionCount=capacity-1;result.totalTicks6000=600000;
        for(unsigned i=0;i<result.sectionCount;++i)result.sectionTimes6000[i]=100000*(i+1);
        auto timeout=result;timeout.resultStatus=2;completeOriginalResultSections(timeout);require(timeout.sectionCount==capacity-1,"Timeout appended finish");
        completeOriginalResultSections(result);require(result.sectionCount==capacity&&result.sectionTimes6000[capacity-1]==600000,"Missing final section");
        completeOriginalResultSections(result);require(result.sectionCount==capacity,"Finish appended twice");
    }
    if(argc>3){
        OriginalResultsState live;live.livePanel=true;live.edgeAnchored=true;live.bestTimes6000={1100000,1150000,1200000};live.modelBestAvailable=true;
        for(unsigned comparison=0;comparison<2;++comparison){
            live.differenceAvailable={comparison!=0,comparison!=0};live.differences6000={1200,-2400};
            std::vector<std::uint32_t> pixels(1280*720,0xff34485b);native.paint(pixels,1280,720,live);
            for(unsigned y=0;y<720;++y)for(unsigned x=0;x<1000;++x)require(pixels[y*1280+x]==0xff34485b,"Live panel overwrote other HUD regions");
            bitmap(std::filesystem::path(argv[3])/("live-records-"+std::to_string(comparison)+".bmp"),pixels,1280,720);
        }
    }
    for(unsigned record:{OriginalResultsState::courseRecord,OriginalResultsState::modelRecord,OriginalResultsState::personalBest}){
        OriginalResultsState announcement;announcement.recordFlags=OriginalResultsState::newRecord|record;
        announcement.announcementOnly=true;announcement.edgeAnchored=true;
        const auto commands=native.drawList(announcement);
        require(commands.size()==2,"Record announcement contains only title and record type");
        for(const auto& command:commands)require(command.bank==OriginalResultsDraw::Bank::timeAttack&&command.index>=20&&command.index<=23,"Race HUD leaked into record announcement");
        for(const auto size:{std::pair{640,480},std::pair{1280,720},std::pair{2560,1080}}){
            const auto [w,h]=size;std::vector<std::uint32_t> pixels(w*h,0xff34485b);
            native.paint(pixels,w,h,announcement);unsigned changed=0;
            for(int y=0;y<h;++y)for(int x=0;x<w;++x){
                if(y<h/3||y>=2*h/3)require(pixels[y*w+x]==0xff34485b,"Race clocks or HUD remain outside announcement");
                changed+=pixels[y*w+x]!=0xff34485b;
            }
            require(changed>1000,"Record announcement artwork absent");
            if(argc>3)bitmap(std::filesystem::path(argv[3])/("announcement-"+std::to_string(record)+"-"+std::to_string(w)+".bmp"),pixels,w,h);
        }
    }
    OriginalResultsState bad;bad.sectionTimes6000[0]=1;bad.sectionCount=1;bool rejected=false;try{native.drawList(bad);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"non-cumulative result rejected");
    std::cout<<"PASS "<<cases<<" actual-byte TA results cases, "<<draws<<" draws, "<<comparisons<<" exact comparisons, "<<instructions<<" instructions; declared base-HUD/graphics/PR1-division boundaries.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
