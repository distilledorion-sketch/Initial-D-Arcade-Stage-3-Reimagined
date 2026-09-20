#pragma once
#include "math_types.h"
#include "original_car_dimensions.h"
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>

namespace idas3 {
// Retail car-selection path: 12DC60 -> 10F040, 12E900 -> 10F420,
// and 12EB20 -> 10F840/10F8C0. Coordinates are original model coordinates.
struct OriginalShowroomFrame {
    Vec3 eye,target,carPosition;
    float carYaw=0;
    float verticalFieldOfView=pi/4;
};
class OriginalShowroom {
public:
    void reset() { yaw_=0; }
    // The original updates every car, including unselected cars. Preserve this
    // common phase across selection changes. Advance at 60Hz, not render rate.
    void advanceTicks(std::uint32_t count=1) {
        while(count--) yaw_+=std::bit_cast<float>(0x3c449ba6u); // 10F45C
    }
    OriginalShowroomFrame frame(unsigned originalCarId) const {
        if(originalCarId>=originalCarRideHeightWords.size())throw std::out_of_range("Original showroom car ID");
        return { {from(0xc001a36e),from(0x3f7c6fbd),from(0x40c1d2f2)},
                 {from(0xbe96872b),from(0x3f9a6e98),from(0xbf0ed917)},
                 {0,originalCarRideHeight(originalCarId),0},yaw_,pi/4 };
    }
    float yaw() const { return yaw_; }
    // Transmission selector124C80 uses its own freshly initialized single-car
    // pose (10F220) and the same tick delta, with this distinct source camera.
    OriginalShowroomFrame transmissionFrame(unsigned originalCarId) const {
        auto result=frame(originalCarId);
        result.eye={from(0xbfc425af),from(0x3fbf414a),from(0x409ea177)};
        result.target={from(0x3e4bedfa),from(0x3f7d44bb),from(0xbf0ed917)};
        return result;
    }
private:
    static constexpr float from(std::uint32_t word){return std::bit_cast<float>(word);}
    float yaw_=0;
};
}
