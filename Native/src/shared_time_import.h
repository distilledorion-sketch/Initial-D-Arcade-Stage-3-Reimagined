#pragma once
#include "local_driver_profiles.h"
#include "local_save_slots.h"
#include "original_record_rules.h"
#include "time_attack_records.h"
#include <sstream>
#include <map>

namespace idas3 {
// Read-only migration from successful personal records. The aggregate local
// ranking CSV is intentionally excluded: older builds could store timeouts there.
inline std::string sharedPersonalImportJson(const std::filesystem::path& root){
    struct Row {TimeAttackEntry entry;int manual=-1,night=-1;};
    std::map<unsigned,Row> best;
    const auto add=[&](Row row){const auto& e=row.entry;
        if(e.condition>=22||e.weather>1||e.car>=35||e.ticks6000<60000||e.ticks6000>=10800000||
           ((e.condition==16||e.condition==17)&&e.weather!=1))return;
        for(auto c:e.nameGlyphs)if(c>221)return;
        const unsigned key=(e.condition*2+e.weather)*35+e.car;
        auto it=best.find(key);if(it==best.end()||e.ticks6000<it->second.entry.ticks6000)best[key]=row;
    };
    LocalSaveSlots slots(root/"saves");
    for(int slot=-1;slot<int(LocalSaveSlots::count);++slot){
        const auto dir=slot<0?root/"driver_profiles_v1":slots.profileDirectory(unsigned(slot));
        if(!std::filesystem::is_directory(dir))continue;
        LocalDriverProfiles profiles(dir);
        for(unsigned car=0;car<35;++car){
            const auto saved=profiles.load(car);
            if(saved.origin!=LocalDriverProfiles::Origin::Saved&&saved.origin!=LocalDriverProfiles::Origin::Backup)continue;
            for(unsigned condition=0;condition<18;++condition)for(unsigned weather=0;weather<2;++weather){
                const auto p=original::originalPersonalTimeAttackRecord(saved.profile,original::originalRecordPartition(condition,weather!=0));
                if(p.night>1)continue;
                Row row;row.entry={condition,weather,car,p.ticks6000};row.entry.intermediate6000=p.intermediate6000;row.night=int(p.night);
                for(unsigned n=0;n<5;++n){const auto glyph=saved.profile.u(44+n*4);row.entry.nameGlyphs[n]=glyph<=221?std::uint8_t(glyph):221;}
                add(row);
            }
        }
        TimeAttackRecords imported;
        if(imported.load(dir/"hakone_personal_v1.csv"))for(const auto& e:imported.entries())if(e.condition>=18)add({e,int(e.manual),int(e.night)});
    }
    std::ostringstream out;out<<"{\"runs\":[";bool first=true;
    for(const auto& [key,row]:best){const auto& e=row.entry;if(!first)out<<',';first=false;
        out<<"{\"imported\":1,\"condition\":"<<e.condition<<",\"weather\":"<<e.weather<<",\"car\":"<<e.car<<",\"ticks6000\":"<<e.ticks6000<<",\"nameGlyphs\":[";
        for(unsigned i=0;i<5;++i){if(i)out<<',';out<<unsigned(e.nameGlyphs[i]);}
        // Preserve checkpoints only when the saved sequence is complete and
        // ordered. Missing history stays explicitly unknown, never fabricated.
        std::array<std::uint32_t,4> splits{};unsigned count=0;std::uint32_t prior=0;bool ended=false,valid=true;
        for(auto t:e.intermediate6000){if(!t){ended=true;continue;}if(ended||t<=prior||t>=e.ticks6000){valid=false;break;}splits[count++]=t;prior=t;}
        if(valid&&count)splits[count]=e.ticks6000;else splits={};
        out<<"],\"splits\":[";for(unsigned i=0;i<4;++i){if(i)out<<',';out<<splits[i];}
        out<<"],\"manual\":"<<row.manual<<",\"night\":"<<row.night<<",\"points\":-1}";
    }
    out<<"]}";return out.str();
}
}
