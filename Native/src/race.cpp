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
    dustFrames.clear();smokeFrames.clear();dustCapture_=smokeCapture_=backfireCapture_=false;
    // Allocate at race loading, never copy an expanding recording mid-corner.
    // Capacity is reused on subsequent races; samples still use exact 60 Hz data.
    frames.reserve(maxFrames);
}
void Replay::beginBackfireCapture(){
    if(!detailed||!frames.empty())throw std::logic_error("Backfire capture must begin with a detailed race");
    backfireCapture_=true;
}
void Replay::beginDustCapture(){
    if(!detailed||!frames.empty()||smokeCapture_)throw std::logic_error("Dust capture must begin with a detailed race");
    dustFrames.clear();dustFrames.reserve(ReplayRoadsideDustFrame::maxFrames);dustCapture_=true;
}
void Replay::recordDust(const ReplayRoadsideDustFrame& input){
    if(!dustCapture_||truncated)return;
    if(dustFrames.size()>=ReplayRoadsideDustFrame::maxFrames||!input.valid()){truncated=true;return;}
    dustFrames.push_back(input);
}
void Replay::beginSmokeCapture(){
    if(!detailed||!frames.empty()||dustCapture_)throw std::logic_error("Smoke capture must begin with a detailed race");
    smokeFrames.clear();smokeFrames.reserve(ReplayCarSmokeFrame::maxFrames);smokeCapture_=true;
}
void Replay::recordSmoke(const ReplayCarSmokeFrame& input){
    if(!smokeCapture_||truncated)return;
    if(smokeFrames.size()>=ReplayCarSmokeFrame::maxFrames||!input.valid()||
        (!smokeFrames.empty()&&((input.scene^smokeFrames.front().scene)&ReplayCarSmokeFrame::fixedSceneMask))){truncated=true;return;}
    smokeFrames.push_back(input);
}
void Replay::record(std::uint64_t tick,Vec3 p,float yaw,float speed,int gear,const ReplayDetail* detail,int backfire,const ReplayTelemetry* telemetry) {
    if(frames.size()>=maxFrames) { truncated=true;return; }
    if(!frames.empty() && tick<=frames.back().tick) return;
    frames.push_back({tick,p,yaw,speed,gear});
    frames.back().dustStep=std::uint32_t(smokeCapture_?smokeFrames.size():dustFrames.size());
    if(backfireCapture_){if(backfire< -1||backfire>1)truncated=true;else frames.back().backfire=backfire;}
    if(detail)frames.back().detail=*detail;
    else if(detailed)truncated=true; // Never publish partially detailed captures.
    if(telemetry){frames.back().telemetry=*telemetry;frames.back().telemetryAvailable=true;}
}
std::vector<std::uint8_t> Replay::sharedBytes(std::uint32_t finish) const {
    if(truncated||frames.empty()||!finish||frames.front().tick>1||frames.back().tick>108000)return {};
    const bool smoke=detailed&&!smokeFrames.empty();
    const bool dust=smoke||(detailed&&!dustFrames.empty());
    if(smoke&&!dustFrames.empty())return {};
    const bool flashes=detailed&&backfireCapture_;
    const unsigned header=flashes?100:96,stride=flashes?168:dust?164:160;
    const auto dustCount=dust?frames.back().dustStep:0u;
    const unsigned effectBytes=smoke?ReplayCarSmokeFrame::bytes:ReplayRoadsideDustFrame::bytes;
    if(dust&&(!dustCount||dustCount>(smoke?smokeFrames.size():dustFrames.size())||dustCount>ReplayRoadsideDustFrame::maxFrames))return {};
    std::vector<std::uint8_t> out;out.reserve(detailed?header+stride*frames.size()+effectBytes*dustCount:16+28*(frames.size()/3+2));
    auto u32=[&](std::uint32_t n){for(unsigned b=0;b<4;++b)out.push_back(std::uint8_t(n>>(b*8)));};
    auto scalar=[&](float n){u32(std::bit_cast<std::uint32_t>(n));};
    u32(detailed?0x32524449:0x31524449);u32(finish);u32(0);u32(60);unsigned count=0;
    // Revision4 adds a per-pose accepted flash state and an explicit world-
    // effects stride. This also supports imported courses/opponent streams
    // without inventing a smoke timeline or changing revisions1..3.
    if(detailed){u32(header);u32(stride);u32(flashes?4:smoke?3:dust?2:1);u32(12);for(auto v:profile)u32(v);for(auto v:priorRecords)u32(v);u32(dustCount);if(flashes)u32(dust?effectBytes:0);}
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
            if(dust||flashes){
                if(f.dustStep>dustCount||(i&&f.dustStep<frames[i-1].dustStep))return {};
                u32(f.dustStep);
            }
            if(flashes){if(f.backfire< -1||f.backfire>1)return {};u32(unsigned(f.backfire+1));}
        }
    }
    for(unsigned i=0;i<dustCount;++i){
        const auto& d=smoke?smokeFrames[i].owner:dustFrames[i];if(!d.valid())return {};
        u32(d.flags);u32(d.surfaces);scalar(d.displacement);
        for(float v:d.footprint)scalar(v);for(const auto& p:d.positions)for(float v:p)scalar(v);
        if(smoke){const auto& s=smokeFrames[i];if(!s.valid()||((s.scene^smokeFrames.front().scene)&ReplayCarSmokeFrame::fixedSceneMask))return {};
            u32(s.intensity);u32(s.scene);u32(s.randomBefore);}
    }
    for(unsigned b=0;b<4;++b)out[8+b]=std::uint8_t(count>>(b*8));
    return out;
}
std::vector<std::uint8_t> Replay::telemetryBytes() const {
    if(!detailed||truncated||frames.empty()||frames.size()>108001)return {};
    std::vector<std::uint8_t> out;out.reserve(16+32*frames.size());
    auto u32=[&](std::uint32_t n){for(unsigned b=0;b<4;++b)out.push_back(std::uint8_t(n>>(b*8)));};
    auto scalar=[&](float n){u32(std::bit_cast<std::uint32_t>(n));};
    u32(0x31544449);u32(4);u32(32);u32(std::uint32_t(frames.size()));
    for(std::size_t i=0;i<frames.size();++i){
        if(!frames[i].telemetryAvailable||(i&&frames[i].tick!=frames[i-1].tick+1))return {};
        const auto& t=frames[i].telemetry;
        for(float v:{t.slip,t.yawRate,t.speedLoss,t.penalty,t.wallImpact,t.pendingRotation,t.applyingRotation}){if(!std::isfinite(v))return {};scalar(v);}
        u32(t.flags);
    }
    return out;
}
bool Replay::save(const std::string& filename) const {
    return saveCopies(filename,{})[0];
}
std::array<bool,2> Replay::saveCopies(const std::string& first,const std::string& second) const {
    std::array<bool,2> saved{};
    if(frames.empty()||truncated) return saved;
    const std::array<std::string,2> filenames{first,second};
    std::array<std::ofstream,2> outputs;
    for(unsigned i=0;i<2;++i){
        if(filenames[i].empty()||(i==1&&first==second))continue;
        outputs[i].open(filenames[i]+".tmp");
        if(outputs[i])outputs[i]<<"tick,speed,yaw,pos_x,pos_y,pos_z,gear"<<(finishTicks6000?",finish_ticks6000\n":"\n");
    }
    if(!outputs[0].is_open()&&!outputs[1].is_open())return saved;
    const auto write=[&](const char* data,std::streamsize count){for(auto& out:outputs)if(out.is_open()&&out)out.write(data,count);};
    // Locale-independent round-trip float precision, buffered as a block.
    // Per-value iostream formatting used to stall the finish frame twice:
    // once for last_run and once for a new personal-best ghost.
    std::array<char,65536> buffer;char* cursor=buffer.data();
    const auto integer=[&](auto value){cursor=std::to_chars(cursor,buffer.data()+buffer.size(),value).ptr;};
    const auto scalar=[&](float value){cursor=std::to_chars(cursor,buffer.data()+buffer.size(),value,std::chars_format::general,9).ptr;};
    for(const auto& f:frames) {
        if(buffer.data()+buffer.size()-cursor<512){write(buffer.data(),cursor-buffer.data());cursor=buffer.data();}
        integer(f.tick);*cursor++=',';scalar(f.speed);*cursor++=',';scalar(f.yaw);*cursor++=',';
        scalar(f.position.x);*cursor++=',';scalar(f.position.y);*cursor++=',';scalar(f.position.z);*cursor++=',';integer(f.gear);
        if(finishTicks6000){*cursor++=',';integer(&f==&frames.back()?finishTicks6000:0);}
        *cursor++='\n';
    }
    write(buffer.data(),cursor-buffer.data());
    for(unsigned i=0;i<2;++i){
        auto& out=outputs[i];if(!out.is_open())continue;
        out.close();if(!out)continue;
        const auto& filename=filenames[i];const auto tmp=filename+".tmp";
        std::error_code ec;
        std::filesystem::rename(tmp,filename,ec);
    // On Windows rename does not replace an existing file. Keep previous ghost
    // recoverable while installing the completely-written replacement.
        if(ec) {
            const auto old=filename+".previous";
            std::filesystem::remove(old,ec);ec.clear();
            std::filesystem::rename(filename,old,ec);if(ec)continue;
            std::filesystem::rename(tmp,filename,ec);
            if(ec){std::error_code restore;std::filesystem::rename(old,filename,restore);continue;}
            std::filesystem::remove(old,ec);
        }
        saved[i]=true;
    }
    if(!second.empty()&&first==second)saved[1]=saved[0];
    return saved;
}
bool Replay::load(const std::string& filename) {
    frames.clear();dustFrames.clear();smokeFrames.clear();dustCapture_=smokeCapture_=backfireCapture_=false;truncated=false;finishTicks6000=0;std::ifstream in(filename);if(!in) return false;
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
