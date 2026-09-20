#include "course_scene_catalog.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

namespace idas3 {
namespace {
template<class T>T read(std::istream& f){T value{};if(!f.read(reinterpret_cast<char*>(&value),sizeof(value)))throw std::runtime_error("Truncated original course scene");return value;}
std::filesystem::path scenePath(const std::filesystem::path& root,std::string_view id,bool night,bool reverse,bool wet){
    constexpr std::array<std::string_view,9> ids={"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df3"};
    if(std::find(ids.begin(),ids.end(),id)==ids.end())return {};
    return root/"data/original_models/courses"/std::string(id)/(std::string("scene_")+(night?"night":"day")+(reverse?"_reverse":"_forward")+(wet?"_wet":"")+".idasscene");
}
std::filesystem::path readPath(std::istream& f,const std::filesystem::path& root){
    const auto n=read<std::uint32_t>(f);if(n==0||n>1024)throw std::runtime_error("Original course path bound");
    std::string text(n,'\0');if(!f.read(text.data(),n))throw std::runtime_error("Truncated course asset path");
    std::filesystem::path path(text);if(path.is_absolute()||path.has_root_name())throw std::runtime_error("Unexpected absolute course asset path");
    for(const auto& part:path)if(part=="..")throw std::runtime_error("Invalid course asset path traversal");
    return root/path;
}
struct Metadata {std::array<std::filesystem::path,4> banks;std::vector<std::filesystem::path> assemblies;std::vector<std::uint32_t> starts,choices;NativeAssembly background;};
Metadata metadata(const std::filesystem::path& file,const std::filesystem::path& root){
    std::ifstream f(file,std::ios::binary);const auto magic=read<std::array<char,8>>(f);
    if(std::memcmp(magic.data(),"ID3SCN1\0",8)!=0||read<std::uint32_t>(f)!=1)throw std::runtime_error("Invalid original course scene format");
    Metadata m;for(auto& bank:m.banks)bank=readPath(f,root);
    const auto assemblies=read<std::uint32_t>(f),transitions=read<std::uint32_t>(f);
    if(assemblies==0||assemblies>4096||transitions==0||transitions>20000)throw std::runtime_error("Original course selection bound");
    for(unsigned i=0;i<assemblies;++i)m.assemblies.push_back(readPath(f,root));
    for(unsigned i=0;i<transitions;++i){const auto start=read<std::uint32_t>(f),choice=read<std::uint32_t>(f);
        if(choice>=assemblies||(!i&&start!=0)||(i&&start<=m.starts.back()))throw std::runtime_error("Invalid original path selection transition");
        m.starts.push_back(start);m.choices.push_back(choice);}
    const auto backgrounds=read<std::uint32_t>(f);if(backgrounds==0||backgrounds>32)throw std::runtime_error("Original background selection bound");
    for(unsigned i=0;i<backgrounds;++i){NativeModelInstance v;v.chunk=read<std::uint32_t>(f);v.transform=read<std::array<float,16>>(f);m.background.instances.push_back(v);}
    if(f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Trailing original course metadata");return m;
}
}
bool OriginalCourseScene::available(const std::filesystem::path& root,std::string_view id,bool night,bool reverse,bool wet){
    try{const auto path=scenePath(root,id,night,reverse,wet);if(path.empty()||!std::filesystem::is_regular_file(path))return false;
        const auto m=metadata(path,root);for(const auto& bank:m.banks)if(!std::filesystem::is_regular_file(bank))return false;
        for(const auto& file:m.assemblies)if(!std::filesystem::is_regular_file(file))return false;return true;
    }catch(const std::exception&){return false;}
}
OriginalCourseScene OriginalCourseScene::load(const std::filesystem::path& root,std::string_view id,bool night,bool reverse,bool wet){
    const auto path=scenePath(root,id,night,reverse,wet);if(path.empty())throw std::runtime_error("Course scene has not been exported");return loadMetadata(root,path);
}
OriginalCourseScene OriginalCourseScene::loadMetadata(const std::filesystem::path& root,const std::filesystem::path& path){
    auto m=metadata(path,root);
    OriginalCourseScene scene;scene.model=NativeModel::load(m.banks[0]);scene.textures=NativeTextureBank::load(m.banks[1]);scene.backgroundModel=NativeModel::load(m.banks[2]);scene.backgroundTextures=NativeTextureBank::load(m.banks[3]);
    for(const auto& file:m.assemblies)scene.assemblies_.push_back(NativeAssembly::load(file,scene.model.chunks.size()));
    auto lampPath=path;lampPath.replace_extension(".idaslamps");
    if(std::filesystem::exists(lampPath)){
        std::ifstream f(lampPath,std::ios::binary);const auto magic=read<std::array<char,8>>(f);
        if(std::memcmp(magic.data(),"ID3LMP1\0",8))throw std::runtime_error("Invalid original course lamp format");
        const auto chunk=read<std::uint32_t>(f),count=read<std::uint32_t>(f),assemblies=read<std::uint32_t>(f);
        if(chunk>=scene.model.chunks.size()||count>256||assemblies!=scene.assemblies_.size())throw std::runtime_error("Original course lamp bounds");
        std::vector<NativeModelInstance> lamps;
        for(unsigned i=0;i<count;++i){
            const auto p=read<std::array<float,3>>(f);
            if(!std::isfinite(p[0])||!std::isfinite(p[1])||!std::isfinite(p[2]))throw std::runtime_error("Non-finite original lamp position");
            // Usui repeats its four glow anchors in the source effect list.
            // Retain both authored draws below, but one physical fixture must
            // contribute illumination only once.
            if(std::none_of(scene.lampPositions_.begin(),scene.lampPositions_.end(),[&](Vec3 other){return other.x==p[0]&&other.y==p[1]&&other.z==p[2];}))
                scene.lampPositions_.push_back({p[0],p[1],p[2]});
            NativeModelInstance lamp;lamp.chunk=chunk;lamp.billboard=true;lamp.transform={1,0,0,p[0],0,1,0,p[1],0,0,1,p[2],0,0,0,1};lamps.push_back(lamp);
        }
        for(auto& assembly:scene.assemblies_){const auto before=read<std::uint32_t>(f);
            if(before>assembly.instances.size())throw std::runtime_error("Original lamp insertion outside assembly");
            assembly.instances.insert(assembly.instances.begin()+before,lamps.begin(),lamps.end());}
        if(f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Trailing original course lamp data");
        // The packed normal field carries the billboard anchor at rendering
        // time. Require the source's unlit planar effect material explicitly.
        if(count)for(const auto& batch:scene.model.chunks[chunk].batches){
            if(!(batch.material[2]&512))throw std::runtime_error("Course lamp unexpectedly requires vertex lighting");
            for(const auto& vertex:batch.vertices)if(vertex.position.z!=0)throw std::runtime_error("Course lamp is not planar");}
    }
    for(const auto& instance:m.background.instances)if(instance.chunk>=scene.backgroundModel.chunks.size())throw std::runtime_error("Background chunk outside original model");
    scene.background_=std::move(m.background);scene.selectionStarts_=std::move(m.starts);scene.selectionAssemblies_=std::move(m.choices);return scene;
}
const NativeAssembly& OriginalCourseScene::assemblyForPathIndex(std::size_t index)const{
    if(selectionStarts_.empty())throw std::runtime_error("Original scene was not loaded");
    const auto it=std::upper_bound(selectionStarts_.begin(),selectionStarts_.end(),index);
    const auto segment=std::size_t(it-selectionStarts_.begin()-1);return assemblies_.at(selectionAssemblies_[segment]);
}
NativeAssembly OriginalCourseScene::backgroundAssembly(Vec3 camera)const{
    auto assembly=background_;for(auto& instance:assembly.instances){instance.transform[3]+=camera.x;instance.transform[11]+=camera.z;}return assembly;
}
}
