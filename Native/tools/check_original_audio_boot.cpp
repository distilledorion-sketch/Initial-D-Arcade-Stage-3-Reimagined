#include "../tests/sh4_scalar_reference.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
using namespace idas3::reference;
namespace {
constexpr unsigned stack=0x0cfe0000, stop=0x0cfdffff, bank=0x0c500000;
unsigned checks=0; std::uint64_t instructions=0;
void eq(unsigned a,unsigned b){++checks;if(a!=b)throw std::runtime_error(hex(a)+" != "+hex(b));}
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
void run(RefCpu&cpu,unsigned start,unsigned end){instructions+=cpu.run(start,end,100000);}
std::string stringAt(const RefMemory&m,unsigned a){std::string s;for(unsigned i=0;i<256;++i){auto c=m.read8(a+i);if(!c)return s;s+=char(c);}throw std::runtime_error("Unterminated source string");}
void start(RefCpu&cpu){cpu.r[15]=stack;cpu.pr=stop;}
}
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("Usage: check_original_audio_boot source-image HOSTFS-directory");
 RefMemory m(argv[1]);m.zeroRegion(stack-16384,16384);m.zeroRegion(0xc8ff1cc,512);m.zeroRegion(0xc31de08,96);
 {
  RefCpu startup(m);std::vector<unsigned> calls;
  startup.callHooks[0xc1412e0]=[&](auto&){calls.push_back(0xc1412e0);};
  startup.callHooks[0xc1416a0]=[&](auto&c){calls.push_back(0xc1416a0);eq(c.r[4],0);};
  run(startup,0xc056a60,0xc056a6c);eq(calls.size(),2);eq(calls[0],0xc1412e0);eq(calls[1],0xc1416a0);
 }
 std::ifstream input(std::filesystem::path(argv[2])/"sound/pack/PACK20.bin.nz",std::ios::binary);
 require(bool(input),"Missing canonical PACK20 bank");std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)),{});
 require(bytes.size()>0x40,"Short PACK20 bank");require(std::string(bytes.begin(),bytes.begin()+4)=="DTPK","PACK20 is not raw DTPK");
 eq(bytes[0x12],46);eq(bytes[0x13],2);
 // Only the actual header is needed at the explicit bank-transfer boundary.
 for(unsigned i=0;i<0x40;++i)m.write8(bank+i,bytes[i]);
 RefCpu cpu(m);start(cpu);unsigned resets=0,uploads=0,unloads=0,streamSetup=0;std::vector<unsigned> commands;
 for(unsigned address:{0xc1cf5e0u,0xc021ec0u,0xc055d60u,0xc2029a0u,0xc1eddc0u,0xc1ede00u,0xc1db8c0u,0xc021aa0u})cpu.callHooks[address]=[](auto&c){c.r[0]=0;};
 cpu.callHooks[0xc04e760]=[&](auto&c){require(stringAt(m,c.r[4]).find("AICADRV")!=std::string::npos,"First driver resource is not AICADRV");c.r[0]=0xc600000;};
 cpu.callHooks[0xc1ec4c0]=[&](auto&c){++resets;eq(c.r[4],0xc600000);eq(c.r[5],0x200);c.r[0]=0;};
 cpu.callHooks[0xc200b40]=[](auto&c){c.r[0]=1;};
 cpu.callHooks[0xc226a00]=[&](auto&c){auto src=stringAt(m,c.r[5]);auto dst=stringAt(m,c.r[4]);for(unsigned i=0;i<=src.size();++i)m.write8(c.r[4]+unsigned(dst.size())+i,i<src.size()?src[i]:0);c.r[0]=c.r[4];};
 cpu.callHooks[0xc04c860]=[](auto&c){c.r[0]=1;};
 cpu.callHooks[0xc04e480]=[&](auto&c){auto path=stringAt(m,c.r[4]);if(path!="/driveA/sound/pack/PACK20.bin")throw std::runtime_error("Boot bank path: "+path);++checks;c.r[0]=bank;};
 cpu.callHooks[0xc1ec7a0]=[&](auto&c){++uploads;eq(c.r[4],bank);eq(c.r[5],0);eq(m.read8(c.r[4]+0x13),2);c.r[0]=0;};
 cpu.callHooks[0xc1ed200]=[&](auto&c){++unloads;eq(c.r[4],0);c.r[0]=0;};
 cpu.callHooks[0xc1cf880]=[&](auto&c){++streamSetup;eq(c.r[4],4);eq(c.r[5],2);eq(c.r[6],0);c.r[0]=0;};
 // Execute the original SH4 command packer; hook only the command FIFO write.
 cpu.callHooks[0xc1ed9c0]=[&](auto&c){commands.push_back(c.r[4]);c.r[0]=0;};
 run(cpu,0xc1412e0,0xc14133c);
 // This is the literal CPU busy wait; skip it, preserving the surrounding code.
 run(cpu,0xc14134c,stop);
 eq(resets,1);eq(uploads,2);eq(unloads,1);eq(streamSetup,1);eq(commands.size(),2);
 // 10A0 is alternate bank volume127, NOT DSP scene selection.
 for(auto command:commands)eq(command,0x007f10a0);
 eq(m.read32(0xc31de58),0xffffffff);eq(m.read8(0xc31de3c),1);
 for(unsigned mode:{0u,0x200u}){RefCpu selector(m);selector.r[12]=mode;run(selector,0xc1ec4f0,0xc1ec50e);eq(m.read32(0xcb0f314),mode?0x800000:0x200000);}
 const std::vector<std::vector<unsigned>> sceneBanks{{},{1},{1},{},{2,3,3,4,5},{},{1,2,3,4}};
 const unsigned musicOwners[]{0,1,1,0,1,0,3},streamOwners[]{1,1,0,0,1,1,1};
 for(unsigned scene=0;scene<sceneBanks.size();++scene){
  auto row=0xc31de94+16*scene;eq(m.read32(row+4),unsigned(sceneBanks[scene].size()));eq(m.read32(row+8),musicOwners[scene]);eq(m.read32(row+12),streamOwners[scene]);
  for(unsigned i=0;i<sceneBanks[scene].size();++i){auto index=m.read32(m.read32(row)+4*i);eq(index,sceneBanks[scene][i]);require(stringAt(m,m.read32(0xc31ed30+12*index))=="PACK"+std::to_string(20+index)+".bin","Scene bank name mismatch");}
  m.write32(0xc31de58,scene);RefCpu same(m);start(same);same.r[4]=scene;same.callHooks[0xc221fc0]=[](auto&c){c.r[0]=0;};
  // Stops at the original same-scene epilogue, before any resource mutation.
  run(same,0xc1416a0,0xc141c80);eq(m.read8(0xc31de3c),1);
 }
 // Follow the shared eight-slot allocator with stock resources present.
 // File bytes/transfer remain external; resource choice and flag mutation are
 // original instructions. PACK20 still occupies slot0 after boot.
 constexpr unsigned owners=0xcd00000,musicOwner=owners+1024;
 m.zeroRegion(owners,4096);
 auto staticBank=[&](unsigned index,unsigned slot){
  RefCpu c(m);start(c);c.r[4]=owners+slot*64;c.r[5]=index;c.callHooks=cpu.callHooks;
  c.callHooks[0xc04e480]=[&](auto& r){require(stringAt(m,r.r[4])=="/driveA/sound/pack/PACK"+std::to_string(index+20)+".bin","Static resource order differs");r.r[0]=bank;};
  c.callHooks[0xc1ec7a0]=[&](auto& r){eq(r.r[5],slot);r.r[0]=0;};
  c.callHooks[0xc1ed9c0]=[](auto& r){r.r[0]=0;};
  run(c,0xc143440,stop);eq(m.read32(owners+slot*64),slot);eq(m.read8(0xc8ff1ec+slot),1);
 };
 staticBank(1,1);
 for(unsigned cue=0;cue<2;++cue){
  RefCpu allocation(m);start(allocation);allocation.r[12]=musicOwner;
  run(allocation,0xc142b96,0xc142bb8);eq(allocation.r[8],2);
  // The existing-file success boundary precedes these original slot stores.
  run(allocation,0xc142be0,0xc142bf2);eq(m.read32(musicOwner),2);eq(m.read8(0xc8ff1ee),1);
  RefCpu unload(m);start(unload);unload.r[4]=musicOwner;unload.callHooks=cpu.callHooks;
  unload.callHooks[0xc1ed200]=[&](auto& r){eq(r.r[4],2);r.r[0]=0;};
  run(unload,0xc142f00,stop);eq(m.read8(0xc8ff1ee),0);eq(m.read32(musicOwner),0xffffffff);
 }
 {
  RefCpu unload(m);start(unload);unload.r[4]=owners+64;unload.callHooks=cpu.callHooks;
  unload.callHooks[0xc1ed200]=[&](auto& r){eq(r.r[4],1);r.r[0]=0;};
  run(unload,0xc143340,stop);eq(m.read8(0xc8ff1ed),0);
 }
 for(unsigned slot=1;slot<=5;++slot)staticBank(sceneBanks[4][slot-1],slot);
 {
  RefCpu allocation(m);start(allocation);allocation.r[4]=0;allocation.r[5]=0;
  allocation.callHooks[0xc0c3e20]=[&](auto& r){eq(r.r[4],0);eq(r.r[5],2);eq(r.r[6],6);eq(m.read32(r.r[15]),7);};
  run(allocation,0xc142580,0xc1425fc);eq(m.read32(0xc31de48),6);eq(m.read32(0xc31de4c),7);
  for(unsigned slot=0;slot<8;++slot)eq(m.read8(0xc8ff1ec+slot),1);
 }
 // Exact engine parameter+4 packing: the preset/bank selector is global;
 // per-handle send commands retain the selected channel nibble.
 std::vector<unsigned> effects;
 for(unsigned family=0;family<36;++family){auto descriptor=m.read32(0xc2fb3d4+4*family);auto parameters=m.read32(descriptor+4);effects.push_back(m.read32(parameters+4));}
 unsigned nonzero=0;
 for(auto effect:effects)for(unsigned handle:{0u,1u,3u}){
  if(!effect)continue;++nonzero;std::vector<unsigned> emitted;RefCpu packer(m);start(packer);packer.r[4]=handle;packer.r[5]=0xa4;packer.r[6]=effect;
  packer.callHooks[0xc1ed9c0]=[&](auto&c){emitted.push_back(c.r[4]);c.r[0]=0;};run(packer,0xc1ed840,stop);
  eq(emitted.size(),2);eq(emitted[0],((effect&0x7f00)<<8)+0x70a4);eq(emitted[1],((effect&0x7f)<<16)|0xa4);
 }
 require(nonzero>0,"No source engine effect selectors were tested");
 // Run the real first-parameter reload and its branch for both groups and
 // auxiliary. This intentionally does not substitute each group's +4 word.
 unsigned initialized=0;
 for(unsigned family=0;family<36;++family)for(unsigned handle:{0u,1u,3u}){
  m.write32(0xca9b530,family);m.write32(0xca9b4ec+52,handle);
  std::vector<unsigned> emitted;RefCpu initializer(m);start(initializer);
  initializer.r[9]=handle;initializer.r[8]=0xc1ed6a0;initializer.r[13]=0xc1ed840;
  initializer.callHooks[0xc1db8c0]=[](auto&c){c.r[0]=0;};
  initializer.callHooks[0xc1ed9c0]=[&](auto&c){emitted.push_back(c.r[4]);c.r[0]=0;};
  run(initializer,handle==3?0xc0c40d2:0xc0c3f82,handle==3?0xc0c4114:0xc0c3fb8);
  auto effect=effects[family];eq(emitted.size(),effect?5:0);if(!effect)continue;
  eq(emitted[0],((effect&0x7f00)<<8)+0x70a4);eq(emitted[1],((effect&0x7f)<<16)|0xa4);
  eq(emitted[2],((handle==3?5:10)<<16)+(handle<<8)+0x10a4);
  eq(emitted[3],(64<<16)+(handle<<8)+0x20a4);
  eq(emitted[4],(64<<16)+(handle<<8)+0x40a5);++initialized;
 }
 std::cout<<"PASS source audio boot: "<<checks<<" comparisons, "<<instructions<<" SH4 instructions; PACK20 first and reload, one driver reset, explicit8MB, ring header2; "<<nonzero<<" engine effect packings and "<<initialized<<" original engine/auxiliary setup slices. Platform file/driver/transfer/FIFO hooks; one boot busy-wait omitted.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
