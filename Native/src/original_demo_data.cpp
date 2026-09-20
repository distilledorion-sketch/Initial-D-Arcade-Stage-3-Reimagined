#include "original_demo_data.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <cmath>

namespace idas3::original {
namespace {
std::vector<std::uint32_t> readWords(const std::filesystem::path& p) {
    std::ifstream f(p,std::ios::binary|std::ios::ate);
    if(!f)throw std::runtime_error("Original demo data unavailable: "+p.string());
    const auto n=f.tellg();
    if(n<0||n>4*1024*1024||n%4)throw std::runtime_error("Invalid original demo data size");
    std::vector<std::uint32_t> out(std::size_t(n)/4);f.seekg(0);
    f.read(reinterpret_cast<char*>(out.data()),n);
    if(!f)throw std::runtime_error("Truncated original demo data");return out;
}
}
OriginalDemoData OriginalDemoData::load(const std::filesystem::path& root) {
    OriginalDemoData result;const auto dir=root/"data/original_assets/attract/demo";
    const auto pose=readWords(dir/"actors.bin");const auto table=readWords(dir/"shots.bin");
    const auto camera=readWords(dir/"cameras.bin");const auto cars=readWords(dir/"cars.bin");
    const auto enemies=readWords(dir/"enemies.bin");
    if(pose.empty()||pose.size()%84||table.empty()||table.size()%6||table.size()/6>256||camera.empty()||camera[0]!=table.size()/6||cars.size()!=table.size()/3||enemies.size()!=cars.size())
        throw std::runtime_error("Original demo bank counts disagree");
    result.actors_.resize(pose.size()/42);
    std::memcpy(result.actors_.data(),pose.data(),pose.size()*4);
    result.shots_.resize(table.size()/6);
    unsigned cumulative=0;std::size_t cursor=1;
    for(unsigned i=0;i<result.shots_.size();++i){
        auto& s=result.shots_[i];std::copy_n(table.begin()+i*6,6,s.frames.begin());
        const auto previousEnd=s.frames[1];
        if(cumulative>=result.frameCount())throw std::runtime_error("Original demo cumulative frame bound");
        for(unsigned k=0;k<4;++k){if(s.frames[k]>=result.frameCount()-cumulative)throw std::runtime_error("Invalid original demo shot range");s.frames[k]+=cumulative;}
        if(s.frames[0]>s.frames[1]||s.frames[2]>s.frames[3])throw std::runtime_error("Reversed original demo shot range");
        cumulative+=previousEnd+1;
        if(cursor>=camera.size())throw std::runtime_error("Missing original demo camera");
        const unsigned byteSize=camera[cursor++],words=byteSize/4;
        if(byteSize%4||words<1||words>256||words>camera.size()-cursor)throw std::runtime_error("Invalid original demo camera record");
        s.camera.assign(camera.begin()+cursor,camera.begin()+cursor+words);cursor+=words;
        for(unsigned actor=0;actor<2;++actor){s.cars[actor]=cars[i*2+actor];s.enemies[actor]=enemies[i*2+actor];if(s.cars[actor]>=35||s.enemies[actor]>=31)throw std::runtime_error("Invalid original demo car binding");}
    }
    if(cumulative!=result.frameCount()||camera.size()-cursor<result.shots_.size()*11)throw std::runtime_error("Incomplete original demo timeline");
    for(auto& s:result.shots_){std::copy_n(camera.begin()+cursor,11,s.descriptor.begin());cursor+=11;}
    if(std::filesystem::exists(dir/"camera_world.bin")){
        const auto frames=readWords(dir/"camera_world.bin");
        if(frames.size()!=std::size_t(result.frameCount())*17)throw std::runtime_error("Original demo camera capture count");
        result.cameraFrames_.resize(result.frameCount());
        for(unsigned i=0;i<result.frameCount();++i){auto& f=result.cameraFrames_[i];
            for(unsigned j=0;j<16;++j){f.world[j]=std::bit_cast<float>(frames[i*17+j]);if(!std::isfinite(f.world[j]))throw std::runtime_error("Original demo nonfinite camera");}
            f.fovPhase=frames[i*17+16];if(!f.fovPhase||f.fovPhase>=32768)throw std::runtime_error("Original demo camera FOV bound");
        }
    }
    return result;
}
const OriginalDemoActor& OriginalDemoData::actor(unsigned frame,unsigned slot)const {
    if(frame>=frameCount()||slot>=2)throw std::out_of_range("Original demo actor frame");
    return actors_[frame*2+slot];
}
unsigned OriginalDemoData::step(OriginalDemoCursor& s)const {
    if(s.shot>=shots_.size()||s.frame>=frameCount())throw std::out_of_range("Original demo cursor");
    unsigned event=0;
    if(s.frame==shots_[s.shot].frames[3]){
        ++s.shot;event=1;if(s.shot>=shots_.size()){s.shot=0;event=2;}
        s.frame=shots_[s.shot].frames[2];
    }else ++s.frame;
    if(s.frame>=frameCount())s.frame=0;
    return event;
}
}
