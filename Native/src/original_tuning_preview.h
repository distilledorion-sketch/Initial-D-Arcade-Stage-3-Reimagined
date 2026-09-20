#pragma once
#include "original_tuning_preview_state.h"
#include "original_result_tuning_visit.h"
#include "original_car_material_rebuild.h"
#include "original_car_render_frame.h"
#include "renderer.h"
namespace idas3::original {
// Result-owned preview, separate from the racing model and saved profile.
class OriginalTuningPreviewPresentation {
public:
    void load(const std::filesystem::path& root,const OriginalBattleProfile&,bool timeAttackRanking=false,bool continueScreen=false);
    // Exactly once per original owner tick, including pre-child result frames.
    // Applies ordered source events, captures this frame's visible pose, then
    // advances078680. mesh() and repeated host renders never advance time.
    void consume(std::span<const OriginalTuningPreviewEvent>,const OriginalBattleProfile&,
        const OriginalTuningData&,OriginalTuningChildKind);
    const Mesh& mesh();
    bool upload(Renderer&)const;
    const NativeTextureBank& groundTextures()const{return groundTextures_;}
    const NativeTextureBank& carTextures()const{return carTextures_;}
    const NativeTextureBank& plateTextures()const{return plateTextures_;}
    const NativeTextureBank& environmentTextures()const{return environmentTextures_;}
    std::uint64_t textureRevision()const{return textureRevision_;}
    const OriginalTuningPreviewState& visibleState()const{return visible_;}
    const OriginalTuningPreviewState& nextState()const{return state_;}
    const OriginalCarAppearanceConfig& appearance()const{return input_.appearance;}
    OriginalShowroomLighting lighting;
    Vec3 eye{0,0,0},target{0,0,-1};
    static constexpr float verticalFieldOfView=pi/4,aspect=4.f/3.f;
    static constexpr float nearClip=.01f,farClip=10000.f;
private:
    NativeModel ground_,baseCar_,car_,plate_;
    NativeTextureBank groundTextures_,carTextures_,plateTextures_,environmentTextures_;
    OriginalFscaTable trig_;
    OriginalCarAssemblyInput input_;
    OriginalCarParts parts_;
    OriginalCarRenderContext context_;
    std::optional<OriginalCarMaterialRebuild> materials_;
    OriginalTuningPreviewState state_,visible_;
    unsigned carId_=~0u,weather_=0;
    std::uint64_t textureRevision_=0;
    bool meshDirty_=true;
    bool timeAttackRanking_=false;
    bool continueScreen_=false;
    Mesh mesh_;
    void rebuild();
};
}
