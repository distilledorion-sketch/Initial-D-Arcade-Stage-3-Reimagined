#include "original_number_plate.h"
#include "car_catalog.h"
#include "original_car_color_catalog.h"
#include "original_rival_appearance_catalog.h"
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace idas3 {
namespace {
template<class T>T read(std::istream& in){T v{};if(!in.read(reinterpret_cast<char*>(&v),sizeof v))throw std::runtime_error("Truncated original plate placement");return v;}
void rotate(original::OriginalMatrix& m,unsigned axis,float degrees,const original::OriginalFscaTable& trig){
    //026D80 exact degree-to-integer phase conversion, including asymmetric+.5.
    float phase=degrees*65536.f;phase=phase/360.f;phase=phase+.5f;
    const auto sc=trig.sinCos(std::uint16_t(std::int32_t(phase)));const float s=sc[0],c=sc[1];
    std::array<float,4> a,b;unsigned first,second;
    if(axis==0){first=1;second=2;a={0,c,s,0};b={0,-s,c,0};}
    else if(axis==1){first=0;second=2;a={c,0,-s,0};b={s,0,c,0};}
    else{first=0;second=1;a={c,s,0,0};b={-s,c,0,0};}
    a=original::transformOriginalVector(m,a);b=original::transformOriginalVector(m,b);
    for(unsigned row=0;row<4;++row){m.elements[first*4+row]=a[row];m.elements[second*4+row]=b[row];}
}
NativeModelInstance instance(std::uint32_t chunk,const original::OriginalMatrix&m){
    NativeModelInstance result;result.chunk=chunk;
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)result.transform[row*4+col]=m.elements[col*4+row];
    return result;
}
}
OriginalNumberPlate OriginalNumberPlate::load(const std::filesystem::path& root,unsigned carId,unsigned factoryColor){
    if(carId>=35)throw std::out_of_range("Original plate car ID");
    if(factoryColor>=original::originalCarColorCounts[carId])throw std::out_of_range("Original plate factory paint");
    const auto directory=factoryColor?std::filesystem::path("colors")/("color_0"+std::to_string(factoryColor)):std::filesystem::path("fresh_player");
    return loadAppearance(root,carId,root/"data/original_models"/originalCarFolders[carId]/directory/"plate.bin",freshDigits());
}
OriginalNumberPlate OriginalNumberPlate::loadRival(const std::filesystem::path& root,unsigned carId,unsigned enemyId){
    if(enemyId>=originalRivalAppearances.size()||originalRivalAppearances[enemyId].car!=carId)throw std::out_of_range("Original rival plate car/preset mismatch");
    const auto name="enemy_"+std::string(enemyId<10?"0":"")+std::to_string(enemyId);
    return loadAppearance(root,carId,root/"data/original_models/rivals"/name/"plate.bin",originalRivalAppearances[enemyId].digits);
}
OriginalNumberPlate OriginalNumberPlate::loadAppearance(const std::filesystem::path& root,unsigned carId,const std::filesystem::path& placement,const std::array<std::uint8_t,5>& digits){
    OriginalNumberPlate out;const std::string name=carId==30?"numberplate_y":"numberplate";
    const auto base=root/"data/original_models/numberplate";
    out.model=NativeModel::load(base/(name+".idasmesh"));
    out.textures=NativeTextureBank::load(root/"data/original_assets/numberplate"/name/"textures/textures.idastex");
    if(out.model.chunks.size()!=11)throw std::runtime_error("Original plate model shape");
    std::ifstream file(placement,std::ios::binary);
    auto magic=read<std::array<char,8>>(file);
    if(std::memcmp(magic.data(),"ID3PLT1\0",8)||read<std::uint32_t>(file)!=1||read<std::uint32_t>(file)!=1)throw std::runtime_error("Original plate placement format");
    (void)read<std::uint32_t>(file);
    auto trig=original::OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
    constexpr std::array<float,5> digitX={-.11f,-.065f,.015f,.06f,.105f}; //2EF1C8
    for(unsigned plate=0;plate<2;++plate){
        const auto v=read<std::array<float,9>>(file);
        for(float x:v)if(!std::isfinite(x)||std::abs(x)>1000)throw std::runtime_error("Invalid original plate transform");
        auto m=original::originalIdentityMatrix();original::translateOriginalMatrix(m,{v[6],v[7],v[8]});
        rotate(m,2,v[5],trig);rotate(m,1,v[4],trig);rotate(m,0,v[3],trig);
        for(unsigned col=0;col<3;++col)for(unsigned row=0;row<4;++row)m.elements[col*4+row]*=v[col];
        out.assembly_.instances.push_back(instance(10,m));
        for(float x:digitX){auto d=m;original::translateOriginalMatrix(d,{x,0,0});out.assembly_.instances.push_back(instance(0,d));}
    }
    out.setDigits(digits);return out;
}
void OriginalNumberPlate::setDigits(const std::array<std::uint8_t,5>& digits){
    for(auto n:digits)if(n>9)throw std::out_of_range("Original plate digit");
    if(assembly_.instances.size()!=12)throw std::runtime_error("Original plate not loaded");
    for(unsigned plate=0;plate<2;++plate)for(unsigned n=0;n<5;++n)assembly_.instances[plate*6+n+1].chunk=digits[n];
}
std::array<std::uint8_t,5> OriginalNumberPlate::playerDigits(const original::OriginalBattleProfile& profile){
    constexpr std::array<std::uint32_t,5> factors{0xf6b0,31673,0x145e3,0x11ced,0x17f4d};
    constexpr std::array<std::uint32_t,5> addends{17706,0xd6b1,0x14573,1507,0xffa7};
    const auto length=std::bit_cast<std::int32_t>(profile.u(76));
    std::uint32_t sum=0;
    for(unsigned i=0;i<5;++i)sum+=(std::int32_t(i)<length?profile.u(44+i*4):0u)*factors[i]+addends[i];
    sum%=100000;std::array<std::uint8_t,5> digits{};
    for(unsigned i=0,divisor=10000;i<5;++i,divisor/=10)digits[i]=std::uint8_t((sum/divisor)%10);
    return digits;
}
}
