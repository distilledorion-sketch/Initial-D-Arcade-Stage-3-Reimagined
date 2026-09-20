#include "original_aura.h"
#include <bit>
#include <fstream>
#include <stdexcept>

namespace idas3::original {
namespace {
constexpr std::array<unsigned,10> tiers{1,1,2,2,3,3,4,5,6,7}; //323B88
constexpr std::array<const char*,10> names{"blue","green","crimson","orange","red","yellow","purple","cyan","white","spa"};
std::vector<std::uint8_t> read(const std::filesystem::path& path,std::size_t expected){
    std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Original aura asset missing: "+path.string());
    std::vector<std::uint8_t> b{std::istreambuf_iterator<char>(f),{}};
    if(b.size()!=expected)throw std::runtime_error("Original aura asset has unexpected size");return b;
}
std::uint32_t little(const std::uint8_t* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
}
OriginalAuraStyle originalAuraStyle(std::uint32_t level,std::uint32_t streak,bool opponent){
    // The original compares these fields as signed32. Host network validation
    // restricts the ordinary range; preserve the source predicate here too.
    const auto l=std::bit_cast<std::int32_t>(level),s=std::bit_cast<std::int32_t>(streak);
    if(l<=10)return {};
    if(l<=20){
        const float growth=std::fma(float(l-11),opponent?.05f:.035f,.65f);
        return {true,0,std::clamp(growth,0.f,1.f)};
    }
    return {true,l<=30?tiers[unsigned(l-21)]:(s>9?9u:8u),1.f};
}
unsigned originalAuraColorFrame(std::uint32_t frame){return (frame%60)/2;}
void OriginalAura::load(const std::filesystem::path& root){
    loaded_=false;validPose_=false;
    const auto dir=root/"data/original_assets/aura";
    authored_=NativeModel::load(dir/"aura.idasmesh");
    if(authored_.chunks.size()!=1||authored_.chunks[0].batches.size()!=1||authored_.chunks[0].batches[0].vertices.size()!=3474||authored_.chunks[0].batches[0].indices.size()!=10134)
        throw std::runtime_error("Original aura model identity mismatch");
    model_=authored_;
    auto& batch=model_.chunks[0].batches[0];
    //17B44C..17B478 builds a fresh VUR mesh, then replaces its ISP/TSP/GMP.
    // Preserve the source submission rather than the import's editing header.
    batch.ich={0x8a00072fu,0x91c00000u,0x849804c0u,0x04000000u,0x941824c0u,0x04000000u,0x4au,3474u};
    batch.material={0x08000500u,0x6868u,0x6aau,0xffffffffu,0u,0xffffffffu,0u,0u,
        0x08000000u,0xffffffffu,0x08000000u,0xffffffffu,0x08000000u,0xffffffffu,0x08000000u,0xffffffffu};
    const auto mapping=read(dir/"vertex_color_indices.bin",3474*4);
    colorIndices_.resize(3474);
    for(unsigned i=0;i<3474;++i){colorIndices_[i]=little(&mapping[i*4]);if(colorIndices_[i]>=1788)throw std::runtime_error("Original aura color index out of range");}
    const auto dimensions=read(dir/"car_dimensions.bin",35*44);
    for(unsigned i=0;i<35;++i){carHalfLengths_[i]=std::bit_cast<float>(little(&dimensions[i*44+8]));if(!std::isfinite(carHalfLengths_[i])||carHalfLengths_[i]<=0)throw std::runtime_error("Original aura car dimension invalid");}
    for(unsigned p=0;p<names.size();++p){
        const auto bytes=read(dir/"colors"/(std::string(names[p])+"_vtx.bin"),30*1788*4);
        auto& colors=colors_[p];colors.resize(30*1788);
        //17B830..17B856 explicitly assembles the file's A,R,G,B bytes.
        for(unsigned i=0;i<colors.size();++i){const auto* b=&bytes[i*4];colors[i]=(std::uint32_t(b[0])<<24)|(std::uint32_t(b[1])<<16)|(std::uint32_t(b[2])<<8)|b[3];}
    }
    assembly_.instances={{0,{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},true}};
    loaded_=true;configure(0,0);
}
void OriginalAura::configure(std::uint32_t level,std::uint32_t streak,bool opponent){
    style_=originalAuraStyle(level,streak,opponent);opponent_=opponent;validPose_=false;
    colorFrame_=publishedPalette_=~0u;colorArgb_=0;
    if(!loaded_)return;
    auto& dst=model_.chunks[0].batches[0].vertices;const auto& src=authored_.chunks[0].batches[0].vertices;
    for(unsigned i=0;i<dst.size();++i){
        //17B9D0/17BA82: X,Z=growth; Y=growth+0.1. Existing billboard
        // publication carries local vertices verbatim, so bake only this scale.
        dst[i].position={src[i].position.x*style_.growth,src[i].position.y*(style_.growth+.1f),src[i].position.z*style_.growth};
    }
}
void OriginalAura::update(std::uint32_t frame,unsigned carId,Vec3 car,Vec3 eye){
    validPose_=loaded_&&style_.visible&&carId<carHalfLengths_.size()&&finite(car)&&finite(eye);
    if(!validPose_)return;
    //17B98A..17B9B0 subtracts eye FROM car, normalizes in3D, scales
    //by90% of that car's half-length, then discards vertical displacement.
    const auto delta=car-eye;const float distance=length(delta);
    Vec3 displacement{};
    if(distance>0&&std::isfinite(distance))displacement=delta*((carHalfLengths_[carId]*.9f)/distance);
    const Vec3 anchor=car+Vec3{displacement.x,0,displacement.z};
    auto& transform=assembly_.instances[0].transform;
    transform[3]=anchor.x;transform[7]=anchor.y;transform[11]=anchor.z;
    //Source alternates local/effect uploads on even/odd ticks. Both select
    //floor(tick/2)%30; retaining a frame for its second tick is equivalent.
    const unsigned selected=originalAuraColorFrame(frame);
    if(selected==colorFrame_&&style_.palette==publishedPalette_)return;
    colorFrame_=selected;publishedPalette_=style_.palette;
    auto& vertices=model_.chunks[0].batches[0].vertices;const auto& colors=colors_[style_.palette];
    colorArgb_=0;
    for(unsigned i=0;i<vertices.size();++i){const auto color=colors[colorFrame_*1788+colorIndices_[i]];vertices[i].color0=vertices[i].color1=color;
        if((color>>24)>(colorArgb_>>24))colorArgb_=color;}
}
} // namespace idas3::original
