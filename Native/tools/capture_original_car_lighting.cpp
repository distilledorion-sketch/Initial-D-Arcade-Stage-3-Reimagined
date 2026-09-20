#include "original_car_lighting_reference.h"
using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("canonical image and FSCA table required");
 CarLightReference r(argv[1],argv[2]);r.carConstructor(r.car,0);r.dumpArray(r.car);
 std::cout<<"embeddedSpot";for(unsigned i=0;i<23;++i)std::cout<<' '<<std::hex<<r.m.read32(r.car+2544+4*i);std::cout<<std::dec<<'\n';
 r.courseConstructor(4,true,false);r.borrow(r.car,4,true,false);r.dumpArray(r.car);
 const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 auto packets=r.packets(r.m.read32(r.car+2540),identity);for(unsigned i=0;i<packets.size();++i){std::cout<<"packet "<<i;for(auto w:packets[i])std::cout<<' '<<std::hex<<w;std::cout<<std::dec<<'\n';}
 const auto point=r.m.read32(r.course+1068);std::cout<<"point";for(unsigned i=0;i<17;++i)std::cout<<' '<<std::hex<<r.m.read32(point+4*i);std::cout<<std::dec<<'\n';
 r.m.write32(r.m.read32(r.car+2540)+68,0);r.m.write8(r.car+2544+24,1);r.c.r[4]=r.m.read32(r.car+2540);r.c.r[5]=r.car+2544;r.run(0x0c0539e0);
 auto spot=r.packets(r.m.read32(r.car+2540),identity);std::cout<<"ownSpotPacket";for(auto w:spot.at(0))std::cout<<' '<<std::hex<<w;std::cout<<std::dec<<'\n';
 for(unsigned phase:{2731u,4551u}){auto p=phase;unsigned w;if(p<32768)w=r.fsca[p];else w=r.fsca[p-32768]^0x80000000;std::cout<<"sin "<<phase<<' '<<std::hex<<w<<std::dec<<'\n';p=(phase+16384)&65535;w=p<32768?r.fsca[p]:r.fsca[p-32768]^0x80000000;std::cout<<"cos "<<phase<<' '<<std::hex<<w<<std::dec<<'\n';}
 std::cout<<"PASS "<<r.instructions<<" original instructions\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
