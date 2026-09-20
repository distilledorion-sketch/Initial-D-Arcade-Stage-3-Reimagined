#include "original_demo_presentation.h"
#include "original_car_body_position.h"
#include "original_race_path.h"
#include "original_rival_appearance_catalog.h"
#include "car_catalog.h"
#include <stdexcept>

namespace idas3 {
namespace {
std::array<Vec3,3> demoBodyAxes(const original::OriginalDemoActor& actor){
    const float yaw=wrapAngle(actor.f(28)+pi),pitch=-actor.f(24),roll=-actor.f(32);
    const auto r=right(yaw),f=forward(yaw);const Vec3 up{0,1,0};
    const auto tiltedUp=up*std::cos(pitch)+f*std::sin(pitch);
    return {r*std::cos(roll)+tiltedUp*std::sin(roll),tiltedUp*std::cos(roll)-r*std::sin(roll),f*std::cos(pitch)-up*std::sin(pitch)};
}
Vec3 demoLocal(Vec3 value,const std::array<Vec3,3>& axes){return {dot(value,axes[0]),dot(value,axes[1]),dot(value,axes[2])};}
// Exit a body envelope along the camera's backward travel. No vertical push:
// the source road clearance, tilt, lens and look direction remain unchanged.
float bodyExit(Vec3 eye,Vec3 backward,Vec3 low,Vec3 high){
    constexpr float clearance=.35f;
    low-=Vec3{clearance,clearance,clearance};high+=Vec3{clearance,clearance,clearance};
    if(eye.x<=low.x||eye.x>=high.x||eye.y<=low.y||eye.y>=high.y||eye.z<=low.z||eye.z>=high.z)return 0;
    float distance=1e9f;
    const auto axis=[&](float p,float d,float lo,float hi){if(std::abs(d)>1e-6f)distance=std::min(distance,((d>0?hi:lo)-p)/d);};
    axis(eye.x,backward.x,low.x,high.x);axis(eye.y,backward.y,low.y,high.y);axis(eye.z,backward.z,low.z,high.z);
    return distance+.001f;
}
}
void OriginalDemoPresentation::load(const std::filesystem::path& root,const original::OriginalDemoData& data){
    if(!data.hasCamera())throw std::runtime_error("Original opening camera capture is missing");
    scene_=OriginalCourseScene::loadMetadata(root,root/"data/original_models/attract/demo/scene.idasscene");
    courseCache_.invalidate();cars_.clear();
    //0E1E00 selects these035F00 rival presets and0286A0 scene variant7.
    // Configurations are the byte-verified existing rival source captures.
    constexpr std::array<unsigned,6> enemies{13,29,8,12,10,30};
    constexpr std::array<unsigned,6> configs{4718593,4718721,4784128,4718601,4718593,0};
    unsigned base=unsigned(scene_.textures.size()+scene_.backgroundTextures.size());
    for(unsigned i=0;i<enemies.size();++i){
        const unsigned enemy=enemies[i],id=originalRivalAppearances.at(enemy).car;
        const auto folder=originalCarFolders.at(id);const auto directory=root/"data/original_models"/folder;
        Car asset;auto model=NativeModel::load(directory/(std::string(folder)+".idasmesh"));
        asset.presentation=CarPresentation::loadRival(root,id,enemy,model.chunks.size());
        auto materials=original::OriginalCarMaterialRebuild::load(directory/"material_layout.bin",id);
        original::OriginalCarAppearanceConfig config(id);config.word=configs[i];
        //035F00 invokes the paint setter even when the saved color is zero.
        original::applyOriginalCarAppearanceCall(config,0x0c028660,(config.word>>25)&7);
        original::applyOriginalCarAppearanceCall(config,0x0c0286a0,7);
        materials.rebuild(config,0);asset.model=materials.apply(model);
        asset.textures=NativeTextureBank::load(root/"data/original_assets/cars"/folder/"textures/textures.idastex");
        asset.plate=OriginalNumberPlate::loadRival(root,id,enemy);asset.textureBase=base;
        Mesh body;body.originalCar(asset.model,asset.presentation.pose({},false,false),{},0);
        asset.low={1e9f,1e9f,1e9f};asset.high=-asset.low;
        for(const auto& range:body.ranges)if(!range.billboard)for(unsigned j=range.first;j<range.first+range.count;++j){
            const auto p=body.vertices[j].position;
            asset.low={std::min(asset.low.x,p.x),std::min(asset.low.y,p.y),std::min(asset.low.z,p.z)};
            asset.high={std::max(asset.high.x,p.x),std::max(asset.high.y,p.y),std::max(asset.high.z,p.z)};
        }
        asset.presentation.resetHeadlights();
        base+=unsigned(asset.textures.size()+asset.plate.textures.size());
        // Init enables all six lamps without drawing their popup motors.
        // Keep counter -1 until this particular source ACar is first selected.
        cars_.emplace(enemy,std::move(asset));
    }
    // Evaluate the existing native source queries once during background load.
    // Rendering a shot never seeks using a guessed nearest road or advances
    // collision caches at the host refresh rate.
    const auto path=original::OriginalRacePath::load(root,6);
    const auto collision=original::OriginalCollisionData::load(root/"data/original_physics/collision_k_df_0.rcl");
    std::map<unsigned,OriginalCarBodyPosition> bodyQueries;
    std::array<original::OriginalPathCoordinate,2> coordinate{};
    original::OriginalDemoCursor cursor;
    positions_.resize(data.frameCount());
    for(unsigned frame=0;frame<data.frameCount();++frame){
        const auto& shot=data.shots()[cursor.shot];
        if(frame==shot.frames[2])for(unsigned slot=0;slot<2;++slot)coordinate[slot]={std::int32_t(shot.descriptor[7+slot*2]),0};
        for(unsigned slot=0;slot<2;++slot){
            const auto& actor=data.actor(frame,slot);const Vec3 position{actor.f(0),actor.f(4),actor.f(8)};
            path.project({position.x,position.y,position.z},coordinate[slot],false);
            positions_[frame].body[slot]=bodyQueries[shot.enemies[slot]].update(collision,shot.cars[slot],position);
            auto& presentation=cars_.at(shot.enemies[slot]).presentation;
            presentation.advanceOriginalFrame(original::OriginalDemoData::headlightsOn(frame,slot));
            positions_[frame].headlights[slot]=presentation.headlightState();
        }
        if(coordinate[0].index<0)throw std::runtime_error("Original opening path index is negative");
        positions_[frame].pathIndex=unsigned(coordinate[0].index);data.step(cursor);
    }
    // The captured camera uses raw actor coordinates; displayed bodies include
    // ride height, slope and their complete model extents. Some close passes
    // consequently enter a body. Find clearance once, across the whole shot,
    // so the correction cannot pump or snap as individual frames approach it.
    for(const auto& shot:data.shots()){
        float pullback=0;
        for(unsigned frame=shot.frames[2];frame<=shot.frames[3];++frame){
            const auto& m=data.camera(frame).world;const Vec3 eye{m[12],m[13],m[14]};
            const auto backward=normalized(Vec3{m[8],0,m[10]});
            for(unsigned slot=0;slot<2;++slot){const auto& car=cars_.at(shot.enemies[slot]);const auto axes=demoBodyAxes(data.actor(frame,slot));
                pullback=std::max(pullback,bodyExit(demoLocal(eye-positions_[frame].body[slot],axes),demoLocal(backward,axes),car.low,car.high));}
        }
        for(unsigned frame=shot.frames[2];frame<=shot.frames[3];++frame){const auto& m=data.camera(frame).world;
            positions_[frame].cameraOffset=normalized(Vec3{m[8],0,m[10]})*pullback;}
    }
    meshFrame_=meshTimeline_=~0u;
}
bool OriginalDemoPresentation::upload(Renderer& renderer)const{
    if(!renderer.loadTextures(scene_.textures)||!renderer.loadTextures(scene_.backgroundTextures,true))return false;
    // Bases were assigned in source preset order; map iteration is different.
    for(unsigned enemy:{13u,29u,8u,12u,10u,30u}){const auto& car=cars_.at(enemy);
        if(!renderer.loadTextures(car.textures,true)||!renderer.loadTextures(car.plate.textures,true))return false;}
    return true;
}
const Mesh& OriginalDemoPresentation::mesh(const original::OriginalDemoData& data,original::OriginalDemoCursor cursor,unsigned timeline){
    if(meshFrame_==cursor.frame&&meshTimeline_==timeline)return mesh_;
    mesh_.vertices.clear();mesh_.ranges.clear();const auto& camera=data.camera(cursor.frame).world;
    const Vec3 eye=Vec3{camera[12],camera[13],camera[14]}+cameraOffset(cursor.frame);
    mesh_.originalCar(scene_.backgroundModel,scene_.backgroundAssembly(eye),{},0,unsigned(scene_.textures.size()));
    // Identify both source scenery banks without replacing the intro's
    // fallback lighting with the race ARRAY lightset. Mark before car append
    // so identical materials cannot coalesce scenery and a car into one range.
    for(auto& range:mesh_.ranges)range.courseGeometry=true;
    courseCache_.appendTo(mesh_,scene_.model,scene_.assemblyForPathIndex(positions_.at(cursor.frame).pathIndex),true);
    const auto& shot=data.shots()[cursor.shot];
    for(unsigned slot=0;slot<2;++slot){
        auto& car=cars_.at(shot.enemies[slot]);const auto& actor=data.actor(cursor.frame,slot);
        CarWheelPose wheels;wheels.steeringRadians=actor.f(60);
        for(unsigned i=0;i<4;++i){wheels.suspensionY[i]=actor.f(64+i*4);wheels.rotationRadians[i]=actor.f(96+i*4);}
        const bool braking=(actor.words[92/4]&1)!=0;
        car.presentation.restoreHeadlightState(positions_[cursor.frame].headlights[slot]);
        const auto& assembly=car.presentation.pose(wheels,original::OriginalDemoData::headlightsOn(timeline,slot),braking);
        const auto position=positions_[cursor.frame].body[slot];
        const float yaw=wrapAngle(actor.f(28)+pi),pitch=-actor.f(24),roll=-actor.f(32);
        mesh_.originalCar(car.model,assembly,position,yaw,pitch,roll,car.textureBase,car.presentation.illuminatedChunks(),true);
        mesh_.originalCar(car.plate.model,car.plate.assembly(),position,yaw,pitch,roll,car.textureBase+unsigned(car.textures.size()),{},true);
    }
    meshFrame_=cursor.frame;meshTimeline_=timeline;return mesh_;
}
}
