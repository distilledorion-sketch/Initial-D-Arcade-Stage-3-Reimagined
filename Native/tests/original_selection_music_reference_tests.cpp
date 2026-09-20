#include "original_selection_music.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
#include <vector>
using namespace idas3::original;using namespace idas3::reference;
namespace {
constexpr unsigned obj=0x0cd00000,stack=0x0cfff000,stopPc=0x00ed0000;
std::uint64_t cases=0,checks=0,instructions=0,loaderHooks=0;
void eq(unsigned actual,unsigned expected,const char* what){++checks;if(actual!=expected)throw std::runtime_error(std::string(what)+" "+hex(actual)+" != "+hex(expected));}
void seed(RefMemory&m,const OriginalSelectionMusicState&s){m.clear();m.zeroRegion(obj,0x10000);m.zeroRegion(stack-0x4000,0x5000);m.zeroRegion(0xc8ff000,0x1000);m.zeroRegion(0xc31de00,0x60);
 m.write32(0xc31de08,obj);m.write32(obj,unsigned(s.handle0));m.write8(obj+4,s.playing4);m.write32(obj+8,s.command8);
 m.write8(obj+20,s.loadBusy20);m.write16(obj+22,s.controlArgument22);m.write16(obj+24,s.sourceLevel24);
 m.write32(obj+28,unsigned(s.selectedCue28));m.write8(obj+32,s.loaded32);m.write8(obj+33,s.pendingStart33);m.write8(obj+34,s.volumePending34);m.write32(obj+36,s.delay36);
 m.write32(0xc31eb34,unsigned(s.volumeCounter0C31EB34));if(s.handle0>=0)m.write8(0xc8ff1ec+unsigned(s.handle0),s.slotRegistered?1:0);
}
void compareState(RefMemory&m,const OriginalSelectionMusicState&s){
 for(auto [offset,value]:std::initializer_list<std::pair<unsigned,unsigned>>{{0,unsigned(s.handle0)},{8,s.command8},{28,unsigned(s.selectedCue28)},{36,s.delay36}})eq(m.read32(obj+offset),value,"manager word");
 for(auto [offset,value]:std::initializer_list<std::pair<unsigned,unsigned>>{{4,s.playing4},{20,s.loadBusy20},{32,s.loaded32},{33,s.pendingStart33},{34,s.volumePending34}})eq(m.read8(obj+offset),value,("manager byte "+std::to_string(offset)).c_str());
 eq(m.read16(obj+22),s.controlArgument22,"control argument");eq(m.read16(obj+24),s.sourceLevel24,"source level");eq(m.read32(0xc31eb34),unsigned(s.volumeCounter0C31EB34),"volume counter");
}
void run(RefCpu&c,unsigned pc){c.r[4]=obj;c.r[15]=stack;c.pr=stopPc;try{instructions+=c.run(pc,stopPc,10000);}catch(const std::exception&e){throw std::runtime_error(hex(pc)+" at "+hex(c.pc)+": "+e.what());}}
void commandHooks(RefCpu&c,std::vector<OriginalSelectionMusicCommand>&commands){
 c.callHooks[0xc1ed340]=[&](auto&cpu){commands.push_back({OriginalSelectionMusicOperation::Control,cpu.memory.read32(obj+28),cpu.r[5],cpu.r[6],std::int32_t(cpu.r[4])});};
 c.callHooks[0xc1ed9c0]=[&](auto&cpu){commands.push_back({OriginalSelectionMusicOperation::Start,cpu.memory.read32(obj+28),cpu.r[4],0,std::int32_t(cpu.memory.read32(obj))});};
}
void compareCommands(const std::vector<OriginalSelectionMusicCommand>&a,const OriginalSelectionMusicCommands&e){eq(unsigned(a.size()),e.count,"command count");for(unsigned i=0;i<a.size();++i){eq(unsigned(a[i].operation),unsigned(e.commands[i].operation),"operation");eq(a[i].cue,e.commands[i].cue,"cue");eq(a[i].word,e.commands[i].word,"command");eq(a[i].argument,e.commands[i].argument,"argument");eq(unsigned(a[i].handle),unsigned(e.commands[i].handle),"handle");}}
void descriptors(RefMemory&m){
 for(unsigned i=0;i<3;++i){const auto&d=originalSelectionMusicDescriptor(OriginalSelectionMusicCue(i));const unsigned a=0xc31df04+i*80;
  for(unsigned n=0;n<d.filename.size();++n)eq(m.read8(a+n),unsigned(d.filename[n]),"bank filename");eq(m.read8(a+unsigned(d.filename.size())),0,"filename terminator");
  eq(m.read32(a+64),d.command,"A8 command");eq(m.read32(a+68),d.slotMask,"slot mask");eq(m.read32(a+72),0,"synchronous loader");eq(m.read16(a+76),d.sourceLevel,"descriptor level");
 }
 for(const auto&binding:originalSelectionMusicBindings()){
  // Every exact call/delay slot establishes cue ID independently of symbols.
  eq(m.read16(binding.requestAddress)&0xf0ff,0x400b,"request jsr opcode");eq(m.read16(binding.requestAddress+2),0xe400|unsigned(binding.cue),"request cue delay slot");
  const auto literalLoad=m.read16(binding.requestAddress-2);eq(literalLoad>>12,13,"request target PC-relative load");
  eq(m.read32(((binding.requestAddress+2)&~3)+(literalLoad&255)*4),0xc141ec0,"request wrapper target");
  eq(unsigned(*originalSelectionMusicCueForInit(binding.initAddress)),unsigned(binding.cue),"Init binding");
 }
}
void manager(RefMemory&m){std::mt19937 rng(0x143060);
 for(unsigned trial=0;trial<2048;++trial){OriginalSelectionMusicState s;
  s.handle0=(trial%7==0)?-1:int(trial%8);s.slotRegistered=trial%3!=0;s.selectedCue28=int(trial%3);s.command8=originalSelectionMusicDescriptor(OriginalSelectionMusicCue(trial%3)).command;
  s.playing4=std::uint8_t(rng()%3);s.loadBusy20=std::uint8_t(rng()%3);s.loaded32=std::uint8_t(rng()%3);s.pendingStart33=std::uint8_t(rng()%3);s.volumePending34=std::uint8_t(rng()%3);
  s.delay36=trial%7==0?0x80000000u:trial%11==0?0x7fffffffu:unsigned(int(rng()%9)-3);
  s.sourceLevel24=std::uint16_t(rng());s.controlArgument22=std::uint16_t(rng());s.volumeCounter0C31EB34=int(rng()%11)-3;
  const bool busy=trial%2;seed(m,s);RefCpu c(m);std::vector<OriginalSelectionMusicCommand> commands;commandHooks(c,commands);c.callHooks[0xc1ecf40]=[&](auto&cpu){cpu.r[0]=busy?1:0;};
  auto expected=s;const auto events=tickOriginalSelectionMusic(expected,busy);run(c,0xc143060);compareState(m,expected);compareCommands(commands,events);++cases;
  for(bool exit:{false,true}){seed(m,s);RefCpu direct(m);commands.clear();commandHooks(direct,commands);expected=s;const auto e=exit?exitOriginalSelectionMusic(expected):stopOriginalSelectionMusic(expected);run(direct,exit?0xc1431e0:0xc143140);compareState(m,expected);compareCommands(commands,e);++cases;}
 }
}
void requests(RefMemory&m){
 for(unsigned trial=0;trial<256;++trial){OriginalSelectionMusicState s;s.handle0=int(trial%8);s.slotRegistered=trial%3!=0;s.selectedCue28=int(trial%4)-1;s.playing4=trial%2;s.loaded32=trial%5!=0;
  s.sourceLevel24=7;s.command8=0xdeadbeef;s.pendingStart33=trial%2;s.delay36=56;
  const auto cue=OriginalSelectionMusicCue((trial/3)%3);const OriginalSelectionMusicLoadResult load{int((trial+1)%8),false,true,true};
  auto expected=s;const auto events=requestOriginalSelectionMusic(expected,cue,load);seed(m,s);RefCpu c(m);std::vector<OriginalSelectionMusicCommand> commands;commandHooks(c,commands);
  // Full143140 and142F00 execute. Bank I/O and its successful decoded
  // resource result are explicit; request equality/queue decisions are not hooked.
  c.callHooks[0xc1ed200]=[&](auto&cpu){commands.push_back({OriginalSelectionMusicOperation::Unload,unsigned(s.selectedCue28),0,0,std::int32_t(cpu.r[4])});cpu.r[0]=0;};
  c.callHooks[0xc055d60]=[](auto&){};
  c.callHooks[0xc142b00]=[&](auto&cpu){++loaderHooks;const auto selected=OriginalSelectionMusicCue(cpu.r[5]);const auto&d=originalSelectionMusicDescriptor(selected);
    m.write32(obj+28,unsigned(selected));m.write32(obj+8,d.command);m.write8(obj+20,0);m.write16(obj+22,8);m.write16(obj+24,d.sourceLevel);
    m.write32(obj,unsigned(load.handle));m.write8(obj+4,0);m.write8(obj+32,1);m.write8(obj+33,0);m.write8(0xc8ff1ec+unsigned(load.handle),1);
    commands.push_back({OriginalSelectionMusicOperation::Load,unsigned(selected),d.command,d.sourceLevel,load.handle});
  };
  c.r[4]=unsigned(cue);c.r[15]=stack;c.pr=stopPc;instructions+=c.run(0xc141ec0,stopPc,10000);eq(c.r[0],events.cueChanged,"same-cue return");if(c.r[0])run(c,0xc142fc0);
  compareState(m,expected);compareCommands(commands,events);++cases;
 }
}
}
int main(int argc,char**argv){try{if(argc!=2)throw std::runtime_error("canonical image required");RefMemory memory(argv[1]);descriptors(memory);manager(memory);requests(memory);
 std::cout<<"Selection music: "<<cases<<" cases, "<<checks<<" comparisons, "<<instructions<<" original instructions; "<<loaderHooks<<" explicit decoded-bank boundaries\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

