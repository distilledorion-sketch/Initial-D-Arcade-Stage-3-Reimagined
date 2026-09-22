#pragma once
#include "native_assets.h"
#include <string_view>

namespace idas3 {
class OriginalCourseScene {
public:
    // A variant becomes available only after its complete selection metadata and
    // all referenced original assets exist. Imported Akina catalog assets are
    // additive; the earlier standalone Akina assets remain intact.
    static bool available(const std::filesystem::path& root,std::string_view courseId,bool night,bool reverse,bool wet=false);
    static OriginalCourseScene load(const std::filesystem::path& root,std::string_view courseId,bool night,bool reverse,bool wet=false);
    static OriginalCourseScene loadMetadata(const std::filesystem::path& root,const std::filesystem::path& metadataPath);
    NativeModel model,backgroundModel;
    NativeTextureBank textures,backgroundTextures;
    const NativeAssembly& assemblyForPathIndex(std::size_t originalForwardPathIndex)const;
    NativeAssembly backgroundAssembly(Vec3 cameraWorld)const;
    std::span<const NativeAssembly> assemblies()const{return assemblies_;}
    std::size_t assemblyCount()const{return assemblies_.size();}
    std::span<const Vec3> lampPositions()const{return lampPositions_;}
private:
    std::vector<Vec3> lampPositions_;
    std::vector<NativeAssembly> assemblies_;
    std::vector<std::uint32_t> selectionStarts_,selectionAssemblies_;
    NativeAssembly background_;
};
}
