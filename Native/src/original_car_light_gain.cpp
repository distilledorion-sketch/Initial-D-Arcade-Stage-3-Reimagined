#include "original_car_light_gain.h"
#include <array>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace idas3::original {
OriginalCarLightGain OriginalCarLightGain::load(const std::filesystem::path& root,
    unsigned course,bool night,bool wet,bool reverse){
    if(course>=9)throw std::invalid_argument("Original car light course must be0..8");
    OriginalCarLightGain out;out.reverse_=reverse;
    //042700 uses the following night constructor for daytime rain. Happo has
    // no stream in either constructor; Snow043600 delegates to19DDC0.
    const bool nightConstructor=night||wet;
    constexpr std::array<const char*,9> names{"k_ez","s_nm","h_hd","k_df",nullptr,"s_uh","n_sy","k_tu","k_df"};
    constexpr std::array<unsigned,9> counts{1069,1401,3301,4089,0,2951,3301,3729,4089};
    if(!names[course]||(nightConstructor&&(course==0||course==1||course==6||course==7)))return out;
    const auto file=root/"data/original_course_lighting/gain"/(std::string(names[course])+"_sdw.bin");
    std::ifstream f(file,std::ios::binary|std::ios::ate);
    if(!f||f.tellg()!=std::streamoff(counts[course]*4))throw std::runtime_error("Invalid original car light stream: "+file.string());
    out.values_.resize(counts[course]);f.seekg(0);
    f.read(reinterpret_cast<char*>(out.values_.data()),std::streamsize(out.values_.size()*4));
    if(!f)throw std::runtime_error("Truncated original car light stream");
    for(float v:out.values_)if(!std::isfinite(v)||v<0.f||v>1.f)throw std::runtime_error("Invalid original car light coefficient");
    return out;
}
float OriginalCarLightGain::evaluate(std::int32_t index,float fraction)const{
    if(values_.empty())return 1.f;
    const auto period=std::int32_t(values_.size()-1);
    if(index<0||index>=period||!std::isfinite(fraction))throw std::invalid_argument("Original car light coordinate outside source stream");
    // Reverse course path096200 already returns a reversed coordinate. Source
    //03D100 converts it back before accessing the unswapped stream.
    if(reverse_){index=period-index-1;fraction=1.f-fraction;}
    const float complement=1.f-fraction;
    const float next=values_[std::size_t(index)+1]*fraction;
    const float value=std::fma(values_[std::size_t(index)],complement,next);
    if(0.f>value)return 0.f;
    if(!(1.f>value))return 1.f;
    return value;
}
}
