#pragma once
#include "native_assets.h"
#include "original_matrix.h"
#include <string_view>

namespace idas3 {
// The separately owned 083FC0 tree-placement objects. Their meshes and material
// words live in the selected course bank; this component owns only placements
// and the source 084320 spatial/sector admission state.
class OriginalCourseObjects {
public:
    static bool available(const std::filesystem::path& root,std::string_view courseId,
                          bool night,bool reverse,bool wet);
    static OriginalCourseObjects load(const std::filesystem::path& root,std::string_view courseId,
                                     bool night,bool reverse,bool wet,std::size_t chunkCount);
    // target is the selected primary/static assembly. The exported before
    // ordinals include any companion lamp insertions already in that assembly.
    // routePosition is the source path sample supplied to course-owner update,
    // not the render camera or the physical vehicle's lateral displacement.
    void appendTo(NativeAssembly& target,std::size_t forwardPathIndex,Vec3 routePosition)const;
    std::vector<NativeAssemblyInsertion> insertionsForPathIndex(
        std::size_t forwardPathIndex,Vec3 routePosition,std::size_t staticInstanceCount)const;
    std::size_t placementCount()const;
    std::size_t ownerCount()const{return owners_.size();}
    // Host presentation preference. Zero retains exact arcade admission for
    // source verification; this does not modify authored placement records.
    void setMinimumDrawDistance(float metres);
private:
    struct Placement {NativeModelInstance instance;Vec3 position;std::uint32_t sector=0;bool bySector=false;};
    struct Owner {
        original::OriginalMatrix gridMatrix;
        float cellX=0,cellZ=0,farSquared=0;
        std::int32_t columns=0,rows=0,innerX=0,innerZ=0,outerX=0,outerZ=0;
        std::uint32_t sectorMode=0;
        std::vector<Placement> placements;
        std::vector<std::vector<std::uint32_t>> cells;
        std::array<std::vector<std::uint32_t>,32> sectors;
    };
    struct Draw {std::uint32_t owner=0,chunkBase=0,before=0;std::array<std::int32_t,4> ranges{};};
    struct Selection {std::uint32_t staticCount=0;std::vector<Draw> draws;};
    std::vector<Owner> owners_;
    std::vector<Selection> selections_;
    std::vector<std::uint32_t> starts_,choices_;
    std::uint32_t pathCount_=0;
    std::size_t chunkCount_=0;
    float minimumDrawDistance_=0;
    void appendOwner(NativeAssembly& target,const Draw& draw,Vec3 routePosition)const;
};
}
