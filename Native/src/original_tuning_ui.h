#pragma once
#include "original_tuning_child.h"
#include "native_assets.h"

namespace idas3 {
struct OriginalTuningUiDraw {
    enum class Bank { tune,shop,common,continuation } bank=Bank::shop;
    std::uint32_t index=0;
    float x=0,y=0,z=0,scaleX=1,scaleY=1;
    //1153C0 on a copied YES/NO model: -1 retains its source material.
    int choiceColors=-1;
};
struct OriginalTuningUiDescription {
    std::uint32_t address=0;
    float x=0,y=388,size=26;
};
struct OriginalTuningUiGlyph {std::uint32_t index=0;float x=0,y=0,z=0,size=26;};
class OriginalTuningUi {
public:
    static OriginalTuningUi load(const std::filesystem::path& gameRoot);
    static void applyChoiceColors(NativeModelChunk&,bool selected);
    static OriginalTuningUiDescription optionalDescription(const original::OriginalTuningChild&,const original::OriginalTuningData&);
    std::vector<OriginalTuningUiDraw> drawList(const original::OriginalTuningChild&,const original::OriginalTuningData&,std::uint32_t sharedCountdown=879)const;
    std::vector<OriginalTuningUiGlyph> descriptionGlyphs(const original::OriginalTuningData&,const OriginalTuningUiDescription&)const;
    void paint(std::span<std::uint32_t> argb,int width,int height,const original::OriginalTuningChild&,const original::OriginalTuningData&,const OriginalTuningUiDescription& = {},std::uint32_t sharedCountdown=879)const;
    // Straight-alpha carrier for composition over the live 3D preview.
    void paintOverlay(std::span<std::uint32_t> argb,int width,int height,const original::OriginalTuningChild&,const original::OriginalTuningData&,const OriginalTuningUiDescription& = {},std::uint32_t sharedCountdown=879)const;
private:
    void paintImpl(std::span<std::uint32_t>,int,int,const original::OriginalTuningChild&,const original::OriginalTuningData&,const OriginalTuningUiDescription&,std::uint32_t,bool)const;
    std::array<NativeModel,4> models_;
    std::array<NativeTextureBank,4> textures_;
    NativeTextureBank font_;
    std::array<std::uint16_t,9216> glyphMap_{};
    std::array<std::pair<std::uint16_t,float>,35> spacing_{};
};
}
