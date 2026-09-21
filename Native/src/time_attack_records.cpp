#include "time_attack_records.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace idas3 {
namespace {
constexpr const char* legacyHeader="condition,weather,car,finish_ticks6000";
constexpr const char* namedHeader="condition,weather,car,finish_ticks6000,name0,name1,name2,name3,name4,manual,night";
constexpr const char* splitHeader="condition,weather,car,finish_ticks6000,name0,name1,name2,name3,name4,manual,night,split1,split2,split3";
bool valid(const TimeAttackEntry& e){return e.condition<24&&e.weather<2&&e.car<35&&e.ticks6000>0&&e.ticks6000<10800000
    && (e.intermediate6000==std::array<std::uint32_t,3>{} || (e.intermediate6000[0]>0&&e.intermediate6000[0]<e.intermediate6000[1]&&e.intermediate6000[1]<e.intermediate6000[2]&&e.intermediate6000[2]<e.ticks6000))
    &&std::all_of(e.nameGlyphs.begin(),e.nameGlyphs.end(),[](auto c){return c<=221;});}
}
TimeAttackBest TimeAttackRecords::best(unsigned condition,unsigned weather,unsigned car)const{
    TimeAttackBest out;
    for(const auto& e:entries_)if(e.condition==condition&&e.weather==weather){
        if(!out.course||e.ticks6000<out.course)out.course=e.ticks6000;
        if(e.car==car&&(!out.model||e.ticks6000<out.model))out.model=e.ticks6000;
    }
    // Personal records belong to the selected original driver profile.
    return out;
}
TimeAttackEntry TimeAttackRecords::personalBest(unsigned condition,unsigned weather,unsigned car)const{
    TimeAttackEntry best;
    for(const auto& e:entries_)if(e.condition==condition&&e.weather==weather&&e.car==car&&(!best.ticks6000||e.ticks6000<best.ticks6000))best=e;
    return best;
}
void TimeAttackRecords::record(TimeAttackEntry entry){
    if(!valid(entry))throw std::invalid_argument("Invalid native Time Attack record");
    entries_.push_back(entry);
    std::stable_sort(entries_.begin(),entries_.end(),[](auto a,auto b){return a.ticks6000<b.ticks6000;});
    std::array<unsigned,48> count{};std::array<std::array<bool,35>,48> models{};
    std::erase_if(entries_,[&](auto e){const auto key=e.condition*2+e.weather;
        const bool keep=count[key]<10||!models[key][e.car];++count[key];models[key][e.car]=true;return !keep;});
}
bool TimeAttackRecords::load(const std::filesystem::path& file){
    std::ifstream in(file);if(!in)return false;
    std::string line;std::getline(in,line);const bool splits=line==splitHeader,named=line==namedHeader||splits;
    if(!named&&line!=legacyHeader)return false;
    TimeAttackRecords loaded;std::size_t rows=0;
    while(std::getline(in,line)){
        // 24 directions x 2 weather states, top ten plus 34 other model bests.
        if(++rows>24u*2u*44u)return false;
        std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);TimeAttackEntry e;std::string extra;
        if(!(row>>e.condition>>e.weather>>e.car>>e.ticks6000))return false;
        if(named){
            for(auto& code:e.nameGlyphs){unsigned value=0;if(!(row>>value)||value>221)return false;code=std::uint8_t(value);}
            unsigned manual=0,night=0;if(!(row>>manual>>night)||manual>1||night>1)return false;
            e.manual=manual!=0;e.night=night!=0;
        }
        if(splits)for(auto& split:e.intermediate6000)if(!(row>>split))return false;
        if(row>>extra||!valid(e))return false;
        loaded.record(e);
    }
    if(!in.eof())return false;entries_=std::move(loaded.entries_);return true;
}
bool TimeAttackRecords::save(const std::filesystem::path& file)const{
    const std::filesystem::path temporary=file.string()+".tmp",backup=file.string()+".previous";
    std::ofstream out(temporary);if(!out)return false;
    out<<splitHeader<<'\n';
    for(const auto& e:entries_){
        out<<e.condition<<','<<e.weather<<','<<e.car<<','<<e.ticks6000;
        for(auto code:e.nameGlyphs)out<<','<<unsigned(code);
        out<<','<<unsigned(e.manual)<<','<<unsigned(e.night);
        for(auto split:e.intermediate6000)out<<','<<split;out<<'\n';
    }
    out.close();if(!out)return false;
    std::error_code error;std::filesystem::rename(temporary,file,error);if(!error)return true;
    std::filesystem::remove(backup,error);error.clear();std::filesystem::rename(file,backup,error);if(error)return false;
    std::filesystem::rename(temporary,file,error);
    if(error){std::error_code restore;std::filesystem::rename(backup,file,restore);return false;}
    std::filesystem::remove(backup,error);return true;
}
}
