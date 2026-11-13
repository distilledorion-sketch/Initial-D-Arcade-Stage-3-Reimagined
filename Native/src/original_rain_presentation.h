#pragma once
#include "original_rain_owner.h"
#include "original_rain_glare.h"
#include "original_smoke_view.h"
#include "original_collision.h"
#include "renderer.h"
#include <fstream>

namespace idas3 {
// The replay owns weather state and collision scratch. Source ticks prepare
// one immutable draw snapshot; host repaints neither query the road nor use RNG.
class OriginalRainPresentation {
public:
    struct Car : original::OriginalRainCar {
        unsigned model=0;
        bool blindAttack=false,braking=false;
    };
    struct Glare {
        unsigned car=0,lamp=0,chunk=0;
        original::OriginalRainGlareDraw draw;
    };
    struct DrawCounts {unsigned streaks=0,splashes=0,mist=0,glare=0;};
    NativeTextureBank textures;
    void load(const std::filesystem::path& root){
        const auto directory=root/"data/original_assets/weather/rain";
        model_=NativeModel::load(directory/"rain.idasmesh");
        textures=NativeTextureBank::load(directory/"textures/textures.idastex");
        if(model_.chunks.size()!=11||textures.size()!=11||textures.at(6).width!=16||textures.at(6).height!=64)
            throw std::runtime_error("Invalid original rain assets");
        for(auto& batch:model_.chunks[6].batches)
            original::originalRainGlareMaterial(batch.ich,batch.material,56);
        for(unsigned chunk:{7u,8u})for(auto& batch:model_.chunks[chunk].batches)
            original::originalRainGlareMaterial(batch.ich,batch.material,80);
        //32437C's complete rear/front anchor records match33C5F4 bitwise.
        // Consume the existing source export, retaining rear anchors only.
        std::ifstream file(root/"data/original_assets/effects/taillight/anchors.bin",std::ios::binary);
        const auto read=[&](auto& value){file.read(reinterpret_cast<char*>(&value),sizeof value);};
        std::array<char,8> magic{};read(magic);unsigned count=0;read(count);
        if(magic!=std::array<char,8>{'I','D','3','L','T','R','1',0}||count!=anchors_.size())
            throw std::runtime_error("Invalid rain lamp anchors");
        for(auto& anchors:anchors_){
            float width=0;unsigned front=0;read(width);read(anchors.rearCount);read(front);
            if((anchors.rearCount!=2&&anchors.rearCount!=4)||(front!=2&&front!=4))
                throw std::runtime_error("Invalid rain rear lamp count");
            for(unsigned i=0;i<anchors.rearCount+front;++i){
                std::array<float,3> point{};read(point);
                for(float value:point)if(!std::isfinite(value))throw std::runtime_error("Invalid rain lamp coordinate");
                if(i<anchors.rearCount)anchors.points[i]=point;
            }
        }
        if(!file||file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Incomplete rain lamp anchors");
    }
    bool loaded()const{return model_.chunks.size()==11;}
    bool active()const{return active_;}
    unsigned updates()const{return updates_;}
    const original::OriginalRainOwner& owner()const{return owner_;}
    const original::OriginalCollisionQuery& query()const{return query_;}
    const std::vector<Glare>& glare()const{return glare_;}
    void reset(){
        owner_={};glare_.clear();query_={};trace_={};surface_={};
        original::clearOriginalCollisionQuery(query_);
        view_=viewInverse_=original::originalIdentityMatrix();active_=false;night_=updates_=0;
    }
    void begin(const std::array<Car,2>& cars,unsigned night){
        if(!loaded()||!cars[0].available)throw std::runtime_error("Replay rain requires assets and a primary car");
        reset();night_=night;active_=true;
        //E2CCE ->17D560 ->17D8FE: ending/replay passes1. Initial motion is
        //zero and stays zero; ordinary race rain uses a different owner mode.
        owner_.reset({cars[0],cars[1]},1);
    }
    void advance(const original::OriginalMatrix& view,std::uint16_t fov,
            const std::array<Car,2>& cars,const original::OriginalCollisionData& collision,unsigned& random){
        if(!active_)return;
        view_=view;viewInverse_=view;original::invertOriginalMatrix(viewInverse_);glare_.clear();
        const auto effectsView=original::originalSmokeView(view,fov);
        const auto next=[&]{random=random*0x41c64e6du+12345u;return float((random>>16)&32767u)/32768.f;};
        owner_.advance(view,{cars[0],cars[1]},next,[&](const auto& allowed,const auto& previousWorld,auto& randomFloat){
            std::array<original::OriginalRainGlareCar,2> lamps;
            for(unsigned i=0;i<2;++i){
                const auto& car=cars[i];auto& lamp=lamps[i];lamp.available=car.available;
                if(!car.available)continue;
                const auto& anchors=anchors_.at(car.model);
                lamp.world=previousWorld[i];lamp.blindAttack=car.blindAttack;lamp.braking=car.braking;
                lamp.rearCount=anchors.rearCount;lamp.rearAnchors=anchors.points;
            }
            original::originalRainRearGlare(view,effectsView.planes[0],night_,lamps,allowed,
                [&](const auto& from,const auto& to){
                    for(unsigned axis=0;axis<3;++axis){query_.setf(32+axis*4,from[axis]);query_.setf(44+axis*4,to[axis]);}
                    const bool hit=original::queryOriginalCollisionSwept(collision,query_,trace_,surface_);
                    return original::OriginalRainGlareHit{hit,query_.u(28)};
                },randomFloat,[&](unsigned car,unsigned lamp,unsigned chunk,const auto& draw){glare_.push_back({car,lamp,chunk,draw});});
        });
        ++updates_;
    }
    DrawCounts append(Mesh& mesh,unsigned textureBase,const original::OriginalFscaTable& table)const{
        DrawCounts counts;if(!active_||!updates_)return counts;
        const auto& fields=owner_.fields;
        NativeAssembly assembly;assembly.instances.resize(1);
        for(unsigned i=0;i<fields.streaks.size();++i)if(fields.visibleStreaks[i]){
            //Texture6 is16x64; source TSP U/V size fields are1/3.
            mesh.beginRange(textureBase+6,original::rainTspWithoutSize|11u,original::rainPcw,
                original::rainIsp,0x6aa,true,true,0,false,std::nullopt,false,1);
            const auto quad=fields.streakQuad(i,night_);
            for(unsigned index:{0u,1u,2u,2u,1u,3u}){const auto& v=quad[index];
                mesh.vertices.push_back({{v.position[0],v.position[1],v.position[2]},
                    {0,1,0},{1,1,1,float(v.argb>>24)/255.f},v.u,v.v});++mesh.ranges.back().count;}
            ++counts.streaks;
        }
        for(unsigned i=0;i<fields.splashes.size();++i)if(fields.visibleSplashes[i]){
            appendChunk(mesh,assembly,4,original::originalRainSplashViewMatrix(view_,fields.splashes[i].position,table),textureBase);
            ++counts.splashes;
        }
        for(const auto& mist:fields.drawnWheelMist)if(mist.active==1){
            appendChunk(mesh,assembly,6,original::originalRainWheelMistViewMatrix(view_,mist),textureBase);++counts.mist;
        }
        for(const auto& lamp:glare_){appendChunk(mesh,assembly,lamp.chunk,lamp.draw.viewMatrix,textureBase,lamp.draw.alpha);++counts.glare;}
        return counts;
    }
private:
    void appendChunk(Mesh& mesh,NativeAssembly& assembly,unsigned chunk,const original::OriginalMatrix& matrix,
            unsigned textureBase,std::optional<unsigned> alpha=std::nullopt)const{
        original::OriginalMatrix world;
        for(unsigned column=0;column<4;++column){const auto i=column*4;
            const auto v=original::transformOriginalVector(viewInverse_,
                {matrix.elements[i],matrix.elements[i+1],matrix.elements[i+2],matrix.elements[i+3]});
            for(unsigned row=0;row<4;++row)world.elements[i+row]=v[row];
        }
        auto& instance=assembly.instances[0];instance.chunk=chunk;
        for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)
            instance.transform[row*4+column]=world.elements[column*4+row];
        const auto first=mesh.ranges.size(),firstVertex=mesh.vertices.size();
        mesh.originalCar(model_,assembly,{0,0,0},0,textureBase);
        //GMP diffuse is already expanded into host vertices. Apply this
        //draw's source alpha there without cloning an authored model per lamp.
        if(alpha)for(auto i=firstVertex;i<mesh.vertices.size();++i)mesh.vertices[i].color.a=float(*alpha&255u)/255.f;
        for(auto i=first;i<mesh.ranges.size();++i){mesh.ranges[i].emissive=true;mesh.ranges[i].viewMask=1;}
    }
    struct Anchors {unsigned rearCount=0;std::array<std::array<float,3>,4> points{};};
    std::array<Anchors,35> anchors_{};
    NativeModel model_;
    original::OriginalRainOwner owner_;
    original::OriginalMatrix view_=original::originalIdentityMatrix(),viewInverse_=view_;
    original::OriginalCollisionQuery query_;
    original::OriginalTriangleSearchTrace trace_;
    original::OriginalSurfaceScratch surface_;
    std::vector<Glare> glare_;
    bool active_=false;
    unsigned night_=0,updates_=0;
};
}
