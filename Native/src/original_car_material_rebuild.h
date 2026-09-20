#pragma once
#include "native_assets.h"
#include "original_car_appearance_config.h"

namespace idas3::original {
// Ordinary native material records. GMP sharing between draw batches is
// explicit; the 47 original copied models have independent material storage.
class OriginalCarMaterialRebuild {
public:
    struct Material {std::uint32_t sourceOffset=0;std::array<std::uint32_t,16> words{};};
    struct Batch {std::uint32_t sourceOffset=0,material=0;std::array<std::uint32_t,8> words{};};
    struct Chunk {std::uint32_t sourceOffset=0,sourceSize=0;std::vector<Material> materials;std::vector<Batch> batches;};
    struct State {
        std::array<std::uint32_t,8> paintMask{};
        std::array<std::uint32_t,3> rgb{};
        std::uint32_t specular=0,gloss=0;
        float bodyAlpha=0,glassAlpha=0,bodyShadowAlpha=0,glassShadowAlpha=0;
    };
    static OriginalCarMaterialRebuild load(const std::filesystem::path&,unsigned car);
    //0267C0's native constructor, then02988E..029AD0 on every part rebuild.
    // Configuration dirty state is consumed exactly by029E60.
    void rebuild(OriginalCarAppearanceConfig&,std::uint32_t profileCondition);
    // Produces an independently owned render model with original copied chunks
    // appended. Source vertices, UVs and indices remain exactly as imported.
    NativeModel apply(const NativeModel& authored)const;
    const State& state()const{return state_;}
    const std::vector<Chunk>& chunks()const{return chunks_;}
    const std::array<int,212>& semanticChunks()const{return slots_;}
    std::size_t authoredChunkCount()const{return authoredCount_;}
private:
    struct Reference {unsigned chunk=0,material=0;};
    void initialize();
    void alpha(unsigned slot,float value);
    unsigned car_=0;
    std::size_t authoredCount_=0;
    std::array<int,212> slots_{};
    std::array<int,47> copySources_{};
    std::vector<Chunk> authored_,chunks_;
    std::vector<Reference> paint_,gloss_;
    State state_;
};
}
