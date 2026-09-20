#include "original_mode_menu.h"
#include "sh4_scalar_reference.h"
#include <bit>
#include <fstream>
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
void bitmap(const std::filesystem::path& p,const std::vector<std::uint32_t>& data,int w,int h){std::ofstream f(p,std::ios::binary);auto u16=[&](unsigned v){f.put(char(v));f.put(char(v>>8));};auto u32=[&](unsigned v){for(int i=0;i<4;i++)f.put(char(v>>(8*i)));};u16(0x4d42);u32(unsigned(54+data.size()*4));u32(0);u32(54);u32(40);u32(w);u32(unsigned(-h));u16(1);u16(32);u32(0);u32(unsigned(data.size()*4));u32(2835);u32(2835);u32(0);u32(0);for(auto v:data)u32(v);}
int main(int argc,char**argv){try{
 if(argc<2)throw std::runtime_error("canonical image required");RefMemory m(argv[1]);constexpr unsigned object=0xd000000,stack=0xd010000,stop=0xff0000;std::size_t instructions=0,checks=0;
 for(unsigned old=0;old<3;old++)for(unsigned next=0;next<3;next++)for(unsigned frames:{0u,1u,20u,100u}){
  m.clear();m.zeroRegion(object,2048);m.zeroRegion(stack,0x10000);OriginalModeMenuState state;state.selected=OriginalGameMode(old);state.selectedFrames=frames;state.focusFrames={1,5,20};state.focusValues={.05f,.25f,1.f};
  m.write32(object+432,old);m.write32(object+440,frames);for(unsigned i=0;i<3;i++){m.write32(object+444+4*i,state.focusFrames[i]);m.writeFloat(object+456+4*i,state.focusValues[i]);}
  RefCpu c(m);c.r[4]=object;c.r[5]=next;c.r[15]=stack+0xf000;c.pr=stop;instructions+=c.run(0xc1934a0,stop,1000);selectOriginalGameMode(state,OriginalGameMode(next));require(m.read32(object+432)==unsigned(state.selected)&&m.read32(object+440)==state.selectedFrames,"original mode selection state");checks+=2;
  for(unsigned i=0;i<3;i++){require(m.read32(object+444+4*i)==unsigned(state.focusFrames[i])&&m.read32(object+456+4*i)==std::bit_cast<unsigned>(state.focusValues[i]),"original mode focus reset");checks+=2;}
 }
 for(float phase:{-1.f,0.f,.01f,.25f,.5f,.75f,1.f,2.f}){OriginalModeMenuState state;RefCpu c(m);c.r[4]=object;c.r[15]=stack+0xf000;c.pr=stop;c.setFloat(4,phase);instructions+=c.run(0xc1934e0,stop,1000);setOriginalModeConfirmation(state,phase);require(m.read32(object+436)==std::bit_cast<unsigned>(state.confirmationPhase),"original mode phase clamp");++checks;}
 for(unsigned selected=0;selected<3;selected++)for(unsigned tick=0;tick<24;tick++)for(float phase:{0.f,.25f,.5f,.75f,1.f}){
  m.clear();m.zeroRegion(object,2048);m.zeroRegion(stack,0x10000);OriginalModeMenuState state;state.selected=OriginalGameMode(selected);state.confirmationPhase=phase;state.selectedFrames=tick;state.focusFrames={int(tick),int(tick),int(tick)};state.focusValues={0,0,0};m.write32(object+432,selected);m.write32(object+440,tick);m.writeFloat(object+436,phase);for(unsigned i=0;i<3;i++){m.write32(object+444+4*i,tick);m.write32(object+456+4*i,0);}for(unsigned off:{416u,420u,424u,428u})m.write32(object+off,object+off+600);
  RefCpu c(m);c.r[4]=object;c.r[15]=stack+0xf000;c.pr=stop;for(unsigned a:{0xc1b8fe0u,0xc1baf40u,0xc1bb6e0u,0xc05a8e0u,0xc1d7120u,0xc1b81c0u,0xc193500u,0xc1f6610u,0xc1f65c0u,0xc1f6ac0u})c.callHooks[a]=[](auto& cpu){cpu.r[0]=1;};instructions+=c.run(0xc193740,stop,10000);stepOriginalModeMenu(state);require(m.read32(object+440)==state.selectedFrames,"source menu tick");++checks;for(unsigned i=0;i<3;i++){require(m.read32(object+444+4*i)==unsigned(state.focusFrames[i])&&m.read32(object+456+4*i)==std::bit_cast<unsigned>(state.focusValues[i]),"source capped focus timer");checks+=2;}
  const auto draws=originalModeMenuDraws(state);require(!draws.empty(),"original mode command list");for(const auto& d:draws)require(std::isfinite(d.x)&&std::isfinite(d.y)&&std::isfinite(d.scale),"nonfinite mode draw");
 }
 if(argc>2){OriginalModeMenu menu;menu.load(argv[2]);auto folder=std::filesystem::path(argv[2])/"verification/original-mode-menu";std::filesystem::create_directories(folder);for(unsigned i=0;i<3;i++){OriginalModeMenuState state;state.selected=OriginalGameMode(i);state.selectedFrames=20;bitmap(folder/(std::to_string(i)+".bmp"),menu.paint(1280,720,state),1280,720);}}
 std::cout<<"PASS mode selection/phase/timers: "<<checks<<" exact comparisons,"<<instructions<<" actual instructions. Render/matrix boundaries hooked; draw/layout not opcode-certified.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
