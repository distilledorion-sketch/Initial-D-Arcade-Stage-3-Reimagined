#pragma once
#include "original_demo_data.h"
#include "course_scene_catalog.h"
#include "car_presentation.h"
#include "original_number_plate.h"
#include "renderer.h"
#include <map>

namespace idas3 {
// Immutable recorded motion/cameras with independently owned source assets.
// No simulation, driver profile, input, or audio state belongs to this renderer.
class OriginalDemoPresentation {
public:
    void load(const std::filesystem::path& root,const original::OriginalDemoData&);
    bool upload(Renderer&)const;
    const Mesh& mesh(const original::OriginalDemoData&,original::OriginalDemoCursor,unsigned sourceTimeline);
    std::size_t sceneIndex(unsigned frame)const{return positions_.at(frame).pathIndex;}
    Vec3 cameraOffset(unsigned frame)const{return positions_.at(frame).cameraOffset;}
private:
    struct Car {
        NativeModel model;NativeTextureBank textures;
        CarPresentation presentation;OriginalNumberPlate plate;
        unsigned textureBase=0;
        Vec3 low,high;
    };
    struct Positions {std::array<Vec3,2> body{};std::array<OriginalHeadlightState,2> headlights;std::size_t pathIndex=0;Vec3 cameraOffset;};
    OriginalCourseScene scene_;
    CourseMeshCache courseCache_;
    std::map<unsigned,Car> cars_;
    std::vector<Positions> positions_;
    Mesh mesh_;
    unsigned meshFrame_=~0u,meshTimeline_=~0u;
};
}
