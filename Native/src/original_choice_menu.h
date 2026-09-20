#pragma once
#include "native_assets.h"
#include <cstdint>
#include <vector>

namespace idas3::original {
enum class OriginalChoiceScreen { Transmission, Route, Weather, Time };
enum class OriginalChoiceColors { Source, Selected, SelectedWeatherTime, InactiveTransmission, InactiveRoute, InactiveWeatherTime };
struct OriginalChoiceMenuState {
    OriginalChoiceScreen screen=OriginalChoiceScreen::Transmission;
    int selected=0; // AT/forward/dry/day=0; MT/reverse/wet/night=1.
    int course=3;
    float confirmationPhase=0;
    std::uint32_t frame=0;
};
struct OriginalChoiceDraw {
    std::uint32_t selector=0; // Original bank<<16 | chunk.
    float x=0,y=0,z=0,scale=1; // Original model coordinates.
    OriginalChoiceColors colors=OriginalChoiceColors::Source;
};
// Recovered direct commands from1B7CC0,197E00,198880,1992E0.
// Recursive1B8FE0 components and live3D car are separate caller layers.
std::vector<OriginalChoiceDraw> originalChoiceMenuDraws(const OriginalChoiceMenuState& state);
// Shared parent199F60 layer, active under all TA route/weather/time screens.
std::vector<OriginalChoiceDraw> originalChoiceCourseUnderlay(int course);
// Original1B8240 plus each screen constructor's1B81C0 color buffer.
NativeModelChunk materializeOriginalChoiceColors(const NativeModelChunk& chunk,OriginalChoiceColors colors);
const char* originalChoiceBankName(std::uint32_t bank);
}
