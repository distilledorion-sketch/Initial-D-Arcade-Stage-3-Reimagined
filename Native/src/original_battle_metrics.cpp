#include "original_battle_metrics.h"
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace idas3::original {
OriginalBattleMetrics::OriginalBattleMetrics(std::span<const OriginalRacePoint> points,bool reverse):reverse_(reverse){
    if(points.size()<2||points.size()>1000000)throw std::invalid_argument("Invalid original centreline count");
    for(auto p:points)for(float v:p)if(!std::isfinite(v))throw std::invalid_argument("Non-finite original centreline");
    cumulative_.resize(points.size());
    for(std::size_t i=1;i<points.size();++i){
        //099DA0 subtracts inF32, calls1F6C90(FIPR/FSQRT), then adds inF32.
        const float x=points[i][0]-points[i-1][0],y=points[i][1]-points[i-1][1],z=points[i][2]-points[i-1][2];
        double squared=double(x)*double(x);squared+=double(y)*double(y);squared+=double(z)*double(z);squared+=0.0;
        const float length=std::sqrt(float(squared));
        cumulative_[i]=cumulative_[i-1]+length;
    }
    if(!std::isfinite(cumulative_.back())||cumulative_.back()<=0)throw std::invalid_argument("Invalid original centreline length");
}
OriginalBattleMetrics OriginalBattleMetrics::load(const std::filesystem::path& root,std::uint32_t condition){
    if(condition>=18)throw std::invalid_argument("Original battle metrics condition must be0..17");
    constexpr std::array<const char*,9> folders{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df"};
    const auto path=root/"data/courses"/(std::string(folders[condition/2])+"_path.bin");
    std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Original centreline missing: "+path.string());const auto bytes=f.tellg();f.seekg(0);
    std::uint32_t count{},components{};f.read(reinterpret_cast<char*>(&count),4);f.read(reinterpret_cast<char*>(&components),4);
    if(!f||count<2||count>1000000||components!=3||bytes!=std::streamoff(8+count*12ull))throw std::runtime_error("Invalid original centreline header");
    std::vector<OriginalRacePoint> points(count);f.read(reinterpret_cast<char*>(points.data()),std::streamsize(count)*12);if(!f)throw std::runtime_error("Truncated original centreline");
    return {points,(condition&1)!=0};
}
float OriginalBattleMetrics::totalLength()const{if(cumulative_.empty())throw std::logic_error("Original battle metrics not loaded");return cumulative_.back();}
float OriginalBattleMetrics::distance(OriginalPathCoordinate coordinate)const{
    const float total=totalLength();const auto period=std::int32_t(cumulative_.size()-1);
    if(!std::isfinite(coordinate.fraction))throw std::invalid_argument("Non-finite original path fraction");
    // Exact equivalent of099360 repeated signed-period subtraction/addition.
    auto index=coordinate.index%period;auto laps=coordinate.index/period;if(index<0){index+=period;--laps;}
    float fraction=coordinate.fraction;if(reverse_){index=period-index-1;fraction=1.0f-fraction;}
    float value=cumulative_[std::size_t(index)+1]*fraction;
    value=std::fma(cumulative_[std::size_t(index)],1.0f-fraction,value);
    if(reverse_)value=total-value;
    return std::fma(total,float(laps),value);
}
float OriginalBattleMetrics::advantage(OriginalPathCoordinate player,OriginalPathCoordinate rival)const{return distance(player)-distance(rival);}
float OriginalBattleMetrics::positionFraction(OriginalPathCoordinate current)const{return distance(current)/totalLength();}
std::string_view originalBattlePortraitBank(std::uint32_t enemy,std::uint32_t mode){
    if(mode==2)return "face_bunt";if(mode!=0)return {};
    constexpr std::array<std::string_view,31> names{"face_ituk","face_kenj","face_sing","face_toru","face_atuo","face_mako","face_toky","face_kent","face_kei1","face_iktn","face_naka","face_sudo","face_ryo1","face_tak1","face_seij","face_sudo","face_kai","face_miki","face_daik","face_skai","face_tomo","face_nobu","face_skmt","face_wata","face_kyko","face_ryo2","face_evo5","face_evo6","face_kei2","face_tak2","face_bunt"};
    if(enemy>=names.size())throw std::invalid_argument("Original portrait enemy must be0..30");return names[enemy];
}
}
