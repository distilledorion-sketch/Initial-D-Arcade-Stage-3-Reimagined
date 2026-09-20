#pragma once
#include "native_assets.h"
#include "original_collision.h"
#include <functional>

namespace idas3::original {
// Original car+2636,0D4C80,lightobj chunk7,k_light_test8. This is an
// independently drawn road-projected mesh, not an ELAN light descriptor.
class OriginalHeadlightProjection {
public:
    using Matrix=std::array<float,16>; // source XF column-major world matrix
    using Record=std::array<std::uint32_t,8>;
    using Query=std::function<bool(OriginalCollisionQuery&)>;
    void load(const std::filesystem::path& projectRoot);
    void reset();
    // Exact0D5320 copies the complete64-byte road query into all9 points.
    // Preserve original road data/cached triangle semantics in the callback.
    void bindRoad(const OriginalCollisionQuery&);
    // Original0D84A0 copies projected records into the12 strip vertices.
    // Host draw/render calls must not implicitly publish or advance.
    void publish();
    // Exact0D50A0 at the existing source contact-query boundary. The source
    //034E60 draws the previously published model before invoking this update.
    unsigned advance(const Matrix& carVisualWorldMatrix,const Query&);
    const NativeModel& model() const {return model_;}
    const std::array<Record,9>& records()const{return records_;}
    const std::array<OriginalCollisionQuery,9>& queries()const{return queries_;}
private:
    NativeModel originalModel_,model_;
    std::array<Record,9> authored_{},records_{};
    std::array<unsigned,12> vertexOffsets_{};
    std::array<OriginalCollisionQuery,9> queries_{};
    bool loaded_{};
};
} // namespace idas3::original
