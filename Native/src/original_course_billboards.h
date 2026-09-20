#pragma once
#include "native_assets.h"
#include <string_view>

namespace idas3 {
// Original spectator/human position lists, selected by the source course
// primary/static callbacks. The course bank supplies their original artwork.
class OriginalCourseBillboards {
public:
    static bool available(const std::filesystem::path& root,std::string_view courseId,
                          bool night,bool reverse,bool wet);
    static OriginalCourseBillboards load(const std::filesystem::path& root,std::string_view courseId,
                                        bool night,bool reverse,bool wet,std::size_t chunkCount);
    // Ordinals address the unchanged base scene, including existing lamps.
    const std::vector<NativeAssemblyInsertion>& insertionsForPathIndex(
        std::size_t forwardPathIndex,std::size_t staticInstanceCount)const;
    std::size_t selectionCount()const{return selections_.size();}
private:
    std::uint32_t pathCount_=0;
    std::vector<std::uint32_t> starts_,choices_;
    std::vector<std::vector<NativeAssemblyInsertion>> selections_;
};
}
