#pragma once
#include "native_assets.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace idas3 {
// Recovered 17D2A0 state machine. Contact cues are read-only copies; the cup
// never changes the driving state or consumes its random stream.
struct OriginalWaterCup {
    unsigned frame=0;
    int water=29,rightSplash=-1,leftSplash=-1,mode=0,unused=0,severity=0;
    void reset(){*this={};}
    void advance(float movement,const std::array<std::uint32_t,5>& cues){
        if(mode!=3&&mode!=4)mode=cues[4]?2:std::abs(movement)>.02f?1:0;
        const auto impact=[&](int side,int strength){
            mode=side;severity=strength;frame=0;rightSplash=leftSplash=-1;
        };
        if(cues[2])impact(3,0);
        if(cues[3])impact(4,0);
        const float a=std::bit_cast<float>(cues[0]),b=std::bit_cast<float>(cues[1]);
        if(a>0)impact(4,a<.05f?1:2);
        if(b>0)impact(3,b<.05f?1:2);
        if(mode==0)water=29;
        else if(mode==1){const int f=frame%14;water=f<=6?26+f:40-f;}
        else if(mode==2){const int f=frame%7;water=f<=2?22+4*f:36-4*(f-3);}
        else {
            // 323BE4: peak, outward duration, total duration, frame step.
            constexpr int curves[6][4]={{45,15,30,1},{50,10,20,2},{59,5,10,6},
                {15,15,30,1},{10,10,20,2},{0,5,10,6}};
            const auto& c=curves[severity+(mode==4?3:0)];
            const int f=int(frame),sign=mode==3?1:-1;
            water=f<c[1]?(mode==3?30:29)+sign*f*c[3]:c[0]-sign*(f-c[1])*c[3];
            auto& splash=mode==3?rightSplash:leftSplash;
            if(severity==2)splash=f>=4&&f<=6?splash+1:-1;
            if(f>=c[2]){mode=1;unused=0;splash=-1;}
        }
        water=std::clamp(water,0,59);++frame;
    }
};

class OriginalWaterCupArtwork {
public:
    void load(const std::filesystem::path& root){
        const auto folder=root/"data/original_assets/hud/cupwater";
        model_=NativeModel::load(folder/"cupwater.idasmesh");
        textures_=NativeTextureBank::load(folder/"textures/textures.idastex");
        if(model_.chunks.size()!=70||textures_.size()!=7)throw std::runtime_error("Incomplete original water-cup artwork");
    }
    void paint(std::span<std::uint32_t> pixels,int width,int height,const OriginalWaterCup& state,
               float x,float y,float scale)const{
        SpritePlacement p;p.scale=scale;p.invertY=true;p.authoredHeight=0;
        p.offsetX=x;p.offsetY=y;p.straightAlphaOverlay=true;
        const auto draw=[&](unsigned chunk){compositeOriginalMenuChunk(pixels,width,height,textures_,model_.chunks.at(chunk),p);};
        // 17D0D2: cup, selected water frame, right/left splash.
        draw(0);draw(9+unsigned(state.water));
        if(state.rightSplash>=0)draw(6+unsigned(state.rightSplash));
        if(state.leftSplash>=0)draw(3+unsigned(state.leftSplash));
    }
private:
    NativeModel model_;
    NativeTextureBank textures_;
};
}
