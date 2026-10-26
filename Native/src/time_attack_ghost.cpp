#include "time_attack_ghost.h"
#include "imported_course_catalog.h"
#include <bit>
#include <fstream>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace idas3 {
namespace {
constexpr std::uint32_t magic=0x31474449; // IDG1, little endian
void put(std::ostream& out,std::uint32_t n){for(unsigned i=0;i<4;++i)out.put(char(n>>(8*i)));}
std::uint32_t get(std::istream& in){std::uint32_t n=0;for(unsigned i=0;i<4;++i)n|=std::uint32_t(std::uint8_t(in.get()))<<(8*i);return n;}
// Preserve IDG1 byte-for-byte, but cross the stream boundary once per block,
// not once per byte of every recorded pose. Keep memory bounded for long runs.
constexpr std::size_t ghostFrameBytes=28,ghostBlockFrames=1024;
using GhostBlock=std::array<std::uint8_t,ghostFrameBytes*ghostBlockFrames>;
void putWord(std::uint8_t*& cursor,std::uint32_t n){for(unsigned i=0;i<4;++i)*cursor++=std::uint8_t(n>>(8*i));}
std::uint32_t getWord(const std::uint8_t*& cursor){std::uint32_t n=0;for(unsigned i=0;i<4;++i)n|=std::uint32_t(*cursor++)<<(8*i);return n;}
bool valid(const Replay& run,unsigned car){
    if(car>=35||run.truncated||!run.finishTicks6000||run.finishTicks6000>=10800000||run.frames.size()<2||run.frames.size()>Replay::maxFrames)return false;
    for(std::size_t i=0;i<run.frames.size();++i){
        const auto& f=run.frames[i];
        if(f.tick!=i+1||!std::isfinite(f.position.x)||!std::isfinite(f.position.y)||!std::isfinite(f.position.z)||
           !std::isfinite(f.yaw)||!std::isfinite(f.detail.pitch)||!std::isfinite(f.detail.roll))return false;
    }
    return run.finishTicks6000<=run.frames.back().tick*100;
}
// 0 is absent, 1 is an ordinary entry, -1 is an error or a linked/wrong type.
// Check every directory beneath the supplied save root before removing a file.
int plainEntry(const std::filesystem::path& path,bool directory){
    std::error_code ec;const auto status=std::filesystem::symlink_status(path,ec);
    if(ec==std::errc::no_such_file_or_directory||status.type()==std::filesystem::file_type::not_found)return 0;
    if(ec||(directory?!std::filesystem::is_directory(status):!std::filesystem::is_regular_file(status)))return -1;
#if defined(_WIN32)
    const auto attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return -1;
#endif
    return 1;
}
int plainSubdirectory(const std::filesystem::path& root,const std::filesystem::path& relative){
    auto current=root;const int base=plainEntry(current,true);if(base!=1)return base;
    for(const auto& part:relative){current/=part;const int entry=plainEntry(current,true);if(entry!=1)return entry;}
    return 1;
}
bool removeRetiredFile(const std::filesystem::path& path){
    const int entry=plainEntry(path,false);if(entry==0)return true;if(entry<0)return false;
    std::error_code ec;std::filesystem::remove(path,ec);return !ec;
}
bool removeRetiredCopies(const std::filesystem::path& path){
    bool success=true;
    for(const char* suffix:{"",".tmp",".previous"}){auto copy=path;copy+=suffix;if(!removeRetiredFile(copy))success=false;}
    return success;
}
}
std::filesystem::path TimeAttackGhost::path(const std::filesystem::path& profiles,unsigned course,bool reverse,bool wet){
    const auto revision=localTimeAttackRevision(course);
    return profiles/"ghosts_v1"/("course_"+std::to_string(course)+(reverse?"_reverse":"_forward")+(wet?"_wet":"_dry")+
        (revision?"_r"+std::to_string(revision):"")+".idghost");
}
std::filesystem::path TimeAttackGhost::telemetryPath(const std::filesystem::path& saves,const std::string& runKey,unsigned course){
    return saves/(runKey+(course==17?"_r"+std::to_string(localTimeAttackRevision(course)):"")+".csv");
}
bool TimeAttackGhost::removeRetiredOdawaraGhosts(const std::filesystem::path& saves){
    if(saves.empty())return false;
    const int root=plainEntry(saves,true);if(root==0)return true;if(root<0)return false;
    bool success=true;
    for(int slot=-1;slot<5;++slot){
        const auto relative=slot<0?std::filesystem::path("driver_profiles_v1")/"ghosts_v1":
            std::filesystem::path("saves")/("slot_"+std::to_string(slot+1))/"driver_profiles_v1"/"ghosts_v1";
        const int directory=plainSubdirectory(saves,relative);
        if(directory<0){success=false;continue;}if(directory==0)continue;
        for(bool reverse:{false,true})for(bool wet:{false,true})for(unsigned revision=0;revision<localTimeAttackRevision(17);++revision){
            const auto file=saves/relative/(std::string("course_17")+(reverse?"_reverse":"_forward")+(wet?"_wet":"_dry")+
                (revision?"_r"+std::to_string(revision):"")+".idghost");
            if(!removeRetiredCopies(file))success=false;
        }
    }
    // Older native builds also kept per-model best telemetry beside the cabinet
    // CSV. It is a ghost, not a user-selected .idreplay archive. Current Odawara
    // writes use _r2, so retries never remove runs earned after the reset.
    for(const char* owner:{"original_race_v3_","development_v01_"})for(bool reverse:{false,true})for(bool wet:{false,true})
    for(unsigned car=0;car<35;++car)for(bool automatic:{false,true}){
        const auto file=saves/(std::string(owner)+"odawara"+(reverse?"_rev":"_fwd")+(wet?"_wet":"_dry")+"_car"+
            std::to_string(car)+(automatic?"_at":"_mt")+".csv");
        if(!removeRetiredCopies(file))success=false;
    }
    return success;
}
bool TimeAttackGhost::load(const std::filesystem::path& path){
    replay=Replay{};car=0;
    std::ifstream in(path,std::ios::binary);if(!in||get(in)!=magic)return false;
    Replay loaded;loaded.finishTicks6000=get(in);const auto model=get(in),count=get(in);
    // Check size before allocation; truncated/corrupt files never produce a ghost.
    if(count<2||count>Replay::maxFrames)return false;
    in.seekg(0,std::ios::end);if(in.tellg()!=std::streamoff(16+28ull*count))return false;in.seekg(16);
    loaded.frames.reserve(count);
    GhostBlock block;
    for(std::size_t base=0;base<count;base+=ghostBlockFrames){
        const auto size=std::min(ghostBlockFrames,std::size_t(count)-base);
        if(!in.read(reinterpret_cast<char*>(block.data()),std::streamsize(size*ghostFrameBytes)))return false;
        const auto* cursor=block.data();
        for(std::size_t i=0;i<size;++i){
            ReplayFrame f{};f.tick=getWord(cursor);
            for(float* v:{&f.position.x,&f.position.y,&f.position.z,&f.yaw,&f.detail.pitch,&f.detail.roll})*v=std::bit_cast<float>(getWord(cursor));
            loaded.frames.push_back(f);
        }
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
    GhostBlock block;
    for(std::size_t base=0;base<run.frames.size();base+=ghostBlockFrames){
        const auto size=std::min(ghostBlockFrames,run.frames.size()-base);auto* cursor=block.data();
        for(std::size_t i=0;i<size;++i){
            const auto& f=run.frames[base+i];putWord(cursor,unsigned(f.tick));
            for(float v:{f.position.x,f.position.y,f.position.z,f.yaw,f.detail.pitch,f.detail.roll})putWord(cursor,std::bit_cast<std::uint32_t>(v));
        }
        if(!out.write(reinterpret_cast<const char*>(block.data()),std::streamsize(size*ghostFrameBytes)))return Saved::Failed;
    }
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
