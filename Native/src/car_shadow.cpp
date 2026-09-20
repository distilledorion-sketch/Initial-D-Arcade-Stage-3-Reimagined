#include "car_shadow.h"
#include "renderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace idas3 {
bool appendCarContactShadow(Mesh& mesh,const CarShadowFootprint& footprint){
    const auto finite=[](Vec3 point){return std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z);};
    for(auto point:footprint.roadPoints)if(!finite(point))return false;
    for(float value:{footprint.widthScale,footprint.lengthScale,footprint.opacity,footprint.surfaceLift,footprint.separation})
        if(!std::isfinite(value))return false;
    if(footprint.widthScale<=0||footprint.lengthScale<=0)return false;
    const auto& p=footprint.roadPoints;
    const Vec3 across=((p[1]-p[0])+(p[3]-p[2]))*.5f;
    const Vec3 along=((p[0]-p[2])+(p[1]-p[3]))*.5f;
    if(length(cross(across,along))<.01f)return false;
    const float airborne=std::clamp(footprint.separation/1.5f,0.0f,1.0f);
    const float opacity=std::clamp(footprint.opacity,0.0f,.65f)*(1-airborne*airborne*(3-2*airborne));
    if(opacity<=.001f)return false;
    const float width=std::clamp(footprint.widthScale,.1f,2.0f);
    const float depth=std::clamp(footprint.lengthScale,.1f,2.5f);
    const float lift=std::clamp(footprint.surfaceLift,.002f,.03f);
    const auto vertex=[&](float x,float z,float alpha){
        const float u=.5f+.5f*x*width,v=.5f+.5f*z*depth;
        const Vec3 position=lerp(lerp(p[2],p[3],u),lerp(p[0],p[1],u),v);
        const Vec3 du=lerp(p[3]-p[2],p[1]-p[0],v);
        const Vec3 dv=lerp(p[0]-p[2],p[1]-p[3],u);
        Vec3 normal=cross(dv,du);
        if(length(normal)<1e-5f)normal=cross(along,across);
        normal=normalized(normal);if(normal.y<0)normal=-normal;
        return Vertex{position+normal*lift,normal,{.018f,.022f,.029f,alpha*opacity}};
    };
    constexpr std::size_t segments=32,rings=5;
    constexpr std::array<float,rings> radii{.35f,.60f,.78f,.91f,1.0f};
    constexpr std::array<float,rings> alphas{.92f,.70f,.42f,.13f,0.0f};
    std::array<std::array<Vertex,segments>,rings> vertices;
    for(std::size_t ring=0;ring<rings;++ring)for(std::size_t segment=0;segment<segments;++segment){
        const float angle=2*pi*float(segment)/float(segments);
        // A mildly rounded rectangular footprint follows the body more closely
        // than a circular blob, while every boundary vertex remains transparent.
        const auto rounded=[](float value){return std::copysign(std::pow(std::abs(value),.80f),value);};
        vertices[ring][segment]=vertex(radii[ring]*rounded(std::cos(angle)),radii[ring]*rounded(std::sin(angle)),alphas[ring]);
    }
    constexpr std::uint32_t count=std::uint32_t(segments*3+(rings-1)*segments*6);
    if(mesh.vertices.size()>(std::numeric_limits<std::uint32_t>::max)()-count)return false;
    // Route through the existing explicit material-state path. PCW list2 is
    // translucent; TSP uses alpha/inverse-alpha blending and disables fog;
    // ISP compare6 maps to LESS_EQUAL and bit26 disables depth writes.
    constexpr std::uint32_t tsp=(4u<<29)|(5u<<26)|(1u<<20)|(2u<<22);
    constexpr std::uint32_t pcw=(2u<<24)|2u,isp=(6u<<29)|(1u<<26),gmp=512u;
    const auto first=std::uint32_t(mesh.vertices.size());
    mesh.vertices.reserve(mesh.vertices.size()+count);
    const auto triangle=[&](Vertex a,Vertex b,Vertex c){
        if(dot(cross(b.position-a.position,c.position-a.position),a.normal+b.normal+c.normal)<0)std::swap(b,c);
        mesh.vertices.push_back(a);mesh.vertices.push_back(b);mesh.vertices.push_back(c);
    };
    const auto center=vertex(0,0,1);
    for(std::size_t segment=0;segment<segments;++segment){
        const auto next=(segment+1)%segments;
        triangle(center,vertices[0][segment],vertices[0][next]);
        for(std::size_t ring=1;ring<rings;++ring){
            triangle(vertices[ring-1][segment],vertices[ring][segment],vertices[ring][next]);
            triangle(vertices[ring-1][segment],vertices[ring][next],vertices[ring-1][next]);
        }
    }
    mesh.ranges.push_back({first,count,0xffffffffu,tsp,pcw,isp,gmp,true});
    return true;
}
} // namespace idas3
