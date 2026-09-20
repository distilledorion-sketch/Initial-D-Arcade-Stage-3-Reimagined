#include "original_time_attack_visit.h"
#include "original_gasstand_data.h"
#include "sh4_scalar_reference.h"
#include <algorithm>
#include <iostream>
using namespace idas3;using namespace idas3::original;using namespace idas3::reference;
namespace {
constexpr unsigned obj=0x0d000000,stack=0x0d080000,stop=0x00ff0000,fadeObj=obj+0x1000;
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
}
int main(int argc,char** argv){try{
 require(argc==2,"canonical image argument");RefMemory memory(argv[1]);std::uint64_t comparisons=0,instructions=0;
 // Actual07F340/04BE00/04BF00 scalar owner and fade execution. Drawing,
 // matrix/platform submission and the parent's exit callback are boundaries.
 for(unsigned scenario=0;scenario<4;++scenario){
  memory.clear();memory.zeroRegion(obj,0x100000);RefCpu cpu(memory);bool sourceFinished=false;
  memory.write32(obj+12,obj+0x500);memory.write32(obj+0x500+44,0x00ff0010);
  memory.write32(obj+324,obj+0x2000);memory.write32(obj+328,obj+0x3000);memory.write32(obj+344,fadeObj);
  memory.write32(obj+340,90);memory.write32(fadeObj+16,1);memory.write32(0x0c31c99c+28,6);
  cpu.callHooks[0x00ff0010]=[&](auto&){sourceFinished=true;};
  for(unsigned address:{0x0c1bda20u,0x0c1fcc60u,0x0c1f6ac0u,0x0c1f67e0u,0x0c1fbd60u,0x0c1f65c0u,0x0c1f6610u,0x0c078720u,0x0c078680u,0x0c0c5200u})cpu.callHooks[address]=[](auto&){};
  cpu.callHooks[0x0c2223b8]=[](auto& c){c.fpul=unsigned(std::int32_t(c.r[4])/std::int32_t(c.r[5]));};
  OriginalTimeAttackVisit visit;OriginalTimeAttackVisit::Setup setup;setup.condition=6;setup.courseRankingQualified=true;setup.ticks6000=500000;setup.localRecords.push_back({6,0,0,500000});visit.beginAfterResults(setup);
  for(unsigned tick=1;tick<1000;++tick){
   const bool press=scenario==1||(scenario==2&&tick==100)||(scenario==3&&tick==15);
   memory.write8(0x0c92ed00,press?0x80:0);cpu.r[4]=obj;cpu.r[15]=stack;cpu.pr=stop;
   instructions+=cpu.run(0x0c07f340,stop,10000);visit.advance({press});
   require(sourceFinished==(visit.stage()==OriginalTimeAttackVisit::Stage::Continue),"Native/source ranking exit tick mismatch");++comparisons;
   if(sourceFinished)break;
   require(memory.read32(fadeObj+16)==visit.phase()+1,"Native/source ranking fade phase mismatch");
   require(memory.read32(fadeObj+32)==(visit.fadeArgb()>>24),"Native/source ranking fade alpha mismatch");comparisons+=2;
  }
  require(sourceFinished,"Bounded ranking scenario must finish");
 }
 // Execute18E0A0 and1CB200 through its descriptor submission to verify the
 // extra mode1 Y offset, exact glyph identity and variable-width advances.
 for(unsigned kind=0;kind<3;++kind)for(unsigned row=0;row<(kind==0?8:kind==1?3:6);++row){
  const auto& text=originalTimeAttackAdviceText(kind,row);memory.clear();memory.zeroRegion(obj,0x100000);RefCpu cpu(memory);
  constexpr unsigned font=obj+0x1000;memory.write32(obj+104,font);
  memory.writeFloat(font+32,1);memory.writeFloat(font+36,1);memory.writeFloat(font+40,5);
  for(unsigned line=0;line<3;++line)memory.write32(obj+108+line*4,text.addresses[line+1]);
  std::vector<std::array<float,3>> expected;
  for(unsigned line=0;line<3;++line){float x=172;for(char value:text.text[line+1]){
    const auto found=std::find(gasstand_data::fontCharacters.begin(),gasstand_data::fontCharacters.end(),value);
    if(found==gasstand_data::fontCharacters.end()){x+=5;continue;}
    const auto index=unsigned(found-gasstand_data::fontCharacters.begin());expected.push_back({float(index),x,94.f+23.f*line});
    x=std::fma(gasstand_data::word(gasstand_data::fontWidthWords[index]),1.f,x);
  }}
  std::size_t next=0;cpu.callHooks[0x0c226ae0]=[&](auto& c){unsigned count=0;while(memory.read8(c.r[4]+count))++count;c.r[0]=count;};
  cpu.callHooks[0x0c1e7e80]=[&](auto& c){require(next<expected.size(),"Extra source coaching glyph");const auto& e=expected[next++];
    require(memory.read32(c.r[4])==unsigned(e[0]),"Source coaching glyph identity");
    require(memory.readFloat(c.r[4]+4)==e[1]&&memory.readFloat(c.r[4]+8)==e[2],"Source coaching glyph position");comparisons+=3;
  };
  cpu.r[4]=obj;cpu.r[15]=stack;cpu.pr=stop;instructions+=cpu.run(0x0c18e0a0,stop,200000);
  require(next==expected.size(),"Missing source coaching glyph");
 }
 //110A60's finite coordinate preparation, fed its zero-initialized model
 // anchor, and actual111000/1926A0/156720 countdown formatting.
 memory.clear();memory.zeroRegion(obj,0x100000);RefCpu ctor(memory);
 ctor.r[14]=obj;memory.write32(obj+204,obj+60);memory.write32(obj+228,obj+24);
 instructions+=ctor.run(0x0c110b10,0x0c110b7c,1000);
 const float unitsX=memory.readFloat(obj+24)*100.f,tensX=memory.readFloat(obj+36)*100.f;
 const float singleX=memory.readFloat(obj+48)*100.f,y=-memory.readFloat(obj+28)*100.f;
 for(unsigned ticks=0;ticks<=1679;++ticks){
  memory.clear();memory.zeroRegion(obj,0x100000);RefCpu cpu(memory);
  memory.write32(obj+4,obj+0x1000);memory.write32(obj+8,obj+0x2000);
  unsigned child=0,bcd=0;
  cpu.callHooks[0x0c2223b8]=[](auto& c){c.fpul=c.r[4]/c.r[5];};
  cpu.callHooks[0x0c0dbb20]=[&](auto& c){child=c.r[4];bcd=c.r[5];};
  cpu.r[4]=obj;cpu.r[5]=ticks;cpu.r[15]=stack;cpu.pr=stop;
  instructions+=cpu.run(0x0c111000,stop,1000);
  const auto draws=originalTimeAttackCountdownDraws(ticks);const bool two=child==obj+0x2000;
  require(draws.size()==(two?2u:1u),"Source countdown digit count");
  require(draws[0].chunk==(bcd&15)+1&&draws[0].x==(two?unitsX:singleX)&&draws[0].y==y,"Source countdown units/anchor");
  comparisons+=4;
  if(two){require(draws[1].chunk==((bcd>>4)&15)+1&&draws[1].x==tensX&&draws[1].y==y,"Source countdown tens/anchor");comparisons+=3;}
 }
 std::cout<<"PASS source presentation:4 ranking timelines,17 advice rows,1680 countdown cases,"<<comparisons<<" comparisons,"<<instructions<<" original instructions\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
