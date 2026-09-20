#pragma once
#include "original_course_lighting.h"

namespace idas3::original {
struct OriginalCarLighting {
    OriginalCourseLight ownSpot{}; // ACar+2544, shared by other arrays.
    OriginalLightVector ambient{.2f,.2f,.2f};
    float gain{1.f}; // ARRAY+88; independent of ambient.
};
struct OriginalCarLightingSetup {
    unsigned course{},numericRaceMode{2};
    bool night{},wet{};
};
struct OriginalCarAmbientInputs {
    OriginalLightVector courseAmbient{}; // Actual course+12/+16/+20.
    unsigned course{};
    bool wet{},night{},rival{},rivalLightBeforeRequest{};
    float signedAdvantage{}; // HUD+100, used only by night/rival/lamps-off.
};
struct OriginalRaceLightingSets {
    OriginalCourseLighting course,player,rival;
    bool hasRival{};
};
// Actual033A00 light construction: no light is initially in the car ARRAY.
OriginalCarLighting originalCarLighting();
// Header RGB consumed by063280/0638C0. Generic19AFA0 copies its authored
// ambient; Happo040D60 changes only the ARRAY and retains base header .3.
OriginalLightVector originalCourseCarAmbient(const OriginalCarLightingSetup&);
// Actual034840 tail, called at pose publication. Matrix is ACar+2404 visual
// WORLD matrix, before the body-only normal/ride-height displacement.
void publishOriginalCarLight(OriginalCarLighting&,const OriginalLightMatrix&);
// Actual035120/035200 embedded-light enable. This is outer byte81, not the
// model's headlight motor state. Publication and ambient precede the request.
void setOriginalCarLightEnabled(OriginalCarLighting&,bool);
void updateOriginalCarAmbient(OriginalCarLighting&,const OriginalCarAmbientInputs&);
// Happo040D60 point1068 is borrowed by car arrays, not its course ARRAY.
OriginalCourseLight originalHappoCarPointLight();
// Pure registration snapshot. courseBase is the course's current authored
// ARRAY before the once-only night registrations. Uses explicit numeric mode:
// modes2/3 exclude the rival; modes0/1 have the original two-car routing.
// A night car receives the OTHER car's spot; the course receives player then
// rival. All current shared enabled/color/pose values are copied at submission.
OriginalRaceLightingSets composeOriginalRaceLighting(const OriginalCourseLighting& courseBase,
    const OriginalCarLighting& player,const OriginalCarLighting& rival,
    const OriginalCarLightingSetup&);
} // namespace idas3::original
