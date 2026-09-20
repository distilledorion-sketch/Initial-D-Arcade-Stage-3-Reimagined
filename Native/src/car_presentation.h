#pragma once
#include "native_assets.h"
#include "original_matrix.h"
#include "original_headlights.h"
#include "car_wheel_pose.h"
#include "original_car_material_rebuild.h"
#include "original_car_render_frame.h"
#include <optional>

namespace idas3 {
class CarPresentation {
public:
    static CarPresentation load(const std::filesystem::path& root,unsigned carId,std::size_t chunkCount,unsigned factoryColor=0);
    static CarPresentation loadRival(const std::filesystem::path& root,unsigned carId,unsigned enemyId,std::size_t chunkCount);
    static CarPresentation loadPlayerProfile(const std::filesystem::path& root,const original::OriginalBattleProfile&,unsigned materialVariant=0);
    // Build a private display from explicit part setters without granting those
    // parts to a saved driver (for example the tuning-package showroom).
    static CarPresentation loadConfiguredAppearance(const std::filesystem::path& root,const original::OriginalBattleProfile&,original::OriginalCarAppearanceConfig,
        unsigned overlayLayers=0,std::optional<unsigned> environmentTexture=std::nullopt);
    // Advance once per original 60 Hz presentation frame, independently of render FPS.
    // The first requested state initializes immediately, including fresh night cars.
    void advanceOriginalFrame(bool lightsOn);
    void resetHeadlights(){headlights_.reset();}
    const OriginalHeadlightState& headlightState()const{return headlights_;}
    // Recorded attract frames restore their already evaluated60Hz motor state.
    void restoreHeadlightState(const OriginalHeadlightState& state){headlights_=state;}
    // Updates original wheel/caliper, headlight and rear-lamp instances.
    const NativeAssembly& pose(const CarWheelPose& wheels,bool lightsOn=false,bool braking=false);
    const NativeAssembly& assembly()const{return posedAssembly_;}
    const NativeAssembly& profilePlateAssembly()const{return profilePlateAssembly_;}
    bool usesPlayerProfile()const{return profileMaterials_.has_value();}
    std::size_t lightingAttachmentCount(bool lightsOn,bool braking)const{return lightingStates_[unsigned(braking)+2*unsigned(lightsOn)].extras.instances.size();}
    std::size_t animatedInstanceCount()const{return programs_.size();}
    std::span<const std::uint32_t> illuminatedChunks()const{return illuminatedChunks_;}
    // Apply to this appearance's privately loaded model, before rendering.
    // Validates every captured source material before making any change.
    void applyMaterials(NativeModel& model)const;
    std::size_t materialPatchCount()const{return materials_.size();}
private:
    static CarPresentation loadAppearance(const std::filesystem::path& root,const std::filesystem::path& base,unsigned carId,std::size_t chunkCount);
    struct Operation {std::uint32_t kind=0;std::int32_t channel=0;std::array<float,16> values{};};
    struct Program {std::uint32_t instance=0;std::vector<Operation> operations;};
    struct MaterialPatch {std::uint32_t chunk=0,batch=0;std::array<std::uint32_t,24> before{},after{};};
    struct HeadlightProgram {std::uint32_t chunk=0;std::vector<Operation> operations;};
    struct LightingState {NativeAssembly extras;std::vector<std::uint32_t> order;};
    NativeAssembly assembly_;
    NativeAssembly posedAssembly_;
    std::array<LightingState,4> lightingStates_;
    original::OriginalFscaTable trig_;
    std::vector<Program> programs_;
    std::vector<MaterialPatch> materials_;
    OriginalHeadlightState headlights_;
    std::array<HeadlightProgram,2> headlightPrograms_;
    std::uint32_t headlightInstance_=0xffffffffu;
    std::array<int,5> lampChunks_{};
    std::size_t baseCount_=0,rearLampInstance_=0;
    std::vector<std::uint32_t> illuminatedChunks_;
    std::optional<original::OriginalCarMaterialRebuild> profileMaterials_;
    std::optional<unsigned> profileEnvironmentTexture_;
    original::OriginalCarAssemblyInput profileInput_;
    original::OriginalCarParts profileParts_;
    original::OriginalCarRenderContext profileContext_;
    NativeAssembly profilePlateAssembly_;
};
}
