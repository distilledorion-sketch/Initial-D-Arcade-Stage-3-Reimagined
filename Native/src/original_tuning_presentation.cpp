#include "unity_ui_capture.h"
#include "original_tuning_presentation.h"
#include <stdexcept>
namespace idas3 {
float originalTuningDescriptionSize(const original::OriginalTuningChild&s){
    return s.kind==original::OriginalTuningChildKind::basic&&s.car==19&&s.package==0&&s.selected==6?24.f:26.f;
}
OriginalTuningPresentation::OriginalTuningPresentation(const std::filesystem::path&root):ui_(OriginalTuningUi::load(root)){}
void OriginalTuningPresentation::begin(const original::OriginalTuningChild&s,const original::OriginalTuningData&data){
    using Kind=original::OriginalTuningChildKind;
    if(s.kind!=Kind::basic&&s.kind!=Kind::performance&&s.kind!=Kind::optionalPart)throw std::invalid_argument("Missing original tuning presentation child");
    data.car(s.car);description_={};description_.size=originalTuningDescriptionSize(s);
    if(s.kind==Kind::optionalPart)description_=OriginalTuningUi::optionalDescription(s,data);
    kind_=s.kind;car_=s.car;active_=true;
}
void OriginalTuningPresentation::consume(const original::OriginalTuningChildFrame&frame){
    if(!active_)throw std::logic_error("Original tuning presentation has not begun");
    if(frame.descriptionChanged){description_.address=frame.descriptionAddress;description_.x=frame.descriptionX;description_.y=frame.descriptionY;}
}
void OriginalTuningPresentation::clear(){description_={};kind_=original::OriginalTuningChildKind::none;active_=false;}
void OriginalTuningPresentation::validate(const original::OriginalTuningChild&s)const{
    if(!active_||s.kind!=kind_||s.car!=car_)throw std::logic_error("Original tuning presentation child changed without begin");
}
std::vector<OriginalTuningUiDraw> OriginalTuningPresentation::drawList(const original::OriginalTuningChild&s,const original::OriginalTuningData&data,std::uint32_t countdown)const{
    validate(s);return ui_.drawList(s,data,countdown);
}
void OriginalTuningPresentation::paintOverlay(std::span<std::uint32_t>pixels,int width,int height,const original::OriginalTuningChild&s,const original::OriginalTuningData&data,std::uint32_t countdown)const{
    validate(s);
    if(width<=0||height<=0||pixels.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid original tuning presentation canvas");
    if(width==640&&height==480){ui_.paintOverlay(pixels,width,height,s,data,description_,countdown);return;}
    // Rasterize the authored640x480 composition once before fitting it. This
    // preserves original glyph/pixel placement at every window aspect ratio.
    NativeImage source{640,480,std::vector<std::uint32_t>(640*480)};unityUiClear(source.argb.data(),640,480);
    ui_.paintOverlay(source.argb,640,480,s,data,description_,countdown);
    const float scale=std::min(float(width)/640,float(height)/480);
    compositeImage(pixels,width,height,source,(width-640*scale)*.5f,(height-480*scale)*.5f,640*scale,480*scale);
}
}
