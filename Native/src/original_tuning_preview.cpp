#include "original_tuning_preview.h"
#include "car_catalog.h"
#include <cmath>
#include <stdexcept>
namespace idas3::original {
namespace {
NativeAssembly instance(unsigned chunk,const OriginalMatrix& matrix){
    NativeAssembly out;NativeModelInstance item;item.chunk=chunk;
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)item.transform[row*4+col]=matrix.elements[col*4+row];
    out.instances.push_back(item);return out;
}
}
void OriginalTuningPreviewPresentation::load(const std::filesystem::path& root,const OriginalBattleProfile& profile,bool timeAttackRanking,bool continueScreen){
    timeAttackRanking_=timeAttackRanking;continueScreen_=continueScreen;
    // Continue's close source transform needs its own host presentation camera,
    // rather than the result camera at the origin. Keep the full rotating car
    // below the choice panel with the existing 45-degree lens.
    eye=continueScreen?Vec3{0,.65f,4.f}:Vec3{0,0,0};
    target=eye+Vec3{0,0,-1};
    carId_=profile.u(16);weather_=profile.u(32);const auto folder=originalCarFolders.at(carId_);
    const auto base=root/"data/original_models"/folder;
    const auto groundPath=root/(timeAttackRanking?"data/original_assets/attract/ranking/s_ground":"data/original_assets/tuning/selcrs2");
    ground_=NativeModel::load(groundPath/(timeAttackRanking?"s_ground.idasmesh":"selcrs2.idasmesh"));
    groundTextures_=NativeTextureBank::load(groundPath/"textures/textures.idastex");
    environmentTextures_=NativeTextureBank::load(root/"data/original_assets/tuning/environment/textures.idastex");
    baseCar_=NativeModel::load(base/(std::string(folder)+".idasmesh"));
    carTextures_=NativeTextureBank::load(root/"data/original_assets/cars"/folder/"textures/textures.idastex");
    const auto plateName=carId_==30?"numberplate_y":"numberplate";
    plate_=NativeModel::load(root/"data/original_models/numberplate"/(std::string(plateName)+".idasmesh"));
    plateTextures_=NativeTextureBank::load(root/"data/original_assets/numberplate"/plateName/"textures/textures.idastex");
    trig_=OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
    input_=OriginalCarAssemblyInput();input_.appearance=originalPlayerAppearanceConfig(profile,0);
    input_.overlayLayers=2; //07812A, while078132 clears dynamic car effects.
    input_.primaryUsesCurrent=input_.secondaryUsesCurrent=true;
    parts_=OriginalCarParts::load(base/"assembly_parts.bin");
    materials_=OriginalCarMaterialRebuild::load(base/"material_layout.bin",carId_);
    context_={};context_.semanticChunks=materials_->semanticChunks();context_.carChunkCount=materials_->chunks().size();
    context_.digits=originalTuningPreviewPlateDigits(profile);
    //077EE2 normalizes incoming(.4,-1,-1);077FC4 sets RGB.7,
    //077FFC..078002 writes ambient.3 before053480 submits the light chain.
    //054380 copies light+44/+48/+52 directly to its descriptor; current
    //XF does not rotate this light when078720 submits it through053480.
    const auto incoming=originalTuningPreviewIncomingLight();
    lighting.parameters={{-incoming[0],-incoming[1],-incoming[2]},{.7f,.7f,.7f},76.f/255.f,2.f};
    state_={};visible_=state_;rebuild();advanceOriginalCarHeadlights(input_);++textureRevision_;meshDirty_=true;
}
void OriginalTuningPreviewPresentation::rebuild(){
    if(!materials_)throw std::logic_error("Original tuning preview has not loaded");
    materials_->rebuild(input_.appearance,weather_);car_=materials_->apply(baseCar_);
    //026CBC finds the texture handle in the last GMP of semantic140.
    //029DA0(selector3) replaces that secondary layer's image with the source
    //128x128 menu environment. Copied secondary models retain that binding.
    const auto environmentChunk=context_.semanticChunks.at(140);
    if(environmentChunk>=0){
        const auto& records=materials_->chunks().at(unsigned(environmentChunk)).materials;
        if(records.empty())throw std::runtime_error("Original preview environment material missing");
        const auto texture=records.back().words[9];
        if(texture==0xffffffffu||texture>=carTextures_.size())throw std::runtime_error("Original preview environment texture bound");
        for(unsigned slot=140;slot<=186;++slot)if(const auto chunk=context_.semanticChunks[slot];chunk>=0)
            for(auto& batch:car_.chunks.at(unsigned(chunk)).batches)if(batch.material[9]==texture)
                batch.material[9]=unsigned(carTextures_.size()+plateTextures_.size());
    }
    //077C80 calls029000 with the car ID, rather than the race player's -1.
    input_.visibility=originalCarVisibility(input_.appearance,std::int32_t(carId_));
    input_.wheelOffsets=originalCarWheelOffsets(input_.appearance);meshDirty_=true;
}
void OriginalTuningPreviewPresentation::consume(std::span<const OriginalTuningPreviewEvent> events,
    const OriginalBattleProfile& profile,const OriginalTuningData& data,OriginalTuningChildKind kind){
    if(profile.u(16)!=carId_)throw std::logic_error("Original tuning preview car changed during its visit");
    bool changed=false;
    for(const auto& event:events){
        switch(event.kind){
        case OriginalTuningPreviewEvent::Kind::startPreview:
            if(const auto focus=originalTuningPreviewFocus(profile,data,kind))state_.focus=*focus;
            break;
        case OriginalTuningPreviewEvent::Kind::focusZero:state_.focus=0;break;
        case OriginalTuningPreviewEvent::Kind::resetFocus:state_.focus=11;break;
        case OriginalTuningPreviewEvent::Kind::carCall:
            if(event.call.address!=0x0c029040){
                applyOriginalCarAppearanceCall(input_.appearance,event.call.address,event.call.argument5,event.call.argument6);changed=true;
            }
            break;
        }
    }
    if(changed)rebuild();
    visible_=state_;meshDirty_=true;advanceOriginalTuningPreview(state_);
}
bool OriginalTuningPreviewPresentation::upload(Renderer& renderer)const{
    return renderer.loadTextures(groundTextures_)&&renderer.loadTextures(carTextures_,true)&&renderer.loadTextures(plateTextures_,true)&&renderer.loadTextures(environmentTextures_,true);
}
const Mesh& OriginalTuningPreviewPresentation::mesh(){
    if(!meshDirty_)return mesh_;
    mesh_.vertices.clear();mesh_.ranges.clear();const auto scene=originalTuningPreviewScene(carId_,visible_.angle,trig_,continueScreen_?2u:timeAttackRanking_?1u:0u);
    mesh_.originalCar(ground_,instance(0,scene.background),{},0,0);
    mesh_.originalCar(ground_,instance(3,scene.shadow),{},0,0);
    const unsigned carBase=unsigned(groundTextures_.size()),plateBase=carBase+unsigned(carTextures_.size());
    for(unsigned pass=0;pass<2;++pass){
        Mesh carMesh;context_.current=pass?scene.reflection:scene.car;
        const auto frame=originalCarRenderFrame(originalCarAssemblyPose(input_,parts_),context_,trig_);
        for(const auto& draw:frame.items)if(draw.isGeometry()){
            if(draw.bank==OriginalCarRenderBank::wheelBlur)throw std::logic_error("Stationary tuning car emitted wheel blur");
            const bool plate=draw.bank==OriginalCarRenderBank::numberPlate;
            carMesh.originalCar(plate?plate_:car_,instance(draw.chunk,draw.matrix),{},0,0,0,plate?plateBase:carBase,{},true,true);
        }
        if(pass){const auto light=lighting.parameters.directionToLight;
            for(auto& range:carMesh.ranges)range.originalLightDirection=std::array{light.x,-light.y,light.z};}
        mesh_.append(carMesh);
    }
    meshDirty_=false;return mesh_;
}
}
