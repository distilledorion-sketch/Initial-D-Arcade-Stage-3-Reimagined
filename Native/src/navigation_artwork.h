#pragma once
#include "native_assets.h"
#include <algorithm>
#include <cmath>
#include <span>

namespace idas3 {
// Rebuild the road from the authored navigation centreline. A small distance
// field keeps the curve smooth at any HUD scale without shipping large RGBA
// atlases or uploading a new map every frame. The original on-disk bank stays
// intact; its material supplies the day/night colour and blending as before.
inline NativeImage navigationArtwork(const NativeImage& source,
        std::span<const std::array<float,5>> path,float sourceStrokeWidth){
    constexpr unsigned size=1024;
    constexpr float range=8.f;
    std::array<unsigned,256> fields{},roads{};
    for(auto color:source.argb){
        const unsigned red=(color>>16)&255;
        if(red==255)++fields[color>>24];
        if(red<=17)++roads[color>>24];
    }
    const auto mode=[](const auto& counts){return unsigned(std::max_element(counts.begin(),counts.end())-counts.begin());};
    const unsigned fieldAlpha=mode(fields),roadAlpha=mode(roads);
    const float radius=sourceStrokeWidth*float(size)/(2.f*source.width);
    std::vector<float> distances(std::size_t(size)*size,radius+range);
    // Translate the authored UV bank once, never wrap individual vertices.
    // Irohazaka extends past U=1 and its material clamps at the edge;
    // per-point wrapping would draw a spurious chord across the whole map.
    const float shiftU=-std::floor(path.front()[0]),shiftV=-std::floor(path.front()[1]);
    const auto point=[&](const auto& p){return std::array<float,2>{(p[0]+shiftU)*size,(p[1]+shiftV)*size};};
    for(std::size_t i=1;i<path.size();++i){
        const auto a=point(path[i-1]),b=point(path[i]);
        const float dx=b[0]-a[0],dy=b[1]-a[1],length=dx*dx+dy*dy,pad=radius+range;
        const int left=std::max(0,int(std::floor(std::min(a[0],b[0])-pad)));
        const int right=std::min(int(size)-1,int(std::ceil(std::max(a[0],b[0])+pad)));
        const int top=std::max(0,int(std::floor(std::min(a[1],b[1])-pad)));
        const int bottom=std::min(int(size)-1,int(std::ceil(std::max(a[1],b[1])+pad)));
        for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x){
            const float px=x+.5f-a[0],py=y+.5f-a[1];
            const float t=length>0?std::clamp((px*dx+py*dy)/length,0.f,1.f):0.f;
            auto& distance=distances[std::size_t(y)*size+x];
            distance=std::min(distance,std::hypot(px-t*dx,py-t*dy));
        }
    }
    NativeImage image{size,size,{},true};image.argb.resize(distances.size());
    for(std::size_t i=0;i<distances.size();++i){
        const unsigned encoded=unsigned(std::lround(255.f*std::clamp(.5f+(distances[i]-radius)/(2.f*range),0.f,1.f)));
        image.argb[i]=0xff000000u|(encoded<<16)|(fieldAlpha<<8)|roadAlpha;
    }
    return image;
}
}
