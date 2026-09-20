// Isolated ACar lightset/geometry matrix and parent ambient proof.
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
#include <algorithm>
using namespace idas3::reference;
using Matrix=std::array<float,16>;
static Matrix multiply(const Matrix&a,const Matrix&b){Matrix r{};for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row){double x=double(a[row])*b[col*4];x+=double(a[4+row])*b[col*4+1];x+=double(a[8+row])*b[col*4+2];x+=double(a[12+row])*b[col*4+3];r[col*4+row]=float(x);}return r;}
static unsigned bits(float x){return std::bit_cast<unsigned>(x);}
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("canonical image required");RefMemory m(argv[1]);
 constexpr unsigned car=0xd000000,array=car+0x4000,tls=car+0x10000,node=tls+0x100,frame=tls+0x1000,stack=car+0x5ff00,matStack=0xce00000,stop=0xf000000;
 std::uint64_t checks=0,instructions=0;unsigned drawCases=0,spotCases=0,ambientCases=0;
 auto check=[&](bool yes,const char*why){++checks;if(!yes)throw std::runtime_error(why);};
 std::mt19937 rng(0x34c20);std::uniform_real_distribution<float> number(-3.f,3.f);
 for(unsigned sample=0;sample<256;++sample){
  m.clear();m.zeroRegion(car,0x60000);m.zeroRegion(matStack,4096);m.zeroRegion(0xc98ad00,64);
  m.write32(0xc98ad0c,0x00200000);m.write32(0xc98ad10,matStack);m.write32(0xc98ad14,matStack);
  m.write32(tls+4,node);m.write32(node+4,node);m.write32(node,node);m.write32(car+2540,array);m.write32(car+2468,0xc380b8c);
  Matrix view{},world{};for(unsigned i=0;i<16;++i){view[i]=number(rng);world[i]=number(rng);if(i%4==3)view[i]=world[i]=i==15?1.f:0.f;}
  if(sample<2){view={1,0,0,0,0,1,0,0,0,0,1,0,10,20,30,1};if(sample==1)view[0]=-1;}
  for(unsigned i=0;i<16;++i)m.writeFloat(car+2404+4*i,world[i]);
  const std::array<float,3> normal{number(rng),number(rng),number(rng)};const float height=.1f+float(sample%35)*.01f;
  for(unsigned i=0;i<3;++i)m.writeFloat(car+2472+i*4,float(i+sample));
  RefCpu cpu(m);for(unsigned i=0;i<16;++i)cpu.xf[i]=bits(view[i]);cpu.r[4]=car;cpu.r[15]=stack;cpu.pr=stop;
  unsigned order=0;cpu.callHooks[0xc221fc0]=[&](auto&q){q.r[0]=tls;};
  cpu.callHooks[0xc0538a0]=[&](auto&q){check(order++==0&&q.r[4]==array,"ACar submits its own ARRAY first");for(unsigned i=0;i<16;++i)check(q.xf[i]==bits(view[i]),"lightset uses enclosing view only");};
  cpu.callHooks[0xc022ce0]=[&](auto&q){check(order++==1&&q.r[4]==car+2720,"body query follows lightset");for(unsigned i=0;i<3;++i){check(m.read32(q.r[4]+32+4*i)==bits(float(i+sample)),"body query uses published actor XYZ");m.writeFloat(q.r[4]+4*i,normal[i]);}q.r[0]=1;};
  cpu.callHooks[0xc029ec0]=[&](auto&q){check(q.r[4]==car,"ride coefficient owner");q.fr[0]=bits(height);};
  Matrix translation{1,0,0,0,0,1,0,0,0,0,1,0,normal[0]*height,normal[1]*height,normal[2]*height,1};
  const Matrix expected=multiply(multiply(view,translation),world);
  cpu.callHooks[0xc026d80]=[&](auto&q){check(order++==2&&q.r[4]==car,"assembly draw follows road offset and car matrix");for(unsigned i=0;i<16;++i)check(q.xf[i]==bits(expected[i]),"geometry uses view*bodyNormalTranslation*carWorld");};
  instructions+=cpu.run(0xc034c20,stop,10000);check(order==3,"one lightset/query/geometry sequence");
  for(unsigned i=0;i<16;++i)check(cpu.xf[i]==bits(view[i]),"draw restores incoming matrix");++drawCases;
  // The embedded spot position/direction are written at every ACar update,
  // independently of outer lamp or brake state and before projector publish.
  for(unsigned flags=0;flags<4;++flags){m.write8(car+80,flags&1);m.write8(car+81,flags>>1);m.write8(car+2568,71);
   cpu.r[13]=car;cpu.r[14]=frame;cpu.r[15]=stack;
   instructions+=cpu.run(0xc034b26,0xc034b8e,10000);
   Matrix offset{1,0,0,0,0,1,0,0,0,0,1,0,0,-.5f,-3.f,1};const auto expectedSpot=multiply(world,offset);
   for(unsigned i=0;i<3;++i){check(m.read32(car+2544+44+4*i)==bits(expectedSpot[12+i]),"spot world position current actor matrix offset");check(m.read32(car+2544+68+4*i)==bits(world[8+i]),"spot direction current actor Z column");}
   check(m.read8(car+2568)==71,"spot transform does not change enabled flag");for(unsigned i=0;i<16;++i)check(cpu.xf[i]==bits(view[i]),"spot update restores unrelated matrix");++spotCases;
  }
 }
 constexpr unsigned race=car+0x20000,course=race+0x1000,rival=race+0x2000,rivalArray=race+0x6000,hud=race+0x8000,actor=race+0x9000;
 m.clear();m.zeroRegion(car,0x60000);m.write32(race+1036,course);m.write32(race+1048,car);m.write32(race+1052,rival);m.write32(race+1404,hud);
 for(unsigned owner:{car,rival}){m.write32(owner+2388,0xc3811a4);m.write32(owner+2392,0xc381184);m.write32(owner+2540,owner==car?array:rivalArray);m.write32(owner+2396,actor);}
 const std::array<float,8> gaps{-100.f,-5.00001f,-5.f,-4.99999f,-2.5f,0.f,1.f,100.f};
 for(unsigned courseId=0;courseId<9;++courseId)for(unsigned wet=0;wet<2;++wet)for(unsigned night=0;night<2;++night)for(unsigned light=0;light<2;++light)for(unsigned direction=0;direction<2;++direction)for(float gap:gaps){
  m.write32(0xc31c99c,1);m.write32(0xc31c99c+4,courseId);m.write32(0xc31c99c+8,direction);m.write32(0xc31c99c+32,wet);m.write32(race+1664,night);m.write32(race+1640,0);m.writeFloat(hud+100,gap);m.write8(rival+81,light);
  const std::array<float,3> ambient{number(rng),number(rng),number(rng)};for(unsigned i=0;i<3;++i)m.writeFloat(course+12+4*i,ambient[i]);
  for(unsigned owner:{car,rival}){
   const unsigned target=owner==car?array:rivalArray;for(unsigned i=0;i<3;++i)m.write32(target+72+4*i,0x7f800001);
   RefCpu cpu(m);cpu.r[4]=race;cpu.r[15]=stack;cpu.pr=stop;unsigned publishes=0;
   cpu.callHooks[0xc034840]=[&](auto&q){check(q.r[4]==owner,"parent updates correct car before ambient");for(unsigned i=0;i<3;++i)check(m.read32(target+72+4*i)==0x7f800001,"ambient setter follows publication");++publishes;};
   cpu.callHooks[0xc035220]=[](auto&){};cpu.callHooks[0xc035120]=[](auto&){};cpu.callHooks[0xc035200]=[](auto&){};
   instructions+=cpu.run(owner==car?0xc063280:0xc0638c0,stop,10000);check(publishes==1,"exact parent car publication count");
   float factor=1.f;if(owner==rival&&night&&!light&&gap>-5.f)factor=std::clamp(-.2f*gap,0.f,1.f);
   for(unsigned i=0;i<3;++i){float expected=courseId==6&&wet?.6f-ambient[i]:ambient[i];if(owner==rival&&night)expected*=factor;check(m.read32(target+72+4*i)==bits(expected),"exact per-car ambient inversion/night lamp-off gap scale");}++ambientCases;
  }
 }
 std::cout<<"PASS "<<drawCases<<" complete034C20 matrix/order cases, "<<spotCases<<" actual034B26 spot updates, "<<ambientCases<<" full parent/0354E0 ambient cases; "<<checks<<" checks / "<<instructions<<" original instructions. TLS, lightset packet, road/ride inputs, geometry, pose and unrelated lamp-request leaves are explicit boundaries. Matrix operations, spot writes, exact parent ambient and adapter code execute. No hardware/runtime/device/user data.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
