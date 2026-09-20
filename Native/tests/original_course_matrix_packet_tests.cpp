// Bounded original course helper -> instance packet proof. No hardware runtime.
// Only resource lookup, queue submission, and the seven store-queue PREF
// instructions are boundaries. Every matrix read/write/sign operation executes.
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
#include <algorithm>
using namespace idas3::reference;

int main(int argc,char**argv)try {
 if(argc!=2)throw std::runtime_error("canonical image required");
 RefMemory m(argv[1]);std::uint64_t checks=0,instructions=0;unsigned cases=0;
 const auto check=[&](bool b,const char*why){++checks;if(!b)throw std::runtime_error(why);};
 constexpr unsigned owner=0xd000000,bank=0xd010000,chunk=0xd020000,stack=0xd030000,sq=0xe0010000,stop=0xff0000;
 // These are platform transfers only. RAM retains the complete packet. The
 // ordinary RAM prefetch at1DBFF6 is left intact. No matrix op is substituted.
 constexpr unsigned prefs[]={0xc1dbff2,0xc1dc024,0xc1dc040,0xc1dc064,0xc1dc082,0xc1d80f4,0xc1d810c};
 std::mt19937 rng(0x1dbfe0);std::uniform_real_distribution<float> random(-2.f,2.f);
 for(unsigned path=0;path<4;++path)for(unsigned sample=0;sample<256;++sample){
  m.clear();m.zeroRegion(owner,0x40000);m.zeroRegion(0xc980000,0x10000);m.zeroRegion(sq,0x1000);
  for(auto pc:prefs){check((m.read16(pc)&0xf0ff)==0x0083,"Canonical SQ PREF identity");m.write16(pc,0x0009);}
  m.write32(owner+332,bank);m.write32(owner+336,bank);m.write32(chunk+64,0x100);m.write32(chunk+68,0x180);
  m.write32(0xc980264,0x10010000);m.write32(0xc37f110,0);
  const float nearClip=.01f+float(sample)*.001f,farClip=10000.f+float(sample),projection=.25f+float(sample)*.002f;
  const float envU=random(rng),envV=random(rng);
  m.writeFloat(0xc980234+16,nearClip);m.writeFloat(0xc980234+20,farClip);m.writeFloat(0xc980234+24,projection);
  m.writeFloat(0xc980234+28,envU);m.writeFloat(0xc980234+32,envV);
  RefCpu c(m);std::array<unsigned,16> original{};
  // Affine XF matrices include identity, signed axes, rotation, reflection,
  // arbitrary nonuniform scale/shear and translations. Source just consumes XF.
  for(unsigned i=0;i<16;++i){float f=(sample<4?(i%5==0?1.f:0.f):random(rng));if(i%4==3)f=i==15?1.f:0.f;original[i]=std::bit_cast<unsigned>(f);}
  if(sample==1){original[0]^=0x80000000;original[10]^=0x80000000;}
  if(sample==2){original[5]^=0x80000000;}
  if(sample==3){original[0]=0;original[2]=std::bit_cast<unsigned>(-1.f);original[8]=std::bit_cast<unsigned>(1.f);original[10]=0;}
  c.xf=original;for(unsigned i=0;i<16;++i)c.fr[i]=0x3e800000+i;
  c.r[4]=path==3?chunk:owner;c.r[5]=sample;c.r[15]=stack+0xff00;c.pr=stop;
  unsigned lookup=0,submit=0;
  c.callHooks[0xc05a8e0]=[&](auto&q){check(q.r[4]==bank&&q.r[5]==sample,"Course chunk lookup arguments");q.r[0]=chunk;++lookup;};
  c.callHooks[0xc205640]=[&](auto&q){check(q.r[4]==(path==2?2:0),"Source queue list");check(m.read32(q.r[5])==0x10010000&&m.read32(q.r[5]+4)==224,"Source queued packet extent");++submit;};
  const unsigned entries[]={0xc19cec0,0xc19cf00,0xc19cf40,0xc1d7120};
  try{instructions+=c.run(entries[path],stop,2000);}catch(const std::exception&e){throw std::runtime_error("path "+std::to_string(path)+" sample "+std::to_string(sample)+" pc "+hex(c.pc)+": "+e.what());}
  check(lookup==(path==3?0:1)&&submit==1,"One original instance submission");
  check(m.read32(sq)==0x08000400&&m.read32(sq+4)==15&&m.read32(sq+8)==127,"Instance header");
  check(m.read32(sq+32)==0x08000200&&m.read32(sq+96)==0x08000100,"Normal/model-view headers");
  check(m.read32(sq+36)==std::bit_cast<unsigned>(envU)&&m.read32(sq+76)==std::bit_cast<unsigned>(envV),"Instance env offsets copy projection block28/32");
  check(m.read32(sq+100)==std::bit_cast<unsigned>(nearClip)&&m.read32(sq+152)==std::bit_cast<unsigned>(farClip)&&m.read32(sq+156)==(std::bit_cast<unsigned>(projection)^0x80000000),"Instance projection fields");
  // Independent primary elan_struct.h / ElanState::updateMatrix layout.
  // lm is stored transposed relative to tm's contiguous column vectors.
  for(unsigned column=0;column<3;++column)for(unsigned row=0;row<3;++row){
   const auto normal=m.read32(sq+40+12*row+4*column);
   const auto model=m.read32(sq+104+12*column+4*row)^(row==1?0:0x80000000);
   check(normal==original[4*column+row],"Source lm equals incoming XF linear matrix");
   check(model==original[4*column+row],"Primary tm X/Z decoding equals same XF linear matrix");
   check(model==normal,"Normals and positions share the same linear transform");
  }
  for(unsigned row=0;row<3;++row)check((m.read32(sq+140+row*4)^(row==1?0:0x80000000))==original[12+row],"Primary translation decoding");
  for(unsigned i=0;i<16;++i)check(c.xf[i]==original[i],"Packet writer restores complete XF");
  check(!c.fpscrSz,"Packet writer restores scalar register mode");
  check(m.read32(0xc980264)==0x100100e0,"Packet allocation advances224 bytes");++cases;
 }
 std::cout<<"PASS "<<cases<<" source course/background/reflection/direct packet cases, "<<checks<<" checks / "<<instructions<<" instructions including7 explicit SQ PREF no-ops per case. Original1D7120/1D73A0/1D8060/1DBFE0 execute; lm and decoded tm linear parts equal incoming XF bitwise, all16 XF values restored, env offsets preserved. Resource lookup and platform queue hooks only; no device/renderer/user data.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
