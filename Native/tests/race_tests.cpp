#include "race.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
using namespace idas3;
void require(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
int main(){try{
    for(int displayHz:{30,60,120,144,240}) {
        FixedClock clock;int ticks=0;
        for(int i=0;i<displayHz*30;i++)clock.advance(1.0/displayHz,[&]{++ticks;});
        require(ticks==1800,"display rate must not change 60Hz clock");
    }
    FixedClock stall;int count=0;stall.advance(10,[&]{++count;});require(count==6,"bounded catchup");
    RaceClock race;race.start(1000);
    for(int i=0;i<179;i++)race.tick(0);
    require(race.phase==RacePhase::Countdown&&race.ticks==0,"countdown not race time");
    race.tick(0);require(race.phase==RacePhase::Running,"countdown complete");
    race.tick(999);require(race.sector==0,"teleport must not finish");
    for(int i=1;i<=1000;i++)race.tick(float(i));
    require(race.phase==RacePhase::Finished&&race.sector==4,"natural progress finishes");
    auto savedTicks=race.ticks;race.tick(1000);require(race.ticks==savedTicks,"results freeze time");
    Replay rep;rep.record(1,{0,0,0},3.13f,10,2);rep.record(2,{1,0,0},-3.13f,11,3);
    require(std::abs(rep.sample(1.5).yaw)>3,"yaw interpolation takes shortest path");
    require(std::abs(rep.sample(1.5).position.x-.5f)<1e-5,"position interpolation");
    require(rep.sample(2).gear==3,"exact-tick gear matches recorded shift");
    Replay longRun;for(int i=1;i<=60*300;i++)longRun.record(i,{float(i),0,0},0,10,2);
    require(longRun.frames.size()==60*300&&!longRun.truncated,"five minute runs remain complete");
    auto shared=longRun.sharedBytes(1800000);
    auto read=[&](std::size_t at){return unsigned(shared[at])|(unsigned(shared[at+1])<<8)|(unsigned(shared[at+2])<<16)|(unsigned(shared[at+3])<<24);};
    require(read(0)==0x31524449&&read(4)==1800000&&read(8)==6001&&read(12)==60,"portable replay header and 20Hz sample count");
    require(shared.size()==16+28*6001&&read(16)==1&&read(44)==4&&read(shared.size()-28)==18000,"replay retains start, regular samples and exact finish frame");
    longRun.truncated=true;require(longRun.sharedBytes(1800000).empty(),"truncated replay cannot be uploaded");longRun.truncated=false;
    Replay exact;exact.detailed=true;ReplayDetail detail;
    detail.rpm=7123.456f;detail.bodyPosition={1.1f,2.2f,3.3f};detail.pitch=.123f;detail.roll=-.234f;detail.steering=.345f;
    detail.suspension={.1f,.2f,.3f,.4f};detail.rotation={.11f,.22f,.33f,.44f};detail.elapsed=100;detail.remaining=512345;detail.capacity=4;
    exact.record(1,{1,2,3},.3f,42,4,&detail);detail.rpm=4567.891f;detail.elapsed=148;exact.record(2,{2,3,4},.4f,43,5,&detail);
    auto detailed=exact.sharedBytes(148);require(detailed.size()==96+2*160,"full 60Hz samples preserved without decimation");
    ReplayDetail restored;std::memcpy(&restored,detailed.data()+96+160+28,sizeof(restored));
    require(std::memcmp(&restored,&detail,sizeof(detail))==0,"RPM, body, suspension, wheels and clocks retain exact source bits");
    exact.frames.back().tick=4;require(exact.sharedBytes(148).empty(),"detailed replay rejects even a single missing simulation frame");
    auto dir=std::filesystem::temp_directory_path()/"idas3_remake_race_test";std::filesystem::create_directories(dir);
    auto file=(dir/"test.csv").string();require(rep.save(file),"save ghost");Replay loaded;require(loaded.load(file)&&loaded.frames.size()==2,"load ghost");
    require(rep.save(file),"replace existing ghost");
    Replay precision;
    for(unsigned i=1;i<=18000;++i)precision.record(i,{float(i)*.1234567f,-float(i)*.00001234567f,float(i)*123.4567f},float(i)*-.00234567f,float(i)*.03456789f,2);
    precision.finishTicks6000=1799999;
    require(precision.save(file)&&loaded.load(file)&&loaded.frames.size()==precision.frames.size(),"buffered five minute ghost round trip");
    for(std::size_t i=0;i<precision.frames.size();++i){const auto& a=precision.frames[i];const auto& b=loaded.frames[i];
        require(a.tick==b.tick&&a.gear==b.gear&&a.speed==b.speed&&a.yaw==b.yaw&&a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z,"CSV retains exact finite float values across buffer boundaries");
    }
    require(loaded.finishTicks6000==precision.finishTicks6000,"buffered ghost retains exact finish");
    const auto copyA=(dir/"copy-a.csv").string(),copyB=(dir/"copy-b.csv").string(),denied=(dir/"denied.csv").string();
    const auto bytes=[](const auto& path){std::ifstream in(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});};
    const auto expected=bytes(file);
    require(precision.saveCopies(copyA,copyB)==std::array<bool,2>{true,true},"both telemetry copies save");
    require(bytes(copyA)==expected&&bytes(copyB)==expected,"format-once copies retain exact legacy CSV bytes");
    require(precision.saveCopies(copyA,copyA)==std::array<bool,2>{true,true}&&bytes(copyA)==expected,"identical destination is written once");
    {std::ofstream out(denied);out<<"previous ghost";}
    std::filesystem::create_directory(denied+".tmp");
    const auto sentinel=std::filesystem::path(denied+".tmp")/"keep";{std::ofstream out(sentinel);out<<"keep";}
    require(precision.saveCopies(copyA,denied)==std::array<bool,2>{true,false},"failed best copy does not block last-run publication");
    require(bytes(copyA)==expected&&bytes(denied)=="previous ghost"&&bytes(sentinel)=="keep","failed best copy preserves existing file and obstruction");
    require(precision.saveCopies(denied,copyB)==std::array<bool,2>{false,true},"failed last-run copy does not block new best");
    require(bytes(copyB)==expected&&bytes(denied)=="previous ghost","independent copy failure preserves prior ghost");
    precision.truncated=true;
    require(precision.saveCopies(copyA,copyB)==std::array<bool,2>{false,false}&&bytes(copyA)==expected&&bytes(copyB)==expected,"truncated input cannot replace either copy");
    precision.truncated=false;
    std::filesystem::remove(copyA);std::filesystem::remove(copyB);std::filesystem::remove(denied);std::filesystem::remove(sentinel);std::filesystem::remove(denied+".tmp");
    rep.finishTicks6000=148;require(rep.save(file)&&loaded.load(file),"save exact original finish");
    require(loaded.finishTicks6000==148&&loaded.frames.back().tick==2,"finish precision survives reload independently of frame tick");
    rep.frames.back().gear=0;require(!rep.sharedBytes(148).empty()&&rep.save(file)&&loaded.load(file)&&loaded.frames.back().gear==0,"finish neutral survives replay export and CSV reload");
    {std::ofstream out(file);out<<"tick,speed,yaw,pos_x,pos_y,pos_z,gear\n1,nan,0,0,0,0,1\n";}
    require(!loaded.load(file)&&loaded.frames.empty(),"reject corrupt ghost");
    std::filesystem::remove(file);std::filesystem::remove(dir);
    std::cout<<"Race timing, refresh independence, finish validation, replay interpolation and persistence passed.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
