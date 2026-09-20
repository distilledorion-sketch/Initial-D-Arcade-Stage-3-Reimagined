#include "original_legend_menu.h"
#include "original_battle_profile.h"
#include <array>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
constexpr float lit(std::uint32_t bits){return std::bit_cast<float>(bits);}
// Exact original2A2088 and33BD50 pointer-target data, canonical efda831f...
using Row=std::array<std::uint32_t,8>;
struct CourseRow {std::uint32_t banner,background,count;std::array<Row,6> rivals;};
constexpr std::array<CourseRow,9> courses{{
    {0x20024u,0x20019u,0x3u,{{
        {{0x3000au,0x1u,0x50051u,0x50067u,0x50036u,0x50010u,0x50008u,0x50004u}},
        {{0x3000eu,0x2u,0x50054u,0x50067u,0x50039u,0x50011u,0x5000cu,0x50004u}},
        {{0x30019u,0x3u,0x5005eu,0x5006bu,0x50043u,0x50012u,0x50008u,0x50009u}},
        {},
        {},
        {},
    }}},
    {0x20027u,0x2001du,0x3u,{{
        {{0x30021u,0x2u,0x50064u,0x5006fu,0x50049u,0x50010u,0x50008u,0x50004u}},
        {{0x30004u,0x3u,0x5004bu,0x5006fu,0x50030u,0x50011u,0x5000cu,0x50004u}},
        {{0x30011u,0x4u,0x50057u,0x5006au,0x5003cu,0x50012u,0x50008u,0x50009u}},
        {},
        {},
        {},
    }}},
    {0x2001fu,0x20014u,0x3u,{{
        {{0x3001fu,0x3u,0x50062u,0xffffffffu,0x50047u,0x50010u,0x50002u,0x50004u}},
        {{0x3000fu,0x4u,0x50055u,0x50066u,0x5003au,0x50011u,0x50006u,0x50004u}},
        {{0x3000du,0x6u,0x50053u,0x50066u,0x50038u,0x50012u,0x50006u,0x50009u}},
        {},
        {},
        {},
    }}},
    {0x20021u,0x20016u,0x6u,{{
        {{0x30009u,0x3u,0x50050u,0x50067u,0x50035u,0x50010u,0x50002u,0x50004u}},
        {{0x30013u,0x5u,0x50059u,0x5006bu,0x5003eu,0x50011u,0x50006u,0x50004u}},
        {{0x3001cu,0x6u,0x50060u,0x50068u,0x50045u,0x50012u,0x50002u,0x50009u}},
        {{0x30016u,0x7u,0x5005bu,0x50066u,0x50040u,0x50013u,0x50006u,0x50009u}},
        {{0x3001eu,0x8u,0x50061u,0xffffffffu,0x50046u,0x50014u,0x50006u,0x50009u}},
        {{0x30005u,0xau,0x5004cu,0x50069u,0x50031u,0x50016u,0x50006u,0x50009u}},
    }}},
    {0x20022u,0x20017u,0x3u,{{
        {{0x30006u,0x6u,0x5004du,0x50071u,0x50032u,0x50010u,0x5000eu,0x50009u}},
        {{0x3001au,0x7u,0x5005fu,0x50071u,0x50044u,0x50011u,0x50001u,0x50009u}},
        {{0x30020u,0x8u,0x50063u,0x50070u,0x50048u,0x50012u,0x50001u,0x50009u}},
        {},
        {},
        {},
    }}},
    {0x20023u,0x20018u,0x3u,{{
        {{0x30018u,0x6u,0x5005du,0x50068u,0x50042u,0x50010u,0x50006u,0x50004u}},
        {{0x3001bu,0x7u,0x50060u,0x50068u,0x50045u,0x50011u,0x50006u,0x50009u}},
        {{0x3000bu,0x8u,0x50052u,0xffffffffu,0x50037u,0x50012u,0x50006u,0x50009u}},
        {},
        {},
        {},
    }}},
    {0x20025u,0x2001bu,0x3u,{{
        {{0x30014u,0x6u,0x5005au,0x5006eu,0x5003fu,0x50010u,0x5000eu,0x50004u}},
        {{0x30017u,0x8u,0x5005cu,0x5006eu,0x50041u,0x50011u,0x50001u,0x50004u}},
        {{0x30022u,0x8u,0x50065u,0x5006eu,0x5004au,0x50012u,0x5000eu,0x50009u}},
        {},
        {},
        {},
    }}},
    {0x20026u,0x2001cu,0x6u,{{
        {{0x30010u,0x6u,0x50056u,0xffffffffu,0x5003bu,0x50010u,0x5000eu,0x50004u}},
        {{0x30015u,0x8u,0x5005bu,0x5006du,0x50040u,0x50011u,0x50001u,0x50004u}},
        {{0x30007u,0x8u,0x5004eu,0xffffffffu,0x50033u,0x50012u,0x5000eu,0x50009u}},
        {{0x30008u,0x9u,0x5004fu,0xffffffffu,0x50034u,0x50013u,0x50001u,0x50009u}},
        {{0x3000cu,0xau,0x50053u,0x5006du,0x50038u,0x50014u,0x5000eu,0x50009u}},
        {{0x3001du,0xau,0x50061u,0x5006du,0x50046u,0x50015u,0x50001u,0x50009u}},
    }}},
    {0x20020u,0x20015u,0x1u,{{
        {{0x30012u,0x7u,0x50058u,0xffffffffu,0x5003du,0x50010u,0x50006u,0x50009u}},
        {},
        {},
        {},
        {},
        {},
    }}},
}};

}
OriginalLegendStartMetadata originalLegendStartMetadata(std::uint32_t enemy){
    const auto course=originalRival(enemy).course;const auto& c=courses.at(course);
    for(unsigned choice=0;choice<c.count;++choice)if(originalLegendRivalId(course,choice)==enemy){
        const auto& row=c.rivals[choice];OriginalLegendStartMetadata result;
        switch(row[6]&0xffffu){
            case 2:result.direction=0;break;case 6:result.direction=1;break;
            case 14:result.direction=2;break;case 1:result.direction=3;break;
            case 8:result.direction=5;break;case 12:result.direction=4;break;
            default:throw std::logic_error("Unknown source opponent direction");
        }
        const auto label=row[5]&0xffffu;
        result.extra=label==0x16;result.race=label>=0x15?0:label-0x10+1;return result;
    }
    throw std::out_of_range("Opponent absent from original course menu");
}
std::vector<OriginalChoiceDraw> originalLegendMenuDraws(const OriginalBattleProfile& profile,std::uint32_t course,std::uint32_t choice,float phase,std::uint32_t frame){
    if(course>=9||choice>=originalLegendChoices(profile,course).count)
        throw std::out_of_range("Original Legend choice is not selectable");
    if(!std::isfinite(phase)||phase<0.f||phase>1.f)throw std::invalid_argument("Original Legend confirmation phase outside0..1");
    const auto& c=courses[course];const auto& r=c.rivals[choice];const auto& opponent=originalRival(originalLegendRivalId(course,choice));
    std::vector<OriginalChoiceDraw> out;
    const auto draw=[&](unsigned selector,float x=0.f,float y=0.f,float z=0.f,float scale=1.f,OriginalChoiceColors colors=OriginalChoiceColors::Source){out.push_back({selector,x,y,z,scale,colors});};
    for(unsigned selector:{0x20053u,0x5002fu,0x5002eu,0x50000u,0x50026u,0x5002bu,0x50029u})draw(selector);
    draw(c.banner,0,lit(0xc00b851f));draw(c.background);draw(0x50025);
    //196D9A..DCE: retain the two actual FMACs and F32 operation order.
    const float gap=std::fma(float(6-c.count),12.f,8.f);
    float span=60.f+gap;span=std::fma(span,float(c.count-1),80.f);span*=.5f;
    float x=320.f-span;x*=lit(0x3c23d70a);
    const float y=lit(0xbfc28f5c);float z=lit(0x3ca3d70a);
    for(unsigned i=0;i<c.count;++i){
        if(i==choice){
            x+=lit(0x3ecccccc);z+=lit(0x3ca3d70a);
            draw(c.rivals[i][0],x,y,z,1.f,OriginalChoiceColors::Selected);
            if(phase<lit(0x3c23d70a)||(frame&6u))draw(0x50028,x,y,z+lit(0xbc23d70a));
            float advance=40.f+gap;advance*=lit(0x3c23d70a);x+=advance;
        }else{
            x+=lit(0x3e999999);
            const auto flags=profile.u(1180);
            const bool locked=(course==3&&i==5&&!(flags&0x8000u))||
                (course==7&&i==4&&!(flags&0x08000000u))||
                (course==7&&i==5&&!(flags&0x4000u));
            draw(locked?0x30000:c.rivals[i][0],x,y,z,.75f,locked?OriginalChoiceColors::Source:OriginalChoiceColors::InactiveTransmission);
            //196FAE..196FDC: completed-cycle marker shares the portrait's
            // scaled matrix and is submitted only for unselected portraits.
            const auto clears=profile.byte(116+originalLegendRivalId(course,i))>>4;
            if(clears)draw(clears==1?0x5002c:0x5002d,x,y,z,.75f);
            float advance=30.f+gap;advance*=lit(0x3c23d70a);x+=advance;
        }
    }
    x=lit(0x408ae147);
    for(unsigned i=0;i<r[1];++i){draw(0x5002a,x,lit(0xc0199999),lit(0x3c23d70a));x+=lit(0x3e23d70a);}
    draw(r[2]);
    const float detailZ=r[3]==0xffffffff?0.f:lit(0x3c23d70a);
    draw(r[3]==0xffffffff?0x5006c:r[3],0,0,detailZ);draw(r[4],0,0,detailZ);draw(r[5]);
    x=lit(0x400a3d70);const float infoY=-3.75f,infoZ=lit(0x3c23d70a);
    const auto info=[&](unsigned selector){draw(selector,x,infoY,infoZ);};
    info(0x5000a);x+=lit(0x3e75c28f);info(r[6]);
    if(course==0||course==1)x+=lit(0x3fa3d70a);
    else if(course==4||course==6||course==7)x+=lit(profile.u(12)?0x3f851eb8:0x3fa3d70a);
    else if(course==5)x+=lit(profile.u(12)?0x3f8a3d70:0x3f947ae1);
    else x+=lit(profile.u(12)?0x3f6b851e:0x3f947ae1);
    info(0x50005);x+=lit(0x3e4ccccc);info(r[7]);
    x+=lit(profile.u(8)?0x3f75c28f:0x3f2e147b);info(0x50005);x+=lit(0x3e4ccccc);
    const auto progress=profile.byte(116+originalLegendRivalId(course,choice));
    const unsigned weather=course==8?0x5000d:opponent.weather[(progress>>4)>0?2:(progress&15)>0?1:0]?0x5000f:0x50007;
    info(weather);x+=lit(weather==0x5000d?0x3f75c28f:weather==0x5000f?0x3f35c28f:0x3f2e147b);info(0x50003);
    return out;
}
std::vector<OriginalChoiceDraw> originalLegendMenuDraws(std::uint32_t course,std::uint32_t choice,float phase,std::uint32_t frame){
    auto profile=makeOriginalFreshBattleProfile();profile.setu(4,course);
    selectOriginalRival(profile,originalLegendRivalId(course,choice));
    return originalLegendMenuDraws(profile,course,choice,phase,frame);
}
const char* originalLegendMenuBankName(std::uint32_t bank){
    switch(bank){case 2:return "v3sK00common";case 3:return "v3sK00rivalface";case 5:return "v3sK02rival";default:throw std::out_of_range("Unknown Legend menu bank");}
}
NativeModelChunk materializeOriginalLegendColors(const NativeModelChunk& chunk,OriginalChoiceColors colors){
    if(colors!=OriginalChoiceColors::Source&&colors!=OriginalChoiceColors::Selected&&colors!=OriginalChoiceColors::InactiveTransmission)
        throw std::invalid_argument("Unexpected Legend color state");
    return materializeOriginalChoiceColors(chunk,colors);
}
}
