#pragma once
#include "renderer.h"
#include "original_roadside_dust_system.h"
#include "original_smoke_depth.h"
#include "original_smoke_mesh.h"
#include "original_smoke_view.h"

namespace idas3 {
// Original dust controllers and authored density mesh, adapted at the host
// camera/scene boundary. Its private RNG never changes physics or audio RNG.
class RoadsideDustPresentation {
public:
    struct Checkpoint { original::OriginalRoadsideDustSystem system;unsigned random; };
    Checkpoint checkpoint()const{return {system_,random_};}
    void restore(const Checkpoint& saved){system_=saved.system;random_=saved.random;}
    void load(const std::filesystem::path& root,const NativeTextureBank& textures){
        if(!source_.batches.empty())return;
        auto grids=NativeModel::load(root/"data/original_assets/effects/smoke/smoke_grids.idasmesh");
        if(grids.chunks.size()!=3)throw std::runtime_error("Original smoke grids missing");
        auto source=std::move(grids.chunks[0]); //13EA3E: default grid selector0
        auto mapping=original::mapOriginalSmokeGrid(source,16,16);
        auto kernel=original::originalSmokeDensityKernel(textures.at(1));
        source_=std::move(source);mapping_=std::move(mapping);kernel_=std::move(kernel);
    }
    void reset(){system_.reset();random_=0x37c778u;}
    bool loaded()const{return !source_.batches.empty();}
    const original::OriginalRoadsideDustSystem& system()const{return system_;}
    void advance(unsigned course,unsigned night,unsigned weather,unsigned mode,bool playerRequested,
            const std::array<original::OriginalRoadsideDustCar,2>& cars){
        system_.advance(course,night,weather,mode,playerRequested,cars,[&](){
            random_=random_*0x41c64e6du+12345u;return float((random_>>16)&32767u)/32768.f;
        });
    }
    unsigned append(Mesh& mesh,Vec3 eye,Vec3 target,Vec3 cameraUp,float fieldOfView,
            float aspect,unsigned textureBase,unsigned viewMask,float viewportHeight=480.f)const{
        return appendSystem(system_,mesh,eye,target,cameraUp,fieldOfView,aspect,textureBase,viewMask,viewportHeight);
    }
    // Both recovered controllers use the same authored density grid and
    // display path. Rendering observes their state without advancing it.
    template<class SmokeSystem> unsigned appendSystem(const SmokeSystem& system,
            Mesh& mesh,Vec3 eye,Vec3 target,Vec3 cameraUp,float fieldOfView,
            float aspect,unsigned textureBase,unsigned viewMask,float viewportHeight=480.f)const{
        if(!loaded()||system.order().empty())return 0;
        if(!std::isfinite(fieldOfView)||fieldOfView<=0||fieldOfView>=pi||
           !std::isfinite(aspect)||aspect<=0||!std::isfinite(viewportHeight)||viewportHeight<=0)
            throw std::invalid_argument("Dust camera projection");
        const auto forwardAxis=normalized(target-eye),rightAxis=normalized(cross(cameraUp,forwardAxis));
        const auto up=normalized(cross(forwardAxis,rightAxis));
        // Native cameras use positive forward Z; source display math uses
        // negative Z. This rigid change of basis is inverted for submission.
        auto worldToView=original::originalIdentityMatrix();auto& v=worldToView.elements;
        v={rightAxis.x,up.x,-forwardAxis.x,0,rightAxis.y,up.y,-forwardAxis.y,0,
           rightAxis.z,up.z,-forwardAxis.z,0,-dot(rightAxis,eye),-dot(up,eye),dot(forwardAxis,eye),1};
        const auto phase=std::uint16_t(std::lround(double(fieldOfView)*65536.0/(2.0*double(pi))));
        const auto view=original::originalSmokeView(worldToView,phase,viewportHeight,aspect);
        const float focal=view.focalScale();
        unsigned submitted=0;
        std::vector<original::OriginalSmokeDisplayParticle> particles;particles.reserve(system.particleCapacity);
        for(auto index:system.order()){
            const auto& cloud=system.cloud(index);particles.clear();
            for(const auto& p:cloud.emitter.particles)particles.push_back({p,
                original::transformOriginalPoint(worldToView,p.position),
                original::originalSmokeFrustumDepth(view,p.position,p.size)>0.f});
            const auto display=original::projectOriginalSmokeDisplay(particles,system.parameters(),focal,cloud.gain,
                mapping_.width,mapping_.height,kernel_,128,128,[&](float minZ,float maxZ){
                    return original::originalSmokeFootprintDepth(cloud.footprint,worldToView,
                        original::OriginalRoadsideDustOwner::dimensions[0],original::OriginalRoadsideDustOwner::dimensions[2],
                        cloud.effect,minZ,maxZ).depth;
                },240.f*aspect);
            if(!display.visible)continue;
            auto draw=original::prepareOriginalSmokeMesh(source_,mapping_,display,focal,cloud.color);
            NativeModel model;model.chunks.push_back(std::move(draw.chunk));
            const auto center=eye+rightAxis*draw.translation[0]+up*draw.translation[1]-forwardAxis*draw.translation[2];
            NativeModelInstance instance;instance.transform={
                rightAxis.x*draw.scale[0],up.x*draw.scale[1],-forwardAxis.x,center.x,
                rightAxis.y*draw.scale[0],up.y*draw.scale[1],-forwardAxis.y,center.y,
                rightAxis.z*draw.scale[0],up.z*draw.scale[1],-forwardAxis.z,center.z,0,0,0,1};
            NativeAssembly assembly;assembly.instances.push_back(instance);
            const auto first=mesh.ranges.size();mesh.originalCar(model,assembly,{0,0,0},0,textureBase);
            for(auto range=first;range<mesh.ranges.size();++range)mesh.ranges[range].viewMask=viewMask;
            ++submitted;
        }
        return submitted;
    }
private:
    original::OriginalRoadsideDustSystem system_;
    unsigned random_=0x37c778u;
    NativeModelChunk source_;
    original::OriginalSmokeGridMapping mapping_;
    std::vector<std::uint16_t> kernel_;
};
}
