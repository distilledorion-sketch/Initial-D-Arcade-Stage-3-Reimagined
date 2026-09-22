#pragma once
#include "native_assets.h"
#include <map>
#include <set>
#include <tuple>
namespace idas3 {
// Static ceiling triangles, indexed once at course load. Queries do not
// allocate or inspect the scene mesh. Coordinates match world rendering.
class WeatherShelter {
    struct Triangle {Vec3 a,b,c;float denominator;};
    std::vector<Triangle> triangles_;
    std::map<std::pair<int,int>,std::vector<unsigned>> cells_;
    static int cell(float x){return int(std::floor(x/16.f));}
public:
    void clear(){triangles_.clear();cells_.clear();}
    std::size_t size()const{return triangles_.size();}
    void build(const NativeModel& model,std::span<const NativeAssembly> assemblies){
        clear();std::set<std::pair<unsigned,std::array<float,16>>> seen;
        for(const auto& assembly:assemblies)for(const auto& instance:assembly.instances){
            if(instance.billboard||!seen.emplace(instance.chunk,instance.transform).second)continue;
            const auto& m=instance.transform;
            const auto point=[&](Vec3 p){return Vec3{m[0]*p.x+m[1]*p.y+m[2]*p.z+m[3],m[4]*p.x+m[5]*p.y+m[6]*p.z+m[7],m[8]*p.x+m[9]*p.y+m[10]*p.z+m[11]};};
            for(const auto& batch:model.chunks.at(instance.chunk).batches){
                if(((batch.ich[0]>>24)&7)!=0)continue; // no foliage cutouts/effects
                for(std::size_t i=0;i+2<batch.indices.size();i+=3){
                    const auto& va=batch.vertices[batch.indices[i]];const auto& vb=batch.vertices[batch.indices[i+1]];const auto& vc=batch.vertices[batch.indices[i+2]];
                    const auto a=point(va.position),b=point(vb.position),c=point(vc.position);
                    const auto geometric=cross(b-a,c-a);if(std::abs(geometric.y)<length(geometric)*.15f)continue;
                    const float denominator=(b.z-c.z)*(a.x-c.x)+(c.x-b.x)*(a.z-c.z);
                    if(std::abs(denominator)<1e-5f)continue;
                    const unsigned index=unsigned(triangles_.size());triangles_.push_back({a,b,c,denominator});
                    for(int x=cell(std::min({a.x,b.x,c.x}));x<=cell(std::max({a.x,b.x,c.x}));++x)
                    for(int z=cell(std::min({a.z,b.z,c.z}));z<=cell(std::max({a.z,b.z,c.z}));++z)cells_[{x,z}].push_back(index);
                }
            }
        }
    }
    bool covered(Vec3 p)const{
        const auto found=cells_.find({cell(p.x),cell(p.z)});if(found==cells_.end())return false;
        for(auto i:found->second){const auto& t=triangles_[i];
            const float a=((t.b.z-t.c.z)*(p.x-t.c.x)+(t.c.x-t.b.x)*(p.z-t.c.z))/t.denominator;
            const float b=((t.c.z-t.a.z)*(p.x-t.c.x)+(t.a.x-t.c.x)*(p.z-t.c.z))/t.denominator,c=1-a-b;
            if(a<-.0001f||b<-.0001f||c<-.0001f)continue;
            const float roof=a*t.a.y+b*t.b.y+c*t.c.y;
            if(p.y<=roof+.05f)return true;
        }
        return false;
    }
};
}
