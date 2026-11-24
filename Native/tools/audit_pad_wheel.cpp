// Offline comparison of release controller inputs; does not load a player's save.
#include "host_steering_smoothing.h"
#include "original_driving_session.h"
#include "original_host_input.h"
#include "original_start_grid.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace idas3;
using namespace idas3::original;
namespace {
using R=ControllerResponse;
constexpr R pad=R::FlycastGamepad, wheel=R::FlycastWheel;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int byteForPedal(int raw){
    const auto target=conditionOriginalInputs({32768,static_cast<uint16_t>(raw),0},{128,32,32},{},verifiedGds0033InputConstants(),0).throttle;
    if(target==1)return 255;
    for(int i=0;i<256;i++)if(controllerHostControls(0,0,i,0,wheel).throttle==target)return i;
    throw std::runtime_error("tape needs a non-8-bit pedal level");
}
int wheelFor(float demand){return std::clamp(int(std::lround(demand*80))*256,-32768,32767);}
struct Branch {
    OriginalDrivingSession s; OriginalHostInputState edges; HostSteeringSource source; HostSteeringSmoothing smoothing; float steering=0;
    void start(const std::filesystem::path& root){OriginalDrivingSelection sel;
        sel.physics=makeOriginalFreshTimeAttackSelection(0,0,OriginalWeather::Dry);sel.bodyContactEnabled=false;
        const auto pose=originalStartPose(0,originalSoloStartGridSlot(2));s.reset(root,sel,pose.position,pose.angles);}
    OriginalVehicleInputs input(int x,int y,int gas,int brake,int shift,R profile,int frame){
        auto c=controllerHostControls(static_cast<int16_t>(x),static_cast<int16_t>(y),gas,brake,profile);
        steering=source.advance(steering,0,c.steering,true,true,1.f/60);
        c.steering=smoothing.advance(steering,1.f/60); c.shiftUp=(shift&32)!=0;c.shiftDown=(shift&16)!=0;
        return adaptOriginalHostInput(edges,c,false,true,frame);
    }
};
}
int main(int argc,char**argv)try{
    check(argc==4,"usage: audit_pad_wheel NATIVE_DATA_ROOT accepted-input.csv measures.csv");
    std::ifstream in(argv[2]);std::ofstream out(argv[3]);check(in.good()&&out.good(),"input/output unavailable");
    std::string line;std::getline(in,line);if(!line.empty()&&line.back()=='\r')line.pop_back();
    check(line=="game_frame,raw_steer16,raw_throttle16,raw_brake16,shift_pressed8","wrong tape format");
    Branch p,w,m; for(auto b:{&p,&w,&m})b->start(argv[1]);
    int n=0,steps=0,edges=0,unequalDemand=0,unequalPose=0,unequalSpeed=0,firstDemand=-1,firstPose=-1;
    float peakSpeed=0,maxDiff=0;out<<"frame,axis_x,pad_steer,wheel_steer,matched_wheel_x,shift,pad_kmh,wheel_kmh,matched_wheel_kmh\n"<<std::setprecision(9);
    while(std::getline(in,line)){
        std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);int f,st,t,b,e;
        check(bool(row>>f>>st>>t>>b>>e)&&f==60+n&&st>=0&&st<=65535&&t>=0&&t<=65535&&b>=0&&b<=65535&&!(e&~48),"invalid or discontinuous input tape");++n;
        // Treat the captured signed displacement as a new horizontal device motion.
        // These are constructed device inputs, not records of physical travel.
        int x=st-32768,gas=byteForPedal(t),brake=byteForPedal(b);
        auto pi=p.input(x,0,gas,brake,e,pad,f);auto wi=w.input(x,0,gas,brake,e,wheel,f);
        int mx=wheelFor(p.steering);auto mi=m.input(mx,0,gas,brake,e,wheel,f);
        auto eqPedals=[&](auto& a){return pi.analog.throttle==a.analog.throttle&&pi.analog.brake==a.analog.brake&&a.pressedByte==e;};
        check(pi.pressedByte==e&&eqPedals(wi)&&eqPedals(mi)&&pi.analog.steering==mi.analog.steering,"matched input differs");
        if(f==60)continue; // the first observation is the initial baseline, not an update
        ++steps;edges+=bool(e&16)+bool(e&32);if(pi.analog.steering!=wi.analog.steering){++unequalDemand;if(firstDemand<0)firstDemand=f;}
        p.s.tick(pi);w.s.tick(wi);m.s.tick(mi);
        check(p.s.vehicle().drive.words==m.s.vehicle().drive.words&&p.s.actor().words==m.s.actor().words&&p.s.rollbackDigest()==m.s.rollbackDigest(),"car state differs on matched inputs");
        const auto& pd=p.s.vehicle().drive;const auto& wd=w.s.vehicle().drive;
        if(pd.u(0)!=wd.u(0)||pd.u(4)!=wd.u(4)||pd.u(8)!=wd.u(8)||pd.u(0x10)!=wd.u(0x10)){++unequalPose;if(firstPose<0)firstPose=f;}
        if(pd.u(0x238)!=wd.u(0x238))++unequalSpeed;
        float ps=pd.f(0x238)*3.6f,ws=wd.f(0x238)*3.6f,ms=m.s.vehicle().drive.f(0x238)*3.6f;
        check(std::isfinite(ps)&&std::isfinite(ws)&&std::isfinite(ms),"nonfinite speed");peakSpeed=std::max(peakSpeed,ps);maxDiff=std::max(maxDiff,std::abs(p.steering-w.steering));
        out<<f<<','<<x<<','<<p.steering<<','<<w.steering<<','<<mx<<','<<e<<','<<ps<<','<<ws<<','<<ms<<'\n';
    }
    check(n>1&&edges>0&&peakSpeed>40&&unequalDemand&&unequalPose&&unequalSpeed,"comparison was not exercised");
    std::cout<<"ticks="<<steps<<" matched_state_differences=0 shift_edges="<<edges<<" peak_pad_kmh="<<peakSpeed<<"\nsame_travel_steering_diffs="<<unequalDemand<<" first_steer_frame="<<firstDemand<<" max_demand_diff="<<maxDiff<<" pose_diffs="<<unequalPose<<" first_pose_frame="<<firstPose<<" speed_diffs="<<unequalSpeed<<"\n";
    for(int x:{8192,16384,24576,32767})std::cout<<"x="<<x<<" pad="<<controllerHostControls(x,0,0,0,pad).steering<<" wheel="<<controllerHostControls(x,0,0,0,wheel).steering<<'\n';
    std::cout<<"fixed_x=16384 changing_y=0_to_24576 pad_steer="<<controllerHostControls(16384,0,0,0,pad).steering<<" -> "<<controllerHostControls(16384,24576,0,0,pad).steering<<"\n";
    out.flush();check(bool(out),"output write failed");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
