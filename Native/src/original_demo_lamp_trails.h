#pragma once
#include "original_lamp_trail.h"
#include "original_collision.h"
#include "original_smoke_view.h"
#include "renderer.h"
#include <fstream>

namespace idas3 {
// Recorded opening playback caches each source frame. Ending playback uses
// one reusable frame with the same nine-sample history. Only prepare advances
// history or collision queries; both rendering paths are immutable.
class OriginalDemoLampTrails {
public:
    NativeTextureBank textures;
    struct Sample {
        original::OriginalLampTrailPoint center{};
        float height1000=0;
        unsigned collapsedFrame=0;
        unsigned char opacity=0;
        bool ribbon=false;
    };
    using Frame=std::array<std::array<Sample,4>,2>;
    void load(const std::filesystem::path& root,unsigned frameCount){
        const auto directory=root/"data/original_assets/effects/taillight";
        model_=NativeModel::load(directory/"taillight.idasmesh");
        textures=NativeTextureBank::load(directory/"textures/textures.idastex");
        std::ifstream file(directory/"anchors.bin",std::ios::binary);
        const auto read=[&](auto& x){file.read(reinterpret_cast<char*>(&x),sizeof x);};
        std::array<char,8> magic{};read(magic);unsigned count=0;read(count);
        if(magic!=std::array<char,8>{'I','D','3','L','T','R','1',0}||count!=35||model_.chunks.size()!=8||textures.size()!=2)
            throw std::runtime_error("Invalid original lamp-trail assets");
        for(auto& car:cars_){read(car.width);read(car.rear);read(car.front);
            if((car.rear!=2&&car.rear!=4)||(car.front!=2&&car.front!=4)||!(car.width>0&&car.width<1))
                throw std::runtime_error("Invalid original lamp-trail anchors");
            for(unsigned i=0;i<car.rear+car.front;++i){read(car.points[i]);
                for(float v:car.points[i])if(!std::isfinite(v))throw std::runtime_error("Invalid lamp-trail coordinate");}
        }
        if(!file||file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Incomplete lamp-trail assets");
        reset(frameCount);
    }
    bool loaded()const{return model_.chunks.size()==8&&textures.size()==2;}
    void reset(unsigned frameCount=0){
        frames_.assign(frameCount,{});history_={};collapsed_={};cut_={};
        original::clearOriginalCollisionQuery(query_);
    }
    void beginFrame(bool sceneChanged){cut_.advance(sceneChanged?1:0);}
    void prepare(unsigned frame,unsigned slot,unsigned carId,bool lightsOn,
                 const original::OriginalMatrix& carMatrix,const original::OriginalSmokeView& view,
                 Vec3 eye,const original::OriginalCollisionData& collision,unsigned night=1){
        const auto& car=cars_.at(carId);auto& output=frames_.at(frame).at(slot);
        output={};
        const auto cameraPoint=[&](const original::OriginalLampTrailPoint& local){
            return original::transformOriginalPoint(view.worldToView,original::transformOriginalPoint(carMatrix,local));
        };
        const auto rear4=original::transformOriginalVector(carMatrix,{0,0,-1,0});
        const auto rear=normalized(Vec3{rear4[0],rear4[1],rear4[2]});
        for(unsigned lamp=0;lamp<4;++lamp){
            auto& sample=output[lamp];auto& history=history_[slot][lamp];
            auto& collapsed=collapsed_[slot][lamp];
            if(lamp>=car.rear){collapsed=frame;continue;}
            auto center=car.points[lamp];center[1]+=.3f;
            auto lower=center,plus=center,minus=center;lower[1]-=.1f;
            const float halfWidth=car.width*.5f*.8f;plus[0]+=halfWidth;minus[0]-=halfWidth;
            const auto world=original::transformOriginalPoint(carMatrix,center);
            const auto camera=cameraPoint(center);
            //1A941A..1A9474 uses the original640x480 projection bounds.
            const float extent=-camera[2]*view.halfAngleTangent;
            bool visible=original::originalReplayLampTrailSwitches(night,1,!lightsOn).rear&&!cut_.blocked284
                &&camera[2]<-.01f&&std::abs(camera[1])<=extent&&std::abs(camera[0])<=extent*(4.f/3.f);
            const auto projected=original::projectOriginalLampTrail(
                {camera,cameraPoint(lower),cameraPoint(plus),cameraPoint(minus)},.01f);
            // A camera crossing the lamp plane can yield nonfinite source
            // geometry. Never publish NaNs to either host rendering backend.
            bool finite=std::isfinite(projected.height1000);
            for(const auto& p:projected.points)for(float v:p)finite=finite&&std::isfinite(v);
            sample.center=finite?projected.points[0]:original::OriginalLampTrailPoint{0,0,-.51f};
            history.push(sample.center);visible=visible&&finite;
            const Vec3 towardEye=eye-Vec3{world[0],world[1],world[2]};
            if(visible){
                visible=original::originalSmokeFrustumDepth(view,world,8.05f)>=0
                    &&std::fma(towardEye.x,towardEye.x,towardEye.z*towardEye.z)<=10000.f;
                if(visible){
                    for(unsigned axis=0;axis<3;++axis){query_.setf(32+4*axis,world[axis]);query_.setf(44+4*axis,axis==0?eye.x:axis==1?eye.y:eye.z);}
                    visible=original::queryOriginalCollisionSwept(collision,query_,trace_,surface_)&&!(query_.u(28)&0x8000);
                }
            }
            if(!visible){history.collapse();collapsed=frame;}
            sample.collapsedFrame=collapsed;
            if(!visible)continue;
            const float facing=dot(rear,normalized(towardEye));
            sample.opacity=static_cast<unsigned char>(std::clamp(facing,0.f,1.f)*std::clamp(facing,0.f,1.f)*220.f);
            sample.height1000=projected.height1000;sample.ribbon=history.spansLamp(projected);
        }
    }
    const Frame& frame(unsigned index)const{return frames_.at(index);}
    const auto& history()const{return history_;}
    unsigned append(Mesh& mesh,unsigned frame,unsigned slot,const std::array<float,16>& camera,
                    Vec3 eye,unsigned textureBase,bool liveHistory=false)const{
        const auto world=[&](original::OriginalLampTrailPoint p){
            return eye+Vec3{camera[0],camera[1],camera[2]}*p[0]
                +Vec3{camera[4],camera[5],camera[6]}*p[1]+Vec3{camera[8],camera[9],camera[10]}*p[2];
        };
        const Vec3 normal{camera[8],camera[9],camera[10]};unsigned ribbons=0;
        for(unsigned lamp=0;lamp<4;++lamp){const auto& s=frames_.at(frame).at(slot)[lamp];if(!s.opacity)continue;
            if(s.ribbon){
                const auto firstVertex=mesh.vertices.size();
                original::OriginalLampTrailHistory history;
                if(liveHistory)history=history_[slot][lamp];
                else for(unsigned i=0;i<9;++i)history.points[i]=frames_[std::max(frame>i?frame-i:0u,s.collapsedFrame)][slot][lamp].center;
                const auto strip=history.strip(s.height1000,s.opacity);
                //1D0260/1D0720 then1A8E80..1A8EF8: textured, unlit,
                // source-alpha blending, no face cull. Texture sizes are128x32.
                mesh.beginRange(textureBase,original::lampTrailTsp,original::lampTrailPcw,original::lampTrailIsp,0x6aa,true,true,0,false,std::nullopt,false,1);
                const auto vertex=[&](const original::OriginalLampTrailVertex& v){
                    mesh.vertices.push_back({world(v.position),normal,{1,1,1,float(v.argb>>24)/255.f},v.u,v.v});++mesh.ranges.back().count;
                };
                for(unsigned i=2;i<18;++i){
                    const auto valid=[&](unsigned index){for(float v:strip[index].position)if(!std::isfinite(v))return false;return true;};
                    if(!valid(i-2)||!valid(i-1)||!valid(i))continue;
                    vertex(strip[i&1?i-1:i-2]);vertex(strip[i&1?i-2:i-1]);vertex(strip[i]);
                }
                if(mesh.vertices.size()>firstVertex)++ribbons;
            }
            //1A9922 uses0.01*projectedHeight1000 and the authored sprite
            // selected by33C824. Retain its original UVs and texture color.
            const float scale=.01f*s.height1000;
            for(const auto& batch:model_.chunks[3-lamp].batches){
                //1A9100 changes material diffuse alpha. Carry that value in
                // host vertices, with texture RGBA modulation (as for hilight).
                const unsigned tsp=batch.ich[2]|(1u<<20)|(3u<<6);
                mesh.beginRange(textureBase+batch.material[9],tsp,batch.ich[0],batch.ich[1],batch.material[2],true,true,0,false,std::nullopt,false,1);
                for(unsigned index:batch.indices){const auto& v=batch.vertices[index];
                    mesh.vertices.push_back({world({s.center[0]+v.position.x*scale,s.center[1]+v.position.y*scale,s.center[2]+v.position.z*scale}),
                        normal,{1,1,1,float(s.opacity)/255.f},v.u,v.v});++mesh.ranges.back().count;
                }
            }
        }return ribbons;
    }
private:
    struct Car {float width=0;unsigned rear=0,front=0;std::array<original::OriginalLampTrailPoint,8> points{};};
    std::array<Car,35> cars_{};
    NativeModel model_;
    std::vector<Frame> frames_;
    std::array<std::array<original::OriginalLampTrailHistory,4>,2> history_{};
    std::array<std::array<unsigned,4>,2> collapsed_{};
    original::OriginalLampTrailCut cut_;
    original::OriginalCollisionQuery query_;
    original::OriginalTriangleSearchTrace trace_;
    original::OriginalSurfaceScratch surface_;
};
}
