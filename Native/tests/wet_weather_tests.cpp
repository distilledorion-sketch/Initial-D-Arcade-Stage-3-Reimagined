#include "wet_weather.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>
using namespace idas3;
static unsigned checks=0;
static void check(bool ok){++checks;if(!ok){std::fprintf(stderr,"FAIL %u\n",checks);std::exit(1);}}
static std::vector<WetWeather::Quad> trails(WetWeather& weather,Vec3 eye={0,8,-8},unsigned stride=1){
    weather.build(eye,{0,0,2},true,stride);std::vector<WetWeather::Quad> result;
    for(unsigned i=0;i<weather.count;++i)if(weather.quads[i].waterTrail)result.push_back(weather.quads[i]);
    return result;
}
static bool sameGeometry(const WetWeather::Quad& a,const WetWeather::Quad& b){
    return length(a.center-b.center)<1e-6f&&length(a.up-b.up)<1e-6f&&length(a.across-b.across)<1e-6f;
}
int main(){
    WetWeather w;std::array<WetWeather::Car,2> cars{{
        {{0,0,0},0,30,true,{{{-.72f,0,-1.25f},{.72f,0,-1.25f}}},true},
        {{3,0,5},0,35,true,{{{2.28f,0,3.75f},{3.72f,0,3.75f}}},true}}};
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
    // Every corner lies on the actual tire plane on uphill, downhill and
    // banked roads. These source-texture pairs never become vertical ramps.
    for(const Vec3 slope:std::array<Vec3,4>{{{0,1,-.4f},{0,1,.4f},{.4f,1,0},{.35f,1,-.4f}}}){
        const auto normal=normalized(slope);WetWeather grounded;
        std::array<WetWeather::Car,2> pair{};auto& car=pair[0];
        car.visible=true;car.speed=30;car.contactsValid=true;
        car.rearContacts={Vec3{-.72f,.72f*slope.x/slope.y,0},Vec3{.72f,-.72f*slope.x/slope.y,0}};
        car.rearNormals={normal,normal};
        grounded.advance(3./60.,true,false,pair);const auto first=trails(grounded);check(first.size()==2);
        for(const auto& q:first){
            check(q.texture==1&&q.waterTrail);
            check(std::abs(dot(q.up,normal))<1e-6f&&std::abs(dot(q.across,normal))<1e-6f);
            for(const auto corner:{q.center-q.across-q.up,q.center-q.across+q.up,q.center+q.across-q.up,q.center+q.across+q.up})
                check(std::abs(dot(corner,normal)-.012f)<1e-5f);
            const auto front=q.center+q.up-normal*.012f;
            check(length(front-car.rearContacts[0])<1e-5f||length(front-car.rearContacts[1])<1e-5f);
        }
        // The body still faces +Z while the tires slide across the road in +X.
        // New rainmarks follow that travel; older marks keep their world pose.
        const auto slide=normalized(Vec3{1,-slope.x/slope.y,0})*.9f;
        car.position+=slide;for(auto& p:car.rearContacts)p+=slide;
        grounded.advance(3./60.,true,false,pair);const auto moved=trails(grounded);check(moved.size()==4);
        for(const auto& prior:first){unsigned matches=0;for(const auto& q:moved)if(sameGeometry(prior,q)){++matches;check(q.alpha<prior.alpha);}check(matches==1);}
        unsigned newest=0;for(const auto& q:moved)if(std::abs(q.alpha-.7f)<1e-5f){++newest;check(dot(normalized(q.up),normalized(slide))>.999f);}
        check(newest==2);
        // Steering alone and the camera cannot move an existing road mark.
        car.yaw=pi*.75f;grounded.advance(1./60.,true,false,pair);const auto turned=trails(grounded,{9,6,-4});
        check(turned.size()==moved.size());for(const auto& prior:moved){bool found=false;for(const auto& q:turned)found|=sameGeometry(prior,q);check(found);}
        const auto paused=grounded.quads;const auto pausedCount=grounded.count;
        grounded.advance(.25,true,true,pair);grounded.build({9,6,-4},{0,0,2},true);
        check(grounded.count==pausedCount&&std::memcmp(paused.data(),grounded.quads.data(),sizeof(paused))==0);
    }
    // Both visible cars have independent contact planes and teleport clearing.
    WetWeather remote;cars={{{{0,0,0},0,30,true,{{{-.72f,0,0},{.72f,0,0}}},true},
                            {{8,0,0},0,30,true,{{{7.28f,0,0},{8.72f,0,0}}},true}}};
    remote.advance(3./60.,true,false,cars);check(trails(remote).size()==4);
    cars[0].position.x+=100;for(auto& p:cars[0].rearContacts)p.x+=100;
    remote.advance(1./60.,true,false,cars);const auto retained=trails(remote);check(retained.size()==2);
    for(const auto& q:retained)check(q.center.x>7&&q.center.x<10);
    cars[0].visible=false;cars[1].visible=false;remote.advance(1./60.,true,false,cars);check(trails(remote).empty());

    // Missing, downward and non-finite surfaces emit no speculative horizontal
    // marks. A complete contact pair is required even in reduced detail.
    for(unsigned invalid=0;invalid<4;++invalid){
        WetWeather missing;cars[0].visible=true;cars[0].position={};cars[0].rearContacts={Vec3{-.72f,0,0},Vec3{.72f,0,0}};
        cars[0].contactsValid=invalid!=0;cars[0].rearNormals={Vec3{0,1,0},Vec3{0,1,0}};
        if(invalid==1)cars[0].rearNormals[1]={0,-1,0};
        if(invalid==2)cars[0].rearNormals[1].y=std::numeric_limits<float>::quiet_NaN();
        if(invalid==3)cars[0].rearContacts[1].x=std::numeric_limits<float>::infinity();
        missing.advance(.25,true,false,cars);check(missing.liveSpray()==0&&trails(missing).empty());
    }
    WetWeather reduced;cars={{{{0,0,0},0,30,true,{{{-.72f,0,0},{.72f,0,0}}},true},
                             {{8,0,0},0,30,true,{{{7.28f,0,0},{8.72f,0,0}}},true}}};
    reduced.advance(.25,true,false,cars);
    for(unsigned stride:{1u,2u,4u}){
        const auto selected=trails(reduced,{0,8,-8},stride);check(!selected.empty()&&selected.size()%2==0);
        for(unsigned owner=0;owner<2;++owner){unsigned left=0,right=0;for(const auto& q:selected)if(owner?(q.center.x>4):(q.center.x<4)){
            left+=q.center.x<float(owner*8);right+=q.center.x>float(owner*8);
        }check(left>0&&left==right);}
    }
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
