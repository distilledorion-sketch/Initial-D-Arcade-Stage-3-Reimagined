#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>
namespace idas3::original {
struct OriginalConquerDraw {unsigned chunk=0;float x=0;};
struct OriginalConquerFrame {
    std::array<OriginalConquerDraw,9> draws{};unsigned count=1;
    std::array<unsigned,2> cues{};unsigned cueCount=0;
};
// 0C1960/0C1DA0/0C1DC0: source entrance layers and their 30 authored offsets.
// Advance once per owner tick. Painting/repaints never advance the animation.
struct OriginalConquerAnimation {
    unsigned course=0,frame=0;
    std::array<unsigned,8> phases{},ages{};
    OriginalConquerFrame presentation{};
    void begin(unsigned selected){if(selected>8)throw std::out_of_range("Conquered course");*this={};course=selected;}
    const OriginalConquerFrame& step(){
        static constexpr std::array<std::uint32_t,30> offsetBits{0x40a80000u,0x40a7c0ecu,0x40a703dau,0x40a5c971u,0x40a41284u,0x40a1e061u,0x409f34adu,0x409c1173u,0x409878ffu,0x40946e1au,0x408ff3b6u,0x408b0d45u,0x4085be62u,0x40800b0fu,0x4073ef20u,0x4067111fu,0x4059859du,0x404b5697u,0x403c8f08u,0x402d39c1u,0x401d628du,0x400d150eu,0x3ff8bb6fu,0x3fd691fbu,0x3fb3c750u,0x3f9075a3u,0x4107e56bu,0x40b5a36eu,0x4035e743u,0x00000000u};
        static constexpr std::array<unsigned,8> first{30,35,40,45,0,5,10,15},second{0,5,10,15,30,35,40,45};
        const auto& delays=(course==2||course==3||course==8)?second:first;
        ++frame;presentation={};
        for(unsigned i=0;i<8;++i){
            if(phases[i]==0){
                if(frame>delays[i]){phases[i]=1;if(i==0||i==4)presentation.cues[presentation.cueCount++]=i==0?9:10;}
            }else if(phases[i]==1){
                float x=std::bit_cast<float>(offsetBits.at(ages[i]));
                x+=course==4?std::bit_cast<float>(0x3f933333u):0.f;x*=i<4?-1.f:1.f;
                presentation.draws[presentation.count++]={i+1,x};
                if(++ages[i]>=30)phases[i]=2;
            }else presentation.draws[presentation.count++]={i+1,0};
        }
        return presentation;
    }
};
}
