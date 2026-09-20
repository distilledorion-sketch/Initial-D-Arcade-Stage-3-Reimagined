#include "original_race_path.h"
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace idas3::original {
namespace {
OriginalRacePoint inwardNormal(OriginalRacePoint from,OriginalRacePoint to){
    const float x=to[0]-from[0],z=to[2]-from[2];
    //1F6CF0: Y is cleared before FIPR, ordered double accumulation and a
    // single F32 result, followed by FSRRA and three separate multiplications.
    double squared=double(x)*double(x);squared+=double(0.0f)*double(0.0f);
    squared+=double(z)*double(z);squared+=double(0.0f)*double(0.0f);
    const float inverse=1.0f/std::sqrt(static_cast<float>(squared));
    const float nx=x*inverse,ny=0.0f*inverse,nz=z*inverse;
    return {nz,ny,-nx};
}
std::vector<OriginalRacePoint> readPoints(const std::filesystem::path& p){
    std::ifstream f(p,std::ios::binary);std::uint32_t count{},components{};
    f.read(reinterpret_cast<char*>(&count),4);f.read(reinterpret_cast<char*>(&components),4);
    if(!f||components!=3||count<21||count>1000000)throw std::runtime_error("Invalid original race path: "+p.string());
    std::vector<OriginalRacePoint> result(count);
    f.read(reinterpret_cast<char*>(result.data()),std::streamsize(count)*12);
    if(!f)throw std::runtime_error("Truncated original race path: "+p.string());
    return result;
}
}
OriginalRacePath::OriginalRacePath(std::vector<OriginalRacePoint> center,
    std::vector<OriginalRacePoint> left,std::vector<OriginalRacePoint> right,bool reverse)
    :center_(std::move(center)),reverse_(reverse){
    if(center_.size()<21||center_.size()!=left.size()||left.size()!=right.size()||
        center_.size()>std::size_t(std::numeric_limits<std::int32_t>::max()))
        throw std::invalid_argument("Original race path streams require equal counts and at least20 cells");
    cells_.resize(center_.size()-1);
    for(std::size_t i=0;i<cells_.size();++i){
        const std::array<OriginalRacePoint,4> q{left[i],right[i],right[i+1],left[i+1]};
        for(std::size_t j=0;j<4;++j)cells_[i].planes[j]={q[j],inwardNormal(q[j],q[(j+1)%4])};
    }
}
OriginalRacePath OriginalRacePath::load(const std::filesystem::path& root,std::uint32_t condition){
    static constexpr std::array<const char*,9> ids{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df"};
    if(condition>=18)throw std::invalid_argument("Original race path condition must be0..17");
    const auto prefix=root/"data/courses"/ids[condition/2];
    return {readPoints(prefix.string()+"_path.bin"),readPoints(prefix.string()+"_path_l.bin"),
        readPoints(prefix.string()+"_path_r.bin"),(condition&1)!=0};
}
bool OriginalRacePath::contains(std::int32_t index,OriginalRacePoint p,std::array<float,4>& d)const{
    for(std::size_t j=0;j<4;++j){
        const auto& plane=cells_[std::size_t(index)].planes[j];
        const float x=p[0]-plane.anchor[0],y=p[1]-plane.anchor[1],z=p[2]-plane.anchor[2];
        float value=plane.normal[1]*y;
        value=std::fma(plane.normal[0],x,value);
        d[j]=std::fma(plane.normal[2],z,value);
    }
    //OriginalFCMP/GT tests zero>distance; preserve its signed-zero semantics.
    return !(0.0f>d[0]||0.0f>d[1]||0.0f>d[2]||0.0f>d[3]);
}
bool OriginalRacePath::project(OriginalRacePoint p,OriginalPathCoordinate& out,bool fullSearch)const{
    const auto count=period();
    if(count<20||out.index>=count||out.index< -1||(reverse_&&out.index<0))
        throw std::invalid_argument("Original race projection previous coordinate is outside its source memory contract");
    std::int32_t candidate=reverse_?count-out.index-1:out.index;
    std::array<float,4> d{};
    bool found=candidate>=0&&contains(candidate,p,d);
    if(candidate>=0&&!found){
        const auto previous=candidate;
        for(std::int32_t offset=0;offset<20;++offset){
            auto next=previous+offset;if(next>=count)next-=count;
            if(contains(next,p,d)){candidate=next;found=true;break;}
            next=previous-offset;if(next<0)next+=count;
            if(contains(next,p,d)){candidate=next;found=true;break;}
        }
    }
    if(!found&&fullSearch)for(std::int32_t i=0;i<count;++i)
        if(contains(i,p,d)){candidate=i;found=true;break;}
    if(!found)return false;
    const float total=d[2]+d[0];
    const float fraction=d[0]/total;
    out.index=reverse_?count-candidate-1:candidate;
    out.fraction=reverse_?1.0f-fraction:fraction;
    return true;
}
bool advanceOriginalRacePathHistory(const OriginalRacePath& path,OriginalRacePathHistory& h,
    OriginalRacePoint position,std::uint32_t time,bool fullSearch){
    const auto next=h.activeIndex==0?1u:0u,previous=next==0?1u:0u;
    h.activeIndex=next;h.positions[next]=position;h.coordinates[next]=h.coordinates[previous];
    const bool found=path.project(position,h.coordinates[next],fullSearch);
    advanceOriginalRaceProgress(h.progress,h.coordinates[previous],h.coordinates[next],path.period());
    h.publishedCoordinate=h.coordinates[next];h.sampleTimes[next]=time;
    return found;
}
} // namespace idas3::original
