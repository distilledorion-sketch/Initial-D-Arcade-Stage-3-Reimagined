#pragma once
#include "original_car_visibility.h"
#include "original_car_wheel_offsets.h"
#include "original_headlights.h"
#include <filesystem>
#include <vector>

namespace idas3::original {
struct OriginalCarPartTransform {
    std::array<float,3> scale{},rotationDegrees{},translation{};
};
struct OriginalCarParts {
    std::array<OriginalCarPartTransform,23> transforms;
    static OriginalCarParts load(const std::filesystem::path&);
};
// Native render commands, not original instructions or a guest address space.
// Draw commands retain attempted slot and resolved semantic, so the material
// owner can select its independent copied chunks and omit absent geometry.
enum class CarAssemblyCommandKind {
    Push,Pop,LoadPrimary,LoadSecondary,Translate,Scale,RotateXPhase,
    RotateYPhase,RotateZPhase,RotateXRadians,RotateYRadians,
    DrawMaterial,DrawDirect,NumberPlate,WheelBlur,BodyEffect,WheelEffect,
    MainDrawState,LayerDrawState,LayerParameters
};
struct CarAssemblyCommand {
    CarAssemblyCommandKind kind{};
    std::array<std::uint32_t,4> words{};
    bool operator==(const CarAssemblyCommand&)const=default;
};
struct OriginalCarAssemblyInput {
    OriginalCarAppearanceConfig appearance;
    OriginalCarVisibility visibility;
    OriginalCarWheelOffsets wheelOffsets;
    bool lights=false,braking=false,hasParts=true;
    std::uint32_t overlayLayers=3,effects=0;
    bool primaryUsesCurrent=false,secondaryUsesCurrent=false;
    std::array<float,2> secondaryParameters{};
    std::array<float,3> effectParameters{};
    float steering=0;
    std::array<float,4> suspension{},spin{},wheelBlur{};
    // Advance exactly once per original presentation frame. Rendering an
    // already built command list does not update this state again.
    OriginalHeadlightState headlights;
};
std::vector<CarAssemblyCommand> originalCarAssemblyCommands(OriginalCarAssemblyInput&,const OriginalCarParts&);
void advanceOriginalCarHeadlights(OriginalCarAssemblyInput&);
// Render an already advanced presentation state at any host refresh rate.
std::vector<CarAssemblyCommand> originalCarAssemblyPose(const OriginalCarAssemblyInput&,const OriginalCarParts&);
}
