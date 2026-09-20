#include "wet_weather.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace idas3;
static unsigned checks=0;
static void check(bool ok){++checks;if(!ok){std::fprintf(stderr,"FAIL %u\n",checks);std::exit(1);}}
int main(){
    WetWeather w;std::array<WetWeather::Car,2> cars{{{{0,0,0},0,30,true},{{3,0,5},0,35,true}}};
    w.advance(1,true,false,cars);check(w.frame()==15); // bounded catch-up
    check(w.liveSpray()>0&&w.liveSpray()<=96);
    w.build({0,2,-6},{0,1,2},true);check(w.count>0&&w.count<=352);
    bool rain=false,spray=false;
    for(unsigned i=0;i<w.count;++i){auto q=w.quads[i];rain|=!q.waterTrail&&q.texture==6;spray|=q.waterTrail&&q.texture==1;
        if(q.waterTrail)check(std::abs(q.up.y)<=.21f&&length(q.up)>length(q.across));
        else check(length(q.up)>10*length(q.across));
        check(std::isfinite(q.center.x)&&std::isfinite(q.center.y)&&std::isfinite(q.center.z)&&q.alpha>=0&&q.alpha<=1);}
    check(rain&&spray);
    const auto snapshot=w.quads;const auto count=w.count,live=w.liveSpray();const auto tick=w.frame();
    w.advance(.25,true,true,cars);w.build({0,2,-6},{0,1,2},true);
    check(w.frame()==tick&&w.liveSpray()==live&&w.count==count);
    check(std::memcmp(snapshot.data(),w.quads.data(),sizeof(snapshot))==0);
    // Two frame rates produce identical simulation and rendered positions.
    WetWeather a,b;
    for(int i=0;i<120;++i)a.advance(1./120.,true,false,cars);
    for(int i=0;i<30;++i)b.advance(1./30.,true,false,cars);
    a.build({0,2,-6},{0,1,2},true);b.build({0,2,-6},{0,1,2},true);
    check(a.frame()==60&&b.frame()==60&&a.count==b.count);
    check(std::memcmp(a.quads.data(),b.quads.data(),sizeof(a.quads))==0);
    // No spray at rest; existing spray dissipates instead of a permanent cloud.
    cars[0].speed=cars[1].speed=0;
    for(int i=0;i<60;++i)w.advance(1./60.,true,false,cars);
    check(w.liveSpray()==0);
    cars[0].speed=cars[1].speed=40;
    for(int i=0;i<60;++i)w.advance(1./60.,true,false,cars);
    check(w.liveSpray()>0);cars[0].visible=cars[1].visible=false;
    w.advance(1./60.,true,false,cars);check(w.liveSpray()==0);
    w.advance(1./60.,false,false,cars);w.build({},{0,0,1},false);
    check(w.count==0&&w.frame()==0&&w.liveSpray()==0);
    for(unsigned i=0;i<36000;++i){w.advance(1./60.,true,false,cars);w.build({float(i),100,-float(i)},{float(i),100,-float(i)+1},true);check(w.count<=352);}
    std::printf("PASS %u wet-weather checks\n",checks);
}
