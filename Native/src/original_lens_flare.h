#pragma once
#include "original_matrix.h"
#include "original_rival_dialog.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <span>

namespace idas3::original {
struct OriginalLensFlarePiece {float distance=0,size=0;std::uint32_t chunk=0;};
inline constexpr std::array<float,3> originalLensFlareAnchor{
    std::bit_cast<float>(0xc04ccccdu),std::bit_cast<float>(0x40833334u),
    std::bit_cast<float>(0xbe99999au)};
//180640..1806B8: divide in software double, multiply by the original
//0x1000 tangent, then narrow once. The authored Y is one ULP above4.1f.
inline float originalLensFlareAnchorScale(){
    const float tangent=originalDialogTangent(0x1000);
    return float((double(-originalLensFlareAnchor[2])/2.4)*double(tangent));
}
//180A00 fills only the projected Z axis and translation. Its homogeneous Z
//word is the exception descriptor left at the same stack slot by180B00;
//180BB6 always writes0C38124C there on either admitted direction branch.
//The first two columns are irrelevant: every projected point has X=Y=0.
inline OriginalMatrix originalLensFlareAxis(const std::array<float,3>& direction){
    auto translated=originalIdentityMatrix();translateOriginalMatrix(translated,{0,0,-10});
    const auto axis=transformOriginalVector(translated,{direction[0],direction[1],direction[2],
        std::bit_cast<float>(0x0c38124cu)});
    const auto offset=transformOriginalVector(translated,{1,1,1,1});
    OriginalMatrix result;
    for(unsigned i=0;i<4;++i){result.elements[8+i]=axis[i];result.elements[12+i]=offset[i];}
    return result;
}
//180B00 uses the integer ACar+9DC road segment, not distance or camera
//position. The recovered table includes its final segment (last+1 entries).
inline bool originalLensFlareRoadVisible(std::span<const std::uint8_t> table,
        std::int32_t segment,bool reverse){
    if(segment<0||std::size_t(segment)>=table.size())return false;
    const auto index=reverse?table.size()-1-std::size_t(segment):std::size_t(segment);
    return table[index]!=0;
}
//180C84..180CD8: retain camera height, remove horizontal translation, then
//normalize the sun in that view. Both inversions use the original LU path;
//an ordinary host look-at or normalized world-space dot changes its rounding.
inline std::array<float,3> originalLensFlareDirection(OriginalMatrix view,
        const std::array<float,3>& sun){
    if(invertOriginalMatrix(view)==0)return {};
    view.elements[12]=view.elements[14]=0;
    if(invertOriginalMatrix(view)==0)return {};
    auto direction=transformOriginalPoint(view,sun);
    double squared=double(direction[0])*double(direction[0]);
    squared+=double(direction[1])*double(direction[1]);
    squared+=double(direction[2])*double(direction[2]);squared+=0.0;
    const float inverse=1.f/std::sqrt(float(squared));
    for(auto& component:direction)component*=inverse;
    return direction;
}
inline bool originalLensFlareFacing(const std::array<float,3>& direction){
    return std::isfinite(direction[2])&&direction[2]<=-.2f;
}
//180CFA..180D1E. Alpha is later truncated to a byte by180A60.
inline float originalLensFlareStrength(const std::array<float,3>& direction){
    return std::min(1.f,(.2f-direction[2])/1.2f)*.8f;
}
inline std::uint32_t originalLensFlareColor(std::uint32_t authored,float strength){
    return (authored&0x00ffffffu)|(std::uint32_t(strength*255.f)<<24);
}
// Source1D0940 -> ending field-of-view phase ->1D09E0/1D0880. Fixed
// projection for the authored ending camera, not a host tangent approximation.
inline OriginalMatrix originalEndingLensFlareProjection(){
    constexpr std::array<std::uint32_t,16> words{
        0x3fa64a05,0,0,0,0,0x3fddb807,0,0,0,0,0xbf800010,0xbf800000,0,0,0xbca3d715,0};
    OriginalMatrix out;for(unsigned i=0;i<16;++i)out.elements[i]=std::bit_cast<float>(words[i]);return out;
}
// The ending's640x480 source viewport.1FE100 keeps the half-depth mapping
// even though screen placement consumes X/Y and homogeneous W only.
inline OriginalMatrix originalLensFlareViewport(){
    OriginalMatrix out;out.elements={320,0,0,0,0,-240,0,0,0,0,-.5f,0,320,240,.5f,1};return out;
}
//1FD3E0 then1FE480: each matrix multiplication rounds its FTRV result
//before the next stage. Return screen X/Y and reciprocal homogeneous W.
inline std::array<float,3> originalLensFlareScreenPoint(
        const std::array<float,3>& direction,float distance,
        const OriginalMatrix& projection,const OriginalMatrix& viewport){
    const auto axis=originalLensFlareAxis(direction);
    OriginalMatrix screen;
    for(unsigned column=2;column<4;++column){
        const unsigned at=column*4;
        const auto projected=transformOriginalVector(projection,
            {axis.elements[at],axis.elements[at+1],axis.elements[at+2],axis.elements[at+3]});
        const auto pixels=transformOriginalVector(viewport,projected);
        for(unsigned row=0;row<4;++row)screen.elements[at+row]=pixels[row];
    }
    const auto point=transformOriginalVector(screen,{0,0,-distance,1});
    const float reciprocal=1.f/point[3];
    return {point[0]*reciprocal,point[1]*reciprocal,reciprocal};
}
//180D98..180DF4: screen placement, source axis reset, quarter-turn, then
//the two separate size multiplications. This is a view-space model matrix.
inline OriginalMatrix originalLensFlarePieceMatrix(const std::array<float,3>& screen,
        unsigned piece,float size,float strength,const OriginalFscaTable& trig){
    auto result=originalIdentityMatrix();const float anchorScale=originalLensFlareAnchorScale();
    scaleOriginalMatrix(result,{anchorScale,anchorScale,1});
    translateOriginalMatrix(result,originalLensFlareAnchor);
    translateOriginalMatrix(result,{screen[0]*.01f,(-screen[1])*.01f,float(piece)*.001f});
    for(unsigned column=0;column<3;++column)for(unsigned row=0;row<3;++row)
        result.elements[column*4+row]=column==row?1.f:0.f;
    rotateOriginalMatrixPhase(result,0,0x4000,trig);
    scaleOriginalMatrix(result,{.03f,.03f,.03f});
    const float finalScale=size*strength;
    scaleOriginalMatrix(result,{finalScale,finalScale,finalScale});
    return result;
}
}
