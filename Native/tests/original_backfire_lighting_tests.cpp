#include "original_car_lighting_reference.h"
#include "original_backfire_lighting.h"
#include <random>
using namespace idas3::reference;
using namespace idas3::original;
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("canonical image and FSCA table required");
    CarLightReference r(argv[1],argv[2]);
    constexpr unsigned owner=0xd030000,light=owner+0x100,matrix=owner+0x200,point=owner+0x300,array=owner+0x400;
    r.c.r[4]=light;r.c.r[5]=point;r.c.fr[4]=std::bit_cast<unsigned>(.3f);
    r.c.fr[5]=r.c.fr[6]=r.c.fr[7]=0;r.run(0xc054660);
    r.m.write32(owner+40,light);
    const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    r.c.callHooks[0xc1fa9e0]=[](auto&){};
    r.c.callHooks[0xc1f9ea0]=[](auto&q){q.fr[0]=std::bit_cast<unsigned>(.5f);};
    r.c.callHooks[0xc05a8e0]=[](auto&q){q.r[0]=0;};
    r.c.callHooks[0xc1d7120]=[](auto&){};
    r.m.writeFloat(array+88,1);r.c.r[4]=array;r.c.r[5]=light;r.run(0xc0539e0);
    unsigned checks=0,cases=0;
    const auto check=[&](bool good,const char*why){++checks;if(!good)throw std::runtime_error(why);};
    const auto bits=[](float value){return std::bit_cast<unsigned>(value);};
    std::mt19937 random(0x17cb00);std::uniform_real_distribution<float> value(-2.f,2.f);
    for(unsigned sample=0;sample<128;++sample){
        auto world=identity;if(sample)for(unsigned i=0;i<16;++i)if(i%4!=3)world[i]=value(random)*(i>=12?100.f:1.f);
        for(unsigned i=0;i<16;++i)r.m.writeFloat(matrix+4*i,world[i]);
        for(unsigned muffler=0;muffler<3;++muffler)for(unsigned frame=0;frame<3;++frame){
            const bool active=frame<2;
            r.m.write32(owner+12,muffler);r.m.write32(owner+8,frame);r.m.write8(owner+4,active);
            r.c.r[4]=owner;r.c.r[5]=matrix;r.run(0xc17cb00);
            const auto actual=originalBackfireLight(active,world);
            check(r.m.read32(light+20)==1&&actual.kind==OriginalCourseLightKind::Point,"Original point-light kind");
            for(unsigned i=0;i<3;++i){
                check(r.m.read32(light+32+4*i)==bits(actual.color[i]),"Backfire RGB differs from original owner");
                check(r.m.read32(light+44+4*i)==bits(actual.position[i]),"Backfire world anchor differs from original owner");
            }
            check(r.m.read32(light+60)==bits(actual.distance0)&&r.m.read32(light+64)==bits(actual.distance1),"Source attenuation distances changed");
            check(r.m.read32(owner+8)==(active?frame+1:frame),"Original frame sequence");
            check(bool(r.m.read8(owner+4))==(frame==0),"Original flame clears after second frame");
            check(actual.enabled==active,"Host light does not follow accepted cue");
            if(active)for(bool rear:{false,true}){
                auto view=identity;view[0]=view[10]=rear?-1.f:1.f;view[12]=2;view[13]=-3;view[14]=4;
                const auto expected=r.packets(array,view);
                OriginalCourseLighting set;set.count=1;set.lights[0]=actual;
                const auto packet=originalCourseLightingPacket(set,view);
                check(expected.size()==1&&packet.count==1,"Source light registration");
                for(unsigned i=0;i<8;++i)check(packet.lights[0][i]==expected[0][i],"Main/mirror light packet differs from original converter");
            }
            ++cases;
        }
    }
    std::cout<<"PASS "<<cases<<" source-owner cases, "<<checks<<" checks / "<<r.instructions
        <<" original instructions. Original point constructor, full17CB00 transform/color/frame owner and ELAN converter execute; RNG and geometry submissions are explicit boundaries.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
