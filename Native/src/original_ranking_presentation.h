#pragma once
#include "original_ranking_playback.h"
#include "original_ranking_scene_setup.h"
#include "original_car_material_rebuild.h"
#include "original_car_render_frame.h"
#include "renderer.h"
#include <optional>

namespace idas3::original {
// Presentation has no clock, profile writes, or renderer/device ownership.
// Upload ground/car/plate/environment banks after textureRevision changes.
class OriginalRankingPresentation {
public:
    void load(const std::filesystem::path& root);
    void synchronize(const OriginalRankingPlayback&);
    const Mesh& mesh(const OriginalRankingPlayback&);
    const std::vector<std::uint32_t>& paint(int width,int height,
        const OriginalRankingPlayback&,const OriginalRankingRecords&);
    const NativeTextureBank& groundTextures()const{return groundTextures_;}
    const NativeTextureBank& carTextures()const{return carTextures_;}
    const NativeTextureBank& plateTextures()const{return plateTextures_;}
    const NativeTextureBank& environmentTextures()const{return environmentTextures_;}
    std::uint64_t textureRevision()const{return textureRevision_;}
    OriginalShowroomLighting lighting=OriginalRankingSceneSetup::lighting();
    static constexpr Vec3 eye=OriginalRankingSceneSetup::eye,target=OriginalRankingSceneSetup::target;
    static constexpr float verticalFieldOfView=OriginalRankingSceneSetup::verticalFieldOfView;
    static constexpr float aspect=OriginalRankingSceneSetup::aspect;
    static constexpr float nearClip=OriginalRankingSceneSetup::nearClip,farClip=OriginalRankingSceneSetup::farClip;
private:
    std::filesystem::path root_;
    OriginalRankingBoard board_;
    OriginalFscaTable trig_;
    NativeModel ground_,car_,plate_;
    NativeTextureBank groundTextures_,carTextures_,plateTextures_,environmentTextures_;
    OriginalCarAssemblyInput input_;
    OriginalCarParts parts_;
    OriginalCarRenderContext context_;
    std::optional<OriginalCarMaterialRebuild> materials_;
    unsigned carId_=~0u,appearance_=~0u;
    std::uint64_t textureRevision_=0,meshTick_=~0ull,meshResourceRevision_=~0ull;
    Mesh mesh_;
    std::vector<std::uint32_t> pixels_;
    std::uint64_t paintKey_=~0ull;
    int paintWidth_=0,paintHeight_=0;
};
}
