#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace idas3::original {
class OriginalFscaTable {
public:
    static OriginalFscaTable load(const std::filesystem::path& tableFile);
    std::array<float,2> sinCos(std::uint16_t phase) const;
private:
    std::vector<std::uint32_t> halfWave_;
};
struct OriginalMatrix {
    // Column-major XF order, matching original FTRV explicitly.
    std::array<float,16> elements{};
};
OriginalMatrix originalIdentityMatrix();
void translateOriginalMatrix(OriginalMatrix& matrix,const std::array<float,3>& translation);
void scaleOriginalMatrix(OriginalMatrix& matrix,const std::array<float,3>& scale);
//1F67E0/1F68A0/1F6950 update only the two affected columns.
void rotateOriginalMatrixPhase(OriginalMatrix& matrix,unsigned axis,std::uint16_t phase,const OriginalFscaTable& table);
void rotateOriginalMatrixX(OriginalMatrix& matrix,float radians,const OriginalFscaTable& table);
void rotateOriginalMatrixY(OriginalMatrix& matrix,float radians,const OriginalFscaTable& table);
void rotateOriginalMatrixZ(OriginalMatrix& matrix,float radians,const OriginalFscaTable& table);
std::array<float,4> transformOriginalVector(const OriginalMatrix& matrix,const std::array<float,4>& value);
std::array<float,3> transformOriginalPoint(const OriginalMatrix& matrix,const std::array<float,3>& point);
// Actor pose: translation thenY(angle1),X(angle0),Z(angle2), exactly158200.
// The caller owns any enclosing render-matrix stack; no hidden global matrix.
OriginalMatrix originalActorMatrix(const std::array<float,3>& position,
    const std::array<float,3>& angles,const OriginalFscaTable& table);
} // namespace idas3::original
