#pragma once
#include "native_assets.h"
#include <algorithm>
#include <bit>

namespace idas3::original {
// Credits/final-card portion of 0EB2C0. One step is one original 60 Hz tick;
// the 3D driving cinematic is deliberately not approximated by this owner.
struct OriginalEndingTimeline {
    unsigned frame=0,totalFrame=0,phase=0;
    float alpha=255,creditsY=0,photosY=0;
    bool finished=false,finalCard=false;
    bool playStream=false,fadeStream=false,stopStream=false;
    void step(bool skip=false){
        playStream=fadeStream=stopStream=false;
        if(finished)return;
        playStream=frame==1;
        finalCard=phase>6;
        if(!finalCard){
            // 0EBBC0, table 262738. Inclusive ranges, accumulated float32.
            if(totalFrame>=181&&totalFrame<=4737)creditsY+=48.6f/float(4737-181);
            if(totalFrame>=321&&totalFrame<=4400)photosY+=-32.8f/float(4400-321);
            if(skip){fadeStream=true;phase=6;frame=4737;}
        }else if(skip)phase=10;
        constexpr float slowFade=std::bit_cast<float>(0x3fb55555u);
        switch(phase){
        case 0:alpha=std::max(0.f,alpha-slowFade);if(frame>180){phase=5;alpha=0;}break;
        case 5:if(frame>4737)++phase;break;
        case 6:alpha=std::min(255.f,alpha+2.125f);if(frame>4857){++phase;alpha=255;}break;
        case 7:alpha=std::max(0.f,alpha-2.125f);if(frame>4977){++phase;alpha=0;}break;
        case 8:if(frame>5337)++phase;break;
        case 9:
            alpha=std::min(255.f,alpha+slowFade);
            if(frame%3==0)alpha=std::min(255.f,alpha+1.f);
            if(frame>5457)++phase;
            break;
        case 10:alpha=255;finished=stopStream=true;break;
        }
        ++frame;++totalFrame;
    }
};

class OriginalEnding {
public:
    void begin(const std::filesystem::path& root);
    void paint(std::span<std::uint32_t> pixels,int width,int height)const;
    OriginalEndingTimeline timeline;
private:
    NativeModel model_;
    NativeTextureBank textures_;
};
}
