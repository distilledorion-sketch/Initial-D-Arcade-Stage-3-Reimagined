#include "original_course_lighting.h"
#include "original_course_lighting_data.h"
#include "original_happo_lighting_data.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace idas3::original {
namespace {
float f(std::uint32_t w){return std::bit_cast<float>(w);}
std::uint32_t bits(float v){return std::bit_cast<std::uint32_t>(v);}
OriginalLightVector transform(const OriginalLightVector& v,const OriginalLightMatrix& m,float w){
    OriginalLightVector out{};
    for(unsigned i=0;i<3;++i){
        // Primary SH4 PR0 FTRV reference: double products and ordered sums,
        // rounded once to float. This is reference parity, not silicon proof.
        double x=double(m[i])*v[0];x+=double(m[4+i])*v[1];x+=double(m[8+i])*v[2];x+=double(m[12+i])*w;out[i]=float(x);
    }
    return out;
}
OriginalLightVector cross(const OriginalLightVector&a,const OriginalLightVector&b){
    return {std::fma(a[1],b[2],-(a[2]*b[1])),std::fma(a[2],b[0],-(a[0]*b[2])),std::fma(a[0],b[1],-(a[1]*b[0]))};
}
std::uint32_t quantize(float v,float scale){
    const float scaled=v*scale;
    if(!std::isfinite(scaled)||double(scaled)<double(std::numeric_limits<std::int32_t>::min())||double(scaled)>double(std::numeric_limits<std::int32_t>::max()))throw std::invalid_argument("Invalid source light color");
    return std::uint32_t(std::int32_t(scaled))&255u;
}
std::uint32_t rgb(const OriginalLightVector&c,float scale){return quantize(c[0],scale)<<16|quantize(c[1],scale)<<8|quantize(c[2],scale);}
std::uint16_t phase(float degrees){float v=degrees*65536.f;v/=360.f;v+=.5f;return std::uint16_t(std::int32_t(v));}
std::uint32_t angularCoefficient(const OriginalCourseLight&l,std::int16_t dx,std::int16_t dy,std::int16_t dz){
    if(l.angle0==l.angle1)return 0x00003f80u;
    // Source206280..3FC corrects the packed direction's length with three
    // one-unit lookup adjustments, before deriving the angular slope.
    unsigned x=unsigned(std::abs(int(dx))),y=unsigned(std::abs(int(dy))),z=unsigned(std::abs(int(dz)));
    const unsigned index=(x>>10)+((y>>9)&2)+((z>>8)&4),shift=index*4;
    auto correction=[&](unsigned table){return (shift>=32&&shift%32==0?0u:table>>(shift&31))&1u;};
    x+=correction(0x01000010);y+=correction(0x00010100);z+=correction(0x10101000);
    const float fx=float(x)*f(0x3a000000),fy=float(y)*f(0x3a000000),fz=float(z)*f(0x3a000000);
    float length=std::fma(fx,fx,fy*fy);length=std::fma(fz,fz,length);length=std::sqrt(length);
    const float inner=1.f-l.angleCosines[0]*length,outer=1.f-l.angleCosines[1]*length;
    float b=1.f/(inner-outer);b=std::fma(f(0x3b7fffff),b,b);
    const auto bw=bits(b)&0xffff0000u;
    const auto aw=bits(f(bw)*(-outer))&0xffff0000u;
    return bw|(aw>>16);
}
}
OriginalCourseLighting originalCourseLightingTableRow(unsigned course,bool night,bool wet){
    if(course>=9)throw std::out_of_range("Original course lighting index");
    OriginalCourseLighting s;s.sourceRow=course*4+unsigned(night)*2+unsigned(wet);s.sourceAddress=0x0c33c864+s.sourceRow*368;
    const auto&w=originalCourseLightingWords[s.sourceRow];
    auto vec=[&](unsigned offset){return OriginalLightVector{f(w[offset/4]),f(w[offset/4+1]),f(w[offset/4+2])};};
    s.ambient=vec(0);
    for(unsigned group=0;group<2;++group){
        const unsigned count=group?std::min(w[3],3u):std::min(w[22],2u);
        for(unsigned i=0;i<count;++i){auto&l=s.lights[s.count++];l.kind=group?OriginalCourseLightKind::Parallel:OriginalCourseLightKind::RelativeParallel;l.sourceSlot=i;
            l.incomingDirection=vec((group?16:92)+24*i);l.color=vec((group?28:104)+24*i);}
    }
    for(unsigned i=0;i<std::min(w[35],4u);++i){auto&l=s.lights[s.count++];l.kind=OriginalCourseLightKind::Spot;l.sourceSlot=i;
        const unsigned b=144+52*i;l.position=vec(b);l.incomingDirection=vec(b+12);l.angle0=phase(f(w[(b+24)/4]));l.angle1=phase(f(w[(b+28)/4]));
        l.color=vec(b+32);l.distance0=f(w[(b+44)/4]);l.distance1=f(w[(b+48)/4]);l.coefficientWords=originalCourseSpotCoefficientWords[s.sourceRow][i];
        for(unsigned j=0;j<2;++j)l.angleCosines[j]=f(originalCourseSpotCosineWords[s.sourceRow][i][j]);}
    return s;
}
OriginalCourseLighting originalCourseLighting(unsigned course,bool night,bool wet){
    if(course!=4)return originalCourseLightingTableRow(course,night,wet);
    const unsigned row=unsigned(night)*2+unsigned(wet);OriginalCourseLighting s;
    s.sourceRow=16+row;s.sourceAddress=0x0c040d60;s.count=originalHappoLightCounts[row];
    for(unsigned j=0;j<3;++j)s.ambient[j]=f(originalHappoAmbientWords[row][j]);
    for(unsigned i=0;i<s.count;++i){auto&l=s.lights[i];const auto&c=originalHappoCapturedLights[row][i];const auto&w=c.object;
        l.kind=w[5]==2?OriginalCourseLightKind::Spot:c.ownerOffset>=88&&c.ownerOffset<=92?OriginalCourseLightKind::RelativeParallel:OriginalCourseLightKind::Parallel;
        l.sourceSlot=l.kind==OriginalCourseLightKind::Spot?(c.ownerOffset-1028)/4:l.kind==OriginalCourseLightKind::RelativeParallel?(c.ownerOffset-88)/4:0;
        l.enabled=(w[6]&255)!=0;
        for(unsigned j=0;j<3;++j){l.color[j]=f(w[8+j]);l.incomingDirection[j]=f(w[(l.kind==OriginalCourseLightKind::Spot?17:11)+j]);if(l.kind==OriginalCourseLightKind::Spot)l.position[j]=f(w[11+j]);}
        if(l.kind==OriginalCourseLightKind::Spot){l.distance0=f(w[15]);l.distance1=f(w[16]);l.angle0=std::uint16_t(w[21]);l.angle1=std::uint16_t(w[22]);l.coefficientWords=c.coefficients;for(unsigned j=0;j<2;++j)l.angleCosines[j]=f(c.cosines[j]);}
    }
    return s;
}
std::int16_t originalLightDirection12(float value){
    if(!std::isfinite(value))throw std::invalid_argument("Nonfinite original light direction");
    // Only the boundary outcome matters outside the FTRC representable range.
    if(value>1.000016f)return 2047;if(value<=-1.000016f)return -2047;
    const auto v=std::int32_t(value*65535.f);
    if(v>65535)return 2047;if(v<=-65536)return -2047;
    return std::int16_t(v>>5);
}
void updateOriginalCourseRelativeDirections(OriginalCourseLighting&s,const OriginalLightMatrix&m){
    const auto up=transform({0,1,0},m,0),forward=transform({0,0,1},m,0);
    const auto a=cross(up,forward),b=cross(forward,up);
    for(unsigned i=0;i<s.count;++i)if(s.lights[i].kind==OriginalCourseLightKind::RelativeParallel)s.lights[i].incomingDirection=s.lights[i].sourceSlot?b:a;
}
void updateOriginalHappoLighting(OriginalCourseLighting&s,const OriginalLightMatrix&m,const OriginalLightVector&ref){
    if(s.sourceRow<16||s.sourceRow>19||s.sourceAddress!=0x0c040d60)throw std::invalid_argument("Expected actual Happo light state");
    const bool night=(s.sourceRow&2)!=0,wet=(s.sourceRow&1)!=0;
    if(wet){updateOriginalCourseRelativeDirections(s,m);const float color=night?f(0x3e2e147b):f(0x3f11eb85);
        for(unsigned i=0;i<s.count;++i)if(s.lights[i].kind==OriginalCourseLightKind::RelativeParallel)s.lights[i].color={color,color,color};}
    if(!night)return;
    unsigned first=~0u,second=~0u;float nearest=std::numeric_limits<float>::max(),next=nearest;
    for(unsigned i=0;i<s.count;++i){auto&l=s.lights[i];if(l.kind!=OriginalCourseLightKind::Spot)continue;l.enabled=false;
        const float x=ref[0]-l.position[0],y=ref[1]-l.position[1],z=ref[2]-l.position[2];
        const float distance=std::sqrt(float((double(x)*x+double(y)*y)+double(z)*z));
        if(nearest>distance){next=nearest;second=first;nearest=distance;first=i;}else if(next>distance){next=distance;second=i;}
    }
    if(first<s.count)s.lights[first].enabled=true;if(second<s.count)s.lights[second].enabled=true;
}
OriginalCourseLightingPacket originalCourseLightingPacket(const OriginalCourseLighting&s,const OriginalLightMatrix&m){
    if(s.count>s.lights.size())throw std::invalid_argument("Invalid original light count");
    OriginalCourseLightingPacket out;
    for(unsigned i=0;i<s.count;++i){const auto&l=s.lights[i];if(!l.enabled)continue;
        auto&p=out.lights[out.count];
        const auto d=transform(l.incomingDirection,m,0);
        const auto dx=originalLightDirection12(d[0]),dy=originalLightDirection12(d[1]),dz=originalLightDirection12(d[2]);
        const std::uint32_t x=std::uint16_t(dx)&4095,y=std::uint16_t(dy)&4095,z=std::uint16_t(dz)&4095;
        // Point2059A0 retains only the eight upper direction bits. Its
        // constant angular factor makes direction inert for illumination.
        p[0]=0x08000400u;if(l.kind!=OriginalCourseLightKind::Point)p[0]|=(x&15)<<16|(y&15)<<4|(z&15);
        OriginalLightVector color;for(unsigned j=0;j<3;++j)color[j]=l.color[j]*s.gain;
        p[1]=rgb(color,127.f)<<8|out.count;
        const bool positional=l.kind==OriginalCourseLightKind::Spot||l.kind==OriginalCourseLightKind::Point;
        p[2]=(positional?0x41000000u:0x01000000u)|(x>>4)<<16|(y>>4)<<8|(z>>4);
        if(positional){const auto pos=transform(l.position,m,1);for(unsigned j=0;j<3;++j)p[3+j]=bits(pos[j]);p[6]=l.coefficientWords[0];p[7]=l.kind==OriginalCourseLightKind::Point?0x00003f80u:angularCoefficient(l,dx,dy,dz);}
        ++out.count;
    }
    const std::uint32_t mask=(1u<<out.count)-1,ambient=0xff000000u|rgb(s.ambient,255.f);
    out.glm={0x08000400,0x000f00b0,mask|(mask<<16),ambient,0xff000000,mask|(mask<<16),ambient,0xff000000};
    return out;
}
OriginalIrohazakaLightUpdate updateOriginalIrohazakaLighting(OriginalCourseLighting&s,const OriginalLightMatrix&m,const OriginalLightVector&ref){
    if(s.sourceRow<20||s.sourceRow>23)throw std::invalid_argument("Expected Irohazaka lighting state");
    updateOriginalCourseRelativeDirections(s,m);OriginalIrohazakaLightUpdate result;
    if(!(s.sourceRow&2))return result;
    std::array<OriginalCourseLight*,4> spots{};
    for(unsigned i=0;i<s.count;++i)if(s.lights[i].kind==OriginalCourseLightKind::Spot&&s.lights[i].sourceSlot<4)spots[s.lights[i].sourceSlot]=&s.lights[i];
    if(std::any_of(spots.begin(),spots.end(),[](auto*p){return p==nullptr;}))return result;
    std::array<std::pair<float,unsigned>,31> distances;
    for(unsigned i=0;i<31;++i){const auto&w=originalIrohazakaLightPositionWords[i];const float x=ref[0]-f(w[0]),y=ref[1]-f(w[1]),z=ref[2]-f(w[2]);
        distances[i]={float((double(x)*x+double(y)*y)+double(z)*z),i};}
    std::stable_sort(distances.begin(),distances.end(),[](auto a,auto b){return a.first<b.first;});
    for(unsigned i=0;i<4;++i){const auto index=distances[i].second;result.authoredIndices[i]=index;const auto&w=originalIrohazakaLightPositionWords[index];for(unsigned j=0;j<3;++j)spots[i]->position[j]=f(w[j]);
        if(index<4)spots[index]->enabled=true;else result.sourceEnableIndexOutsideFourSlots=true;}
    result.coordinatesUpdated=true;return result;
}
OriginalIrohazakaLightUpdate updateOriginalCourseLighting(OriginalCourseLighting&s,const OriginalLightMatrix&m,const OriginalLightVector&ref){
    if(s.sourceRow>=36)throw std::invalid_argument("Invalid original course light row");
    if(s.sourceRow/4==4){updateOriginalHappoLighting(s,m,ref);return {};}
    if(s.sourceRow/4==5)return updateOriginalIrohazakaLighting(s,m,ref);
    updateOriginalCourseRelativeDirections(s,m);return {};
}
} // namespace idas3::original
