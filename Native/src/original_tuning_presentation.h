#pragma once
#include "original_tuning_ui.h"
namespace idas3 {
//115B80 allocates the basic child text once; its size remains unchanged when
// same-threshold rows reveal further descriptions in that child.
float originalTuningDescriptionSize(const original::OriginalTuningChild&);
class OriginalTuningPresentation {
public:
    explicit OriginalTuningPresentation(const std::filesystem::path& root);
    void begin(const original::OriginalTuningChild&,const original::OriginalTuningData&);
    void consume(const original::OriginalTuningChildFrame&);
    void clear();
    bool active()const{return active_;}
    const OriginalTuningUiDescription& description()const{return description_;}
    std::vector<OriginalTuningUiDraw> drawList(const original::OriginalTuningChild&,const original::OriginalTuningData&,std::uint32_t sharedCountdown)const;
    // Paint is read-only and never advances the original60Hz child. Caller
    // owns the3D preview, full-screen owner fade and profile/input lifecycle.
    void paintOverlay(std::span<std::uint32_t>,int width,int height,const original::OriginalTuningChild&,const original::OriginalTuningData&,std::uint32_t sharedCountdown)const;
private:
    OriginalTuningUi ui_;
    OriginalTuningUiDescription description_;
    original::OriginalTuningChildKind kind_=original::OriginalTuningChildKind::none;
    unsigned car_=0;
    bool active_=false;
    void validate(const original::OriginalTuningChild&)const;
};
}
