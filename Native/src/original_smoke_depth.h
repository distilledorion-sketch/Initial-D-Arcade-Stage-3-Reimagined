#pragma once
#include "original_matrix.h"
#include <cmath>
#include <stdexcept>

namespace idas3::original {
struct OriginalSmokeFootprintDepth {
    std::array<std::array<float,3>,4> cameraCorners{};
    std::array<float,3> cameraCenter{};
    std::array<unsigned,4> nearDepth{};
    float depth=0;
};
// CarSmokeDisp::depth (140BC0). The emitter retains the owner's world matrix;
// its footprint decides whether each wheel's cloud uses its near or far Z.
// Do not replace this with an average depth or a camera-facing wheel test.
inline OriginalSmokeFootprintDepth originalSmokeFootprintDepth(
        const OriginalMatrix& world,const OriginalMatrix& camera,float halfWidth,float halfLength,
        unsigned effectIndex,float minZ,float maxZ){
    if(effectIndex>=4)throw std::invalid_argument("Original smoke effect index");
    OriginalSmokeFootprintDepth result;
    const std::array<std::array<float,4>,4> local{{
        {halfWidth,0,halfLength,1},{-halfWidth,0,halfLength,1},
        {-halfWidth,0,-halfLength,1},{halfWidth,0,-halfLength,1}}};
    for(unsigned i=0;i<4;++i){
        const auto point=transformOriginalVector(world,local[i]);
        // 1F6260 stores XYZ, then 1FD060 supplies a fresh homogeneous one.
        const auto viewed=transformOriginalVector(camera,{point[0],point[1],point[2],1.f});
        for(unsigned j=0;j<3;++j)result.cameraCorners[i][j]=viewed[j];
    }
    const auto center=transformOriginalVector(camera,
        {world.elements[12],world.elements[13],world.elements[14],1.f});
    for(unsigned j=0;j<3;++j)result.cameraCenter[j]=center[j];
    for(unsigned i=0;i<4;++i){
        const unsigned next=(i+1)%4;
        const float x=result.cameraCorners[next][0]-result.cameraCorners[i][0];
        const float z=result.cameraCorners[next][2]-result.cameraCorners[i][2];
        // Y of 1F6BC0(edge, cameraCenter), with the original FMAC rounding.
        const float facing=std::fma(z,result.cameraCenter[0],-(x*result.cameraCenter[2]));
        if(facing<0.f){result.nearDepth[i]=1;result.nearDepth[next]=1;}
    }
    std::swap(result.nearDepth[2],result.nearDepth[3]);
    result.depth=result.nearDepth[effectIndex]?maxZ:minZ;
    return result;
}
}
