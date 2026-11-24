#include "original_data.h"
#include "original_dynamics.h"
#include "original_driving_float.h"
#include "original_tuning.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned base=0x0c900f00;
std::uint64_t checks=0,instructions=0,coefficientCases=0,transitionFrames=0,profileCases=0;
unsigned currentCar=0,currentCondition=0,currentTune=0,currentTick=0;
unsigned bits(float v){return std::bit_cast<unsigned>(v);}
void equal(unsigned expected,unsigned actual,unsigned address){
    ++checks;if(expected!=actual)throw std::runtime_error("Speed/loss differential car="+std::to_string(currentCar)+
        " condition="+std::to_string(currentCondition)+" tune="+std::to_string(currentTune)+" tick="+std::to_string(currentTick)+
        " address="+hex(address)+" original="+hex(expected)+" native="+hex(actual));
}
void compareDrive(const RefMemory& m,const OriginalDriveState& state){
    for(unsigned i=0;i<state.words.size();++i)equal(m.read32(base+i*4),state.words[i],base+i*4);
}
void seed(RefMemory& m,unsigned car,unsigned condition,unsigned tune){
    m.clear();m.zeroRegion(base,OriginalDriveState::byteSize);m.zeroRegion(0x0cfff000,512);
    m.zeroRegion(0x0c92f0e0,8);m.write32(0x0c2f4bc8,0);
    m.write32(0x0c901654,car);m.write32(0x0c9015cc,condition);m.write32(0x0c9015f0,tune);
    m.write32(0x0c9015e4,0);m.write32(0x0c9015d4,0);m.writeFloat(0x0c901650,0);m.writeFloat(0x0caa987c,0);
}
float prepare(RefMemory& m,OriginalDriveState& s,const OriginalFrameParameters& p){
    RefCpu c(m);c.r[15]=0x0cfff100;instructions+=c.run(0x0c15cec0,0x0c15cf88,1000);
    const auto value=prepareOriginalFrame(s,p);equal(c.fr[15],bits(value),0x0c15cf88);compareDrive(m,s);return value;
}
void allTables(RefMemory& m,const std::filesystem::path& root,const OriginalPhysicsData& data){
    // This also compares packaged parameter extraction, independently against
    // the canonical image, rather than supplying both sides with one decoder.
    for(currentCondition=0;currentCondition<18;++currentCondition){
        const auto path=data.loadPath(root/"data/original_physics",currentCondition);
        for(currentCar=0;currentCar<35;++currentCar)for(currentTune=0;currentTune<76;++currentTune){
            OriginalPhysicsSelection selection;selection.conditionCode=currentCondition;selection.vehicleIndex=currentCar;
            selection.upgradeIndex0C9015F0=currentTune;
            const auto p=data.parameters(selection,path);seed(m,currentCar,currentCondition,currentTune);
            equal(m.read32(0x0c28451c+currentCondition*140+currentCar*4),bits(p.frame.table0C28451C),0x0c28451c);
            equal(m.read32(0x0c28866c+currentTune*4),bits(p.frame.table0C28866C),0x0c28866c);
            equal(m.read32(0x0c284f80+currentCar*8),bits(p.loss.cap0C284F80),0x0c284f80);
            equal(m.read32(0x0c284f84+currentCar*8),bits(p.loss.growth0C284F84),0x0c284f84);
            OriginalDriveState state;state.setf(0x214,.7f);state.setf(0x218,-.3f);
            m.writeFloat(base+0x214,.7f);m.writeFloat(base+0x218,-.3f);
            prepare(m,state,p.frame);++coefficientCases;
        }
    }
}
void profileSelections(RefMemory& m,const std::filesystem::path& root,const OriginalPhysicsData& data){
    constexpr unsigned profileAddress=0x0c31c99c,stack=0x0d100000;
    for(unsigned condition:{6u,15u,16u,17u}){
        currentCondition=condition;
        const auto path=data.loadPath(root/"data/original_physics",condition);
        for(currentCar=0;currentCar<35;++currentCar)for(currentTune=0;currentTune<76;++currentTune)
        for(unsigned package=0;package<5;++package)for(unsigned weather:{0u,1u}){
            auto profile=makeOriginalFreshBattleProfile();profile.setu(16,currentCar);
            profile.setu(24,(currentCar+currentTune)%32);profile.setu(32,weather);
            profile.setByte(164,std::uint8_t(currentTune));profile.setByte(152,std::uint8_t(package));
            m.clear();m.zeroRegion(stack,0x10000);
            for(unsigned i=0;i<profile.words.size();++i)m.write32(profileAddress+i*4,profile.words[i]);
            RefCpu c(m);c.r[4]=condition;c.r[15]=stack+0xf000;c.pr=0x00ff0000;
            // Run actual159720/133240 profile getters and engine selection.
            // Stop before the separately-tested player initializer.
            instructions+=c.run(0x0c159720,0x0c1597ce,1000);
            OriginalPhysicsSelection selection;selection.conditionCode=condition;
            applyOriginalProfilePhysicsSelection(selection,profile);
            for(auto [address,value]:std::array<std::pair<unsigned,unsigned>,6>{{
                {0x0c901654,selection.vehicleIndex},{0x0c9015e0,selection.vehicleMode0C9015E0},
                {0x0c9015f0,selection.upgradeIndex0C9015F0},{0x0c9015f4,selection.overrideMode0C9015F4},
                {0x0c9015fc,selection.mode0C9015FC},{0x0c9015c0,selection.mode0C9015C0}}})equal(m.read32(address),value,address);
            const auto p=data.parameters(selection,path);
            c.r[1]=0x0c901654;c.r[11]=0x0c28830c;
            instructions+=c.run(0x0c15e4e6,0x0c15e52e,200);
            equal(c.r[5],p.transmission.profileIndex,0x0c15e52e);equal(c.r[6],p.transmission.maximumGear,0x0c15e52e);
            equal(c.fr[14],bits(p.transmission.workingBase),0x0c15e52e);equal(c.fr[8],bits(p.transmission.lower),0x0c15e52e);
            equal(c.fr[6],bits(p.transmission.upper),0x0c15e52e);equal(c.fr[7],bits(p.transmission.divisor),0x0c15e52e);
            for(unsigned word=0;word<p.profile.words.size();++word){
                const unsigned address=0x0c28825c+c.r[5]*88+word*4;equal(m.read32(address),p.profile.words[word],address);
            }
            const auto initialized=data.initialization(selection,{},{});
            equal(initialized.mode0C9015FC,m.read32(0x0c9015fc),0x0c9015fc);
            equal(initialized.mode0C9015C0,m.read32(0x0c9015c0),0x0c9015c0);
            ++profileCases;
        }
    }
}
void persistentBoostLoss(RefMemory& m,const std::filesystem::path& root,const OriginalPhysicsData& data){
    const std::array<float,10> progress{-150.f,std::nextafter(-100.f,-INFINITY),-100.f,std::nextafter(-100.f,0.f),
        -25.f,-1.f,-.0001f,0.f,1.f,25.f};
    const std::array<float,9> speeds{0.f,std::nextafter(.1f,0.f),.1f,std::nextafter(.1f,1.f),
        std::nextafter(.5f,0.f),.5f,std::nextafter(.5f,1.f),.75f,1.f};
    for(currentCondition=0;currentCondition<18;++currentCondition){
        const auto path=data.loadPath(root/"data/original_physics",currentCondition);
        for(currentCar=0;currentCar<35;++currentCar){
            currentTune=(currentCar*7+currentCondition*13)%76;
            OriginalPhysicsSelection selection;selection.conditionCode=currentCondition;selection.vehicleIndex=currentCar;
            selection.upgradeIndex0C9015F0=currentTune;
            const auto p=data.parameters(selection,path);seed(m,currentCar,currentCondition,currentTune);
            auto frame=p.frame;auto lossParameters=p.loss;
            OriginalDriveState state;OriginalLossState loss{0,2.5f};m.writeFloat(0x0caa9884,loss.persistentPenalty0CAA9884);
            // Cross boost disable/re-enable, suppressor mode and leader changes
            // without recreating the drive or clearing persistent loss.
            for(currentTick=0;currentTick<360;++currentTick){
                frame.global0C9015E4=(currentTick/30)%3!=0;
                frame.global0C9015D4=(currentTick/60)%3==2?1:0;
                frame.global0C901650=progress[currentTick%progress.size()];
                m.write32(0x0c9015e4,frame.global0C9015E4);m.write32(0x0c9015d4,frame.global0C9015D4);
                m.writeFloat(0x0c901650,frame.global0C901650);prepare(m,state,frame);
                const float speed=speeds[(currentTick/3)%speeds.size()];
                const float lateral=currentTick%5==0?0.f:float(currentTick%11)*.025f;
                for(auto [offset,value]:std::array<std::pair<unsigned,float>,4>{{
                    {0x248,speed},{0x214,float(currentTick%4)*.25f},{0x258,lateral},{0x25c,-lateral*.5f}}}){
                    state.setf(offset,value);m.writeFloat(base+offset,value);
                }
                state.setu(0x1a8,currentTick%17==0?1u:0u);m.write32(base+0x1a8,state.u(0x1a8));
                lossParameters.normalizedBrake0CAA98A0=float(currentTick%5)*.25f;
                m.writeFloat(0x0caa98a0,lossParameters.normalizedBrake0CAA98A0);
                // Original EA42 clears the immediate loss each frame. The
                // persistent word and every drive field remain carried here.
                loss.speedLoss0CAA9880=0;m.writeFloat(0x0caa9880,0);
                RefCpu c(m);c.r[0]=base;c.r[12]=0x0caa9884;
                instructions+=c.run(0x0c15e370,0x0c15e4e6,2000);
                updateOriginalLongitudinalLoss(state,loss,lossParameters);compareDrive(m,state);
                equal(m.read32(0x0caa9880),bits(loss.speedLoss0CAA9880),0x0caa9880);
                equal(m.read32(0x0caa9884),bits(loss.persistentPenalty0CAA9884),0x0caa9884);++transitionFrames;
            }
        }
    }
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Usage: original_speed_loss_boundary_reference_tests native_root canonical_image");
    const OriginalDrivingFloatScope floatingPoint;const std::filesystem::path root=argv[1];RefMemory m(argv[2]);
    const auto data=OriginalPhysicsData::load(root/"data/original_physics/tables.bin");
    allTables(m,root,data);profileSelections(m,root,data);persistentBoostLoss(m,root,data);
    std::cout<<"PASS "<<coefficientCases<<" exhaustive car/course/tuning coefficient cases, "<<profileCases<<" profile/package/engine selections and "<<transitionFrames
        <<" carried boost/loss transition frames, "<<checks<<" bit comparisons, "<<instructions<<" original instructions; zero hooks.\n"
        <<"Scope: packaged table extraction, actual159720/133240 profile selection and E4E6 table selector, CEC0..CF88 and E370..E4E6 with explicit normalized speed/contact/brake inputs. Not full-road, complete-race-owner or physical-cabinet parity.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
