// Source course-index producer, independent of native path sampling.
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("canonical image required");
 RefMemory m(argv[1]);std::size_t checks=0,instructions=0,cases=0;
 const auto check=[&](bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);};
 constexpr unsigned course=0xd000000,race=0xd010000,query=0xd020000,stack=0xd030000,limits=0xd040000;
 for(unsigned reverse=0;reverse<2;++reverse)for(unsigned slot=0;slot<2;++slot)
 for(unsigned index:{0u,1u,29u,30u,69u,70u,126u,127u})for(unsigned kind=0;kind<3;++kind){
  m.clear();m.zeroRegion(course,0x50000);
  m.write32(course+64,0xdeadbeef);m.write32(course+68,0xcafebabe);
  RefCpu init(m);init.r[0]=course;
  instructions+=init.run(0xc03a0d2,0xc03a0fa,50);
  check(m.read32(course+64)==0&&m.read32(course+68)==0,"Base constructor clears course coordinate/progress");
  m.write32(course,kind==0?0xc38de4c:0xc38213c);m.write32(course+48,reverse);m.write32(course+24,query);m.write32(course+36,limits);
  m.write32(query,query+128);m.write32(query+128+36,0xf000040);
  for(unsigned i=0;i<4;++i)m.write32(limits+4*i,std::array<unsigned,4>{30,70,128,~0u}[i]);
  m.write32(race+1036,course);m.write32(race+1408,slot);m.write32(race+1412+slot*12,index);
  m.write32(race+1412+(1-slot)*12,index+500);m.write32(race+1460,1000+index);
  RefCpu c(m);c.r[15]=stack+0xff00;c.r[12]=race;c.r[8]=race+1020;
  c.callHooks[0xf000040]=[](auto&q){q.r[0]=129;};
  c.callHooks[0xc2223b8]=[](auto&q){q.fpul=unsigned(signed32(q.r[4])/signed32(q.r[5]));};
  unsigned crowCalls=0;
  c.callHooks[0xc0424c0]=[&](auto&){++crowCalls;};
  if(kind<2)instructions+=c.run(0xc05fe48,0xc05fe6a,2000);
  else{
   // Iro update calls the same base first; stop before unrelated prop setup.
   c.r[4]=course;c.r[5]=index;c.r[6]=1000+index;
   c.callHooks[0xc221fc0]=[](auto&q){q.r[0]=0xd048000;};
   instructions+=c.run(0xc1a59e0,0xc1a5a06,2000);
  }
  check(m.read32(course+64)==index,"Course keeps oriented history index without a second reversal");
  check(m.read32(course+68)==1000+index,"Course keeps accumulated progress separately");
  const unsigned raw=reverse?128-index:index;
  const unsigned sector=raw<30?0:raw<70?1:raw<128?2:3;
  check(m.read32(course+72)==sector,"Sector selection reverses only its temporary raw coordinate");
  check(m.read32(race+1408)==slot,"Course update does not toggle history");
  check(m.read32(race+1412+slot*12)==index&&m.read32(race+1412+(1-slot)*12)==index+500,"Both history slots unchanged");
  check(crowCalls==(kind==0?1:0),"Myogi crow update boundary only");++cases;
 }
 std::cout<<"PASS "<<cases<<" course path bindings / "<<checks<<" checks / "<<instructions<<" original instructions. Base constructor zero; actual05FE48 parent slot fetch and Myogi/Happo setters; Iro prefix through base; both directions preserve oriented index. Only count/quotient/crow/TLS dependencies are explicit. No renderer/whole-runtime/userdata.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
