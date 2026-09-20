#include "original_data.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace idas3::original {
namespace {
std::vector<std::byte> readFile(const std::filesystem::path& path){
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input)throw std::runtime_error("Original physics data is unavailable: "+path.string());
    const auto size=input.tellg();
    if(size<0||size>1024*1024)throw std::runtime_error("Original physics data has an invalid size");
    std::vector<std::byte> result(static_cast<std::size_t>(size));input.seekg(0);
    if(!input.read(reinterpret_cast<char*>(result.data()),size))throw std::runtime_error("Original physics data is truncated");
    return result;
}
std::uint32_t u32(std::span<const std::byte> data,std::size_t offset){
    if(offset>data.size()||data.size()-offset<4)throw std::runtime_error("Truncated original physics table word");
    return std::to_integer<std::uint32_t>(data[offset])|(std::to_integer<std::uint32_t>(data[offset+1])<<8)|
        (std::to_integer<std::uint32_t>(data[offset+2])<<16)|(std::to_integer<std::uint32_t>(data[offset+3])<<24);
}
std::uint32_t fnv1a(std::span<const std::byte> data){std::uint32_t value=2166136261u;for(auto b:data)value=(value^std::to_integer<std::uint32_t>(b))*16777619u;return value;}
void identity(std::span<const std::byte> data,const char* magic){
    if(data.size()<16)throw std::runtime_error("Truncated original physics data header");
    for(std::size_t i=0;i<8;++i)if(data[i]!=std::byte(magic[i]))throw std::runtime_error("Original physics data format mismatch");
}
void validateSelection(const OriginalPhysicsSelection& s){
    if(s.vehicleIndex>=35||s.conditionCode>=18||s.vehicleMode0C9015E0>=32||s.upgradeIndex0C9015F0>=76)
        throw std::invalid_argument("Original physics selection outside the recovered table bounds");
}
constexpr std::array<std::array<std::uint32_t,2>,20> expectedSections{{
    {0x0C283E08,128},{0x0C283E88,144},{0x0C283F18,1540},{0x0C28451C,2520},
    {0x0C284EF4,140},{0x0C284F80,280},{0x0C285098,140},
    {0x0C285124,1260},{0x0C285610,1260},{0x0C285AFC,1260},{0x0C285FE8,1260},
    {0x0C2864D4,1260},{0x0C2869C0,1260},{0x0C286EAC,1260},{0x0C287398,1260},
    {0x0C287884,1260},{0x0C287D70,1260},{0x0C28825C,176},{0x0C28830C,864},{0x0C28866C,304}}};
}
std::filesystem::path originalCollisionFile(std::uint32_t condition,std::uint32_t selector){
    if(condition>=18||selector>1)throw std::invalid_argument("Original collision selection outside18 courses/directions and2 selectors");
    // Exact slots5/6 in042700's0C2EFF40 records. Weather/night records
    // repeat these names. Tsuchisaka intentionally reverses the file suffix.
    constexpr std::array<const char*,9> stems{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df"};
    const auto course=condition/2;
    std::string name="collision_";name+=stems[course];
    if(course>1)name+="_"+std::to_string(course==7?1-selector:selector);
    return name+".rcl";
}
void selectOriginalWeather(OriginalPhysicsSelection& selection,OriginalWeather weather){
    const auto code=static_cast<std::uint32_t>(weather);
    if(code>1)throw std::invalid_argument("Original weather code must be dry0 or rain1");
    selection.mode0C9015FC=code;
}
OriginalPhysicsSelection makeOriginalFreshTimeAttackSelection(std::uint32_t car,std::uint32_t condition,OriginalWeather weather){
    OriginalPhysicsSelection selection;
    selection.vehicleIndex=car;selection.conditionCode=condition;
    //1348A0 clears profile+24, +152 and +164.133A60 changes only car+16.
    selection.vehicleMode0C9015E0=0;selection.upgradeIndex0C9015F0=0;
    selection.overrideMode0C9015F4=0; // override requires AE86 upgrade>4
    selection.mode0C9015C0=condition>15?1u:0u;
    // Mode2 supplies opponent-1; only opponent-2 enables this correction.
    selection.progressEnabled0C9015E4=0;
    selection.progressMode0C9015D4=0; //061B38 clears4004D8 before159720
    selection.progress0C901650=0.0f; // ignored while the correction is disabled
    selectOriginalWeather(selection,weather);validateSelection(selection);
    return selection;
}
OriginalPhysicsData OriginalPhysicsData::load(const std::filesystem::path& path){
    const auto file=readFile(path);identity(file,"ID3TBL01");
    if(u32(file,8)!=1||u32(file,12)!=expectedSections.size()||file.size()<48+16*expectedSections.size())
        throw std::runtime_error("Unsupported original physics data version/directory");
    constexpr char digest[]="efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335";
    const auto nibble=[](char c){return c<='9'?c-'0':c-'a'+10;};
    for(std::size_t i=0;i<32;++i)if(file[16+i]!=std::byte(nibble(digest[i*2])*16+nibble(digest[i*2+1])))
        throw std::runtime_error("Original physics source identity mismatch");
    OriginalPhysicsData result;std::size_t cursor=48+16*expectedSections.size();
    for(std::size_t i=0;i<expectedSections.size();++i){
        const auto address=u32(file,48+i*16),size=u32(file,52+i*16),offset=u32(file,56+i*16),checksum=u32(file,60+i*16);
        if(address!=expectedSections[i][0]||size!=expectedSections[i][1]||offset!=cursor||offset>file.size()||size>file.size()-offset)
            throw std::runtime_error("Invalid original physics table directory");
        const auto section=std::span<const std::byte>(file).subspan(offset,size);
        if(fnv1a(section)!=checksum)throw std::runtime_error("Original physics table checksum mismatch");
        result.sections_.push_back({address,{section.begin(),section.end()}});cursor+=size;
    }
    if(cursor!=file.size())throw std::runtime_error("Unexpected original physics table trailing bytes");
    return result;
}
std::span<const std::byte> OriginalPhysicsData::bytes(std::uint32_t address,std::size_t size) const{
    for(const auto& section:sections_)if(address>=section.address&&address-section.address<=section.bytes.size()&&
            size<=section.bytes.size()-(address-section.address))return std::span<const std::byte>(section.bytes).subspan(address-section.address,size);
    throw std::out_of_range("Requested word is not an exported original physics table");
}
std::uint32_t OriginalPhysicsData::word(std::uint32_t address) const{return u32(bytes(address,4),0);}
float OriginalPhysicsData::scalar(std::uint32_t address) const{return std::bit_cast<float>(word(address));}
OriginalPhysicsPath OriginalPhysicsData::loadPath(const std::filesystem::path& root,std::uint32_t condition) const{
    if(condition>=18)throw std::invalid_argument("Original physics path condition outside18 cases");
    std::ostringstream name;name<<"path_"<<std::setw(2)<<std::setfill('0')<<condition<<".bin";
    const auto file=readFile(root/name.str());identity(file,"ID3PATH1");
    const auto count=word(0x0C283E88+condition*8)+1;
    if(u32(file,8)!=condition||u32(file,12)!=count||file.size()!=16+std::size_t(count)*12)
        throw std::runtime_error("Original physics path does not match its original bounds");
    OriginalPhysicsPath out{condition,count-1,{}};out.points.resize(count);
    for(std::size_t i=0;i<count;++i)for(std::size_t axis=0;axis<3;++axis)out.points[i][axis]=std::bit_cast<float>(u32(file,16+i*12+axis*4));
    return out;
}
std::array<std::uint32_t,11> OriginalPhysicsData::carRecord(std::uint32_t index) const{
    if(index>=35)throw std::invalid_argument("Original vehicle index outside35 cars");
    std::array<std::uint32_t,11> record{};for(std::size_t i=0;i<11;++i)record[i]=word(0x0C283F18+index*44+std::uint32_t(i*4));return record;
}
OriginalVehicleParameters OriginalPhysicsData::parameters(const OriginalPhysicsSelection& s,const OriginalPhysicsPath& path) const{
    validateSelection(s);
    if(path.conditionCode!=s.conditionCode||path.inclusiveLastIndex!=word(0x0C283E88+s.conditionCode*8)||path.points.size()!=path.inclusiveLastIndex+1)
        throw std::invalid_argument("Selected original path and physics condition disagree");
    OriginalVehicleParameters p;const auto index=(s.conditionCode/2)*140+s.vehicleIndex*4;
    p.frame.table0C28451C=scalar(0x0C28451C+s.conditionCode*140+s.vehicleIndex*4);
    p.frame.table0C28866C=scalar(0x0C28866C+s.upgradeIndex0C9015F0*4);
    p.frame.global0C9015E4=s.progressEnabled0C9015E4;p.frame.global0C9015D4=s.progressMode0C9015D4;p.frame.global0C901650=s.progress0C901650;
    p.angular.carRecord0C283F18_08=scalar(0x0C283F18+s.vehicleIndex*44+8);p.angular.carRecord0C283F18_0C=scalar(0x0C283F18+s.vehicleIndex*44+12);
    p.angular.table0C285124=scalar(0x0C285124+index);p.angular.table0C285AFC=scalar(0x0C285AFC+index);p.angular.table0C285610=scalar(0x0C285610+index);
    p.angular.table0C287398=scalar(0x0C287398+index);p.angular.table0C285FE8=scalar(0x0C285FE8+index);
    p.steeringMemory.table0C2864D4=scalar(0x0C2864D4+index);p.steeringMemory.table0C286EAC=scalar(0x0C286EAC+index);
    p.steeringMemory.table0C287884=scalar(0x0C287884+index);p.steeringMemory.table0C2869C0=scalar(0x0C2869C0+index);p.steeringMemory.table0C287D70=scalar(0x0C287D70+index);
    p.loss.condition0C9015CC=s.conditionCode;p.loss.cap0C284F80=scalar(0x0C284F80+s.vehicleIndex*8);p.loss.growth0C284F84=scalar(0x0C284F84+s.vehicleIndex*8);
    const auto row=idas3::decodeOriginalPowertrainRow(bytes(0x0C28830C+s.vehicleIndex*24,24));
    const auto overrideRow=idas3::decodeOriginalPowertrainRow(bytes(0x0C288654,24));
    p.transmission=idas3::selectOriginalTransmissionParameters(row,overrideRow,s.overrideMode0C9015F4!=0);
    p.profile=idas3::decodeOriginalTransmissionProfile(bytes(0x0C28825C+p.transmission.profileIndex*88,88));
    p.road={s.conditionCode,s.vehicleMode0C9015E0,path.points,path.inclusiveLastIndex};
    //15EE00 supplies the live multiplier globals; do not imply this factory
    // performs the original initializer or upstream contact-state preparation.
    return p;
}
OriginalInitializationInputs OriginalPhysicsData::initialization(const OriginalPhysicsSelection& s,
        std::array<float,3> position,std::array<float,3> angles) const{
    validateSelection(s);
    return {position,angles,s.vehicleIndex,word(0x0C285098+s.vehicleIndex*4),word(0x0C284EF4+s.vehicleIndex*4),
        word(0x0C283E08+s.vehicleMode0C9015E0*4),s.mode0C9015FC,s.conditionCode>15?1u:0u};
}
} // namespace idas3::original
