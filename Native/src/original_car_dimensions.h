#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>
namespace idas3 {
//029EC0 reads this35-car table at2F4ED8. Live034C20 multiplies it by
//the source surface normal before multiplying the actor matrix.
inline constexpr std::array<std::uint32_t,35> originalCarRideHeightWords={0x3e947ae1u,0x3e947ae1u,0x3e947ae1u,0x3e99999au,0x3e99999au,0x3ea00d1bu,0x3ea3d70au,0x3ea8f5c3u,0x3ea8f5c3u,0x3e9eb852u,0x3ea3d70au,0x3e9eb852u,0x3e9eb852u,0x3e9eb852u,0x3e9eb852u,0x3e947ae1u,0x3e947ae1u,0x3e99999au,0x3e9eb852u,0x3e99999au,0x3e99999au,0x3ea3d70au,0x3ea3d70au,0x3ea3d70au,0x3e9eb852u,0x3e99999au,0x3e99999au,0x3e99999au,0x3e9eb852u,0x3e99999au,0x3e8a3d71u,0x3ea3d70au,0x3ea8f5c3u,0x3ea8f5c3u,0x3ea8f5c3u};
inline float originalCarRideHeight(unsigned carId){
    if(carId>=originalCarRideHeightWords.size())throw std::out_of_range("Original car ride-height ID");
    return std::bit_cast<float>(originalCarRideHeightWords[carId]);
}
}
