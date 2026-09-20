#include "original_course_light_path.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Canonical original image required");
    RefMemory m(argv[1]);
    constexpr unsigned object=0x0d000000,storage=0x0d010000,data=0x0d020000,output=0x0d030000,stack=0x0d040000,stop=0x0f000000;
    unsigned checks=0,cases=0;std::uint64_t instructions=0;
    for(unsigned count:{21u,129u,513u})for(bool reverse:{false,true}){
        m.clear();m.zeroRegion(object,0x50000);m.write32(object,0x0c386194);m.write32(object+12,storage);m.write32(object+28,unsigned(reverse));
        m.write32(storage,count);m.write32(storage+4,data);std::vector<Vec3> points(count);
        for(unsigned i=0;i<count;++i){points[i]={float(i)*1.125f,float(i*i)*.03125f,-float(i)*7.25f};
            m.writeFloat(data+i*12,points[i].x);m.writeFloat(data+i*12+4,points[i].y);m.writeFloat(data+i*12+8,points[i].z);}
        if(reverse)std::reverse(points.begin(),points.end());
        const int period=int(count)-1;
        for(int index=-3*period-1;index<=3*period+1;++index){
            RefCpu c(m);c.r[4]=object;c.r[5]=std::bit_cast<unsigned>(index);c.r[6]=output;c.r[15]=stack+0xff00;c.pr=stop;
            c.callHooks[0x0c2223b8]=[&](RefCpu& cpu){
                if(cpu.r[5]!=unsigned(period))throw std::runtime_error("Unexpected point-reference division boundary");
                cpu.fpul=std::bit_cast<unsigned>(signed32(cpu.r[4])/period);
            };
            instructions+=c.run(0x0c099460,stop,2000);
            const auto actual=originalCourseLightReference(points,index);const std::array<float,3> values{actual.x,actual.y,actual.z};
            for(unsigned i=0;i<3;++i){++checks;if(std::bit_cast<unsigned>(values[i])!=m.read32(output+i*4))throw std::runtime_error("Original light reference point differs");}
            ++cases;
        }
    }
    std::cout<<"PASS "<<cases<<" course light path cases / "<<checks<<" bit comparisons / "<<instructions<<" original instructions; complete099460 and09AF60, signed quotient leaf explicit, both directions, endpoints and wrapped indices.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
