#include "original_feedback.h"
#include "original_driving_float.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace idas3ffb {
namespace {
// Verified words from the original image. No host tuning multipliers.
constexpr std::array<int,35> carLimit{
    90,90,90,94,95,93,95,98,97,90,91,91,92,90,94,93,97,95,
    96,98,90,91,98,98,97,90,90,98,98,91,88,96,92,89,99}; //31FE04
constexpr std::array<int,11> globalTorque{-35,-28,-21,-14,-7,0,7,14,21,28,35}; //31FD54
constexpr std::array<int,11> cardTorque{-40,-30,-20,-10,0,10,20,30,40,70,70}; //31FDD8
constexpr std::array<float,11> globalDamper{10,10,15,20,25,35,40,45,50,55,60}; //31FD80
constexpr std::array<float,11> cardDamper{.2f,.4f,.6f,.8f,1,1.2f,1.4f,1.6f,1.8f,2.6f,2.6f}; //31FDAC
// First float of the35 original 24-byte engine records at0C270EB0. The force
// producer reads this fixed per-car denominator, not an upgraded engine target.
constexpr std::array<float,35> engineDenominator{
    8000,8000,8000,8000,8000,9000,9000,10000,10000,9000,
    8000,9000,9000,9000,9000,10000,10000,10000,10000,9000,
    9000,9000,9000,9000,8000,8000,9000,9000,9000,9000,
    12000,9000,9000,9000,10000};
bool truncate(float value,int& out) noexcept {
    if(!std::isfinite(value)||value>=2147483648.f||value<=-2147483648.f)return false;
    out=static_cast<int>(value);return true; // SH4 FTRC: toward zero.
}
int half(int value) noexcept {
    return value>=0?value/2:-int((std::uint32_t(-std::int64_t(value))+1u)/2u);
}
bool valid(const OriginalInput& in) noexcept {
    if(in.carIndex>=35||in.globalSetting>10||in.cardSteeringLevel>10||in.condition>17)return false;
    for(float v:{in.steering1CC,in.headingFactor27C,in.acceleration24C,
        in.frontContact3B4,in.frontContact3B8,in.normalizedFrontContact3C4,in.normalizedFrontContact3C8,
        in.wallMagnitude158,in.drive258,in.drive25C,in.drive260,in.drive264,
        in.normalizedSpeed248,in.filteredRpm,in.transmissionNormalized})
        if(!std::isfinite(v))return false;
    return true;
}
}

void OriginalProducer::reset() noexcept {
    state_.ramp=0;state_.torqueCode=128;state_.damperParameter=0;
    //159F40 intentionally leaves the previous-rumble latch unchanged.
}

OriginalOutput OriginalProducer::update(const OriginalInput& in) noexcept {
    OriginalOutput out;
    if(!valid(in))return out;
    const idas3::original::OriginalDrivingFloatScope fp;
    const auto ramp=state_.ramp<256?state_.ramp+1:state_.ramp;
    const float factor=1.f-in.headingFactor27C;

    //15A61E..15A688: front road-response sign and normalized magnitude are
    // separate fields. Inconsistent signs produce zero, not an invented kick.
    int road=0;
    if(in.frontContact3B4>in.frontContact3B8){
        const float difference=in.normalizedFrontContact3C4-in.normalizedFrontContact3C8;
        if(!std::isfinite(difference))return out;
        if(difference>1.f)road=40;
        else if(difference<0.f)road=0;
        else if(!truncate(difference*40.f,road))return out;
    }else{
        const float difference=in.normalizedFrontContact3C8-in.normalizedFrontContact3C4;
        if(!std::isfinite(difference))return out;
        if(difference>1.f)road=-40;
        else if(difference<0.f)road=0;
        else if(!truncate((-difference)*40.f,road))return out;
    }
    //15A68A..15A78E: a valid wall corner overrides the road response.
    if(in.wallContact150&&in.wallCorner154<=3){
        float wall=in.wallMagnitude158*4.f;
        wall*=float(carLimit[in.carIndex]+cardTorque[in.cardSteeringLevel]);
        int magnitude=0;if(!truncate(wall,magnitude))return out;
        std::int64_t signedWall=magnitude;if(in.wallCorner154&1)signedWall=-signedWall;
        const int cap=in.snow438?16:24;
        road=int(std::clamp(signedWall,std::int64_t(-cap),std::int64_t(cap)));
    }
    //15A790..15A83E: preserve each single-precision operation and truncation.
    float center=in.steering1CC*100.f;center+=center;center*=factor;
    int centering=0;if(!truncate(center,centering))return out;
    std::int64_t torque=128+std::int64_t(road)-centering;
    if(torque<std::numeric_limits<std::int32_t>::min()||
       torque>std::numeric_limits<std::int32_t>::max())return out;
    int limit=carLimit[in.carIndex]+cardTorque[in.cardSteeringLevel];
    limit+=globalTorque[in.globalSetting];limit=std::clamp(limit,1,127);
    if(in.wet434&&!truncate(float(limit)/1.5f,limit))return out;
    if(in.snow438&&!truncate(float(limit)*.5f,limit))return out;
    if(in.oil140)limit=half(half(limit));
    int firstTrim=0;if(!truncate(float(limit)*.9f,firstTrim))return out;
    float trim=float(firstTrim)*(in.acceleration24C+1.f);trim*=.5f;
    int accelerationTrim=0;if(!truncate(trim,accelerationTrim))return out;
    limit=int(std::clamp(std::int64_t(limit)-accelerationTrim,std::int64_t(1),std::int64_t(127)));
    torque=std::clamp(torque,std::int64_t(128-limit),std::int64_t(128+limit));
    const int window=int(ramp>>1);
    torque=std::clamp(torque,std::int64_t(128-window),std::int64_t(128+window));

    //15A87E..15A8E6: command6 first parameter is always2. The varying second
    // parameter is squared angular response, with snow/oil and braking rules.
    float damper=globalDamper[in.globalSetting]*factor;damper*=factor;
    damper*=cardDamper[in.cardSteeringLevel];
    int pole=0;if(!truncate(damper,pole))return out;
    if(in.snow438)pole=half(pole);
    if(in.oil140)pole=half(half(pole));
    if(in.automaticBrake1A8)pole=90;
    pole=std::clamp(pole,1,120);

    //15A8E4..15AA72: original rumble terms and order, including one FMAC.
    float wallPair=std::fabs(in.drive258);wallPair+=std::fabs(in.drive25C);
    float carPair=std::fabs(in.drive260);carPair+=std::fabs(in.drive264);
    float rumble=std::fma(wallPair,80.f,carPair);
    const float speedAndRpm=in.normalizedSpeed248+in.filteredRpm/engineDenominator[in.carIndex];
    float surface=((in.surface164&15)==4||(in.surface168&15)==4)?1.f:0.f;
    if(in.snow438)surface=0.f;
    const float speed35=in.normalizedSpeed248*35.f;surface*=speed35;
    // These two original surface terms are zeroed by executed delay slots.
    // Keep their arithmetic/addition order; do not enable extra vibrations.
    float disabled1=((in.surface164&15)==1||(in.surface168&15)==1)?1.f:0.f;
    const float speed5=in.normalizedSpeed248*5.f;disabled1*=speed5;disabled1*=0.f;
    float disabled20=((in.surface164&240)==0x20||(in.surface168&240)==0x20)?1.f:0.f;
    const float speed25=in.normalizedSpeed248*25.f;disabled20*=speed25;disabled20*=0.f;
    float roadFlag=0.f;
    if(std::uint32_t(in.condition-6)<=1){
        roadFlag=80.f*in.normalizedSpeed248;if(!in.roadFlag14C)roadFlag*=0.f;
    }
    float gear=0.f;
    if(in.gear!=in.gearSnapshot&&in.normalizedSpeed248>std::bit_cast<float>(0x3c23d70au))
        gear=in.transmissionNormalized*60.f;
    rumble+=speedAndRpm;rumble+=surface;rumble+=disabled1;rumble+=disabled20;
    rumble+=roadFlag;rumble+=gear;
    if(!std::isfinite(rumble))return out;
    int amplitude=47;
    if(rumble<47.f&&!truncate(rumble,amplitude))return out;
    // Negative amplitudes arise only in nonphysical controlled inputs; refuse
    // an invalid device packet rather than adding a lower clamp to the ROM math.
    if(amplitude<0)return out;

    out.valid=true;out.torqueCode=int(torque);out.damperParameter=pole;
    out.rumbleAmplitude=amplitude;out.limit=limit;
    out.sendTorque=out.torqueCode!=state_.torqueCode;
    out.sendDamper=pole!=state_.damperParameter;
    if(amplitude!=0){out.sendRumble=true;out.rumbleFrequencyHalfHz=63;}
    else if(state_.rumbleLatch==1){out.sendRumble=true;out.rumbleFrequencyHalfHz=0;}
    state_.ramp=ramp;state_.torqueCode=out.torqueCode;state_.damperParameter=pole;
    if(out.sendRumble)state_.rumbleLatch=amplitude!=0?1u:0u;
    return out;
}
}
