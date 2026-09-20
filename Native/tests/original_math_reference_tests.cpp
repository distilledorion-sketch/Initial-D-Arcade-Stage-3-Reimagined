#include "original_math.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
#include <vector>
using namespace idas3::reference;
using namespace idas3::original;
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Pass the canonical original image");RefMemory memory(argv[1]);
    std::vector<float> angles{0.f,-0.f,1.f,-1.f,3.1415927410125732f,-3.1415927410125732f,1e-6f,-1e-6f,100000.f,-100000.f,100000000.f,-100000000.f};
    for(int i=-100;i<=100;i++){
        const float center=i*1.5707963705062866f;
        angles.push_back(center);angles.push_back(std::nextafter(center,INFINITY));angles.push_back(std::nextafter(center,-INFINITY));
    }
    std::mt19937 random(0x1fa120);std::uniform_real_distribution<float> distribution(-20000.f,20000.f);
    for(int i=0;i<10000;i++)angles.push_back(distribution(random));
    std::size_t instructions=0,checks=0;
    for(float angle:angles)for(bool cosine:{false,true}){
        memory.clear();memory.zeroRegion(0x70000000,4096);RefCpu cpu(memory);cpu.r[15]=0x70000ff0;cpu.pr=0xffff0000;cpu.setFloat(4,angle);
        instructions+=cpu.run(cosine?0x0c1f9960:0x0c1fa0e0,0xffff0000,1000);
        const auto value=cosine?originalCosF32(angle):originalSinF32(angle);
        const auto actual=std::bit_cast<std::uint32_t>(value);
        if(actual!=cpu.fr[0])throw std::runtime_error(std::string(cosine?"cos":"sin")+" mismatch anglebits="+hex(std::bit_cast<std::uint32_t>(angle))+" expected="+hex(cpu.fr[0])+" actual="+hex(actual));
        checks++;
    }
    std::cout<<"PASS original sine/cosine: "<<checks<<" bit-exact comparisons, "<<instructions<<" original instructions, zero hooks\n";
    std::vector<float> inverseInputs{0.f,-0.f,1.f,-1.f,2.f,-2.f,1e30f,-1e30f,1e-30f,-1e-30f};
    for(float boundary:{-1.f,0.f,1.f}){
        inverseInputs.push_back(std::nextafter(boundary,INFINITY));
        inverseInputs.push_back(std::nextafter(boundary,-INFINITY));
    }
    for(int index=0;index<=4096;++index)inverseInputs.push_back(float(index)/2048.f-1.f);
    std::uniform_real_distribution<float> inverseDistribution(-100.f,100.f);
    for(int i=0;i<2000;++i)inverseInputs.push_back(inverseDistribution(random));
    std::size_t inverseChecks=0,inverseInstructions=0;
    const auto checkInverse=[&](std::uint32_t entry,float first,float second,std::uint32_t actual){
        memory.clear();memory.zeroRegion(0x70000000,4096);RefCpu cpu(memory);cpu.r[15]=0x70000ff0;cpu.pr=0xffff0000;
        cpu.setFloat(4,first);cpu.setFloat(5,second);
        inverseInstructions+=cpu.run(entry,0xffff0000,2000);
        if(actual!=cpu.r[0])throw std::runtime_error("Inverse angle mismatch entry="+hex(entry)+" first="+hex(std::bit_cast<std::uint32_t>(first))+" second="+hex(std::bit_cast<std::uint32_t>(second))+" expected="+hex(cpu.r[0])+" actual="+hex(actual));
        ++inverseChecks;
    };
    for(float value:inverseInputs){
        checkInverse(0x0c1f9640,value,0,originalAsinAngle(value));
        checkInverse(0x0c1f9700,value,0,originalAtanAngle(value));
    }
    const std::vector<float> axes{-1e30f,-100.f,-1.f,-1e-30f,-0.f,0.f,1e-30f,1.f,100.f,1e30f};
    for(float first:axes)for(float second:axes)checkInverse(0x0c1f97c0,first,second,originalAtan2Angle(first,second));
    for(int index=0;index<4096;++index){
        const float first=inverseDistribution(random),second=inverseDistribution(random);
        checkInverse(0x0c1f97c0,first,second,originalAtan2Angle(first,second));
    }
    std::cout<<"PASS original integer-angle asin/atan/atan2: "<<inverseChecks<<" bit-exact comparisons, "<<inverseInstructions<<" original instructions, zero hooks\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
