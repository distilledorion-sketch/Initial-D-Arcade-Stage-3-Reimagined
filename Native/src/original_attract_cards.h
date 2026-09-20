#pragma once
#include "native_assets.h"
#include <cstdint>
#include <filesystem>
#include <span>

namespace idas3::original {
struct OriginalAttractCardState {
    unsigned child=3,frame=0,phase=0;
    unsigned displayedFrame=0,meshFrame=0,fade=255,fadeRgb=0,background=0xffffff;
    bool logoPanel=false,completed=false;
};
// Source scene owners3,4,5. Cabinet load/network/card ready boundaries are
// explicit inputs; ordinary native play supplies all-ready/no-card hardware.
struct OriginalAttractCardReadiness {
    bool resourcesReady=true,networkWaiting=false,cardEnabled=false,cardReady=true;
};
void resetOriginalAttractCard(OriginalAttractCardState&,unsigned child);
void stepOriginalAttractCard(OriginalAttractCardState&,const OriginalAttractCardReadiness& readiness={});
std::uint32_t originalAttractCardFadeArgb(const OriginalAttractCardState&);
// Original1FA280(0x1000) rational tangent, preserving source float order.
float originalAttractCardProjectionScale();
class OriginalAttractCards {
public:
    void load(const std::filesystem::path& root);
    void reset(unsigned child){resetOriginalAttractCard(state_,child);}
    void step(){stepOriginalAttractCard(state_);}
    bool completed()const{return state_.completed;}
    unsigned frame()const{return state_.frame;}
    std::uint32_t fadeArgb()const{return originalAttractCardFadeArgb(state_);}
    const OriginalAttractCardState& state()const{return state_;}
    void paint(std::span<std::uint32_t>,int width,int height)const;
private:
    OriginalAttractCardState state_;
    NativeModel sega_,rosso_,rossoPanel_,caution_;
    NativeTextureBank segaTextures_,rossoPanelTextures_,cautionTextures_;
};
}
