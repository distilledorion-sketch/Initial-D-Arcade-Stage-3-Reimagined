#pragma once
#include "original_car_assembly.h"
#include "original_matrix.h"
#include "native_assets.h"

namespace idas3::original {
enum class OriginalCarRenderBank {car,numberPlate,wheelBlur};
struct OriginalCarRenderContext {
    OriginalMatrix current=originalIdentityMatrix();
    OriginalMatrix primary=originalIdentityMatrix(),secondary=originalIdentityMatrix();
    std::array<int,212> semanticChunks;
    std::size_t carChunkCount=0;
    std::array<std::uint8_t,5> digits{2,2,9,3,6};
    std::uint32_t wheelBlurDiffuse=0x00ffffff;
    OriginalCarRenderContext(){semanticChunks.fill(-1);}
};
// State submissions stay in the ordered stream: the graphics consumer owns
// the original per-list matrix state. matrix is the CPU matrix at submission,
// not an assertion that every source graphics mode uses it identically.
struct OriginalCarRenderItem {
    CarAssemblyCommandKind operation{};
    OriginalCarRenderBank bank=OriginalCarRenderBank::car;
    std::uint32_t chunk=0xffffffffu;
    OriginalMatrix matrix;
    std::array<std::uint32_t,4> parameters{};
    std::uint32_t materialDiffuse=0;
    bool isGeometry()const;
};
struct OriginalCarRenderFrame {
    std::vector<OriginalCarRenderItem> items;
    OriginalMatrix finalMatrix;
    std::uint32_t wheelBlurDiffuse=0;
    // Native row-major instances for consumers of an individual geometry
    // bank. Full graphics-state fidelity requires consuming items in order.
    NativeAssembly geometry(OriginalCarRenderBank bank)const;
};
// Replays an already built assembly without advancing headlight or car state.
// Executes the source matrix operations, slot resolver, number-plate helper
// and wheel-blur placement/alpha arithmetic using ordinary native objects.
OriginalCarRenderFrame originalCarRenderFrame(std::span<const CarAssemblyCommand>,
    const OriginalCarRenderContext&,const OriginalFscaTable&);
}
