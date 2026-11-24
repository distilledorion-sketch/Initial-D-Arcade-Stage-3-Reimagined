#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <map>
#include <string>
#include <stdexcept>

namespace idas3 {
struct ImportedCourseDefinition {
    unsigned id,handlingCondition;
    const char *slug,*name,*folder;
    bool nightOnly,specialStage;
    std::uint32_t sceneFlags;
    float accelerationScale=1.f;
    // Optional base condition for the original per-car speed coefficient.
    // Handling, road rules and steering/loss tables still use handlingCondition.
    std::optional<unsigned> speedCondition{};
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
    {17,14,"odawara","ODAWARA","ODAWARA",false,false,67108864u,1.f,0u},
}};
inline constexpr unsigned supportedCourseCount=9+unsigned(importedCourseDefinitions.size());
inline constexpr bool communityLeaderboardCourse(unsigned id){return id<supportedCourseCount;}
inline constexpr unsigned supportedConditionCount=supportedCourseCount*2;
// Revision 1 retired pre-.45 Gunsai/Odawara handling. The requested Odawara
// leaderboard/ghost reset starts revision 2 without disturbing Gunsai's epoch.
inline constexpr unsigned localTimeAttackRevision(unsigned course){return course==17?2u:course==16?1u:0u;}
// User IDs occupy a separate range. A condition and its weather still fit a signed int.
inline constexpr bool isUserCourseId(unsigned id){return id>=0x10000000u&&id<=0x1fffffffu;}
inline constexpr bool validLocalCondition(unsigned condition){return condition<supportedConditionCount||isUserCourseId(condition/2);}
struct UserCourseInfo {std::string slug,name;unsigned route;};
inline std::map<unsigned,UserCourseInfo> userCourseCatalog;
// Presentation only: never swap the stable route, collision or leaderboard IDs.
// Odawara's source-ordered path (condition 34) runs clockwise.
inline unsigned originalCoursePresentationCondition(unsigned condition){
    const auto course=condition/2,reverse=condition&1;
    if(isUserCourseId(course)){
        const auto it=userCourseCatalog.find(course);
        if(it!=userCourseCatalog.end())return it->second.route^reverse;
    }
    return course<9?condition:course==17?(reverse^1u):course==16?8u+reverse:6u+reverse;
}
inline bool isImportedCourseId(int id){return (id>=9&&id<int(supportedCourseCount))||userCourseCatalog.contains(unsigned(id));}
inline ImportedCourseDefinition importedCourseDefinition(unsigned id){
    if(isUserCourseId(id)){
        const auto& info=userCourseCatalog.at(id);
        return {id,6,info.slug.c_str(),info.name.c_str(),"",false,false,0};
    }
    if(!isImportedCourseId(int(id)))throw std::invalid_argument("Unknown imported course");
    return importedCourseDefinitions[id-9];
}
inline bool courseRequiresNight(unsigned id){return id==4||id==8||(isImportedCourseId(int(id))&&importedCourseDefinition(id).nightOnly);}
}
