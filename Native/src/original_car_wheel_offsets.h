#pragma once
#include "original_car_appearance_config.h"
namespace idas3::original {
struct OriginalCarWheelOffsets {float front=0,rear=0;};
//029AD0..029B66 stores ACar318/31C.026D80 adds these to authored
// wheel X positions, with the original per-side transforms.
OriginalCarWheelOffsets originalCarWheelOffsets(const OriginalCarAppearanceConfig&);
}
