#include "unity_ui_capture.h"
#include "original_ranking_presentation.h"
#include "car_catalog.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace idas3::original {
namespace {
NativeAssembly instance(unsigned chunk,const OriginalMatrix& matrix){
    NativeAssembly out;NativeModelInstance item;item.chunk=chunk;
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)item.transform[r*4+c]=matrix.elements[c*4+r];
    out.instances.push_back(item);return out;
}
}
void OriginalRankingPresentation::load(const std::filesystem::path& root){
    root_=root;board_.load(root);trig_=OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
    auto base=root/"data/original_assets/attract/ranking/s_ground";
    ground_=NativeModel::load(base/"s_ground.idasmesh");
    groundTextures_=NativeTextureBank::load(base/"textures/textures.idastex");
    // Same029DA0(selector3) source image used by the result preview.
    environmentTextures_=NativeTextureBank::load(root/"data/original_assets/tuning/environment/textures.idastex");
    if(environmentTextures_.size()!=1||environmentTextures_.at(0).width!=128||environmentTextures_.at(0).height!=128)
        throw std::runtime_error("Original ranking environment selector3 image");
    carId_=appearance_=~0u;materials_.reset();meshTick_=meshResourceRevision_=paintKey_=~0ull;
    ++textureRevision_;
}
void OriginalRankingPresentation::synchronize(const OriginalRankingPlayback& playback){
    if(!playback.resources.hasCar)return;
    const auto id=playback.resources.car,packed=playback.resources.packedAppearance;
    if(carId_==id&&appearance_==packed)return;
    const auto folder=originalCarFolders.at(id);const auto base=root_/"data/original_models"/folder;
    if(carId_!=id){
        carTextures_=NativeTextureBank::load(root_/"data/original_assets/cars"/folder/"textures/textures.idastex");
        const auto name=id==30?"numberplate_y":"numberplate";
        plate_=NativeModel::load(root_/"data/original_models/numberplate"/(std::string(name)+".idasmesh"));
        plateTextures_=NativeTextureBank::load(root_/"data/original_assets/numberplate"/name/"textures/textures.idastex");
        ++textureRevision_;
    }
    OriginalCarAssemblyInput fresh;input_=fresh;input_.appearance=OriginalCarAppearanceConfig(id);
    // Replay the verified setters, including wheel/car variant remapping.
    OriginalRankingResourceState resource;resource.phase=2;resource.car=id;resource.packedAppearance=packed;
    OriginalRankingPageState page;OriginalRankingCarSource source;source.car=id;
    const auto commands=stepOriginalRankingResources(resource,page,source).carCommands;
    for(const auto& command:commands)if(command.address>=0x0c0283c0&&command.address<=0x0c0287a0)
        applyOriginalCarAppearanceCall(input_.appearance,command.address,command.argument);
    materials_=OriginalCarMaterialRebuild::load(base/"material_layout.bin",id);
    // The source reads the global profile weather here; attract runs against
    // a separate fresh cabinet profile (0), never the user's saved driver.
    materials_->rebuild(input_.appearance,0);
    car_=materials_->apply(NativeModel::load(base/(std::string(folder)+".idasmesh")));
    input_.visibility=originalCarVisibility(input_.appearance,std::int32_t(id)); //029000(id)
    input_.wheelOffsets=originalCarWheelOffsets(input_.appearance);
    input_.primaryUsesCurrent=input_.secondaryUsesCurrent=true; //02640E/026416
    advanceOriginalCarHeadlights(input_); //Lights stay off; this source state is stable thereafter.
    parts_=OriginalCarParts::load(base/"assembly_parts.bin");
    context_.semanticChunks=materials_->semanticChunks();context_.carChunkCount=materials_->chunks().size();
    // The recovered resource commands include029DA0(selector3).026CBC
    // obtains its texture handle from the last GMP of semantic140; the
    // secondary copied models retain that shared handle. Only that binding
    // changes. Other car textures, material parameters and the ground stay
    // authored, and reflected core geometry retains the source layer mask0.
    const auto environmentChunk=context_.semanticChunks.at(140);
    if(environmentChunk>=0){
        const auto& records=materials_->chunks().at(unsigned(environmentChunk)).materials;
        if(records.empty())throw std::runtime_error("Original ranking environment material missing");
        const auto texture=records.back().words[9];
        if(texture==0xffffffffu||texture>=carTextures_.size())throw std::runtime_error("Original ranking environment texture bound");
        for(unsigned slot=140;slot<=186;++slot)if(const auto chunk=context_.semanticChunks[slot];chunk>=0)
            for(auto& batch:car_.chunks.at(unsigned(chunk)).batches)if(batch.material[9]==texture)
                batch.material[9]=unsigned(carTextures_.size()+plateTextures_.size());
    }
    carId_=id;appearance_=packed;meshTick_=meshResourceRevision_=~0ull;
}
const Mesh& OriginalRankingPresentation::mesh(const OriginalRankingPlayback& playback){
    synchronize(playback);
    if(meshTick_==playback.ticks&&meshResourceRevision_==playback.resourceRevision)return mesh_;
    mesh_.vertices.clear();mesh_.ranges.clear();
    const auto scene=originalRankingScene(playback.resources.car,playback.events.drawYawUnits,playback.events.slideX,trig_);
    if(playback.events.drawCar){
        const unsigned carTextureBase=unsigned(groundTextures_.size());
        const unsigned plateTextureBase=carTextureBase+unsigned(carTextures_.size());
        mesh_.originalCar(ground_,instance(scene.shadowChunk,scene.shadow),{},0,0);
        context_.digits=playback.plateDigits;
        for(unsigned pass=0;pass<2;++pass){
            input_.overlayLayers=pass?0:playback.drawLayerMask;
            context_.current=pass?scene.reflection:scene.car;
            const auto frame=originalCarRenderFrame(originalCarAssemblyPose(input_,parts_),context_,trig_);
            // Keep source submission order across car and number-plate banks.
            for(const auto& draw:frame.items)if(draw.isGeometry()){
                if(draw.bank==OriginalCarRenderBank::wheelBlur)throw std::logic_error("Stationary ranking car emitted wheel blur");
                const bool isPlate=draw.bank==OriginalCarRenderBank::numberPlate;
                mesh_.originalCar(isPlate?plate_:car_,instance(draw.chunk,draw.matrix),{},0,0,0,isPlate?plateTextureBase:carTextureBase,{},true,true);
            }
        }
    }
    mesh_.originalCar(ground_,instance(scene.backgroundChunk,scene.background),{},0,0);
    meshTick_=playback.ticks;meshResourceRevision_=playback.resourceRevision;return mesh_;
}
const std::vector<std::uint32_t>& OriginalRankingPresentation::paint(int width,int height,
    const OriginalRankingPlayback& playback,const OriginalRankingRecords& records){
    if(width<=0||height<=0)throw std::invalid_argument("Ranking canvas dimensions");
    const auto& page=playback.page;const unsigned frame=std::min(playback.boardFrame,83u);
    const std::uint64_t key=frame+(std::uint64_t(page.courseIndex)<<8)+(std::uint64_t(page.conditionIndex)<<12)
        +(std::uint64_t(page.wet)<<18)+(std::uint64_t(page.detailMode)<<20)+(std::uint64_t(page.detailPage)<<24);
    if(key==paintKey_&&width==paintWidth_&&height==paintHeight_)return pixels_;
    NativeImage source;source.width=640;source.height=480;source.argb.resize(640*480);unityUiClear(source.argb.data(),640,480);
    board_.paint(source.argb,640,480,records,originalRankingCourse(page),page.conditionIndex&1,page.wet,frame,page.detailMode!=0,page.detailPage);
    pixels_.assign(std::size_t(width)*height,0);unityUiClear(pixels_.data(),width,height);
    const float scale=std::min(float(width)/640.f,float(height)/480.f);
    compositeImage(pixels_,width,height,source,(width-640*scale)*.5f,(height-480*scale)*.5f,640*scale,480*scale);
    paintKey_=key;paintWidth_=width;paintHeight_=height;return pixels_;
}
}

