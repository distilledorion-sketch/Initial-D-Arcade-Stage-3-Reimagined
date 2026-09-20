#pragma once
#include "native_assets.h"
#include <optional>
#include <stdexcept>

namespace idas3 {
// Tsuchisaka owner+568 was zero in the static scene capture. Its original
// primary passes (1A6A8C / 1A77AA) submit the last bank chunk after 95 / 96.
// The race setup's profile bit28 excludes the early rivals and Bunta; their
// effective road opponent IDs are 24, 25 and 31 (15D7xx's physics exclusion).
inline bool originalCourseOilEnabled(unsigned course,bool wet,unsigned opponent){
    return course==7&&!wet&&opponent!=24&&opponent!=25&&opponent!=31;
}
inline std::optional<NativeAssemblyInsertion> originalCourseOilInsertion(
        const NativeAssembly& base,unsigned course,bool night,bool wet,unsigned opponent){
    if(!originalCourseOilEnabled(course,wet,opponent))return std::nullopt;
    const unsigned anchor=night?96:95,chunk=night?105:104;
    for(std::size_t i=0;i<base.instances.size();++i)if(base.instances[i].chunk==anchor){
        NativeAssemblyInsertion out;out.before=i+1;
        NativeModelInstance oil;oil.chunk=chunk;
        oil.transform={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        out.assembly.instances.push_back(oil);return out;
    }
    throw std::runtime_error("Original Tsuchisaka oil insertion anchor is missing");
}
}
