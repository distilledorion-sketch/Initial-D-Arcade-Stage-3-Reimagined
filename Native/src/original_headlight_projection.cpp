#include "original_headlight_projection.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace idas3::original {
namespace {
float f(unsigned word){return std::bit_cast<float>(word);}
unsigned word(float x){return std::bit_cast<unsigned>(x);}
std::array<float,3> position(const OriginalHeadlightProjection::Matrix&m,float x,float y,float z){
    std::array<float,3> result{};
    for(unsigned i=0;i<3;++i){double value=double(m[i])*x;value+=double(m[4+i])*y;value+=double(m[8+i])*z;value+=m[12+i];result[i]=float(value);}
    return result;
}
template<class T>void readExact(const std::filesystem::path&p,T&out){
    std::ifstream in(p,std::ios::binary);in.read(reinterpret_cast<char*>(&out),sizeof(out));
    if(!in||in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Original headlight asset size mismatch");
}
}
void OriginalHeadlightProjection::load(const std::filesystem::path&root){
    const auto dir=root/"data/original_assets/headlight_projection";
    auto bank=NativeModel::load(dir/"lightobj.idasmesh");
    if(bank.chunks.size()!=8)throw std::runtime_error("Original lightobj bank shape");
    originalModel_.chunks={bank.chunks[7]};const auto&c=originalModel_.chunks[0];
    if(c.index!=7||c.batches.size()!=1||c.batches[0].vertices.size()!=12||c.batches[0].ich[6]!=0x4a)
        throw std::runtime_error("Original projected headlight model shape");
    readExact(dir/"k_light_test8.bin",authored_);readExact(dir/"k_light_test8.tbl",vertexOffsets_);
    for(unsigned i=0;i<9;++i){if(authored_[i][0]!=((i*4+1)/3))throw std::runtime_error("Original headlight point mapping");
        for(unsigned j=1;j<6;++j)if(!std::isfinite(f(authored_[i][j])))throw std::runtime_error("Invalid original headlight coordinate");}
    for(auto offset:vertexOffsets_)if(offset<192||offset>=576||offset%32)throw std::runtime_error("Original headlight vertex offset");
    loaded_=true;reset();
}
void OriginalHeadlightProjection::reset(){
    if(!loaded_)throw std::logic_error("Original headlight projection is not loaded");
    model_=originalModel_;records_=authored_;queries_={};
    for(auto&q:queries_){q.setf(4,1.f);q.setu(56,~0u);q.setu(60,~0u);}
}
void OriginalHeadlightProjection::bindRoad(const OriginalCollisionQuery&road){for(auto&q:queries_)q=road;}
void OriginalHeadlightProjection::publish(){
    if(!loaded_)throw std::logic_error("Original headlight projection is not loaded");
    auto&vertices=model_.chunks[0].batches[0].vertices;unsigned group=0;
    for(unsigned i=0;i<12;++i){while(group+1<9&&i>=records_[group+1][0])++group;
        const auto&r=records_[group];auto&v=vertices[(vertexOffsets_[i]-192)/32];
        v.position={f(r[1]),f(r[2]),f(r[3])};v.u=f(r[4]);v.v=f(r[5]);v.color0=r[6];v.color1=r[7];}
}
unsigned OriginalHeadlightProjection::advance(const Matrix&car,const Query&query){
    if(!loaded_||!query)throw std::logic_error("Original headlight projection lacks assets/query");
    for(float v:car)if(!std::isfinite(v))throw std::invalid_argument("Nonfinite headlight world matrix");
    Matrix local=car;const auto anchor=position(car,0.f,0.f,1.5f);
    local[12]=anchor[0];local[13]=anchor[1]+6.f;local[14]=anchor[2];
    unsigned calls=0;
    for(unsigned i=0;i<9;++i){const auto&a=authored_[i];auto&r=records_[i];auto&q=queries_[i];
        const auto p=position(local,f(a[1])*7.f,f(a[2]),f(a[3])*20.f);
        for(unsigned j=0;j<3;++j)q.setf(32+4*j,p[j]);++calls;
        const bool hit=query(q);for(unsigned j=0;j<3;++j)r[j+1]=word(p[j]);
        if(!hit){r[2]=word(car[13]);r[6]=r[7]=0;i=(i/3+1)*3-1;continue;}
        const float correction=-q.f(24)+.1f;r[2]=word(p[1]+correction);
        if(q.u(28)&0x8000)r[6]=r[7]=0x007f7f7f;
        else{r[6]=a[6];r[7]=a[7];}
    }
    return calls;
}
} // namespace idas3::original
