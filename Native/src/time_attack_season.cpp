#include "time_attack_season.h"
#include "time_attack_records.h"
#include "time_attack_ghost.h"
#include "local_driver_profiles.h"
#include "local_save_slots.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace idas3 {
namespace {
// Never follow a link while migrating a known save tree. No recursive removal.
int plain(const std::filesystem::path& path,bool directory){
    std::error_code error;const auto status=std::filesystem::symlink_status(path,error);
    if(error==std::errc::no_such_file_or_directory||status.type()==std::filesystem::file_type::not_found)return 0;
    if(error||(directory?!std::filesystem::is_directory(status):!std::filesystem::is_regular_file(status)))return -1;
#if defined(_WIN32)
    const auto attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return -1;
#endif
    return 1;
}
int subdirectory(const std::filesystem::path& root,const std::filesystem::path& relative){
    auto current=root;
    for(const auto& part:relative){current/=part;const int state=plain(current,true);if(state!=1)return state;}
    return 1;
}
}
TimeAttackSeasonMigration migrateTimeAttackSeason(const std::filesystem::path& userdata){
    TimeAttackSeasonMigration result;
    if(userdata.empty()){result.complete=false;return result;}
    const int root=plain(userdata,true);if(root==0)return result;
    if(root<0){result.complete=false;return result;}
    const auto records=[&](const std::filesystem::path& file){
        const int state=plain(file,false);if(state==0)return;
        if(state<0){result.complete=false;return;}
        const auto archive=".before-time-attack-season-"+std::to_string(timeAttackSeason)+".bak";
        for(const auto& suffix:{std::string(".tmp"),std::string(".previous"),archive,archive+".tmp"}){
            auto copy=file;copy+=suffix;if(plain(copy,false)<0){result.complete=false;return;}
        }
        TimeAttackRecords store;
        if(!store.load(file)){result.complete=false;return;}
        if(store.needsMigration()){
            if(store.save(file))++result.recordFilesMigrated;else result.complete=false;
        }
    };
    records(userdata/"time_attack_records_v1.csv");
    for(int slot=-1;slot<int(LocalSaveSlots::count);++slot){
        const auto relative=slot<0?std::filesystem::path("driver_profiles_v1"):
            std::filesystem::path("saves")/("slot_"+std::to_string(slot+1))/"driver_profiles_v1";
        const int directory=subdirectory(userdata,relative);
        if(directory<0){result.complete=false;continue;}if(directory==0)continue;
        const auto path=userdata/relative;records(path/"hakone_personal_v1.csv");
        const LocalDriverProfiles profiles(path);
        for(unsigned car=0;car<35;++car){
            const auto file=profiles.path(car);
            bool safe=true;
            for(const char* suffix:{"",".previous",".lock"}){auto copy=file;copy+=suffix;if(plain(copy,false)<0)safe=false;}
            const auto archive=".before-time-attack-season-"+std::to_string(timeAttackSeason)+".bak";
            for(const auto& suffix:{archive,archive+".tmp",".previous"+archive,".previous"+archive+".tmp"}){
                auto copy=file;copy+=suffix;if(plain(copy,false)<0)safe=false;
            }
            if(!safe){result.complete=false;continue;}
            const auto loaded=profiles.load(car);
            if(loaded.origin==LocalDriverProfiles::Origin::Unreadable){result.complete=false;continue;}
            if(loaded.needsTimeAttackSeasonMigration){
                if(profiles.save(car,loaded.profile))++result.profilesMigrated;else result.complete=false;
            }
        }
    }
    if(!TimeAttackGhost::removePreviousSeasonGhosts(userdata))result.complete=false;
    return result;
}
}
