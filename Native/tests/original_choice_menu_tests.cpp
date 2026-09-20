#include "original_choice_menu.h"
#include "sh4_scalar_reference.h"
#include <bit>
#include <iostream>
#include <map>
using namespace idas3::original;
using namespace idas3::reference;
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
struct Matrix{float x=0,y=0,z=0,scale=1;};
int main(int argc,char**argv){try{
    if(argc<2)throw std::runtime_error("Canonical image required");
    RefMemory m(argv[1]);constexpr unsigned object=0xd000000,stack=0xd010000,stop=0xff0000,selectedBuffer=0xd020000,inactiveBuffer=0xd020100;
    std::size_t cases=0,comparisons=0,instructions=0;
    const unsigned entries[]={0xc1b7cc0,0xc197e00,0xc198880,0xc1992e0,0xc199f60};
    for(int screen=0;screen<5;++screen)for(int course=0;course<(screen==1 || screen==4?9:1);++course)for(int selection=0;selection<(screen==4?1:2);++selection)for(unsigned frame=0;frame<(screen==4?1u:8u);++frame)for(float phase:{0.f,.25f,1.f}){
        m.clear();m.zeroRegion(object,2048);m.zeroRegion(stack,0x10000);
        const bool mission=screen==0;const unsigned offset=mission?4:0;
        m.write32(object+416+offset,selection);m.writeFloat(object+420+offset,phase);m.write32(object+424+offset,frame);
        m.write32(object+428+offset,selectedBuffer);m.write32(object+432+offset,inactiveBuffer);m.write32(0xc31c9a0,course);
        RefCpu c(m);c.r[4]=object;c.r[15]=stack+0xf000;c.pr=stop;
        Matrix matrix;std::vector<Matrix> matrices;std::vector<OriginalChoiceDraw> captured;
        std::map<unsigned,OriginalChoiceColors> colors;
        for(unsigned entry:{0xc1b8fe0u,0xc1b90e0u,0xc1baf40u,0xc1bb6e0u})c.callHooks[entry]=[](auto& cpu){cpu.r[0]=1;};
        c.callHooks[0xc05a8e0]=[](auto& cpu){cpu.r[0]=cpu.r[5];};
        c.callHooks[0xc1f6610]=[&](auto&){matrices.push_back(matrix);};
        c.callHooks[0xc1f65c0]=[&](auto&){require(!matrices.empty(),"Unbalanced source matrix stack");matrix=matrices.back();matrices.pop_back();};
        c.callHooks[0xc1f6ac0]=[&](auto& cpu){matrix.x+=matrix.scale*cpu.getFloat(4);matrix.y+=matrix.scale*cpu.getFloat(5);matrix.z+=cpu.getFloat(6);};
        c.callHooks[0xc1f69d0]=[&](auto& cpu){require(cpu.getFloat(4)==cpu.getFloat(5),"Nonuniform choice scale");matrix.scale*=cpu.getFloat(4);};
        c.callHooks[0xc1b8240]=[&](auto& cpu){
            require(cpu.r[4]==selectedBuffer || cpu.r[4]==inactiveBuffer,"Unexpected source color buffer");
            colors[cpu.r[5]]=cpu.r[4]==selectedBuffer?(screen>=2?OriginalChoiceColors::SelectedWeatherTime:OriginalChoiceColors::Selected):mission?OriginalChoiceColors::InactiveTransmission:screen==1?OriginalChoiceColors::InactiveRoute:OriginalChoiceColors::InactiveWeatherTime;
        };
        c.callHooks[0xc1d7120]=[&](auto& cpu){captured.push_back({cpu.r[4],matrix.x,matrix.y,matrix.z,matrix.scale,colors.contains(cpu.r[4])?colors.at(cpu.r[4]):OriginalChoiceColors::Source});};
        instructions+=c.run(entries[screen],stop,10000);
        const auto native=screen==4?originalChoiceCourseUnderlay(course):originalChoiceMenuDraws({OriginalChoiceScreen(screen),selection,course,phase,frame});
        require(captured.size()==native.size(),"Original choice command count differs");
        for(std::size_t i=0;i<native.size();++i){const auto&a=captured[i];const auto&b=native[i];
            if(a.selector!=b.selector || a.colors!=b.colors || std::bit_cast<unsigned>(a.x)!=std::bit_cast<unsigned>(b.x) || std::bit_cast<unsigned>(a.y)!=std::bit_cast<unsigned>(b.y) || std::bit_cast<unsigned>(a.z)!=std::bit_cast<unsigned>(b.z) || std::bit_cast<unsigned>(a.scale)!=std::bit_cast<unsigned>(b.scale)){
                std::cerr<<"screen="<<screen<<" course="<<course<<" selected="<<selection<<" frame="<<frame<<" phase="<<phase<<" draw="<<i<<" original="<<std::hex<<a.selector<<" native="<<b.selector<<std::dec<<" xyz="<<a.x<<','<<a.y<<','<<a.z<<" vs="<<b.x<<','<<b.y<<','<<b.z<<'\n';
                throw std::runtime_error("Original choice draw differs");
            }
            comparisons+=6;
        }
        require(m.read32(object+424+offset)==frame+(screen!=4),"Original choice tick differs");++comparisons;++cases;
    }
    std::cout<<"PASS "<<cases<<" original choice captures, "<<comparisons<<" exact command/state comparisons, "<<instructions<<" original instructions. Draw/bank/matrix boundaries hooked; renderer parity not asserted.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
