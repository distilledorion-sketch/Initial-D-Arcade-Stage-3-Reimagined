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
    // Snow shares only the bounded emitter infrastructure, never rain streaks
    // or water-trail materials. Both vehicles use the rear contact positions.
    WetWeather snow;
    cars={{{{0,0,0},0,30,true,{{{-2,4,1},{2,4,1}}},true},
           {{8,0,0},0,30,true,{{{6,4,1},{10,4,1}}},true}}};
    snow.advance(3./60.,true,false,cars,true);
    snow.build({0,6,-6},{0,4,2},true);
    bool flakes=false;unsigned powder=0;
    for(unsigned i=0;i<snow.count;++i){const auto& q=snow.quads[i];
        check(!q.waterTrail&&(q.texture==1||q.texture==7));
        check(std::abs(length(q.up)-length(q.across))<1e-5f);
        check(std::abs(dot(q.up,q.across))<1e-5f);
        flakes|=q.texture==1;
        if(q.texture==7){++powder;check(std::abs(q.center.y-4.12f)<1e-4f);}
    }
    check(flakes&&powder==4);
    const auto snowSnapshot=snow.quads;const auto snowCount=snow.count;
    snow.advance(.25,true,true,cars,true);snow.build({0,6,-6},{0,4,2},true);
    check(snow.count==snowCount&&std::memcmp(snowSnapshot.data(),snow.quads.data(),sizeof(snowSnapshot))==0);
    snow.build({0,6,-6},{0,4,2},true,4);check(snow.count>0&&snow.count<snowCount);
    WetWeather sa,sb;
    for(unsigned i=0;i<120;++i)sa.advance(1./120.,true,false,cars,true);
    for(unsigned i=0;i<30;++i)sb.advance(1./30.,true,false,cars,true);
    sa.build({0,6,-6},{0,4,2},true);sb.build({0,6,-6},{0,4,2},true);
    check(sa.count==sb.count&&std::memcmp(sa.quads.data(),sb.quads.data(),sizeof(sa.quads))==0);
    cars[0].speed=cars[1].speed=0;
    for(unsigned i=0;i<60;++i)snow.advance(1./60.,true,false,cars,true);
    snow.build({0,6,-6},{0,4,2},true);check(snow.liveSpray()==0&&snow.count>0);
    for(unsigned i=0;i<36000;++i){snow.advance(1./60.,true,false,cars,true);snow.build({float(i),100,-float(i)},{float(i),100,-float(i)+1},true);check(snow.count<=352);}
    // Rain/snow transitions discard the prior effect even while paused.
    snow.advance(0,true,true,cars,false);snow.build({0,6,-6},{0,4,2},true);
    check(snow.frame()==0&&snow.liveSpray()==0);
    for(unsigned i=0;i<snow.count;++i)check(snow.quads[i].texture==6);
    snow.advance(0,false,false,cars);snow.build({},{0,0,1},false);check(snow.count==0);
    std::printf("PASS %u rain/snow weather checks\n",checks);
}
