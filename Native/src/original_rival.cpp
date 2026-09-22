#include "original_rival.h"
#include "original_math.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace idas3::original {
namespace {
constexpr std::uint32_t tableBase=0x0C271618,tableEnd=0x0C283DE4;
constexpr float lit(std::uint32_t bits){return std::bit_cast<float>(bits);}
std::vector<std::byte> read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Missing original rival data");const auto size=f.tellg();if(size<0||size>2000000)throw std::runtime_error("Invalid rival data size");std::vector<std::byte> out(static_cast<std::size_t>(size));f.seekg(0);f.read(reinterpret_cast<char*>(out.data()),size);if(!f)throw std::runtime_error("Truncated rival data");return out;}
std::uint32_t u32(std::span<const std::byte> b,std::size_t at){if(at>b.size()||b.size()-at<4)throw std::out_of_range("Rival data word");std::uint32_t out;std::memcpy(&out,b.data()+at,4);return out;}
std::uint32_t fnv(std::span<const std::byte> b){std::uint32_t value=2166136261;for(auto x:b)value=(value^std::to_integer<std::uint8_t>(x))*16777619;return value;}
std::int32_t signedWord(std::uint32_t v){return std::bit_cast<std::int32_t>(v);}
float angle(float x,float z){float value=float(signedWord(originalAtan2Angle(x,z)));value*=lit(0x40490FDB);value*=lit(0x38000000);return value;}
std::int32_t ftrc(float v){if(v>=2147483648.f)return INT32_MAX;if(v<=-2147483648.f||std::isnan(v))return INT32_MIN;return std::int32_t(v);}
}
OriginalRivalData OriginalRivalData::load(const std::filesystem::path& root){
    auto file=read(root/"tables.bin");
    constexpr std::array<std::uint8_t,32> sourceSha{0xef,0xda,0x83,0x1f,0x12,0x12,0xdb,0x54,0xcc,0x2e,0x4b,0xa5,0x34,0x24,0xfe,0x39,0x0f,0x91,0xb2,0xda,0xab,0xb0,0x7e,0x93,0xc1,0xe3,0x89,0xc3,0x73,0x6d,0x03,0x35};
    if(file.size()!=56+tableEnd-tableBase||std::memcmp(file.data(),"ID3RIV01",8)||u32(file,8)!=1||u32(file,12)!=tableBase||u32(file,16)!=tableEnd-tableBase||std::memcmp(file.data()+24,sourceSha.data(),32))throw std::runtime_error("Original rival table identity mismatch");
    const auto payload=std::span<const std::byte>(file).subspan(56);if(fnv(payload)!=u32(file,20))throw std::runtime_error("Original rival table checksum mismatch");
    OriginalRivalData out;out.bytes_.assign(payload.begin(),payload.end());return out;
}
std::uint32_t OriginalRivalData::word(std::uint32_t a)const{if(a<tableBase||a>=tableEnd||(a&3))throw std::out_of_range("Original rival lookup address");return u32(bytes_,a-tableBase);}
OriginalRivalPath OriginalRivalData::loadPath(const std::filesystem::path& root,std::uint32_t condition,bool alternate)const{
    if(condition>=18)throw std::invalid_argument("Rival condition must be0..17");std::ostringstream name;name<<"path_"<<std::setw(2)<<std::setfill('0')<<condition<<(alternate?"_alternate.bin":".bin");auto file=read(root/name.str());const auto last=word(tableBase+condition*8);
    if(file.size()<24||std::memcmp(file.data(),alternate?"ID3RVA01":"ID3RVP01",8)||u32(file,8)!=condition||u32(file,12)!=last||u32(file,16)!=file.size()-24)throw std::runtime_error("Original rival path identity mismatch");const auto payload=std::span<const std::byte>(file).subspan(24);if(fnv(payload)!=u32(file,20)||payload.size()/12<last+11)throw std::runtime_error("Original rival path lookahead/checksum mismatch");
    OriginalRivalPath out{condition,last,{}};out.points.resize(payload.size()/12);for(std::size_t i=0;i<out.points.size();++i)for(std::size_t k=0;k<3;++k)out.points[i][k]=std::bit_cast<float>(u32(payload,i*12+k*4));return out;
}
bool updateOriginalRivalPace(OriginalRivalState& r,OriginalActorState& pub,std::uint32_t& ticks,
    const OriginalRivalData& data,const OriginalRivalPath& path,const OriginalRivalPaceInputs& in,
    const OriginalDriveState& player,const OriginalActorState& actor){
    ++ticks;
    if(r.u(0)==0)return false;
    if(in.condition0C9015CC>=18||in.profile0CAA9868>=32||path.condition!=in.condition0C9015CC||path.inclusiveLastIndex!=data.word(tableBase+path.condition*8)||path.points.size()<path.inclusiveLastIndex+11||r.u(12)>path.inclusiveLastIndex)
        throw std::invalid_argument("Original rival pace selection/path state outside source memory contract");
    const auto profile=in.profile0CAA9868,condition=in.condition0C9015CC;
    const auto last=std::int32_t(path.inclusiveLastIndex);
    auto nearest=std::int32_t(r.u(12)),candidate=nearest-64;if(candidate<0)candidate+=last;
    float best=lit(0x4CBEBC20);r.setu(20,0);
    for(unsigned i=0;i<128;++i,++candidate){if(candidate>last)candidate=0;
        const auto& p=path.points[std::size_t(candidate)];const float x=p[0]-r.f(200),z=p[2]-r.f(208);
        float distance=z*z;distance=std::fma(x,x,distance);if(best>distance){best=distance;nearest=candidate;}
    }
    if(nearest==last){nearest=0;r.setu(16,1);}r.setu(12,std::uint32_t(nearest));
    const auto delta=signedWord(std::uint32_t(nearest)-player.u(0x118));
    const auto upper=signedWord(data.word(0x0C271E2C+profile*8)),lower=signedWord(data.word(0x0C271E30+profile*8));
    const std::int32_t band=upper>delta&&lower<delta?0:delta>=0?1:-1;r.setu(88,std::uint32_t(band));
    const auto& next=path.points[std::size_t(nearest)+1];const auto& ahead=path.points[std::size_t(nearest)+11];
    float bend=angle(next[0]-r.f(200),next[2]-r.f(208))-angle(ahead[0]-r.f(200),ahead[2]-r.f(208));
    if(std::abs(bend)>lit(0x40490FDB))bend+=bend>0?lit(0xC0C90FDB):lit(0x40C90FDB);
    bend=std::abs(bend);bend/=lit(0x40060A92);r.setf(64,bend);
    float coefficient=data.scalar(0x0C271CB4+profile*12);coefficient*=data.scalar(0x0C2723E4+(in.level0C9015D0&15)*4);
    if(profile==31){const auto progressIndex=data.word(0x0C27239C+condition*4);if(progressIndex>=in.progress0C901604.size())throw std::invalid_argument("Original rival progress index");
        const float progress=float(in.progress0C901604[progressIndex]&15),low=data.scalar(0x0C27230C+condition*4),high=data.scalar(0x0C272354+condition*4);
        coefficient=high-low;coefficient=progress*coefficient;coefficient/=15.f;coefficient+=low;
    }
    if(player.u(0x434))coefficient*=lit(0x3F7851EC);if(player.u(0x438))coefficient*=lit(0x3F6E147B);
    float target=float(signedWord(data.word(0x0C2724A4+condition*4000+std::uint32_t(nearest)*4)));
    if(band==0){
        if(signedWord(ticks)<=779)target*=data.scalar(0x0C271CB0+profile*12);
        for(unsigned region=0;region<2;++region){const auto base=0x0C271F2C+profile*24+region*12;const auto begin=signedWord(data.word(base)),end=signedWord(data.word(base+4));if(nearest>begin&&nearest<end)target*=data.scalar(base+8);}
        target*=coefficient;target*=data.scalar(0x0C27222C+(in.opponentProgress0C901644&15)*4);
    }else target*=band==1?lit(0x3F333333):lit(0x3F99999A);
    if(in.aiDifficulty)target*=1.f+.05f*float(std::min(in.aiDifficulty,2u));
    float correction=target-r.f(68);correction/=3.f;if(correction< -1.f)correction=-1.f;else if(correction>1.f)correction=1.f;
    float speed=r.f(68)+correction;r.setf(68,speed);
    const bool deceleration=data.scalar(0x0C271CAC+profile*12)>correction;
    auto flags=(pub.u(92)&~3u)|std::uint32_t(deceleration);if(deceleration&&(std::uint32_t(ftrc(speed))&3)==0)flags|=2;pub.setu(92,flags);
    if(player.u(0x1A8))speed=player.f(0x248)*player.f(0x250);
    if(!(speed>0.f))speed=0.f;
    if((actor.u(0x50)&0x8000)==0)speed=0.f;
    if(condition>3&&r.u(16)==1)speed=0.f;
    r.setf(68,speed);r.setf(72,speed/200.f);
    return true;
}
} // namespace idas3::original
