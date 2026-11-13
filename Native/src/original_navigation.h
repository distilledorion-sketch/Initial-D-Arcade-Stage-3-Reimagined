#pragma once
#include "native_assets.h"
#include "course.h"
#include "navigation_artwork.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace idas3 {
// Navigation's own UV path, artwork and view extents. These are independent
// of road width and of the car's steering angle. Source owners: 0C0C1FC0,
// 0C0C24C0, 0C0C3240; per-course table 0C2FB118 (36-byte records).
class OriginalNavigation {
public:
    struct Point {
        float u=0,v=0,du=0,dv=1;
        std::array<float,2> offset(const Point& other)const {
            const float x=other.u-u,y=other.v-v;
            // Navigation UV is world +X/-Z. The game's right-handed view
            // therefore has screen-right (dv,-du), not (-dv,du).
            return {dv*x-du*y,-(du*x+dv*y)};
        }
    };
    struct Bank {
        NativeModel model;
        NativeTextureBank textures;
        std::vector<std::array<float,5>> path;
        std::vector<float> cumulative;
        std::vector<std::array<float,2>> tangents;
        float extent=0;
        bool closed=false;
        unsigned day=0,night=1,player=6,rival=7,frame=5;
        NativeModelChunk window;
        Point sample(float fraction,bool reverse)const {
            // Myogi/Usui race coordinates can include completed laps. Their
            // marker must keep circulating instead of sticking at the finish.
            if(closed)fraction-=std::floor(fraction);
            fraction=std::clamp(fraction,0.f,1.f);
            if(reverse)fraction=1.f-fraction;
            const float distance=fraction*cumulative.back();
            auto hi=std::upper_bound(cumulative.begin(),cumulative.end(),distance);
            const auto i=std::min(path.size()-2,std::size_t(hi-cumulative.begin())-1);
            const float length=cumulative[i+1]-cumulative[i];
            const float t=length>0?(distance-cumulative[i])/length:0;
            return sampleSegment(i,t,reverse);
        }
        Point sampleCourse(const Course& course,float distance)const {
            // The navigation and road records correspond one-for-one. Map
            // the actual road segment rather than stretching 3D race distance
            // over the independently measured 2D artwork length.
            if(course.points.size()!=path.size()||course.cumulative.size()!=path.size())
                return sample(distance/std::max(1.f,course.length),course.reversed);
            if(closed&&(distance<0||distance>course.length))distance-=std::floor(distance/course.length)*course.length;
            distance=std::clamp(distance,0.f,course.length);
            const auto hi=std::upper_bound(course.cumulative.begin(),course.cumulative.end(),distance);
            auto i=std::min(path.size()-2,std::size_t(hi-course.cumulative.begin())-1);
            float t=(distance-course.cumulative[i])/(course.cumulative[i+1]-course.cumulative[i]);
            if(course.reversed){i=path.size()-2-i;t=1.f-t;}
            return sampleSegment(i,t,course.reversed);
        }
        Point sampleSegment(std::size_t i,float t,bool reverse)const {
            const auto& a=path[i];const auto& b=path[i+1];
            // Spatial interpolation removes segment-boundary snaps without
            // a temporal filter that would make the map trail behind the car.
            const float dx=std::lerp(tangents[i][0],tangents[i+1][0],t);
            const float dy=std::lerp(tangents[i][1],tangents[i+1][1],t);
            const float norm=std::hypot(dx,dy),direction=reverse?-1.f:1.f;
            return {std::lerp(a[0],b[0],t),std::lerp(a[1],b[1],t)+2.f,
                norm>0?dx/norm*direction:0.f,norm>0?dy/norm*direction:direction};
        }
        void paint(std::span<std::uint32_t> target,int width,int height,float left,float top,
                float edge,float zoom,bool dark,const Point& p) {
            window=model.chunks.at(dark?night:day);
            const float half=extent/zoom;
            for(auto& batch:window.batches)for(auto& vertex:batch.vertices){
                const float x=vertex.position.x,y=-vertex.position.y;
                const float across=(x*2.f-1.f)*half,along=(1.f-y*2.f)*half;
                vertex.u=p.u+p.dv*across+p.du*along;
                vertex.v=p.v-p.du*across+p.dv*along;
                vertex.position={left+x*edge,top+y*edge,0};
            }
            SpritePlacement place;place.authoredHeight=0;place.straightAlphaOverlay=true;
            place.sourceTextureFiltering=true;
            compositeOriginalMenuChunk(target,width,height,textures,window,place);
        }
    };
    void setRoot(const std::filesystem::path& root){root_=root/"data/original_assets/navigation";}
    Bank* find(std::string_view course){
        constexpr std::array<std::string_view,8> ids{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu"};
        const auto found=std::find(ids.begin(),ids.end(),course);if(found==ids.end())return nullptr;
        const auto i=std::size_t(found-ids.begin());
        if(!banks_[i])banks_[i]=load(i);
        return &*banks_[i];
    }
private:
    std::filesystem::path root_;
    std::array<std::optional<Bank>,8> banks_;
    Bank load(std::size_t index)const {
        constexpr std::array<const char*,8> codes{"ez","nm","hd","df","vh","uh","sy","tu"};
        constexpr std::array<float,8> extents{.22f,.22f,.08f,.08f,.07f,.055f,.1f,.08f};
        constexpr unsigned chunks[8][5]={{0,1,6,7,5},{5,6,3,4,2},{0,1,6,7,5},{0,1,6,7,5},
            {5,5,3,4,2},{6,5,3,4,2},{5,6,3,4,2},{2,2,7,8,6}};
        const auto dir=root_/codes.at(index);Bank b;
        b.model=NativeModel::load(dir/"navigation.idasmesh");b.textures=NativeTextureBank::load(dir/"textures.idastex");
        b.extent=extents[index];b.closed=index<2;
        b.day=chunks[index][0];b.night=chunks[index][1];b.player=chunks[index][2];b.rival=chunks[index][3];b.frame=chunks[index][4];
        std::ifstream f(dir/"path.bin",std::ios::binary|std::ios::ate);
        if(!f)throw std::runtime_error("Missing original navigation path");
        auto bytes=f.tellg();f.seekg(0);unsigned count=0,components=0;
        f.read(reinterpret_cast<char*>(&count),4);f.read(reinterpret_cast<char*>(&components),4);
        if(!f||count<2||count>100000||components!=5||bytes!=std::streamoff(8+count*20ull))throw std::runtime_error("Invalid navigation path");
        b.path.resize(count);f.read(reinterpret_cast<char*>(b.path.data()),count*20);
        if(!f)throw std::runtime_error("Truncated navigation path");
        // Circuit road paths include a duplicate start at the end; the
        // navigation assets omit it. Retain that closing segment on the map.
        if(b.closed){b.path.push_back(b.path.front());++count;}
        b.cumulative.resize(count);b.tangents.resize(count);
        // 0C0C2816..285E adjusts negative-Y authoring once before the
        // update's +2 UV translation. Clamped texture banks need both steps.
        if(b.path.front()[1]<0)for(auto& p:b.path)p[1]-=1.f;
        for(unsigned i=0;i<count;++i){
            for(float v:b.path[i])if(!std::isfinite(v))throw std::runtime_error("Nonfinite navigation path");
            if(i)b.cumulative[i]=b.cumulative[i-1]+std::hypot(b.path[i][0]-b.path[i-1][0],b.path[i][1]-b.path[i-1][1]);
        }
        if(!(b.cumulative.back()>0))throw std::runtime_error("Empty navigation route");
        for(unsigned i=0;i<count;++i){
            const auto lo=b.closed&&(i==0||i==count-1)?count-2:i?i-1:0;
            const auto hi=b.closed&&(i==0||i==count-1)?1:std::min(i+1,count-1);
            const float dx=b.path[hi][0]-b.path[lo][0],dy=b.path[hi][1]-b.path[lo][1],norm=std::hypot(dx,dy);
            b.tangents[i]={norm>0?dx/norm:0.f,norm>0?dy/norm:1.f};
        }
        // Widths measured at half coverage in the source's 512px artwork.
        // Measure only the nearest connected stroke: Irohazaka's close
        // hairpins would otherwise count neighbouring roads as extra width.
        constexpr float strokes[]{8.25f,7.5f,3.5f,2.125f,2.5f,1.875f,3.125f,1.875f};
        const auto texture=b.model.chunks.at(b.day).batches.at(0).material[9];
        b.textures.replaceImage(texture,navigationArtwork(b.textures.at(texture),b.path,strokes[index]));
        return b;
    }
};
}
