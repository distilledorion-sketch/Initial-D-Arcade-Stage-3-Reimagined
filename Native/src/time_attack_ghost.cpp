#include "time_attack_ghost.h"
#include "imported_course_catalog.h"
#include <bit>
#include <fstream>

namespace idas3 {
namespace {
constexpr std::uint32_t magic=0x31474449; // IDG1, little endian
void put(std::ostream& out,std::uint32_t n){for(unsigned i=0;i<4;++i)out.put(char(n>>(8*i)));}
std::uint32_t get(std::istream& in){std::uint32_t n=0;for(unsigned i=0;i<4;++i)n|=std::uint32_t(std::uint8_t(in.get()))<<(8*i);return n;}
bool valid(const Replay& run,unsigned car){
    if(car>=35||run.truncated||!run.finishTicks6000||run.finishTicks6000>=10800000||run.frames.size()<2||run.frames.size()>Replay::maxFrames)return false;
    for(std::size_t i=0;i<run.frames.size();++i){
        const auto& f=run.frames[i];
        if(f.tick!=i+1||!std::isfinite(f.position.x)||!std::isfinite(f.position.y)||!std::isfinite(f.position.z)||
           !std::isfinite(f.yaw)||!std::isfinite(f.detail.pitch)||!std::isfinite(f.detail.roll))return false;
    }
    return run.finishTicks6000<=run.frames.back().tick*100;
}
}
std::filesystem::path TimeAttackGhost::path(const std::filesystem::path& profiles,unsigned course,bool reverse,bool wet){
    const auto revision=localTimeAttackRevision(course);
    return profiles/"ghosts_v1"/("course_"+std::to_string(course)+(reverse?"_reverse":"_forward")+(wet?"_wet":"_dry")+
        (revision?"_r"+std::to_string(revision):"")+".idghost");
}
bool TimeAttackGhost::load(const std::filesystem::path& path){
    replay=Replay{};car=0;
    std::ifstream in(path,std::ios::binary);if(!in||get(in)!=magic)return false;
    Replay loaded;loaded.finishTicks6000=get(in);const auto model=get(in),count=get(in);
    // Check size before allocation; truncated/corrupt files never produce a ghost.
    if(count<2||count>Replay::maxFrames)return false;
    in.seekg(0,std::ios::end);if(in.tellg()!=std::streamoff(16+28ull*count))return false;in.seekg(16);
    loaded.frames.reserve(count);
    for(unsigned i=0;i<count;++i){
        ReplayFrame f{};f.tick=get(in);
        for(float* v:{&f.position.x,&f.position.y,&f.position.z,&f.yaw,&f.detail.pitch,&f.detail.roll})*v=std::bit_cast<float>(get(in));
        loaded.frames.push_back(f);
    }
    if(!in||!valid(loaded,model))return false;
    replay=std::move(loaded);car=model;return true;
}
TimeAttackGhost::Saved TimeAttackGhost::saveBest(const std::filesystem::path& path,const Replay& run,unsigned car){
    if(!valid(run,car))return Saved::Failed;
    TimeAttackGhost prior;
    if(prior.load(path)&&prior.replay.finishTicks6000<=run.finishTicks6000)return Saved::Unchanged;
    std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);if(ec)return Saved::Failed;
    auto temporary=path;temporary+=".tmp";auto backup=path;backup+=".previous";
    std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out)return Saved::Failed;
    put(out,magic);put(out,run.finishTicks6000);put(out,car);put(out,unsigned(run.frames.size()));
    for(const auto& f:run.frames){put(out,unsigned(f.tick));for(float v:{f.position.x,f.position.y,f.position.z,f.yaw,f.detail.pitch,f.detail.roll})put(out,std::bit_cast<std::uint32_t>(v));}
    out.close();if(!out)return Saved::Failed;
    std::filesystem::rename(temporary,path,ec);if(!ec)return Saved::Replaced;
    std::filesystem::remove(backup,ec);ec.clear();std::filesystem::rename(path,backup,ec);if(ec)return Saved::Failed;
    std::filesystem::rename(temporary,path,ec);
    if(ec){std::error_code restore;std::filesystem::rename(backup,path,restore);return Saved::Failed;}
    std::filesystem::remove(backup,ec);return Saved::Replaced;
}
ReplayFrame TimeAttackGhost::sample(double tick) const{
    auto pose=replay.sample(tick);if(replay.frames.empty())return pose;
    const auto lower=std::size_t(std::clamp(std::floor(tick)-1,0.,double(replay.frames.size()-1)));
    const auto upper=std::min(lower+1,replay.frames.size()-1);
    const float blend=float(std::clamp(tick-double(replay.frames[lower].tick),0.,1.));
    pose.detail.pitch=lerpAngle(replay.frames[lower].detail.pitch,replay.frames[upper].detail.pitch,blend);
    pose.detail.roll=lerpAngle(replay.frames[lower].detail.roll,replay.frames[upper].detail.roll,blend);
    return pose;
}
}
