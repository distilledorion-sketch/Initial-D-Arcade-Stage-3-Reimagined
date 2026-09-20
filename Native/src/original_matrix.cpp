#include "original_matrix.h"
#include <bit>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace idas3::original {
namespace {
std::uint32_t read32(const std::vector<std::uint8_t>& data,std::size_t offset){return std::uint32_t(data.at(offset))|(std::uint32_t(data.at(offset+1))<<8)|(std::uint32_t(data.at(offset+2))<<16)|(std::uint32_t(data.at(offset+3))<<24);}
std::uint16_t rotationPhase(float radians){
    const float scaled=std::bit_cast<float>(0x4622F983u)*radians;
    std::uint32_t value;
    if(std::isnan(scaled)||scaled<=-2147483648.0f)value=0x80000000u;
    else if(scaled>=2147483648.0f)value=0x7FFFFFFFu;
    else value=std::uint32_t(std::int32_t(scaled));
    return std::uint16_t(value);
}
void setColumn(OriginalMatrix& m,std::size_t column,const std::array<float,4>& v){for(std::size_t i=0;i<4;++i)m.elements[column*4+i]=v[i];}
}
OriginalFscaTable OriginalFscaTable::load(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Original matrix FSCA data unavailable");
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
    constexpr char magic[]="ID3FSCA1";
    if(bytes.size()!=16+32768*4)throw std::runtime_error("Original FSCA table size mismatch");
    for(std::size_t i=0;i<8;++i)if(bytes[i]!=std::uint8_t(magic[i]))throw std::runtime_error("Original FSCA table format mismatch");
    if(read32(bytes,8)!=32768||read32(bytes,12)!=0x18553798u)throw std::runtime_error("Original FSCA table identity mismatch");
    std::uint32_t checksum=2166136261u;for(std::size_t i=16;i<bytes.size();++i)checksum=(checksum^bytes[i])*16777619u;
    if(checksum!=0x18553798u)throw std::runtime_error("Original FSCA numerical data checksum mismatch");
    OriginalFscaTable out;out.halfWave_.resize(32768);
    for(std::size_t i=0;i<32768;++i)out.halfWave_[i]=read32(bytes,16+i*4);
    return out;
}
std::array<float,2> OriginalFscaTable::sinCos(std::uint16_t phase) const{
    if(halfWave_.size()!=32768)throw std::runtime_error("Original FSCA lookup was not loaded");
    const auto sine=[&](std::uint16_t index){return std::bit_cast<float>(halfWave_[index&0x7FFFu]^((index&0x8000u)?0x80000000u:0u));};
    return {sine(phase),sine(std::uint16_t(phase+0x4000u))};
}
OriginalMatrix originalIdentityMatrix(){OriginalMatrix out;for(const auto index:{0,5,10,15})out.elements[index]=1.0f;return out;}
std::array<float,4> transformOriginalVector(const OriginalMatrix& m,const std::array<float,4>& value){
    std::array<float,4> out{};
    for(std::size_t row=0;row<4;++row){
        double sum=double(m.elements[row])*double(value[0]);
        sum+=double(m.elements[row+4])*double(value[1]);
        sum+=double(m.elements[row+8])*double(value[2]);
        sum+=double(m.elements[row+12])*double(value[3]);
        out[row]=float(sum);
    }
    return out;
}
void translateOriginalMatrix(OriginalMatrix& m,const std::array<float,3>& v){setColumn(m,3,transformOriginalVector(m,{v[0],v[1],v[2],1.0f}));}
void scaleOriginalMatrix(OriginalMatrix& m,const std::array<float,3>& v){
    for(unsigned column=0;column<3;++column)for(unsigned row=0;row<4;++row)m.elements[column*4+row]*=v[column];
}
void rotateOriginalMatrixPhase(OriginalMatrix& m,unsigned axis,std::uint16_t phase,const OriginalFscaTable& table){
    const auto sc=table.sinCos(phase);const float s=sc[0],c=sc[1];
    std::array<float,4> a,b;unsigned first,second;
    if(axis==0){first=1;second=2;a={0,c,s,0};b={0,-s,c,0};}
    else if(axis==1){first=0;second=2;a={c,0,-s,0};b={s,0,c,0};}
    else if(axis==2){first=0;second=1;a={c,s,0,0};b={-s,c,0,0};}
    else throw std::out_of_range("Original matrix rotation axis");
    a=transformOriginalVector(m,a);b=transformOriginalVector(m,b);
    setColumn(m,first,a);setColumn(m,second,b);
}
void rotateOriginalMatrixX(OriginalMatrix& m,float angle,const OriginalFscaTable& table){
    const auto sc=table.sinCos(rotationPhase(angle));
    const auto y=transformOriginalVector(m,{0.0f,sc[1],sc[0],0.0f});
    const auto z=transformOriginalVector(m,{0.0f,-sc[0],sc[1],0.0f});
    setColumn(m,1,y);setColumn(m,2,z);
}
void rotateOriginalMatrixY(OriginalMatrix& m,float angle,const OriginalFscaTable& table){
    const auto sc=table.sinCos(rotationPhase(angle));
    const auto x=transformOriginalVector(m,{sc[1],0.0f,-sc[0],0.0f});
    const auto z=transformOriginalVector(m,{sc[0],0.0f,sc[1],0.0f});
    setColumn(m,0,x);setColumn(m,2,z);
}
void rotateOriginalMatrixZ(OriginalMatrix& m,float angle,const OriginalFscaTable& table){
    const auto sc=table.sinCos(rotationPhase(angle));
    const auto x=transformOriginalVector(m,{sc[1],sc[0],0.0f,0.0f});
    const auto y=transformOriginalVector(m,{-sc[0],sc[1],0.0f,0.0f});
    setColumn(m,0,x);setColumn(m,1,y);
}
std::array<float,3> transformOriginalPoint(const OriginalMatrix& m,const std::array<float,3>& p){const auto v=transformOriginalVector(m,{p[0],p[1],p[2],1.0f});return {v[0],v[1],v[2]};}
OriginalMatrix originalActorMatrix(const std::array<float,3>& position,const std::array<float,3>& angles,const OriginalFscaTable& table){
    auto out=originalIdentityMatrix();translateOriginalMatrix(out,position);
    rotateOriginalMatrixY(out,angles[1],table);rotateOriginalMatrixX(out,angles[0],table);rotateOriginalMatrixZ(out,angles[2],table);return out;
}
} // namespace idas3::original
