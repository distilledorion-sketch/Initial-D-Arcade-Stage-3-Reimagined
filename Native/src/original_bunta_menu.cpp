#include "original_bunta_menu.h"
#include <algorithm>

namespace idas3::original {
namespace {
// Fifteen XY pairs at 0C2A3324, preserving the original float literals.
constexpr std::array<std::array<float,2>,15> positions{{
    {1.6200000047683716f,-3.3399999141693115f},{1.6200000047683716f,-3.5199999809265137f},
    {1.7799999713897705f,-3.3399999141693115f},{1.7799999713897705f,-3.5199999809265137f},
    {1.9399999380111694f,-3.3399999141693115f},{2.259999990463257f,-3.3399999141693115f},
    {2.259999990463257f,-3.5199999809265137f},{2.419999837875366f,-3.3399999141693115f},
    {2.419999837875366f,-3.5199999809265137f},{2.5799999237060547f,-3.3399999141693115f},
    {2.8999998569488525f,-3.3399999141693115f},{2.8999998569488525f,-3.5199999809265137f},
    {3.059999942779541f,-3.3399999141693115f},{3.059999942779541f,-3.5199999809265137f},
    {3.2200000286102295f,-3.3399999141693115f}
}};
}
OriginalBuntaMenuDraws originalBuntaMenuDraws(std::int32_t level,std::uint32_t frame){
    // The authored table has fifteen stars. Source completion can store16;
    // its renderer's extra XY then reads following ASCII at2A339C and lies
    // far offscreen. Keep all fifteen visible stars without that overread.
    // Bound malformed host values at the same visible limits.
    level=std::clamp(level,0,15);
    OriginalBuntaMenuDraws out;out.backdropChunk=level>10?0u:level>5?2u:1u;
    for(unsigned band=0;band<3;++band){
        float value=float(level)-float(band*5);
        value*=.800000011920929f;value/=5.f;value+=.20000000298023224f;
        value=std::clamp(value,.20000000298023224f,1.f);
        const auto byte=std::uint32_t(value*255.f);
        out.progressColors[band]=0xff000000u|byte<<16|byte<<8|byte;
    }
    // 19AAF0..19ABC8: stagger each star by two frames; the six-frame
    // pop settles at1x. Source thresholds are strictly index>5 and>10.
    for(int star=0;star<level;++star){
        const auto elapsed=std::int64_t(frame)-star*2;
        if(elapsed<0)continue;
        float phase=float(std::min(elapsed,std::int64_t(6)))/6.f;
        float scale;
        if(star>10)scale=2.f-phase;
        else if(star>5){phase*=.75f;scale=1.75f-phase;}
        else {phase*=.5f;scale=1.5f-phase;}
        const auto& xy=positions[std::size_t(star)];
        out.stars.push_back({0x0004001bu,xy[0],xy[1],.019999999552965164f,scale});
    }
    return out;
}
NativeModelChunk materializeOriginalBuntaColors(const NativeModelChunk& chunk,
    const std::array<std::uint32_t,3>& progressColors){
    // 19AA1E..19AA86 writes global color-buffer entries8..13. 1B8240
    // consumes those entries across both mesh strips; preceding entries
    // retain the constructor's white diffuse and zero secondary color.
    auto out=chunk;std::size_t index=0;
    for(auto& batch:out.batches){
        batch.material[2]=0x600u;
        for(auto& vertex:batch.vertices){
            vertex.color0=index>=8&&index<14?progressColors[(index-8)/2]:0xffffffffu;
            vertex.color1=0;++index;
        }
    }
    return out;
}
}
