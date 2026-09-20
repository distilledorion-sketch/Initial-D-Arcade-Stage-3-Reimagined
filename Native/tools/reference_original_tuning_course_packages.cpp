#include "sh4_scalar_reference.h"
#include "original_tuning.h"
#include <fstream>
#include <iostream>
using namespace idas3::reference;
int main(int argc,char**argv){try{RefMemory m(argv[1]);m.zeroRegion(0xd000000,0x20000);auto data=idas3::original::OriginalTuningData::load(argv[2]);std::ofstream out(std::filesystem::path(argv[2])/"data/original_assets/tuning_course/packages.bin",std::ios::binary);unsigned long long total=0;unsigned records=0;
for(unsigned car=0;car<35;car++){unsigned count=unsigned(data.cars[car].packages.size());out.write((const char*)&count,4);for(unsigned selected=0;selected<count;selected++){m.zeroRegion(0xd000000,0x10000);m.write32(0xd000000+1052,0xd008000);RefCpu c(m);c.r[4]=0xd000000;c.r[5]=car;c.r[6]=selected;c.r[15]=0xd01f000;c.pr=0xff0000;total+=c.run(0xc12a820,0xff0000,50000);for(unsigned i=0;i<16;i++){auto v=m.read8(0xd008000+468+i);out.write((const char*)&v,1);}unsigned parts[3]={-1u,-1u,-1u};std::vector<unsigned> unique;for(auto &step:data.cars[car].packages[selected].steps){auto kind=step.words[0],id=step.words[1];if(kind==8||(car==22&&selected==1&&id==33))continue;if(std::find(unique.begin(),unique.end(),id)==unique.end())unique.push_back(id);}for(unsigned i=0;i<3&&i<unique.size();i++)parts[i]=unique[i]%42;out.write((const char*)parts,12);records++;}}
std::cout<<records<<" packages "<<total<<" instructions\n";}catch(std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
