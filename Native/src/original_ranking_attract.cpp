#include "original_ranking_attract.h"
#include "original_ranking_data.h"
#include <bit>
#include <stdexcept>
namespace idas3::original {
OriginalRankingPageState initialOriginalRankingPage(unsigned index){
    if(index>=9)throw std::out_of_range("Original ranking course index");
    OriginalRankingPageState s;s.courseIndex=s.persistedCourseIndex=index;s.conditionIndex=index*2;
    if(ranking_data::courses[index]==8)s.wet=1; //02EBC2..02EBD0
    return s;
}
unsigned originalRankingCourse(const OriginalRankingPageState&s){return ranking_data::courses.at(s.courseIndex);}
std::array<std::uint8_t,5> originalRankingPlateDigits(const std::array<std::uint8_t,5>& name){
    constexpr std::array<std::uint32_t,5> factors{0xf6b0,31673,0x145e3,0x11ced,0x17f4d};
    constexpr std::array<std::uint32_t,5> addends{17706,0xd6b1,0x14573,1507,0xffa7};
    std::uint32_t sum=0;bool terminated=false;
    for(unsigned i=0;i<5;++i){if(name[i]==221)terminated=true;sum+=(terminated?0u:name[i])*factors[i]+addends[i];}
    sum%=100000;std::array<std::uint8_t,5> out{};
    for(unsigned i=0,divisor=10000;i<5;++i,divisor/=10)out[i]=std::uint8_t((sum/divisor)%10);
    return out;
}
OriginalRankingResourceEvents stepOriginalRankingResources(OriginalRankingResourceState&s,
    OriginalRankingPageState& page,const OriginalRankingCarSource& source,std::uint32_t ready){
    if(source.car>=35||s.car>=35)throw std::out_of_range("Original ranking resource car");
    OriginalRankingResourceEvents e;
    const auto draw=[&](float x){if(s.hasCar){e.drawCar=true;e.slideX=x;e.drawYawUnits=s.yawUnits;s.yawUnits-=80;}};
    switch(s.phase){
    case 0:if(page.cooldown==0){s.loadDelay=3;++s.phase;}break;
    case 1:if(--s.loadDelay==0)++s.phase;if(page.refreshCar)s.phase=6;break;
    case 2:
        if(ready!=1){if(page.refreshCar)s.phase=6;break;}
        e.configureCar=true;
        e.carCommands.push_back({0x0c029000,s.car});e.carCommands.push_back({0x0c029000,s.car});
        for(const auto [address,shift]:std::array<std::pair<std::uint32_t,unsigned>,8>{{
            {0x0c0283c0,0},{0x0c028400,3},{0x0c028480,7},{0x0c0284c0,10},
            {0x0c028500,13},{0x0c028540,16},{0x0c028580,19},{0x0c0285c0,22}}})
            e.carCommands.push_back({address,(s.packedAppearance>>shift)&7});
        e.carCommands.push_back({0x0c0286c0,(s.packedAppearance>>28)&3});
        e.carCommands.push_back({0x0c028660,(s.packedAppearance>>25)&7});
        e.carCommands.push_back({0x0c0286a0,0});
        e.carCommands.push_back({0x0c028720,(s.packedAppearance>>30)&1});
        e.carCommands.push_back({0x0c028760,s.packedAppearance>>31});
        e.carCommands.push_back({0x0c029040,0});
        e.plateDigits=originalRankingPlateDigits(source.name);
        e.carCommands.push_back({0x0c029da0,3});s.slideTicks=30;++s.phase;break;
    case 3:{float x=float(std::int32_t(s.slideTicks))*10.f;x=x/30.f;draw(x);if(--s.slideTicks==0)++s.phase;break;}
    case 4:draw(0);if(page.refreshCar)++s.phase;break;
    case 5:{float x=-float(std::int32_t(s.slideTicks));x=x*10.f;x=x/30.f;draw(x);if(++s.slideTicks==30)++s.phase;break;}
    case 6:
        e.destroyCar=s.hasCar;e.createCar=true;s.hasCar=true;s.car=source.car;
        s.packedAppearance=source.packedAppearance;page.refreshCar=false;s.phase=0;break;
    default:break; //Original BRAF bounds dispatches directly to the common tail.
    }
    return e;
}
OriginalRankingPageEvents stepOriginalRankingPage(OriginalRankingPageState&s,OriginalRankingPageInput input){
    if(s.courseIndex>=9||s.conditionIndex>=18||s.wet>1)throw std::out_of_range("Original ranking page state");
    OriginalRankingPageEvents e;
    if(input.detailPressed){
        if(s.detailMode==0){s.detailPage=0;s.detailMode=1;}
        else if(++s.detailPage>3)s.detailMode=0;
        e.clearLeaderboard=true;s.pageTicks=900;s.refreshCar=true;
    }
    const bool advance=input.nextConditionPressed || --s.pageTicks<0;
    if(advance){
        if(s.wet){
            s.wet=0;if(++s.conditionIndex>17)s.conditionIndex=0;
            if((s.conditionIndex&1)==0){
                if(++s.courseIndex>8)s.courseIndex=0;
                if(ranking_data::conditions[s.conditionIndex]<0)++s.conditionIndex;
            }
            if(originalRankingCourse(s)==8)s.wet=1;
        }else s.wet=1;
        if(s.pageTicks>0)s.persistedCourseIndex=s.courseIndex;
        e.clearLeaderboard=true;s.pageTicks=900;s.refreshCar=true;s.cooldown=120;
    }
    if(s.cooldown)--s.cooldown;
    if(s.persistedCourseIndex==s.courseIndex){
        const auto old=s.watchdogTicks;--s.watchdogTicks;
        if(old>=0)return e;
    }
    s.persistedCourseIndex=s.courseIndex;s.completed=true;return e;
}
namespace {
void rotateUnits(OriginalMatrix&m,unsigned axis,std::uint32_t units,const OriginalFscaTable&t){
    auto sc=t.sinCos(std::uint16_t(units));
    std::array<float,4>a,b;unsigned ca,cb;
    if(axis==0){ca=1;cb=2;a=transformOriginalVector(m,{0,sc[1],sc[0],0});b=transformOriginalVector(m,{0,-sc[0],sc[1],0});}
    else{ca=0;cb=2;a=transformOriginalVector(m,{sc[1],0,-sc[0],0});b=transformOriginalVector(m,{sc[0],0,sc[1],0});}
    for(unsigned i=0;i<4;++i){m.elements[ca*4+i]=a[i];m.elements[cb*4+i]=b[i];}
}
void scale(OriginalMatrix&m,float x,float y,float z){for(unsigned i=0;i<4;++i){m.elements[i]*=x;m.elements[4+i]*=y;m.elements[8+i]*=z;}}
}
OriginalRankingScene originalRankingScene(unsigned car,std::uint32_t yaw,float slideX,const OriginalFscaTable&t){
    if(car>=35)throw std::out_of_range("Original ranking car ID");
    OriginalRankingScene out;
    auto base=originalIdentityMatrix();translateOriginalMatrix(base,{slideX,0,0});
    translateOriginalMatrix(base,{0,std::bit_cast<float>(0xbf4ccccdu),-6});rotateUnits(base,0,1024,t);
    out.background=originalIdentityMatrix();translateOriginalMatrix(out.background,{0,std::bit_cast<float>(0xbf4ccccdu),-6});rotateUnits(out.background,0,1024,t);
    auto shadow=base;translateOriginalMatrix(shadow,{0,std::bit_cast<float>(0x3c23d70au),0});rotateUnits(shadow,1,yaw,t);
    auto dimensions=ranking_data::dimensions[car];
    const float x=std::bit_cast<float>(dimensions[0])*std::bit_cast<float>(0x3fcccccdU);
    const float z=std::bit_cast<float>(dimensions[1])*std::bit_cast<float>(0x3fe66666U);
    scale(shadow,x,1,z);out.shadow=shadow;
    float height=std::bit_cast<float>(ranking_data::heights[car]);
    auto body=base;translateOriginalMatrix(body,{0,height,0});rotateUnits(body,1,yaw,t);out.car=body;
    height*= -2;translateOriginalMatrix(body,{0,height,0});scale(body,1,-1,1);out.reflection=body;
    out.nextYawUnits=yaw-80;return out;
}
}
