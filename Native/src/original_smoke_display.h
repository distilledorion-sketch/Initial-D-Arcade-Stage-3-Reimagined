#pragma once
#include "original_smoke_emitter.h"
#include <limits>
#include <span>

namespace idas3::original {
// Inputs at the original camera-transform/frustum boundary in13FC00.
// This module ports projection and the shared density grid. The separate
// original_smoke_depth and original_smoke_mesh modules provide the source
// footprint depth selector and material/UV draw preparation.
struct OriginalSmokeDisplayParticle {
    OriginalSmokeParticle particle;
    std::array<float,3> cameraPosition{}; // source view: forward is negative Z
    bool inFrustum=true;
};
struct OriginalSmokeCircle {float x=0,y=0,radius=0,weight=0;};
struct OriginalSmokeDisplay {
    bool visible=false;
    float left=0,right=0,bottom=0,top=0,minZ=0,maxZ=0;
    std::array<float,3> position{};
    float width=0,height=0;
    std::vector<OriginalSmokeCircle> circles;
    std::vector<float> density;
};

template<class SelectDepth> inline OriginalSmokeDisplay projectOriginalSmokeDisplay(
        std::span<OriginalSmokeDisplayParticle> particles,const OriginalSmokeParameters& parameters,
        float focalScale,float gain,unsigned gridWidth,unsigned gridHeight,
        std::span<const std::uint16_t> kernel,unsigned kernelWidth,unsigned kernelHeight,
        SelectDepth&& selectDepth,float clipHalfWidth=320.f){
    if(gridWidth<2||gridHeight<2||gridWidth>32||gridHeight>32||!kernelWidth||!kernelHeight||
       kernel.size()!=std::size_t(kernelWidth)*kernelHeight||!std::isfinite(focalScale)||focalScale<=0||
       !std::isfinite(clipHalfWidth)||clipHalfWidth<=0)
        throw std::invalid_argument("Original smoke display dimensions/projection");
    OriginalSmokeDisplay out;
    out.left=out.bottom=out.minZ=std::numeric_limits<float>::max();
    out.right=out.top=out.maxZ=-std::numeric_limits<float>::max();
    const float fadeScale=parameters.values[2]*gain;
    for(auto& entry:particles){
        auto& particle=entry.particle;
        if(!prepareOriginalSmokeParticleDisplay(particle))continue;
        const auto& camera=entry.cameraPosition;
        const float reciprocal=1.f/-camera[2];
        const float radius=(particle.size*focalScale)*reciprocal;
        if(radius>300.f||!entry.inFrustum||!(radius>0.f)||!(camera[2]<0.f))continue;
        const float projection=focalScale*reciprocal;
        OriginalSmokeCircle circle{camera[0]*projection,camera[1]*projection,radius,
            (fadeScale/radius)*float(std::bit_cast<std::int32_t>(particle.remaining))};
        out.circles.push_back(circle);
        out.right=(std::max)(out.right,circle.x+radius);out.left=(std::min)(out.left,circle.x-radius);
        out.top=(std::max)(out.top,circle.y+radius);out.bottom=(std::min)(out.bottom,circle.y-radius);
        out.maxZ=(std::max)(out.maxZ,camera[2]);out.minZ=(std::min)(out.minZ,camera[2]);
    }
    if(out.circles.empty())return out;
    // Source default:640x480. The host can explicitly widen the horizontal
    // plane with its viewport, without altering the original4/3 path.
    out.right=(std::min)(out.right,clipHalfWidth);out.left=(std::max)(out.left,-clipHalfWidth);
    out.top=(std::min)(out.top,240.f);out.bottom=(std::max)(out.bottom,-240.f);
    out.width=out.right-out.left;out.height=out.top-out.bottom;
    const float stepX=out.width*(1.f/float(gridWidth-1));
    const float stepY=(1.f/float(gridHeight-1))*out.height;
    const float depth=selectDepth(out.minZ,out.maxZ);
    out.position={(std::fma(out.width,.5f,out.left)/focalScale)*-depth,
        (out.bottom/focalScale)*-depth,depth};
    out.density.assign(std::size_t(gridWidth)*gridHeight,std::bit_cast<float>(0x3c03126fu)); //0D18E0: .008
    const auto ftrc=[](float value)->std::int32_t{
        if(std::isnan(value)||value<=-2147483648.f)return std::numeric_limits<std::int32_t>::min();
        if(value>=2147483648.f)return std::numeric_limits<std::int32_t>::max();
        return std::int32_t(value);
    };
    const auto increment=[](std::int32_t n){return std::bit_cast<std::int32_t>(std::uint32_t(n)+1u);};
    for(const auto& circle:out.circles){
        const int beginX=(std::max)(1,increment(ftrc(((circle.x-out.left)-circle.radius)/stepX)));
        const int endX=(std::min)(int(gridWidth)-1,increment(ftrc(((circle.x-out.left)+circle.radius)/stepX)));
        const int beginY=(std::max)(1,increment(ftrc(((circle.y-out.bottom)-circle.radius)/stepY)));
        const int endY=(std::min)(int(gridHeight)-1,increment(ftrc(((circle.y-out.bottom)+circle.radius)/stepY)));
        // Deliberately restart the kernel at zero after clipping, as1400AE does.
        const auto difference=[](int end,int begin){return std::bit_cast<std::int32_t>(std::uint32_t(end)-std::uint32_t(begin));};
        const float sampleX=float(kernelWidth)/float(difference(endX,beginX));
        const float sampleY=float(kernelHeight)/float(difference(endY,beginY));
        for(int y=beginY;y<endY;++y)for(int x=beginX;x<endX;++x){
            const auto sx=ftrc(float(x-beginX)*sampleX),sy=ftrc(float(y-beginY)*sampleY);
            const float value=float((kernel[std::size_t(sy)*kernelWidth+sx]>>4)&15u);
            auto& density=out.density[std::size_t(y)*gridWidth+x];density=std::fma(value,circle.weight,density);
        }
    }
    const float maximum=std::bit_cast<float>(0x3f7d70a4u); //0D1960: .99
    for(auto& density:out.density)if(density>maximum)density=maximum;
    out.visible=true;return out;
}
}
