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
OriginalMatrix originalLookAtCameraWorld(const std::array<float,3>& eye,const std::array<float,3>& target){
    std::array<float,3> direction;
    for(unsigned i=0;i<3;++i)direction[i]=eye[i]-target[i];
    float squared=direction[2]*direction[2];
    squared=std::fma(direction[0],direction[0],squared);
    squared=std::fma(direction[1],direction[1],squared);
    const float inverse=1.f/std::sqrt(squared);
    std::array<float,3> z;for(unsigned i=0;i<3;++i)z[i]=direction[i]*inverse;
    std::array<float,3> x{direction[2],0,-direction[0]};
    double xSquared=double(x[0])*double(x[0]);
    xSquared+=double(x[1])*double(x[1]);xSquared+=double(x[2])*double(x[2]);xSquared+=0.0;
    const float xInverse=1.f/std::sqrt(float(xSquared));
    for(auto& value:x)value*=xInverse;
    const std::array<float,3> y{
        std::fma(z[1],x[2],-(z[2]*x[1])),
        std::fma(z[2],x[0],-(z[0]*x[2])),
        std::fma(z[0],x[1],-(z[1]*x[0]))};
    OriginalMatrix out;out.elements={x[0],x[1],x[2],0,y[0],y[1],y[2],0,
        z[0],z[1],z[2],0,eye[0],eye[1],eye[2],1};
    //09D2C0 starts with identity and1565A0 submits its local matrix through
    //1F64A0/FTRV, which also canonicalizes zero signs through summation.
    const auto identity=originalIdentityMatrix();
    for(unsigned column=0;column<4;++column){
        const unsigned i=column*4;
        setColumn(out,column,transformOriginalVector(identity,
            {out.elements[i],out.elements[i+1],out.elements[i+2],out.elements[i+3]}));
    }
    return out;
}
OriginalMatrix originalCameraWorldToView(const OriginalMatrix& cameraWorld){
    const auto& source=cameraWorld.elements;
    OriginalMatrix out;
    for(unsigned row=0;row<3;++row){
        for(unsigned column=0;column<3;++column)out.elements[column*4+row]=source[row*4+column];
        double translation=double(source[12])*double(source[row*4]);
        translation+=double(source[13])*double(source[row*4+1]);
        translation+=double(source[14])*double(source[row*4+2]);translation+=0.0;
        out.elements[12+row]=0.f-float(translation);
    }
    out.elements[15]=1;
    return out;
}
float invertOriginalMatrix(OriginalMatrix& matrix){
    auto& a=matrix.elements;
    std::array<unsigned,4> permutation{};
    std::array<float,4> scale{};
    for(unsigned row=0;row<4;++row){
        permutation[row]=row;
        float maximum=0;
        for(unsigned column=0;column<4;++column){
            const float value=std::abs(a[row*4+column]);
            if(value>maximum)maximum=value;
        }
        if(maximum==0)return 0;
        scale[row]=1.f/maximum;
    }
    float determinant=1;
    for(unsigned column=0;column<4;++column){
        float largest=-1;
        unsigned pivot=column;
        for(unsigned row=column;row<4;++row){
            const float candidate=std::abs(a[permutation[row]*4+column])*scale[permutation[row]];
            if(candidate>largest){largest=candidate;pivot=row;}
        }
        const unsigned selected=permutation[pivot];
        if(pivot!=column){permutation[pivot]=permutation[column];permutation[column]=selected;determinant=-determinant;}
        const float diagonal=a[selected*4+column];
        determinant*=diagonal;
        if(diagonal==0)return determinant;
        for(unsigned row=column+1;row<4;++row){
            const unsigned actual=permutation[row];
            const float multiplier=a[actual*4+column]/diagonal;
            a[actual*4+column]=multiplier;
            for(unsigned next=column+1;next<4;++next){
                const float product=multiplier*a[selected*4+next];
                a[actual*4+next]-=product;
            }
        }
    }
    if(determinant==0)return determinant;
    OriginalMatrix inverse;
    for(unsigned column=0;column<4;++column){
        for(unsigned row=0;row<4;++row){
            float value=permutation[row]==column?1.f:0.f;
            for(unsigned previous=0;previous<row;++previous){
                const float product=a[permutation[row]*4+previous]*inverse.elements[previous*4+column];
                value-=product;
            }
            inverse.elements[row*4+column]=value;
        }
        for(int row=3;row>=0;--row){
            float value=inverse.elements[row*4+column];
            for(unsigned next=unsigned(row)+1;next<4;++next){
                const float product=a[permutation[row]*4+next]*inverse.elements[next*4+column];
                value-=product;
            }
            inverse.elements[row*4+column]=value/a[permutation[row]*4+row];
        }
    }
    matrix=inverse;
    return determinant;
}
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
