// Development-only isolated original camera manager capture. Numerical math,
// constructors, derived updates and shot transitions execute original bytes.
// Hooks replace only exception context, allocation, free and debug callbacks.
// No devices, kernel, emulated game, host graphics or invented camera paths.
#include "sh4_scalar_reference.h"
#include "original_demo_data.h"
#include <iostream>
using namespace idas3::reference;using namespace idas3::original;
int main(int argc,char**argv){try{if(argc!=4)throw std::runtime_error("image project output-directory required");RefMemory m(argv[1]);const std::filesystem::path dest(argv[3]);std::filesystem::create_directories(dest);auto data=OriginalDemoData::load(argv[2]);constexpr unsigned context=0xd050000,stack=0xd0ff000,owner=0xd000000,view=0xd040000,stop=0xf000000,script=0xd030000;unsigned heap=0xd600000;
for(auto a:{context,owner,script,view,0xd010000u,0xd020000u,0xce00000u})m.zeroRegion(a,0x10000);m.zeroRegion(stack-0xf000,0x10000);m.zeroRegion(0xd0d0000,0x10000);m.zeroRegion(heap,0x200000);
m.write32(context+4,context+256);m.write32(context+260,context+512);m.write32(context+512,context+512);
m.write32(0xc98ad0c,0x00200000);m.write32(0xc98ad10,0xce00000);m.write32(0xc98ad14,0xce00000);
std::ifstream tf(std::filesystem::path(argv[2])/"data/original_physics/fsca_table.bin",std::ios::binary);tf.seekg(16);std::vector<unsigned>wave(32768);tf.read(reinterpret_cast<char*>(wave.data()),wave.size()*4);if(!tf)throw std::runtime_error("Missing FSCA halfwave");for(unsigned i=0;i<2;++i){m.write32(0xd010000+i*0x1000+0x95c,0xd020000+i*0x1000);m.write32(0xd010000+i*0x1000+0x964+64,0xc380b8c);}
RefCpu c(m);c.fscaHalfWave=wave;c.callHooks[0xc221fc0]=[](auto&c){c.r[0]=context;};c.callHooks[0xc021960]=[&](auto&c){c.r[0]=heap;if(c.r[5]>0x10000||heap+((c.r[5]+15)&~15u)>0xd800000)throw std::runtime_error("Camera heap bound");heap+=(c.r[5]+15)&~15u;};c.callHooks[0xc021ee0]=[&](auto&c){c.r[0]=heap;if(c.r[4]>0x10000||heap+((c.r[4]+15)&~15u)>0xd800000)throw std::runtime_error("Camera heap bound");heap+=(c.r[4]+15)&~15u;};
unsigned long long steps=0;
auto run=[&](unsigned pc,unsigned end=0xf000000){c.r[15]=stack;c.pr=stop;try{steps+=c.run(pc,end,200000);}catch(const std::exception&e){throw std::runtime_error(hex(pc)+" at"+hex(c.pc)+" "+e.what());}};
auto actors=[&](unsigned frame){for(unsigned a=0;a<2;++a){const auto&pose=data.actor(frame,a);for(unsigned i=0;i<42;++i)m.write32(0xd020000+a*0x1000+i*4,pose.words[i]);c.r[13]=0xd010000+a*0x1000;c.r[14]=0xd0d0000;c.r[0]=context;run(0xc03485c,0xc034924);}};
actors(0);
c.callHooks[0xc1fa9e0]=[](auto&){};c.callHooks[0xc0203c0]=[](auto&){};
c.r[4]=owner;run(0xc0ad160);
std::ifstream raw(std::filesystem::path(argv[2])/"data/original_assets/attract/demo/cameras.bin",std::ios::binary);std::vector<unsigned> cw(65536/4);raw.read(reinterpret_cast<char*>(cw.data()),65536);if(!raw)throw std::runtime_error("Missing camera script");for(unsigned i=0;i<cw.size();++i)m.write32(script+i*4,cw[i]);
c.r[4]=owner;c.r[5]=script;c.r[6]=0xd010000;c.r[7]=0xd011000;m.write32(stack,view);run(0xc0adce0);std::cout<<"manager init success, selected="<<hex(m.read32(owner+4))<<"\n";
m.zeroRegion(0xd070000,0x10000);
std::ofstream out(dest/"camera_world.bin",std::ios::binary);std::ofstream csv(dest/"camera_world.csv");if(!out||!csv)throw std::runtime_error("Camera output unavailable");csv<<"frame,shot,eye_x,eye_y,eye_z,fov_phase\n";OriginalDemoCursor cur;
for(unsigned frame=0;frame<data.frameCount();++frame){actors(frame);if(frame>0&&frame==data.shots()[cur.shot].frames[2]){c.r[4]=owner;run(0xc0a9da0);c.r[4]=owner;c.r[5]=0xd010000;c.r[6]=0xd011000;run(0xc0ac060);std::cout<<"shot"<<std::dec<<cur.shot<<" frame"<<frame<<"\n";}
c.r[4]=owner;c.r[2]=0xd070000;run(0xc0a9ee0);for(unsigned i=0;i<16;++i){const unsigned w=m.read32(0xd070000+i*4);if(!std::isfinite(std::bit_cast<float>(w)))throw std::runtime_error("Nonfinite camera frame"+std::to_string(frame));out.write(reinterpret_cast<const char*>(&w),4);}const auto phase=m.read32(view+152);if(phase==0)throw std::runtime_error("Zero FOV frame"+std::to_string(frame));out.write(reinterpret_cast<const char*>(&phase),4);csv<<frame<<","<<cur.shot<<","<<m.readFloat(0xd070000+48)<<","<<m.readFloat(0xd070000+52)<<","<<m.readFloat(0xd070000+56)<<","<<phase<<"\n";data.step(cur);}
std::cout<<"Captured"<<data.frameCount()<<" camera matrices;"<<steps<<" source instructions\n";

}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}


