#pragma once
#include "math_types.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace idas3 {
// 112260 constructor -> 112D40 -> 053480 light chain -> 1D3D00.
// Constants below describe world-space source lights before the hardware's
// view-space direction quantization. Only light1 has nonzero final RGB.
struct OriginalShowroomLighting {
    static constexpr std::array<std::uint32_t,8> glmWords={
        0x08000400,0x000f00b0,0x00030003,0xff262626,
        0xff000000,0x00030003,0xff262626,0xff000000};
    static constexpr Vec3 incomingDirection={
        std::bit_cast<float>(0xbe5dcf93u),
        std::bit_cast<float>(0xbf4ff29au),
        std::bit_cast<float>(0x3f0aa1bcu)};
    // Flycast ELAN negates authored incoming light direction for the shader.
    static constexpr Vec3 directionToLight={
        std::bit_cast<float>(0x3e5dcf93u),
        std::bit_cast<float>(0x3f4ff29au),
        std::bit_cast<float>(0xbf0aa1bcu)};
    static constexpr Vec3 color={1,1,1};
    // Source .15 is truncated to byte38 by 1D3D00, then decoded by ELAN.
    static constexpr float ambientBase=38.f/255.f;
    static constexpr float ambientOffset=0;
    static constexpr float diffuseSpecularFactor=2;
    static constexpr unsigned routing=1; // diffuse to base, specular to offset
    static constexpr unsigned diffuseMode=0,specularMode=0; // single sided
    static constexpr bool ambientMultipliesMaterial=true;
    static constexpr bool diffuseOverflowToSpecular=false;
    // Source scene owners select their own GLM light. Keep the captured
    // selection-screen defaults, while allowing ranking's separate setup.
    struct Parameters {
        Vec3 directionToLight=OriginalShowroomLighting::directionToLight;
        Vec3 color=OriginalShowroomLighting::color;
        float ambientBase=OriginalShowroomLighting::ambientBase;
        float diffuseSpecularFactor=OriginalShowroomLighting::diffuseSpecularFactor;
    } parameters;
    static float glossCoefficient(std::uint32_t gmpGlossWord,unsigned volume=0) {
        const auto value=(gmpGlossWord>>(volume?8:0))&255u;
        return std::ldexp(1.f+float(value&31u)/32.f,int(value>>5)-1);
    }
};
}
