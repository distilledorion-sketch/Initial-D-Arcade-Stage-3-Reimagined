#include "original_rival_motion.h"
#include "original_math.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
constexpr float lit(std::uint32_t bits){return std::bit_cast<float>(bits);}
float angle(float x,float z){float v=float(std::bit_cast<std::int32_t>(originalAtan2Angle(x,z)));v*=lit(0x40490FDB);v*=lit(0x38000000);return v;}
float wrap(float v){if(std::abs(v)>lit(0x40490FDB))v+=v>0?lit(0xC0C90FDB):lit(0x40C90FDB);return v;}
std::int32_t ftrc(float v){if(v>=2147483648.f)return INT32_MAX;if(v<=-2147483648.f||std::isnan(v))return INT32_MIN;return std::int32_t(v);}
bool range(std::uint32_t index,std::uint32_t start,std::uint32_t extent){return index-start<=extent;}
}
void advanceOriginalRivalMotion(OriginalRivalState& r,OriginalActorState& pub,
    const OriginalRivalPath& path,const OriginalRivalPaceInputs& in,std::uint32_t groundValid){
    const auto index=r.u(12),profile=in.profile0CAA9868;
    if(index+3>=path.points.size()||path.condition!=in.condition0C9015CC||profile>=32)throw std::invalid_argument("Original rival movement selection/path");
    bool fartherBody=false,fartherMotion=false;float turnDivisor=80.f;
    if(profile==16&&(range(index,315,5)||range(index,342,18)))fartherBody=fartherMotion=true;
    if(profile==13){
        if(range(index,30,10)){fartherMotion=true;turnDivisor=68.f;}
        if(range(index,110,10)){fartherMotion=true;turnDivisor=60.f;}
        if(range(index,260,25)){fartherMotion=true;turnDivisor=70.f;}
        if(range(index,330,10)){fartherMotion=true;turnDivisor=80.f;}
        if(range(index,495,40)){fartherMotion=true;turnDivisor=140.f;}
        if(range(index,720,15)){fartherMotion=true;turnDivisor=75.f;}
    }
    if(profile-21u<=2){fartherMotion=false;turnDivisor=95.f;}
    const auto& near=path.points[index+1+unsigned(fartherMotion)];
    float delta=wrap(angle(near[0]-r.f(200),near[2]-r.f(208))-r.f(0xac));
    const float turn=std::abs(delta/turnDivisor);
    float rate=delta<0?std::fma(turn,-1.f,r.f(0xb0)):r.f(0xb0)+turn;
    r.setf(0xac,r.f(0xac)+rate);r.setf(0xb0,rate*lit(0x3F59999A));
    const auto& ahead=path.points[index+2+unsigned(fartherBody)];
    delta=wrap(angle(-ahead[0]+r.f(200),-ahead[2]+r.f(208))-r.f(0xe4));
    float impulse=std::abs(delta/110.f);if(delta<0)impulse*=-1.f;
    r.setf(0xb4,impulse);if(r.f(68)==0.f){r.setf(0xb4,0.f);r.setf(0xb8,0.f);}
    rate=r.f(0xb8)+r.f(0xb4);r.setf(0xb8,rate);r.setf(0xe4,r.f(0xe4)+rate);
    r.setf(0xb8,rate*(r.f(68)>60.f?lit(0x3F733333):lit(0x3F666666)));
    r.setf(0xac,wrap(r.f(0xac)));r.setf(0xe4,wrap(r.f(0xe4)));
    const auto intensity=[&](float threshold,float scale){float v=r.f(64)-threshold;return std::clamp(v>0?ftrc(v*scale):0,0,255);};
    const auto front=std::uint8_t(intensity(lit(0x3F4CCCCD),1275.f)),rear=std::uint8_t(intensity(lit(0x3F59999A),1700.f));
    for(auto offset:{84,85,88,89})pub.setByte(offset,0);
    pub.setByte(86,front);pub.setByte(87,front);pub.setByte(90,rear);pub.setByte(91,rear);
    float slipDelta=r.f(0xac)-r.f(0xe4);if(std::abs(slipDelta)>lit(0x402FEDE0))slipDelta=-slipDelta;
    const float oldSlip=r.f(0xbc),phase=std::fma(oldSlip,lit(0x3DA3D70A),r.f(0xc4));r.setf(0xc4,phase);
    slipDelta-=oldSlip;float ripple=originalSinF32(phase);ripple*=oldSlip;ripple*=lit(0x3D23D70A);ripple*=r.f(68);ripple/=200.f;slipDelta+=ripple;
    r.setf(0xbc,wrap(std::fma(wrap(slipDelta),lit(0x3D8F5C29),oldSlip)));
    float step=r.f(68)/60.f;step/=lit(0x40666666);
    const float dx=originalSinF32(r.f(0xac))*step,dz=originalCosF32(r.f(0xac))*step;
    float vx=r.f(44)+dx;vx+=r.f(52);float vz=r.f(48)+dz;vz+=r.f(56);
    r.setf(200,r.f(200)+vx);r.setf(208,r.f(208)+vz);r.setf(212,vx);r.setf(220,vz);
    for(unsigned k=0;k<3;++k)r.setu(0x178+k*4,r.u(0x184+k*4));
    r.setf(0x184,dx);r.setf(0x188,0.f);r.setf(0x18c,dz);
    for(unsigned wheel=0;wheel<4;++wheel){
        float rotation=r.f(68)/(wheel<2?lit(0x44426666):lit(0x44422666));rotation*=65536.f;rotation/=lit(0x3FF1463B);
        const auto count=(r.u(0x2ac+wheel*4)+std::uint32_t(ftrc(rotation)))&65535;
        r.setu(0x2ac+wheel*4,count);float radians=float(count);radians*=lit(0x40490FDB);radians*=lit(0x38000000);pub.setf(96+wheel*4,radians);
    }
    if(std::abs(r.f(204)-path.points[index][1])>2.f||groundValid!=1){r.setf(204,path.points[index][1]);pub.setf(4,path.points[index][1]);}
}
std::array<float,3> queryOriginalRivalWheel(OriginalActorState& pub,unsigned corner,
    const std::array<float,3>& point,OriginalRivalRoadState& road,const OriginalCollisionData& collision){
    auto& q=road.surfaces0CAA9764.at(corner);
    for(unsigned k=0;k<3;++k){q.setu(44+k*4,q.u(32+k*4));q.setf(32+k*4,point[k]);}
    road.surfaceValid0CAA9864=queryOriginalCollisionSurface(collision,q,road.trace,road.surface)?1:0;
    pub.setByte(116+corner,std::uint8_t(q.u(28)));
    return {q.f(0)*1.f,q.f(24)*1.f,q.f(8)*1.f};
}
void finishOriginalRivalRoadContact(OriginalRivalState& r,OriginalActorState& pub,
    std::uint32_t car,const OriginalRivalData& data,OriginalRivalRoadState& road,
    const OriginalCollisionData& collision,const OriginalFscaTable& fsca){
    if(car>=35)throw std::invalid_argument("Original rival car geometry selection");
    std::array<float,11> geometry;for(unsigned i=0;i<11;++i)geometry[i]=data.scalar(0x0c2716a8+car*44+i*4);
    const auto matrix=originalActorMatrix({r.f(200),r.f(204),r.f(208)},{0.f,r.f(0xe4),0.f},fsca);
    const float localY=-geometry[8]-geometry[10];
    std::array<std::array<float,3>,4> wheel{{{geometry[0],localY,geometry[2]},
        {-geometry[0],localY,geometry[2]},{geometry[1],localY,-geometry[3]},
        {-geometry[1],localY,-geometry[3]}}};
    for(auto& point:wheel)point=transformOriginalPoint(matrix,point);
    std::array<std::array<float,3>,4> force;
    for(unsigned i=0;i<4;++i)force[i]=queryOriginalRivalWheel(pub,i,wheel[i],road,collision);
    for(unsigned i=0;i<4;++i){force[i][1]*=-1.f;force[i][1]+=wheel[i][1];}
    for(unsigned i=0;i<4;++i)for(unsigned k=0;k<3;++k)r.setf(0x27c+i*12+k*4,force[i][k]);
    for(unsigned i=0;i<4;++i)r.setf(0x220+i*12,std::clamp(r.f(0x220+i*12),-.5f,.5f));
    for(unsigned i=0;i<4;++i)for(unsigned k=0;k<3;++k)r.setf(0x24c+i*12+k*4,force[i][k]);
    r.setu(4,r.u(4)+1);
    for(unsigned i=0;i<4;++i){wheel[i][1]=force[i][1];pub.setf(124+i*4,force[i][1]+pub.f(64+i*4));}
    for(unsigned i=0;i<4;++i)for(unsigned k=0;k<3;++k)r.setf(0x19c+i*12+k*4,wheel[i][k]-pub.f(k*4));
    std::array<float,3> acceleration;for(unsigned k=0;k<3;++k){acceleration[k]=r.f(0x184+k*4)-r.f(0x178+k*4);r.setf(0x190+k*4,acceleration[k]);}
    //1F6B80 uses FIPR with a zero fourth lane: double products and one final
    //F32 rounding, matching the existing finite primary interpreter contract.
    for(unsigned i=0;i<4;++i){
        double dot=double(acceleration[0])*double(r.f(0x19c+i*12));
        dot+=double(acceleration[1])*double(r.f(0x1a0+i*12));dot+=double(acceleration[2])*double(r.f(0x1a4+i*12));dot+=0.0;
        float target=float(dot)*30.f;target=std::clamp(target,i<2?lit(0xBCA3D70A):lit(0xBDCCCCCD),lit(0x3DCCCCCD));
        float change=(target-r.f(0x1ec+i*4))*lit(0x3C23D70A);
        change=std::clamp(change,lit(0xBC03126F),lit(0x3C03126F));r.setf(0x1fc+i*4,change);
    }
    for(unsigned i=0;i<4;++i){
        const float value=r.f(0x1ec+i*4)+r.f(0x1fc+i*4);r.setf(0x1ec+i*4,value);
        r.setf(0x1fc+i*4,r.f(0x1fc+i*4)*.75f);
        r.setf(0x1dc+i*4,std::clamp(value,lit(0xBD8F5C29),lit(0x3D8F5C29)));
    }
    for(unsigned i=0;i<4;++i){const float value=r.f(0x1dc+i*4);pub.setf(64+i*4,value);r.setf(0x1cc+i*4,value/lit(0x3D8F5C29));}
    for(auto offset:{44,48,52,56})r.setf(offset,0.f);
    float mean=force[0][1]+force[1][1];mean+=force[2][1];mean+=force[3][1];mean*=.25f;
    r.setf(216,mean-r.f(204));r.setf(204,mean);
    float rear=force[2][1]+pub.f(72);rear+=force[3][1];rear+=pub.f(76);
    float front=force[0][1]+pub.f(64);front+=force[1][1];front+=pub.f(68);
    float pitch=(rear-front)*.5f;pitch/=geometry[2]+geometry[3];
    pitch=float(std::bit_cast<std::int32_t>(originalAsinAngle(pitch)));pitch*=lit(0x40490FDB);pitch*=lit(0x38000000);
    float delta=wrap(pitch-r.f(0xe0));delta*=.5f;
    float rate=std::fma(delta,.5f,r.f(0xec));r.setf(0xe0,wrap(r.f(0xe0)+rate));r.setf(0xec,rate*lit(0x3F68F5C3));
    float left=force[0][1]+pub.f(64);left+=force[2][1];left+=pub.f(72);
    float right=force[1][1]+pub.f(68);right+=force[3][1];right+=pub.f(76);
    float roll=(left-right)*.5f;roll/=geometry[0]+geometry[1];
    roll=float(std::bit_cast<std::int32_t>(originalAsinAngle(roll)));roll*=lit(0x40490FDB);roll*=lit(0x38000000);
    delta=wrap(roll-r.f(0xe8));delta*=.5f;rate=std::fma(delta,.5f,r.f(0xf4));
    // The original roll branch stores its new rate directly into E8, unlike
    // the pitch branch's addition to E0. Preserve that asymmetry.
    r.setf(0xe8,wrap(rate));r.setf(0xf4,rate*lit(0x3F68F5C3));
    for(unsigned k=0;k<3;++k){pub.setf(k*4,r.f(200+k*4));pub.setf(24+k*4,r.f(224+k*4));pub.setf(12+k*4,r.f(212+k*4));}
    pub.setf(60,r.f(188)+lit(0x40490FDB));
    //159B20 rejects every exponent255 value. Original diagnostic/host-break
    //services are replaced by a native error after the same state writes.
    for(auto offset:{200,204,208,212,216,220,476,480,484,488,492,496,500,504,508,512,516,520,400,404,408})
        if(!std::isfinite(r.f(offset)))throw std::runtime_error("Original rival invalid-scalar diagnostic");
}
bool updateOriginalRival(OriginalRivalState& r,OriginalActorState& pub,std::uint32_t& counter,
    const OriginalRivalData& data,const OriginalRivalPath& path,const OriginalRivalPaceInputs& inputs,
    const OriginalDriveState& player,const OriginalActorState& actor,std::uint32_t car,
    OriginalRivalRoadState& road,const OriginalCollisionData& collision,const OriginalFscaTable& fsca){
    if(!updateOriginalRivalPace(r,pub,counter,data,path,inputs,player,actor))return false;
    advanceOriginalRivalMotion(r,pub,path,inputs,road.surfaceValid0CAA9864);
    finishOriginalRivalRoadContact(r,pub,car,data,road,collision,fsca);return true;
}
} // namespace idas3::original
