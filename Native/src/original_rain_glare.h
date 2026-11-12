#pragma once
#include "original_rain.h"
#include <stdexcept>

namespace idas3::original {
struct OriginalRainGlareHit { bool hit=false;unsigned flags=0; };
struct OriginalRainGlareDraw {
    std::array<float,3> position{};
    OriginalMatrix viewMatrix{};
    unsigned alpha=0;
    bool draw=false;
};
struct OriginalRainGlareCar {
    bool available=false,blindAttack=false,braking=false;
    OriginalMatrix world=originalIdentityMatrix();
    unsigned rearCount=0;
    std::array<std::array<float,3>,4> rearAnchors{};
};
//156BA0 tests ONE plane at view-owner+192, then returns -cameraZ. It is
// not the four-plane1D0B60 used by smoke and the ordinary lamp trails.
inline float originalRainGlareDepth(const OriginalMatrix& view,
        const std::array<float,3>& plane,const std::array<float,3>& position){
    std::array<float,3> camera{};const auto& m=view.elements;
    for(unsigned row=0;row<3;++row){
        float value=m[row+4]*position[1];
        value=std::fma(position[0],m[row],value);
        value=std::fma(position[2],m[row+8],value);
        camera[row]=value+m[row+12];
    }
    float distance=plane[1]*camera[1];distance=std::fma(plane[0],camera[0],distance);
    distance=std::fma(camera[2],plane[2],distance);distance+=100.05f;
    return distance<0?-1.f:-camera[2];
}
inline unsigned originalRainGlareLevel(unsigned night,const OriginalRainGlareCar& car){
    return unsigned(night==1&&!car.blindAttack)+unsigned(car.braking);
}
//17E940 changes both material banks, preserving the remaining authored words.
inline void originalRainGlareMaterial(std::array<unsigned,8>& ich,
        std::array<unsigned,16>& material,unsigned alpha){
    ich[2]|=0x001000c0;ich[4]|=0x001000c0;
    material[3]=material[5]=(alpha<<24)|0xffffffu;
}
//17F260. Query must perform the original swept query and return its flag word.
// Random is consumed once for every issued draw, including alpha-zero draws.
template<class Query,class RandomFloat> OriginalRainGlareDraw originalRainGlare(
        const OriginalMatrix& view,const std::array<float,3>& plane,
        const OriginalMatrix& carWorld,std::array<float,3> anchor,
        const std::array<float,3>& eye,unsigned level,Query&& query,RandomFloat&& random){
    OriginalRainGlareDraw out;
    auto lampWorld=carWorld;translateOriginalMatrix(lampWorld,anchor);
    for(unsigned axis=0;axis<3;++axis)out.position[axis]=lampWorld.elements[12+axis];
    translateOriginalMatrix(lampWorld,{0,0,-.1f});
    std::array<float,3> start{};for(unsigned axis=0;axis<3;++axis)start[axis]=lampWorld.elements[12+axis];
    if(originalRainGlareDepth(view,plane,out.position)<0)return out;
    const float dx=start[0]-eye[0],dz=start[2]-eye[2];
    if(std::fma(dx,dx,dz*dz)>10000.f)return out;
    const auto hit=query(start,eye);if(!hit.hit||(hit.flags&0x8000))return out;
    std::array<float,3> direction{};for(unsigned axis=0;axis<3;++axis)direction[axis]=eye[axis]-out.position[axis];
    double squared=double(direction[0])*double(direction[0]);
    squared+=double(direction[1])*double(direction[1]);squared+=double(direction[2])*double(direction[2]);squared+=0.0;
    const float length=std::sqrt(float(squared)),inverse=1.f/length;
    for(unsigned axis=0;axis<3;++axis){
        direction[axis]*=inverse;
        const float displacement=direction[axis]*length;
        out.position[axis]=std::fma(displacement,.6f,out.position[axis]);
    }
    double dot=double(-carWorld.elements[8])*double(direction[0]);
    dot+=double(-carWorld.elements[9])*double(direction[1]);
    dot+=double(-carWorld.elements[10])*double(direction[2]);dot+=0.0;
    const float facing=float(dot);if(facing<0)return out;
    // The delayed branch at17F296 selects70 for level1,110 for level2.
    const float factor=level==0?0.f:level==1?70.f:110.f;
    const float alpha=factor*(facing*facing);
    const auto ftrc=[](float value){
        if(std::isnan(value)||value<=-2147483648.f)return 0x80000000u;
        if(value>=2147483648.f)return 0x7fffffffu;
        return unsigned(std::int32_t(value));
    };
    //222300 implements unsigned conversion using a second FTRC above2^31.
    out.alpha=2147483648.f>alpha?ftrc(alpha):0x80000000u+ftrc(alpha-2147483648.f);
    const float jitter=random()*.12f-.06f;
    out.viewMatrix=originalRainBillboardMatrix(view,out.position);
    scaleOriginalMatrix(out.viewMatrix,{jitter+.15f,.2f,.2f});out.draw=true;
    return out;
}
//17F500: the primary/rival anchor height factors intentionally differ. The
// owner supplies surface gates; draw order remains primary then rival lamps.
template<class Query,class RandomFloat,class Publish> void originalRainRearGlare(
        const OriginalMatrix& view,const std::array<float,3>& plane,unsigned night,
        const std::array<OriginalRainGlareCar,2>& cars,const std::array<bool,2>& allowed,
        Query&& query,RandomFloat&& random,Publish&& publish){
    if(!cars[0].available)throw std::invalid_argument("Rain glare requires the primary car");
    auto inverse=view;invertOriginalMatrix(inverse);
    const std::array<float,3> eye{inverse.elements[12],inverse.elements[13],inverse.elements[14]};
    for(unsigned car=0;car<2;++car)if(cars[car].available&&allowed[car]){
        if(cars[car].rearCount>4)throw std::invalid_argument("Invalid original rear lamp count");
        for(unsigned lamp=0;lamp<cars[car].rearCount;++lamp){
            auto anchor=cars[car].rearAnchors[lamp];anchor[1]*=car==0?.4f:.5f;
            const auto result=originalRainGlare(view,plane,cars[car].world,anchor,eye,
                originalRainGlareLevel(night,cars[car]),query,random);
            if(result.draw)publish(car,lamp,7+car,result);
        }
    }
}
}
