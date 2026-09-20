#include "native_multiplayer.h"
#include <cstring>
#include <iostream>
#include <limits>
unsigned checks=0;
void check(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(what);}
template<class F> void rejected(F f){bool threw=false;try{f();}catch(const std::invalid_argument&){threw=true;}check(threw,"malformed input must be rejected");}
int main()try{
    using namespace idas3;
    for(unsigned car=0;car<35;++car)for(unsigned course=0;course<9;++course)for(unsigned slot=0;slot<2;++slot){
        Idas3MultiplayerConfig c{40,1,course,slot,course==8?1u:0u,slot,car,34-car,slot,1};
        validateMultiplayerConfig(c);check(true,"all car/course/grid config domain");
    }
    Idas3MultiplayerConfig c{40,1,3,0,0,0,0,8,0,1};
    for(unsigned field=0;field<10;++field){auto bad=c;auto* words=reinterpret_cast<std::uint32_t*>(&bad);words[field]=0xffffffff;rejected([&]{validateMultiplayerConfig(bad);});}
    Idas3MultiplayerSnapshot s{128,1};s.flags=Idas3MpActive;s.car=8;s.headlightCounter=-1;
    for(unsigned field=0;field<21;++field){auto bad=s;
        // The21 authored float words are the contiguous body/actor/angles/
        // suspension/wheel/speed/rpm/progress segment (offset32 through112).
        float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(reinterpret_cast<char*>(&bad)+32+field*4,&nan,4);
        rejected([&]{sanitizeMultiplayerSnapshot(bad,8);});
    }
    s.speed=1000;s.rpm=-1;s.steering=20;s.suspension[0]=4;s.wheelRotation[1]=100;s.headlightCounter=90;
    const auto t=sanitizeMultiplayerSnapshot(s,8);
    check(t.speed==200&&t.rpm==0&&t.steering<=3.141593f&&t.suspension[0]==2&&std::abs(t.wheelRotation[1])<=3.141593f&&t.headlightCounter==40,"finite overshoot clamp");
    s.bodyPosition[2]=100001;rejected([&]{sanitizeMultiplayerSnapshot(s,8);});s.bodyPosition[2]=0;
    rejected([&]{sanitizeMultiplayerSnapshot(s,0);});s.flags|=128;rejected([&]{sanitizeMultiplayerSnapshot(s,8);});
    std::cout<<"PASS "<<checks<<" native multiplayer ABI/validation checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
