#include "original_course_billboards.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace idas3 {
namespace {
template<class T>T read(std::istream& in){T v{};if(!in.read(reinterpret_cast<char*>(&v),sizeof(v)))throw std::runtime_error("Truncated original course billboards");return v;}
std::filesystem::path filePath(const std::filesystem::path& root,std::string_view id,bool night,bool reverse,bool wet){
    constexpr std::array<std::string_view,9> ids={"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df3"};
    if(std::find(ids.begin(),ids.end(),id)==ids.end())return {};
    return root/"data/original_models/courses"/std::string(id)/(std::string("scene_")+(night?"night":"day")+(reverse?"_reverse":"_forward")+(wet?"_wet":"")+".idasbillboards");
}
}
bool OriginalCourseBillboards::available(const std::filesystem::path& root,std::string_view id,bool night,bool reverse,bool wet){
    const auto path=filePath(root,id,night,reverse,wet);return !path.empty()&&std::filesystem::is_regular_file(path);
}
OriginalCourseBillboards OriginalCourseBillboards::load(const std::filesystem::path& root,std::string_view id,bool night,bool reverse,bool wet,std::size_t chunkCount){
    const auto path=filePath(root,id,night,reverse,wet);if(path.empty())throw std::runtime_error("Unknown original billboard course");
    std::ifstream in(path,std::ios::binary);const auto magic=read<std::array<char,8>>(in);
    if(std::memcmp(magic.data(),"ID3BLB1\0",8)||read<std::uint32_t>(in)!=1)throw std::runtime_error("Invalid original billboard format");
    OriginalCourseBillboards out;out.pathCount_=read<std::uint32_t>(in);
    const auto selections=read<std::uint32_t>(in),changes=read<std::uint32_t>(in);
    if(out.pathCount_<2||out.pathCount_>20000||!selections||selections>4096||!changes||changes>out.pathCount_)throw std::runtime_error("Original billboard selection bound");
    out.selections_.resize(selections);std::size_t total=0;
    for(auto& selection:out.selections_){
        const auto count=read<std::uint32_t>(in);total+=count;
        if(count>8192||total>1000000)throw std::runtime_error("Original billboard instance bound");
        for(unsigned i=0;i<count;++i){
            const auto before=read<std::uint32_t>(in),chunk=read<std::uint32_t>(in);const auto p=read<std::array<float,3>>(in);
            if(before>8192||chunk>=chunkCount||!std::all_of(p.begin(),p.end(),[](float f){return std::isfinite(f);}))throw std::runtime_error("Invalid original billboard instance");
            if(!selection.empty()&&before<selection.back().before)throw std::runtime_error("Original billboard order is not monotonic");
            if(selection.empty()||before!=selection.back().before)selection.push_back({before,{}});
            NativeModelInstance instance;instance.chunk=chunk;instance.billboard=true;
            instance.transform={1,0,0,p[0],0,1,0,p[1],0,0,1,p[2],0,0,0,1};
            selection.back().assembly.instances.push_back(instance);
        }
    }
    for(unsigned i=0;i<changes;++i){
        const auto start=read<std::uint32_t>(in),choice=read<std::uint32_t>(in);
        if(start>=out.pathCount_||choice>=selections||(!i&&start!=0)||(i&&start<=out.starts_.back()))throw std::runtime_error("Invalid original billboard transition");
        out.starts_.push_back(start);out.choices_.push_back(choice);
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Trailing original billboard bytes");return out;
}
const std::vector<NativeAssemblyInsertion>& OriginalCourseBillboards::insertionsForPathIndex(std::size_t index,std::size_t staticCount)const{
    if(index>=pathCount_||starts_.empty())throw std::runtime_error("Original billboard path bound");
    const auto position=std::upper_bound(starts_.begin(),starts_.end(),index)-starts_.begin()-1;
    const auto& selection=selections_.at(choices_.at(position));
    if(!selection.empty()&&selection.back().before>staticCount)throw std::runtime_error("Original billboard insertion outside static assembly");return selection;
}
}
