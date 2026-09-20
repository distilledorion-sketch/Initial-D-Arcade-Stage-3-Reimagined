#include "original_battle_profile.h"
#include <bit>
#include <stdexcept>

namespace idas3::original {
namespace {
constexpr std::array<unsigned,9> firstEnemy{0,3,6,9,18,14,21,24,17};
constexpr std::array<unsigned,9> baseCount{3,3,3,5,3,3,3,4,1};
constexpr std::array<unsigned,8> buntaCourses{0,1,2,3,5,4,6,7};
constexpr std::array<OriginalRivalRecord,31> rivals{{
    {0,0,2,0,0,{0,0,1}}, {0,1,13,0,0,{0,0,1}}, {1,0,16,0,1,{0,0,1}},
    {0,0,25,1,2,{0,0,1}}, {0,1,31,1,2,{0,0,1}}, {1,0,14,1,3,{0,0,1}},
    {0,1,12,2,4,{0,0,1}}, {0,0,10,2,4,{1,1,1}}, {1,0,22,2,5,{0,0,1}},
    {0,1,9,3,6,{0,0,1}}, {0,0,7,3,6,{0,0,1}}, {1,1,19,3,7,{0,0,1}},
    {1,0,24,3,7,{0,0,1}}, {1,0,0,3,7,{0,0,1}},
    {0,0,20,5,10,{0,0,1}}, {1,0,19,5,11,{0,0,1}}, {1,0,3,5,11,{0,0,1}},
    {1,0,6,8,17,{1,1,1}}, {1,0,15,4,9,{0,0,1}}, {1,1,17,4,9,{0,0,1}},
    {1,1,15,4,9,{0,0,1}}, {0,0,5,6,12,{0,0,1}}, {0,1,30,6,12,{1,1,1}},
    {1,0,1,6,13,{0,0,1}}, {0,0,22,7,14,{0,0,1}}, {0,1,24,7,14,{0,0,1}},
    {1,0,32,7,15,{0,0,1}}, {1,1,33,7,15,{0,0,1}}, {1,0,22,7,15,{0,0,1}},
    {1,1,0,7,15,{0,0,1}}, {1,0,29,3,7,{0,0,1}}
}};
std::int32_t s32(std::uint32_t x){return std::bit_cast<std::int32_t>(x);}
}
std::uint32_t OriginalBattleProfile::u(std::uint32_t offset)const{
    if(offset%4||offset>=1228)throw std::out_of_range("Original profile word offset");return words[offset/4];
}
void OriginalBattleProfile::setu(std::uint32_t offset,std::uint32_t value){
    if(offset%4||offset>=1228)throw std::out_of_range("Original profile word offset");words[offset/4]=value;
}
std::uint8_t OriginalBattleProfile::byte(std::uint32_t offset)const{
    if(offset>=1228)throw std::out_of_range("Original profile byte offset");return std::uint8_t(words[offset/4]>>((offset%4)*8));
}
void OriginalBattleProfile::setByte(std::uint32_t offset,std::uint8_t value){
    if(offset>=1228)throw std::out_of_range("Original profile byte offset");const unsigned shift=(offset%4)*8;
    words[offset/4]=(words[offset/4]&~(255u<<shift))|(std::uint32_t(value)<<shift);
}
OriginalBattleProfile makeOriginalFreshBattleProfile(){
    OriginalBattleProfile p;
    for(unsigned i=44;i<=60;i+=4)p.setu(i,220);
    p.setu(472,1);p.setu(476,1);
    for(unsigned i=1;i<=35;++i)p.setByte(1044+i,std::uint8_t(i));
    p.setu(1140,51);p.setu(1164,1);p.setu(1176,1279);p.setu(1180,128);
    p.setByte(1187,4);p.setu(1220,1);p.setu(1224,4);return p;
}
const OriginalRivalRecord& originalRival(std::uint32_t enemy){
    if(enemy>=rivals.size())throw std::out_of_range("Original rival roster contains IDs0..30");return rivals[enemy];
}
std::uint32_t originalLegendRivalId(std::uint32_t course,std::uint32_t choice){
    if(course>=9||choice>=6)throw std::out_of_range("Original rival menu selection");
    if(course==7&&choice==4)return 28;if(course==7&&choice==5)return 29;if(course==3&&choice==5)return 30;
    const auto enemy=firstEnemy[course]+choice;if(enemy>=31)throw std::out_of_range("Original rival menu selection");return enemy;
}
OriginalLegendChoices originalLegendChoices(const OriginalBattleProfile& p,std::uint32_t course){
    if(course>=9)throw std::out_of_range("Original course");
    const auto count=baseCount[course];const auto progress=p.u(80+4*course);
    OriginalLegendChoices r{count,s32(progress)>=std::int32_t(count-1)?count-1:progress};
    const auto flags=p.u(1180);
    if((course==7&&(flags&0x4000))||(course==3&&(flags&0x8000)))return {6,5};
    if(course==7&&(flags&0x08000000))return {5,4};return r;
}
void selectOriginalRival(OriginalBattleProfile& p,std::uint32_t enemy){
    const auto& r=originalRival(enemy);p.setu(24,enemy);p.setu(1180,p.u(1180)&0xefffffffu);
    if(p.u(0)!=2){
        p.setu(8,r.night);p.setu(12,r.direction);p.setu(4,r.course);p.setu(28,r.scene);
        const auto progress=p.byte(116+enemy);p.setu(32,r.weather[(progress>>4)>0?2:(progress&15)>0?1:0]);
    }else p.setu(1180,p.u(1180)|0x10000000u);
    p.setu(20,r.car);if(enemy==24||enemy==25||enemy==30)p.setu(1180,p.u(1180)|0x10000000u);
}
std::uint32_t originalBuntaCourse(std::uint32_t menuIndex,std::int32_t level){
    if(menuIndex>=8)throw std::out_of_range("Original Bunta menu has eight entries");
    return menuIndex==3&&level>10?8:buntaCourses[menuIndex];
}
void selectOriginalBuntaCourse(OriginalBattleProfile& p,std::uint32_t menuIndex){
    if(p.u(0)!=2)throw std::invalid_argument("Original Bunta selection requires profile mode2");
    if(menuIndex>=8)throw std::out_of_range("Original Bunta menu has eight entries");
    //1347C0 is called with level[menuIndex] by the selection controller.
    const auto course=originalBuntaCourse(menuIndex,s32(p.u(1080+menuIndex*4)));
    const auto progressionCourse=course==8?3:course;
    const auto level=s32(p.u(1080+progressionCourse*4));
    p.setu(4,course);p.setu(28,course*2+1);p.setu(12,course==4?1:0);p.setu(8,1);
    p.setu(32,course==8&&level>9?1:0);selectOriginalRival(p,level<=5?13:level<=10?29:30);
}
OriginalBattleSelection originalBattleSelection(const OriginalBattleProfile& p){
    return {p.u(0),p.u(16),p.u(20),p.u(24),p.u(4),p.u(28),p.u(12),p.u(8),p.u(32)};
}
} // namespace idas3::original
