#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace idas3::original {
struct OriginalCollisionData {
    // Converted roads have finite outer strips; start their sweeps on the
    // previous road cell even when the destination lies beyond those strips.
    bool importedSweepFromPrevious=false;
    // RCL1 is already uncompressed despite the source .bin.nz suffix.
    std::vector<std::array<std::uint32_t,9>> materials;
    std::vector<std::array<std::uint32_t,8>> vertices;
    std::vector<std::array<std::int16_t,8>> triangles;
    std::vector<std::array<std::uint32_t,14>> coarseCells;
    static OriginalCollisionData load(const std::filesystem::path& source);
};
struct OriginalCollisionQuery {
    //0..8 normal;12..20 corrected hit;24 queryY-height or swept plane distance;
    //28 flags;
    //32..40 current query point;44..52 previous point;56/60 cached indices.
    std::array<std::uint32_t,16> words{};
    float f(std::size_t offset) const{return std::bit_cast<float>(words.at(offset/4));}
    std::uint32_t u(std::size_t offset) const{return words.at(offset/4);}
    void setf(std::size_t offset,float value){words.at(offset/4)=std::bit_cast<std::uint32_t>(value);}
    void setu(std::size_t offset,std::uint32_t value){words.at(offset/4)=value;}
};
struct OriginalTriangleSearchTrace {
    std::array<std::int32_t,100> indices0C99A904{};
    std::uint32_t count0C99AA94=0;
};
struct OriginalSurfaceScratch {
    // Original static84-byte record at0C99AA98.023A00 always returns null;
    //023A20 always supplies this record, so each query rebuilds coefficients.
    // Word0 triangle index,word1 signed area,words2..10 three edge vectors,
    // words11..20 the original ten cubic height-control coefficients.
    std::array<std::uint32_t,21> words{};
    float f(std::size_t offset) const{return std::bit_cast<float>(words.at(offset/4));}
    void setf(std::size_t offset,float value){words.at(offset/4)=std::bit_cast<std::uint32_t>(value);}
};
struct OriginalSweepScratch {
    // Original56-byte stack iterator from022640/022720. Active0, previousXZ4/8,
    // currentXZ12/16, deltaXZ20/24, triangle28, priorTriangle32,hit36,
    // hit normalXZ40/44, planeConstant48, movementParameter52.
    std::array<std::uint32_t,14> words{};
    float f(std::size_t offset) const{return std::bit_cast<float>(words.at(offset/4));}
    std::uint32_t u(std::size_t offset) const{return words.at(offset/4);}
    void setf(std::size_t offset,float value){words.at(offset/4)=std::bit_cast<std::uint32_t>(value);}
    void setu(std::size_t offset,std::uint32_t value){words.at(offset/4)=value;}
};

float originalCoarseCellDistance(const std::array<std::uint32_t,14>& cell,
    const std::array<float,3>& point);
std::int32_t findOriginalCoarseCell(const OriginalCollisionData& data,
    std::int32_t previousCell,const std::array<float,3>& point);
std::int32_t findOriginalTriangle(const OriginalCollisionData& data,
    std::int32_t startingTriangle,const std::array<float,3>& point,
    OriginalTriangleSearchTrace& trace);
// Original022940 resets only its six documented fields, preserving flags,
// corrected-hit history and input point until its caller updates them.
void clearOriginalCollisionQuery(OriginalCollisionQuery& query);
// Complete original022B80 broad/fine location with cache and error semantics.
// This locates collision data; the original smooth-height/swept-hit evaluation
// (023BE0/022D20) must run afterward to complete the contact query.
bool locateOriginalCollision(const OriginalCollisionData& data,
    OriginalCollisionQuery& query,OriginalTriangleSearchTrace& trace);
//023A40 through the actual023A00/023A20 scratch policy. No approximation of
// the original cubic expression by an algebraically equivalent plane.
void buildOriginalSurfaceCoefficients(const OriginalCollisionData& data,
    std::int32_t triangleIndex,OriginalSurfaceScratch& scratch);
// Complete023BE0 normal/height/material evaluation. Requires valid query+60
// from the original locator and publishes query normal,point,+24=Y-height.
bool evaluateOriginalCollisionSurface(const OriginalCollisionData& data,
    OriginalCollisionQuery& query,OriginalSurfaceScratch& scratch);
// Complete original022CE0 wrapper, including actual location/cache effects.
bool queryOriginalCollisionSurface(const OriginalCollisionData& data,
    OriginalCollisionQuery& query,OriginalTriangleSearchTrace& trace,
    OriginalSurfaceScratch& scratch);
bool beginOriginalCollisionSweep(const OriginalCollisionData& data,
    const OriginalCollisionQuery& query,OriginalTriangleSearchTrace& trace,
    OriginalSweepScratch& sweep);
// Returns true when the original iterator stops (with or without a hit),
// false when it crossed to an allowed neighbor and needs another invocation.
bool advanceOriginalCollisionSweep(const OriginalCollisionData& data,OriginalSweepScratch& sweep);
void publishOriginalCollisionSweep(const OriginalCollisionData& data,
    OriginalCollisionQuery& query,const OriginalSweepScratch& sweep);
// Complete original022D20, including its100-step cap, original fallback,
// swept-hit material flags and smooth-surface evaluation when no edge blocks.
bool queryOriginalCollisionSwept(const OriginalCollisionData& data,
    OriginalCollisionQuery& query,OriginalTriangleSearchTrace& trace,
    OriginalSurfaceScratch& surface);
} // namespace idas3::original
