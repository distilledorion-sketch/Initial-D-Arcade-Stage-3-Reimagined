#include "original_tuning.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace idas3::original {
void applyOriginalProfilePhysicsSelection(OriginalPhysicsSelection& s,const OriginalBattleProfile& p){
    if(p.u(16)>=35||p.byte(164)>=76)throw std::invalid_argument("Unsupported original profile car/upgrade index");
    s.vehicleIndex=p.u(16);
    s.vehicleMode0C9015E0=std::min(p.u(24),31u);
    s.upgradeIndex0C9015F0=p.byte(164);
    s.overrideMode0C9015F4=s.vehicleIndex==0&&p.byte(164)>4&&p.byte(152)<=2;
    s.mode0C9015FC=p.u(32);
    s.mode0C9015C0=std::int32_t(s.conditionCode)>15;
}
namespace {
std::int32_t sign(std::uint32_t v){return std::bit_cast<std::int32_t>(v);}
const OriginalBasicTuningStep& basic(const OriginalBattleProfile& p,const OriginalCarTuningData& d){return d.packages.at(p.byte(152)).steps.at(p.byte(153));}
const OriginalPerformanceTuningStep& performance(const OriginalBattleProfile& p,const OriginalCarTuningData& d){return d.performance.at(p.byte(153));}
OriginalOptionalTuningStep optional(const OriginalBattleProfile& p,const OriginalCarTuningData& d){
    if(!d.optional.empty())return d.optional.at(p.byte(154));
    // Car29 has zero optional entries, but the source still dereferences its
    // table pointer in later eligibility branches. It aliases performance[0].
    if(p.byte(154)==0&&d.sourceRow[2]==d.sourceRow[6]&&d.performance.size()>=2){OriginalOptionalTuningStep out;
        std::copy(d.performance[0].words.begin(),d.performance[0].words.end(),out.words.begin());
        std::copy(d.performance[1].words.begin(),d.performance[1].words.end(),out.words.begin()+3);return out;}
    throw std::invalid_argument("Invalid original optional tuning index");
}
std::uint32_t random(std::uint32_t& seed){seed=seed*0x41c64e6du+12345;return (seed>>16)&0x7fff;}
void advanceOptional(OriginalBattleProfile& p,const OriginalCarTuningData& d){const unsigned next=p.byte(154)+1;p.setByte(154,std::uint8_t(next>=d.optional.size()?0:next));}
}
const OriginalCarTuningData& OriginalTuningData::car(std::uint32_t carId)const{return cars.at(carId);}
OriginalTuningData OriginalTuningData::load(const std::filesystem::path& root){
    std::ifstream f(root/"data/original_tuning/tables.idastune",std::ios::binary);
    if(!f)throw std::runtime_error("Missing original tuning data");
    const std::vector<std::uint8_t> b{std::istreambuf_iterator<char>(f),{}};std::size_t cursor=0;
    const auto u32=[&](){if(cursor+4>b.size())throw std::runtime_error("Truncated tuning data");unsigned v=0;for(unsigned i=0;i<4;++i)v|=unsigned(b[cursor++])<<(8*i);return v;};
    if(b.size()<16||std::string(b.begin(),b.begin()+8)!=std::string("IDASTN1\0",8))throw std::runtime_error("Invalid tuning header");cursor=8;
    if(u32()!=1||u32()!=35)throw std::runtime_error("Unsupported tuning data");
    OriginalTuningData out;
    for(auto& d:out.cars){for(auto& n:d.sourceRow)n=u32();const auto& r=d.sourceRow;
        if(r[1]>8||r[3]>128||r[5]>32||r[7]!=58)throw std::runtime_error("Invalid source tuning counts");
        d.packages.resize(r[1]);for(auto& p:d.packages){p.sourceAddress=u32();const auto count=u32();if(count>128)throw std::runtime_error("Invalid tuning step count");p.steps.resize(count);for(auto& step:p.steps)for(auto& n:step.words)n=u32();}
        d.optional.resize(r[3]);for(auto& step:d.optional)for(auto& n:step.words)n=u32();
        d.colors.resize(r[5]);for(auto& n:d.colors)n=u32();
        d.performance.resize(r[7]);for(auto& step:d.performance)for(auto& n:step.words)n=u32();
    }
    const auto count=u32();if(count>1024)throw std::runtime_error("Invalid tuning text count");
    for(unsigned i=0;i<count;++i){const auto address=u32(),length=u32();if(length>65536||length>b.size()-cursor)throw std::runtime_error("Invalid tuning text length");out.descriptions[address]=std::string(b.begin()+cursor,b.begin()+cursor+length);cursor+=length;}
    if(cursor!=b.size())throw std::runtime_error("Trailing tuning data");return out;
}
OriginalTuningResultSetup prepareOriginalResultTuning(OriginalBattleProfile& p,const OriginalTuningData& tables,std::uint32_t& seed){
    const auto& d=tables.car(p.u(16));OriginalTuningResultSetup out;const auto cooldown=p.byte(155);const auto points=p.u(72);
    bool optionalAvailable=false;
    if(!d.optional.empty()){
        unsigned index=random(seed)%unsigned(d.optional.size()),attempts=0;
        while(p.byte(156+d.optional[index].words[1])==d.optional[index].words[0]&&attempts<d.optional.size()){
            index=random(seed)%unsigned(d.optional.size());++attempts;
        }
        if(attempts>=d.optional.size())p.setByte(154,0);else{p.setByte(154,std::uint8_t(index));optionalAvailable=true;}
    }
    if(!(p.u(1180)&3))return out;
    const auto optionalBranch=[&](){
        const auto step=optional(p,d);
        if(points>=step.words[3]&&cooldown>1&&optionalAvailable){out.kind=OriginalTuningChildKind::optionalPart;out.sourceOwnerKind=2;out.notice=true;p.setByte(155,0);
            p.setu(1176,879); //1177EC→192760 returns879, then1346A0 stores it.
        }
        else if(cooldown<=1)p.setByte(155,std::uint8_t(cooldown+1));
    };
    if(p.u(1180)&0x400){
        if(p.u(1180)&0x800){optionalBranch();return out;}
        const auto level=p.byte(164);
        if(level>performance(p,d).words[0]){
            unsigned index=0;while(index<d.performance.size()&&level>=d.performance[index].words[0])++index;
            if(index==d.performance.size()){--index;p.setu(1180,p.u(1180)|0x800);}
            p.setByte(153,std::uint8_t(index));
        }
        out.threshold=performance(p,d).words[1];
        if(points>=out.threshold&&!(p.u(1180)&0x800)){out.kind=OriginalTuningChildKind::performance;out.sourceOwnerKind=1;return out;}
        optionalBranch();return out;
    }
    out.threshold=basic(p,d).words[2];
    if(points>=out.threshold){out.kind=OriginalTuningChildKind::basic;out.sourceOwnerKind=0;}
    return out;
}
OriginalTuningMutation applyOriginalTuningCommand(OriginalBattleProfile& p,const OriginalTuningData& tables,std::uint32_t command){
    const auto before=p.words;const auto& d=tables.car(p.u(16));OriginalTuningMutation out;
    const auto carCall=[&](unsigned address,unsigned a5=0,unsigned a6=0){out.carCalls.push_back({address,a5,a6});};
    if(command==1){
        const auto row=basic(p,d).words;const unsigned slot=row[0],value=row[1],car=p.u(16),package=p.byte(152);bool handled=false;
        if(slot==8){
            bool special=false;
            if(((car==19||car==1)&&package==0&&value==5))special=true;
            const bool fourth=car==4||car==18||(car==22&&package==1)||(car==25&&package!=0)||car==26||car==30;
            if(fourth&&value==4&&package!=d.packages.size()-1)special=true;
            if(special){p.setByte(166,1);carCall(0x0c028720,1);}
            if(p.byte(164)<value){p.setByte(164,std::uint8_t(value));handled=true;}
        }
        if(!handled){
            if(slot==9)p.setByte(165,std::uint8_t(p.byte(165)*2+1));
            else if(((car==3&&package==3)||(car==4&&package==2))&&slot==7)p.setByte(156+slot,1);
            else if(car==1&&package==0&&slot==1)p.setByte(157,2);
            else if((car==1&&package==2&&slot==6)||(car==16&&package==1&&slot==7)||(car==15&&package==1&&slot==2))p.setByte(156+slot,std::uint8_t(package));
            else p.setByte(156+slot,std::uint8_t(package+1));
        }
        constexpr std::array<unsigned,10> method={0x0c0283c0,0x0c028480,0x0c028400,0x0c0284c0,0x0c028500,0x0c028540,0x0c028580,0x0c0285c0,0,0x0c0286c0};
        if(slot==7)p.setByte(154,std::uint8_t(package+2));
        if(slot<method.size()&&method[slot])carCall(method[slot],p.byte(156+slot));carCall(0x0c029040);
        const auto& steps=d.packages.at(package).steps;unsigned next=p.byte(153)+1;
        while(next<steps.size()&&steps[next].words[0]==8&&p.byte(164)>=steps[next].words[1])++next;
        p.setByte(153,std::uint8_t(std::min(next,unsigned(steps.size()))));
        if(p.byte(153)>=steps.size()){
            p.setu(1180,p.u(1180)|0x400);p.setByte(153,0);
            const unsigned level=p.byte(164);unsigned index=0;
            if(level>=d.performance[0].words[0]){
                while(index<d.performance.size()&&level>=d.performance[index].words[0])++index;
                if(index==d.performance.size()){--index;p.setu(1180,p.u(1180)|0x800);}
                p.setByte(153,std::uint8_t(index));
            }
        }
    }else if(command==2){
        const auto step=performance(p,d).words;p.setByte(164,std::uint8_t(step[0]));
        const unsigned next=p.byte(153)+1;if(next<d.performance.size())p.setByte(153,std::uint8_t(next));else p.setu(1180,p.u(1180)|0x800);
    }else if(command==3){
        const auto step=optional(p,d).words;out.pointsSpent=step[3];p.setu(72,p.u(72)-out.pointsSpent);p.setByte(156+step[1],std::uint8_t(step[0]));
        carCall(0x0c0287a0,step[1],step[0]);carCall(0x0c029040);advanceOptional(p,d);
    }else if(command==4){
        advanceOptional(p,d);if(p.byte(154)==p.byte(163))advanceOptional(p,d);
    }else if(command==5||command==6){const auto step=optional(p,d).words;carCall(0x0c0287a0,step[1],command==5?step[0]:p.byte(156+step[1]));carCall(0x0c029040);}
    out.profileChanged=p.words!=before;return out;
}
}
