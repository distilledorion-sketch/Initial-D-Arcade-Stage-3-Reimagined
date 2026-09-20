#pragma once
#include "native_assets.h"
#include "original_showroom.h"
#include "original_tuning_course_menu.h"
#include <memory>
#include <span>
#include <vector>

namespace idas3 {
struct OriginalTuningCourseOverlay {
    std::vector<std::uint32_t> pixels;
    // Additive RGB is already weighted by source alpha: composite ONE + ONE.
    bool additive=false;
};
struct OriginalTuningCourseDraw {
    unsigned bank=0,chunk=0;
    float x=0,y=0,z=0,scaleX=1,scaleY=1;
    int labelSelection=-1;
};
// Source129240/12BA00, V3 children1AF680 and1B0760. Advance once after
// each original60Hz controller update. All paint calls are read-only.
class OriginalTuningCoursePresentation {
public:
    static OriginalTuningCoursePresentation load(const std::filesystem::path& root);
    OriginalTuningCoursePresentation();
    ~OriginalTuningCoursePresentation();
    OriginalTuningCoursePresentation(OriginalTuningCoursePresentation&&) noexcept;
    OriginalTuningCoursePresentation& operator=(OriginalTuningCoursePresentation&&) noexcept;
    OriginalTuningCoursePresentation(const OriginalTuningCoursePresentation&);
    OriginalTuningCoursePresentation& operator=(const OriginalTuningCoursePresentation&);
    void reset(const original::OriginalTuningCourseMenu&,const original::OriginalBattleProfile&);
    // Neutral source wheel input is .5; callers with calibrated wheel data
    // may pass the original normalized0D43A0 value here.
    void advance(const original::OriginalTuningCourseMenu&,const original::OriginalBattleProfile&,float wheelPosition=.5f);
    OriginalShowroomFrame showroomFrame(unsigned car)const;
    std::uint32_t selectorPhase()const;
    std::uint64_t frames()const;
    std::vector<OriginalTuningCourseDraw> drawList(const original::OriginalTuningCourseMenu&,std::uint32_t countdown)const;
    void paintBackground(std::span<std::uint32_t> canvas,int width,int height)const;
    // Draw background, native car/shadow, these ordered passes, then the
    // controller's original whole-screen fade. Never fade only the car.
    std::vector<OriginalTuningCourseOverlay> overlays(int width,int height,
        const original::OriginalTuningCourseMenu&,std::uint32_t countdown)const;
    void paintCanvas(std::span<std::uint32_t> canvas,int width,int height,
        const original::OriginalTuningCourseMenu&,std::uint32_t countdown)const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
