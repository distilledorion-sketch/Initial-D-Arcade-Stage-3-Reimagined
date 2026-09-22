#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>

namespace idas3 {
struct ImportedCourseDefinition {
    unsigned id,handlingCondition;
    const char *slug,*name,*folder;
    bool nightOnly,specialStage;
    std::uint32_t sceneFlags;
};
inline constexpr std::array<ImportedCourseDefinition,6> importedCourseDefinitions{{
    {9,0,"hakone","HAKONE","HAKONE",false,false,0},
    {10,2,"sadamine","SADAMINE","SADAMINE",false,false,524288u},
    {11,6,"enna","ENNA SKYLINE","ENNA",true,true,1048576u},
    {12,12,"myogi_special","MYOGI (SPECIAL STAGE)","MYOGI_SPECIAL",true,true,1048576u|2097152u},
    {13,8,"usui_special","USUI (SPECIAL STAGE)","USUI_SPECIAL",true,true,1048576u|4194304u},
    {14,4,"momiji","MOMIJI LINE","MOMIJI",true,true,1048576u|8388608u},
}};
inline constexpr unsigned supportedCourseCount=9+unsigned(importedCourseDefinitions.size());
inline constexpr unsigned supportedConditionCount=supportedCourseCount*2;
inline constexpr bool isImportedCourseId(int id){return id>=9&&id<int(supportedCourseCount);}
inline const ImportedCourseDefinition& importedCourseDefinition(unsigned id){
    if(!isImportedCourseId(int(id)))throw std::invalid_argument("Unknown imported course");
    return importedCourseDefinitions[id-9];
}
inline bool courseRequiresNight(unsigned id){return id==4||id==8||(isImportedCourseId(int(id))&&importedCourseDefinition(id).nightOnly);}
}
