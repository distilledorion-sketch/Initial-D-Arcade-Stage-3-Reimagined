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
    float accelerationScale=1.f;
};
inline constexpr std::array<ImportedCourseDefinition,9> importedCourseDefinitions{{
    {9,0,"hakone","HAKONE","HAKONE",false,false,0},
    {10,2,"sadamine","SADAMINE","SADAMINE",false,false,524288u},
    {11,6,"enna","ENNA SKYLINE","ENNA",true,true,1048576u},
    {12,12,"myogi_special","MYOGI (SPECIAL STAGE)","MYOGI_SPECIAL",true,true,1048576u|2097152u},
    {13,8,"usui_special","USUI (SPECIAL STAGE)","USUI_SPECIAL",true,true,1048576u|4194304u},
    {14,4,"momiji","MOMIJI LINE","MOMIJI",true,true,1048576u|8388608u},
    {15,6,"tsubaki","TSUBAKI LINE","TSUBAKI",false,false,16777216u},
    {16,2,"gunsai","GUNSAI","GUNSAI",false,false,33554432u},
    {17,14,"odawara","ODAWARA","ODAWARA",false,false,67108864u,1.2705f},
}};
inline constexpr unsigned supportedCourseCount=9+unsigned(importedCourseDefinitions.size());
inline constexpr unsigned supportedConditionCount=supportedCourseCount*2;
// Presentation only: never swap the stable route, collision or leaderboard IDs.
// Odawara's source-ordered path (condition 34) runs clockwise.
inline constexpr unsigned originalCoursePresentationCondition(unsigned condition){
    const auto course=condition/2,reverse=condition&1;
    return course<9?condition:course==17?(reverse^1u):course==16?8u+reverse:6u+reverse;
}
// Unversioned Gunsai/Odawara times include pre-.45 handling and cannot be
// distinguished from newer runs. Only those courses start a fresh local epoch.
inline constexpr unsigned localTimeAttackRevision(unsigned course){return course==16||course==17?1u:0u;}
inline constexpr bool isImportedCourseId(int id){return id>=9&&id<int(supportedCourseCount);}
inline const ImportedCourseDefinition& importedCourseDefinition(unsigned id){
    if(!isImportedCourseId(int(id)))throw std::invalid_argument("Unknown imported course");
    return importedCourseDefinitions[id-9];
}
inline bool courseRequiresNight(unsigned id){return id==4||id==8||(isImportedCourseId(int(id))&&importedCourseDefinition(id).nightOnly);}
}
