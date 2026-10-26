#include "original_demo_presentation.h"
#include "original_car_body_position.h"
#include "original_race_path.h"
#include "original_rival_appearance_catalog.h"
#include "original_matrix.h"
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
    projectedTextures_=NativeTextureBank::load(root/"data/original_assets/headlight_projection/textures/textures.idastex");
    projectedTextureBase_=base;glareTextureBase_=base+unsigned(projectedTextures_.size());
    glare_.load(root);
    // Evaluate the existing native source queries once during background load.
    // Rendering a shot never seeks using a guessed nearest road or advances
    // collision caches at the host refresh rate.
    const auto path=original::OriginalRacePath::load(root,6);
    const auto collision=original::OriginalCollisionData::load(root/"data/original_physics/collision_k_df_0.rcl");
    std::map<unsigned,OriginalCarBodyPosition> bodyQueries;
    std::map<unsigned,OriginalHeadlightPresentation> projectors;
    for(unsigned enemy:enemies)projectors[enemy].load(root);
    projectedModel_=projectors.begin()->second.model();
    const auto fsca=original::OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
    // Dedicated03F620 dry-night constructor, not the ordinary Akina table:
    // four downward half-white spots,20/40m falloff and120/130 degree angles.
    // Use the race's Akina night ambient with the demo's own lamp locations.
    // The source road projector multiplies destination color; dropping that
    // ambient would turn every unlit section black and erase the beam.
    courseLights_=original::originalCourseLighting(3,true,false);courseLights_.count=0;
    courseLights_.sourceAddress=0x0c03f620;
    const std::array<Vec3,4> anchors{{{190,1142,-349},{166,1142,-288},{1782,681,-1999},{1816,681,-1995}}};
    for(unsigned i=0;i<anchors.size();++i){auto& light=courseLights_.lights[courseLights_.count++];
        light.kind=original::OriginalCourseLightKind::Spot;light.sourceSlot=i;
        light.position={anchors[i].x,anchors[i].y,anchors[i].z};light.incomingDirection={0,-1,0};light.color={.5f,.5f,.5f};
        light.distance0=20;light.distance1=40;light.angle0=0x5555;light.angle1=0x5c72;
        light.coefficientWords[0]=0x4220bf80;light.angleCosines={fsca.sinCos(light.angle0)[1],fsca.sinCos(light.angle1)[1]};
    }
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
            // Prepare immutable beam geometry with the recorded 60 Hz poses,
            // once during background loading. Repainting/seeking must not run
            // collision work or advance a light at the display refresh rate.
            auto& projector=projectors.at(shot.enemies[slot]);
            projector.request(original::OriginalDemoData::headlightsOn(frame,slot),collision,
                bodyQueries[shot.enemies[slot]].query(),{position.x,position.y,position.z});
            projector.publish();
            const auto& vertices=projector.model().chunks[0].batches[0].vertices;
            std::copy(vertices.begin(),vertices.end(),positions_[frame].projectedHeadlights[slot].begin());
            auto matrix=original::originalActorMatrix(
                {position.x,position.y-std::bit_cast<float>(0x3ca3d70au),position.z},
                {actor.f(24),actor.f(28),actor.f(32)},fsca);
            original::rotateOriginalMatrixPhase(matrix,1,0x8000,fsca);
            positions_[frame].lightMatrices[slot]=matrix.elements;
            projector.advance(collision,matrix.elements);
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
    return renderer.loadTextures(projectedTextures_,true)&&renderer.loadTextures(glare_.textures,true);
}
void OriginalDemoPresentation::bindLighting(Renderer& renderer)const{
    // Do not draw the old broad analytic beam on top of source projection.
    renderer.vehicleLights=false;renderer.opponentLights=false;
    renderer.courseLighting=&lighting_.course;renderer.playerLighting=&lighting_.player;renderer.rivalLighting=&lighting_.rival;
    renderer.courseFog=nullptr;renderer.supplementalCourseLampLighting=false;
    renderer.courseLampPositions={{190,1142,-349},{166,1142,-288},{1782,681,-1999},{1816,681,-1995}};
}
const Mesh& OriginalDemoPresentation::mesh(const original::OriginalDemoData& data,original::OriginalDemoCursor cursor,unsigned timeline){
    if(meshFrame_==cursor.frame&&meshTimeline_==timeline)return mesh_;
    mesh_.vertices.clear();mesh_.ranges.clear();const auto& camera=data.camera(cursor.frame).world;
    const Vec3 eye=Vec3{camera[12],camera[13],camera[14]}+cameraOffset(cursor.frame);
    mesh_.originalCar(scene_.backgroundModel,scene_.backgroundAssembly(eye),{},0,unsigned(scene_.textures.size()));
    // Mark the banks before car append so equal materials cannot coalesce
    // scenery and a car into one range with the wrong light scope.
    for(auto& range:mesh_.ranges)range.courseGeometry=true;
    const auto courseBegin=mesh_.ranges.size();
    courseCache_.appendTo(mesh_,scene_.model,scene_.assemblyForPathIndex(positions_.at(cursor.frame).pathIndex),true);
    for(auto i=courseBegin;i<mesh_.ranges.size();++i)mesh_.ranges[i].courseLighting=true;
    // The four untextured0x6ff batches in course chunks1/28 contain the
    // authored street-lamp bulbs/glow. Preserve their unlit source color;
    // fallback night darkening makes the fixtures look switched off.
    // Scope this to the demo course before appending any car materials.
    for(auto& range:mesh_.ranges)if(range.courseGeometry&&range.texture==0xffffffffu&&range.gmp==0x6ff){
        range.emissive=true;range.courseLighting=false;
    }
    const auto& shot=data.shots()[cursor.shot];
    std::array<original::OriginalCarLighting,2> carLights;
    for(unsigned slot=0;slot<2;++slot){
        auto& car=cars_.at(shot.enemies[slot]);const auto& actor=data.actor(cursor.frame,slot);
        CarWheelPose wheels;wheels.steeringRadians=actor.f(60);
        for(unsigned i=0;i<4;++i){wheels.suspensionY[i]=actor.f(64+i*4);wheels.rotationRadians[i]=actor.f(96+i*4);}
        const bool braking=(actor.words[92/4]&1)!=0;
        car.presentation.restoreHeadlightState(positions_[cursor.frame].headlights[slot]);
        const auto& assembly=car.presentation.pose(wheels,original::OriginalDemoData::headlightsOn(timeline,slot),braking);
        const auto position=positions_[cursor.frame].body[slot];
        const float yaw=wrapAngle(actor.f(28)+pi),pitch=-actor.f(24),roll=-actor.f(32);
        const auto carBegin=mesh_.ranges.size();
        mesh_.originalCar(car.model,assembly,position,yaw,pitch,roll,car.textureBase,car.presentation.illuminatedChunks(),true);
        mesh_.originalCar(car.plate.model,car.plate.assembly(),position,yaw,pitch,roll,car.textureBase+unsigned(car.textures.size()),{},true);
        for(auto i=carBegin;i<mesh_.ranges.size();++i)if(!mesh_.ranges[i].emissive)mesh_.ranges[i].carLighting=slot+1;
        carLights[slot]=original::originalCarLighting();
        carLights[slot].ambient=courseLights_.ambient;
        original::publishOriginalCarLight(carLights[slot],positions_[cursor.frame].lightMatrices[slot]);
        original::setOriginalCarLightEnabled(carLights[slot],original::OriginalDemoData::lightEffectsOn(timeline,slot));
    }
    lighting_=original::composeOriginalRaceLighting(courseLights_,carLights[0],carLights[1],{3,0,true,false});
    for(unsigned slot=0;slot<2;++slot){
        if(!original::OriginalDemoData::lightEffectsOn(timeline,slot))continue;
        auto& vertices=projectedModel_.chunks[0].batches[0].vertices;
        const auto& prepared=positions_[cursor.frame].projectedHeadlights[slot];
        std::copy(prepared.begin(),prepared.end(),vertices.begin());
        const auto begin=mesh_.ranges.size();mesh_.originalWorldChunk(projectedModel_,0,projectedTextureBase_);
        for(auto i=begin;i<mesh_.ranges.size();++i)mesh_.ranges[i].emissive=true;
        const auto& actor=data.actor(cursor.frame,slot);
        const Vec3 target=eye-Vec3{camera[8],camera[9],camera[10]},up{camera[4],camera[5],camera[6]};
        glare_.append(mesh_,shot.cars[slot],positions_[cursor.frame].body[slot],
            wrapAngle(actor.f(28)+pi),-actor.f(24),-actor.f(32),true,
            positions_[cursor.frame].headlights[slot].fraction,eye,target,up,glareTextureBase_,1);
    }
    meshFrame_=cursor.frame;meshTimeline_=timeline;return mesh_;
}
}
