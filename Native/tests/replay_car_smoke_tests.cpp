#include "replay_car_smoke_playback.h"
#include "race.h"
#include <fstream>
#include <iostream>
using namespace idas3;
namespace {
unsigned checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
unsigned word(const std::vector<std::uint8_t>& bytes,unsigned at){unsigned out=0;for(unsigned i=0;i<4;++i)out|=unsigned(bytes.at(at+i))<<(8*i);return out;}
void put(std::vector<std::uint8_t>& bytes,unsigned at,unsigned value){for(unsigned i=0;i<4;++i)bytes.at(at+i)=std::uint8_t(value>>(8*i));}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("New fixture directory required");
    const std::filesystem::path output=argv[1];if(std::filesystem::exists(output))throw std::runtime_error("Preserve prior fixtures");
    std::filesystem::create_directories(output);
    unsigned scenario=0,emitting=0,mixed=0;
    for(unsigned course=0;course<9;++course)for(unsigned weather=0;weather<2;++weather)
    for(unsigned night=0;night<2;++night)for(bool alternate:{false,true}){
        Replay recording;recording.beginCapture(true);recording.beginSmokeCapture();
        CarSmokeState live;live.reset(weather,alternate);std::vector<std::uint64_t> expected{live.digest()};
        for(unsigned tick=0;tick<781;++tick){
            original::OriginalCarSmokeScene scene{course,night,weather,course,tick%240<120?0u:2u,tick%180<90};
            std::array<original::OriginalCarSmokeInput,2> cars;
            for(unsigned slot=0;slot<2;++slot){auto& car=cars[slot];car.available=tick%137<119;
                car.owner.displacementMagnitude=tick%111<89?.4f:0;
                car.owner.footprint.elements={1,0,0,0,0,1,0,0,0,0,1,0,float(slot*5),.05f,-float(tick)*.4f,1};
                for(unsigned i=0;i<4;++i){car.owner.positions[i]={float(slot*5)+float(i%2)-.5f,.05f,-float(tick)*.4f-float(i/2)};
                    car.wheelSurfaces[i]=std::uint8_t((tick/35+i)%4==0?6:1);
                    if((tick/27+i)%3==0)car.wheelSurfaces[i]|=0x20;
                    car.intensity[i]=std::uint8_t((tick+i*37)%113<80?60+i*50:0);}
            }
            // Intervening source owners may consume RNG between accepted
            // ticks. Explicit seeds must preserve those gaps as well.
            unsigned seed=live.randomState();if(tick%41==0)seed=seed*0x41c64e6du+12345u;
            const auto input=captureReplayCarSmoke(scene,cars,seed,alternate);check(input.valid(),"Valid source input rejected");
            recording.recordSmoke(input);live.advanceRecorded(scene,cars,seed);expected.push_back(live.digest());
            bool brown=false,white=false;for(auto i:live.system().order()){
                brown|=live.system().cloud(i).color==0xff8b5439;white|=live.system().cloud(i).color==0xffffffff;}
            emitting+=live.system().particleCount()>0;mixed+=brown&&white;
            if(tick>=61){ReplayDetail detail;detail.elapsed=(tick-60)*100;detail.capacity=4;
                recording.record(tick-60,{0,0,-float(tick)*.4f},0,24,3,&detail);}
        }
        const auto raw=recording.sharedBytes(72000);check(raw.size()==96+720*164+781*136,"Revision3 serialized extent");
        check(word(raw,24)==3&&word(raw,92)==781,"Revision3 header lost effect clock");
        const auto tail=std::span(raw).subspan(96+720*164);
        check(std::memcmp(tail.data(),recording.smokeFrames.data(),tail.size())==0,"Effect float/seed bits changed on serialization");
        CarSmokeState played;ReplayCarSmokePlayback timeline;timeline.load(tail,played);check(played.digest()==expected[0],"Initial smoke parameters differ");
        const auto compare=[&](unsigned step){timeline.seek(step,played);check(played.digest()==expected[step],"Seek/restore differs from recorded source state");};
        for(unsigned i=0;i<720;++i){const auto step=word(raw,96+164*i+160);check(step==i+62,"Pre-race smoke prefix lost");compare(step);}
        unsigned rng=0x060860;for(unsigned i=0;i<160;++i){rng=rng*1664525u+1013904223u;compare(rng%782);}
        for(unsigned step:{0u,1u,299u,300u,301u,600u,781u,781u,0u})compare(step);
        const auto before=played.digest();const auto oldStep=timeline.step();
        const auto rejects=[&](std::vector<std::uint8_t> bad){bool rejected=false;try{timeline.load(bad,played);}catch(const std::invalid_argument&){rejected=true;}
            check(rejected&&played.digest()==before&&timeline.step()==oldStep,"Malformed replacement changed playback");};
        for(unsigned at:{0u,8u,12u,76u,128u}){auto bad=std::vector<std::uint8_t>(tail.begin(),tail.end());put(bad,at,at==0?4u:at==128?0x1000u:0x7fc00000u);rejects(std::move(bad));}
        {auto bad=std::vector<std::uint8_t>(tail.begin(),tail.end());put(bad,136+128,word(bad,136+128)^0x20);rejects(std::move(bad));}
        {auto bad=std::vector<std::uint8_t>(tail.begin(),tail.end());bad.pop_back();rejects(std::move(bad));}
        bool rejected=false;try{timeline.seek(782,played);}catch(const std::out_of_range&){rejected=true;}
        check(rejected&&played.digest()==before,"Out-of-range seek changed playback");
        // Export source-generated input for the actual Unity codec/reader.
        if(!alternate){std::ofstream file(output/("scene-"+std::to_string(course)+"-"+std::to_string(weather)+"-"+std::to_string(night)+".idr2"),std::ios::binary);
            file.write(reinterpret_cast<const char*>(raw.data()),raw.size());check(bool(file),"Fixture write failed");}
        auto invalid=recording;invalid.frames.back().dustStep=0;check(invalid.sharedBytes(72000).empty(),"Empty effect extent published");
        invalid=recording;invalid.frames[10].dustStep=0;check(invalid.sharedBytes(72000).empty(),"Backward effect clock published");
        invalid=recording;invalid.smokeFrames.back().scene^=0x20;check(invalid.sharedBytes(72000).empty(),"Invalid public timeline mutation published");
        recording.beginCapture(true);check(recording.smokeFrames.empty()&&recording.dustFrames.empty(),"New capture retained prior effects");
        ++scenario;
    }
    // Revisions1 and2 remain available, with their original layout.
    for(bool dust:{false,true}){Replay old;old.beginCapture(true);if(dust)old.beginDustCapture();ReplayDetail detail;
        for(unsigned tick=1;tick<=2;++tick){if(dust)old.recordDust({});old.record(tick,{},0,0,0,&detail);}
        const auto raw=old.sharedBytes(200);check(word(raw,24)==(dust?2u:1u)&&word(raw,20)==(dust?164u:160u),"Legacy writer changed");}
    check(emitting>0&&mixed>0,"Fixture missed live and mixed smoke/dust");
    std::cout<<"PASS scenarios="<<scenario<<" source_ticks="<<scenario*781<<" checks="<<checks<<" emitting_ticks="<<emitting<<" mixed_ticks="<<mixed
        <<"; exact particles/owners/RNG, prefix, both parameter presets, random seeks, atomic rejection, legacy writers.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
