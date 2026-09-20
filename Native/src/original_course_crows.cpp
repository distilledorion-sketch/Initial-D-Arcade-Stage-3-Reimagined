#include "original_course_crows.h"
#include <bit>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace idas3 {
namespace {
using original::OriginalMatrix;
void rotate(OriginalMatrix& m,unsigned axis,float s,float c){
    const auto a=original::transformOriginalVector(m,axis==0?std::array<float,4>{0,c,s,0}:std::array<float,4>{c,0,-s,0});
    const auto b=original::transformOriginalVector(m,axis==0?std::array<float,4>{0,-s,c,0}:std::array<float,4>{s,0,c,0});
    const unsigned first=axis==0?1:0;
    for(unsigned i=0;i<4;++i){m.elements[first*4+i]=a[i];m.elements[8+i]=b[i];}
}
std::vector<std::array<float,3>> readPath(const std::filesystem::path& path,unsigned count){
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f||f.tellg()!=std::streamoff(8+count*12))throw std::runtime_error("Original crow path extent mismatch");
    f.seekg(0);std::array<unsigned,2> header{};f.read(reinterpret_cast<char*>(header.data()),8);
    if(header!=std::array<unsigned,2>{count,3})throw std::runtime_error("Original crow path header mismatch");
    std::vector<std::array<float,3>> out(count);f.read(reinterpret_cast<char*>(out.data()),count*12);
    if(!f)throw std::runtime_error("Truncated original crow path");
    for(const auto& p:out)for(float v:p)if(!std::isfinite(v))throw std::runtime_error("Nonfinite original crow path");
    return out;
}
}
void resetOriginalCourseCrows(OriginalCourseCrowState& state,std::uint32_t& seed){
    state={};for(auto& chunk:state.animationFrames){seed=seed*0x41c64e6du+12345u;chunk=((seed>>16)&0x7fffu)%30;}
    state.initialized=true;
}
std::uint32_t originalCourseCrowPrecedingSeed(std::uint32_t seed){
    //0xEEB9EB65 is the multiplicative inverse of the source's odd LCG
    //multiplier modulo2^32. Unsigned wrap is part of the source generator.
    static_assert(std::uint32_t(0x41c64e6du*0xeeb9eb65u)==1u);
    for(unsigned i=0;i<19;++i)seed=(seed-12345u)*0xeeb9eb65u;
    return seed;
}
void resetOriginalCourseCrowsBeforeDrivingSeed(OriginalCourseCrowState& state,std::uint32_t drivingEntrySeed){
    auto seed=originalCourseCrowPrecedingSeed(drivingEntrySeed);
    resetOriginalCourseCrows(state,seed);
}
void advanceOriginalCourseCrows(OriginalCourseCrowState& state){
    if(!state.initialized)return;
    for(auto& chunk:state.animationFrames){++chunk;if(chunk>29)chunk=0;}
    if(++state.flightFrame>=900)state.flightFrame=0;
}
OriginalMatrix originalCourseCrowFlightMatrix(const std::array<float,3>& p,const std::array<float,3>& next,const original::OriginalFscaTable& trig){
    const float dx=next[0]-p[0],dy=next[1]-p[1],dz=next[2]-p[2];
    //1FC2A0: form the source camera matrix, invert its rigid transform with
    //1F66A0, then rotateY(32768) exactly as042520. No guessed tangent/up frame.
    const float horizontal2=std::fma(dx,dx,dz*dz);
    const float distance2=std::fma(dy,dy,horizontal2);
    if(!(horizontal2>0)||!(distance2>0))throw std::runtime_error("Degenerate original crow flight segment");
    const float ih=1.f/std::sqrt(horizontal2),il=1.f/std::sqrt(distance2);
    auto view=original::originalIdentityMatrix();
    rotate(view,0,-dy*il,il/ih);rotate(view,1,ih*dx,-dz*ih);
    original::translateOriginalMatrix(view,{-p[0],-p[1],-p[2]});
    auto world=original::originalIdentityMatrix();
    for(unsigned row=0;row<3;++row){
        for(unsigned col=0;col<3;++col)world.elements[col*4+row]=view.elements[row*4+col];
        double dot=double(view.elements[row*4])*view.elements[12];
        dot+=double(view.elements[row*4+1])*view.elements[13];
        dot+=double(view.elements[row*4+2])*view.elements[14];
        dot+=0.;world.elements[12+row]=0.f-float(dot);
    }
    original::rotateOriginalMatrixPhase(world,1,32768,trig);return world;
}
OriginalCourseCrows OriginalCourseCrows::load(const std::filesystem::path& root){
    OriginalCourseCrows out;const auto folder=root/"data/original_assets/crows";
    const auto path=readPath(folder/"flight.bin",900),group=readPath(folder/"group.bin",19);
    for(unsigned i=0;i<19;++i)out.group_[i]=group[i];
    const auto trig=original::OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
    out.flight_.reserve(900);
    for(unsigned i=0;i<900;++i)out.flight_.push_back(originalCourseCrowFlightMatrix(path[i],path[(i+1)%900],trig));
    out.model=NativeModel::load(root/"data/original_models/crows/crow.idasmesh");
    out.textures=NativeTextureBank::load(folder/"textures/textures.idastex");
    if(out.model.chunks.size()!=30||out.textures.size()!=1)throw std::runtime_error("Original crow geometry/texture extent mismatch");
    return out;
}
void OriginalCourseCrows::reset(std::uint32_t& sourceSeed){resetOriginalCourseCrows(state_,sourceSeed);updateAssembly();}
void OriginalCourseCrows::resetBeforeDrivingSeed(std::uint32_t drivingEntrySeed){resetOriginalCourseCrowsBeforeDrivingSeed(state_,drivingEntrySeed);updateAssembly();}
void OriginalCourseCrows::advance(){advanceOriginalCourseCrows(state_);updateAssembly();}
void OriginalCourseCrows::updateAssembly(){
    assembly_.instances.clear();if(!state_.initialized)return;
    if(flight_.size()!=900)throw std::runtime_error("Original crows not loaded");
    assembly_.instances.reserve(19);
    for(unsigned i=0;i<19;++i){
        auto matrix=flight_.at(state_.flightFrame);original::translateOriginalMatrix(matrix,group_[i]);
        NativeModelInstance instance;instance.chunk=state_.animationFrames[i];
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)instance.transform[row*4+col]=matrix.elements[col*4+row];
        assembly_.instances.push_back(instance);
    }
}
}
