#include "race.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <bit>
#include <charconv>

namespace idas3 {
void RaceClock::start(float totalLength) {
    phase=RacePhase::Countdown;ticks=0;countdown=180;sector=0;originalStartDigit=-1;originalStartElapsed=0;
    originalTiming=false;timeUp=false;elapsed6000=0;remaining6000=0;sectionTimes6000={};sectionCapacity=4;
    splits={};progress=0;furthest=0;courseLength=std::max(1.f,totalLength);
}
void RaceClock::tick(float courseProgress) {
    if(phase==RacePhase::Countdown) {
        if(--countdown<=0) phase=RacePhase::Running;
        return;
    }
    if(phase!=RacePhase::Running) return;
    ++ticks;
    if(!std::isfinite(courseProgress)) return;
    const float candidate=std::clamp(courseProgress,0.f,courseLength);
    // Reject nonphysical jumps from ambiguous projection on crossing paths.
    // 25 world units per tick is well above the supported car's maximum travel.
    if(std::abs(candidate-progress)>25.f) return;
    progress=candidate;furthest=std::max(furthest,progress);
    while(sector<4 && furthest>=courseLength*float(sector+1)*.25f-.25f) {
        splits[sector++]=ticks;
    }
    if(sector==4) phase=RacePhase::Finished;
}
void Replay::beginCapture(bool details) {
    frames.clear();truncated=false;detailed=details;profile={};priorRecords={};finishTicks6000=0;
    // Allocate at race loading, never copy an expanding recording mid-corner.
    // Capacity is reused on subsequent races; samples still use exact 60 Hz data.
    frames.reserve(maxFrames);
}
void Replay::record(std::uint64_t tick,Vec3 p,float yaw,float speed,int gear,const ReplayDetail* detail) {
    if(frames.size()>=maxFrames) { truncated=true;return; }
    if(!frames.empty() && tick<=frames.back().tick) return;
    frames.push_back({tick,p,yaw,speed,gear});
    if(detail)frames.back().detail=*detail;
    else if(detailed)truncated=true; // Never publish partially detailed captures.
}
std::vector<std::uint8_t> Replay::sharedBytes(std::uint32_t finish) const {
    if(truncated||frames.empty()||!finish||frames.front().tick>1||frames.back().tick>108000)return {};
    std::vector<std::uint8_t> out;out.reserve(detailed?96+160*frames.size():16+28*(frames.size()/3+2));
    auto u32=[&](std::uint32_t n){for(unsigned b=0;b<4;++b)out.push_back(std::uint8_t(n>>(b*8)));};
    auto scalar=[&](float n){u32(std::bit_cast<std::uint32_t>(n));};
    u32(detailed?0x32524449:0x31524449);u32(finish);u32(0);u32(60);unsigned count=0;
    if(detailed){u32(96);u32(160);u32(1);u32(12);for(auto v:profile)u32(v);for(auto v:priorRecords)u32(v);u32(0);}
    for(std::size_t i=0;i<frames.size();++i){
        if(!detailed&&i%3!=0&&i+1!=frames.size())continue;
        const auto& f=frames[i];
        if(!std::isfinite(f.position.x)||!std::isfinite(f.position.y)||!std::isfinite(f.position.z)||!std::isfinite(f.yaw)||!std::isfinite(f.speed)||f.gear<0||f.gear>6)return {};
        u32(std::uint32_t(f.tick));scalar(f.position.x);scalar(f.position.y);scalar(f.position.z);scalar(f.yaw);scalar(f.speed);u32(unsigned(f.gear));++count;
        if(detailed){
            if(i&&f.tick!=frames[i-1].tick+1)return {};
            const auto& d=f.detail;
            scalar(d.rpm);scalar(d.bodyPosition.x);scalar(d.bodyPosition.y);scalar(d.bodyPosition.z);scalar(d.pitch);scalar(d.roll);scalar(d.steering);
            for(auto v:d.suspension)scalar(v);for(auto v:d.rotation)scalar(v);scalar(d.throttle);scalar(d.brake);
            u32(d.elapsed);u32(std::bit_cast<std::uint32_t>(d.remaining));for(auto v:d.sections)u32(v);u32(d.sector);u32(d.capacity);
            u32(std::bit_cast<std::uint32_t>(d.lightCounter));u32(d.lightMaximum);u32(d.lightPhase);u32(d.lightVisible);scalar(d.lightFraction);u32(d.lights);scalar(d.progress);u32(d.extension);
        }
    }
    for(unsigned b=0;b<4;++b)out[8+b]=std::uint8_t(count>>(b*8));
    return out;
}
bool Replay::save(const std::string& filename) const {
    if(frames.empty()||truncated) return false;
    const auto tmp=filename+".tmp";
    std::ofstream out(tmp); if(!out) return false;
    out<<"tick,speed,yaw,pos_x,pos_y,pos_z,gear"<<(finishTicks6000?",finish_ticks6000\n":"\n");
    // Locale-independent round-trip float precision, buffered as a block.
    // Per-value iostream formatting used to stall the finish frame twice:
    // once for last_run and once for a new personal-best ghost.
    std::array<char,65536> buffer;char* cursor=buffer.data();
    const auto integer=[&](auto value){cursor=std::to_chars(cursor,buffer.data()+buffer.size(),value).ptr;};
    const auto scalar=[&](float value){cursor=std::to_chars(cursor,buffer.data()+buffer.size(),value,std::chars_format::general,9).ptr;};
    for(const auto& f:frames) {
        if(buffer.data()+buffer.size()-cursor<512){out.write(buffer.data(),cursor-buffer.data());cursor=buffer.data();}
        integer(f.tick);*cursor++=',';scalar(f.speed);*cursor++=',';scalar(f.yaw);*cursor++=',';
        scalar(f.position.x);*cursor++=',';scalar(f.position.y);*cursor++=',';scalar(f.position.z);*cursor++=',';integer(f.gear);
        if(finishTicks6000){*cursor++=',';integer(&f==&frames.back()?finishTicks6000:0);}
        *cursor++='\n';
    }
    out.write(buffer.data(),cursor-buffer.data());
    out.close();if(!out) return false;
    std::error_code ec;
    std::filesystem::rename(tmp,filename,ec);
    // On Windows rename does not replace an existing file. Keep previous ghost
    // recoverable while installing the completely-written replacement.
    if(ec) {
        const auto old=filename+".previous";
        std::filesystem::remove(old,ec);ec.clear();
        std::filesystem::rename(filename,old,ec);if(ec) return false;
        std::filesystem::rename(tmp,filename,ec);
        if(ec) { std::error_code restore;std::filesystem::rename(old,filename,restore);return false; }
        std::filesystem::remove(old,ec);
    }
    return true;
}
bool Replay::load(const std::string& filename) {
    frames.clear();truncated=false;finishTicks6000=0;std::ifstream in(filename);if(!in) return false;
    std::string line;std::getline(in,line);
    const bool exactFinish=line=="tick,speed,yaw,pos_x,pos_y,pos_z,gear,finish_ticks6000";
    if(!exactFinish&&line!="tick,speed,yaw,pos_x,pos_y,pos_z,gear") return false;
    std::uint32_t loadedFinish=0;
    std::vector<ReplayFrame> loaded;
    while(std::getline(in,line)) {
        if(loaded.size()>=maxFrames) return false;
        std::replace(line.begin(),line.end(),',',' ');std::istringstream ss(line);
        ReplayFrame f{};std::string excess;
        if(loadedFinish||!(ss>>f.tick>>f.speed>>f.yaw>>f.position.x>>f.position.y>>f.position.z>>f.gear)) return false;
        if(exactFinish&&(!(ss>>loadedFinish)||std::uint64_t(loadedFinish)>f.tick*100))return false;
        if(ss>>excess)return false;
        if(!std::isfinite(f.speed)||!std::isfinite(f.yaw)||!std::isfinite(f.position.x)||!std::isfinite(f.position.y)||!std::isfinite(f.position.z)||f.gear<0||f.gear>6) return false;
        if(!loaded.empty()&&f.tick<=loaded.back().tick) return false;
        loaded.push_back(f);
    }
    if(loaded.empty())return false;frames=std::move(loaded);finishTicks6000=loadedFinish;return true;
}
ReplayFrame Replay::sample(double tick) const {
    if(frames.empty()) return {};
    auto upper=std::lower_bound(frames.begin(),frames.end(),tick,[](const ReplayFrame& f,double t){return double(f.tick)<t;});
    if(upper==frames.begin())return *upper;if(upper==frames.end())return frames.back();
    if(double(upper->tick)==tick)return *upper;
    const auto& a=*(upper-1);const auto& b=*upper;
    float t=float((tick-double(a.tick))/double(b.tick-a.tick));
    return {std::uint64_t(tick),lerp(a.position,b.position,t),lerpAngle(a.yaw,b.yaw,t),a.speed+(b.speed-a.speed)*t,a.gear};
}
std::string formatTime(double seconds) {
    auto ms=std::max(0LL,static_cast<long long>(std::llround(seconds*1000)));
    char out[40];std::snprintf(out,sizeof(out),"%02lld:%02lld.%03lld",ms/60000,(ms/1000)%60,ms%1000);return out;
}
}
