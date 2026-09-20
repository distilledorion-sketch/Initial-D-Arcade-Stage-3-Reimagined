#include "original_dynamics.h"
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
template<std::uint32_t Bits> constexpr float literal() { return std::bit_cast<float>(Bits); }
// Original wrapping is ONE correction to the published copy, while the source
// angular accumulator remains unwrapped. Replacing this with fmod loses state.
float originalWrap(float angle) {
    if (std::abs(angle) > literal<0x40490FDB>()) {
        if (angle > 0.0f) angle += literal<0xC0C90FDB>();
        else angle += literal<0x40C90FDB>();
    }
    return angle;
}
// Actual SH-4 FMAC: fused rounding, verified against local Flycast primary
// core/hw/sh4/interpr/sh4_fpu.cpp. The older generated expression used a
// separate multiply/add and must not be treated as the rounding oracle.
float mac(float accumulator, float a, float b) { return std::fma(a, b, accumulator); }
}

float originalFiprDot3(const std::array<float,3>& a,const std::array<float,3>& b) {
    double sum=double(a[0])*double(b[0]);
    sum+=double(a[1])*double(b[1]);
    sum+=double(a[2])*double(b[2]);
    sum+=double(0.0f)*double(0.0f);
    return float(sum);
}

float prepareOriginalFrame(OriginalDriveState& s,const OriginalFrameParameters& p) {
    float progressModifier=0.0f;
    if(p.global0C9015E4!=0 && p.global0C9015D4==0){
        float bounded=p.global0C901650;
        if(literal<0xC2C80000>()>bounded)bounded=literal<0xC2C80000>(); // -100
        else if(bounded>0.0f)bounded=0.0f;
        progressModifier=-bounded;
        progressModifier*=literal<0x3E99999A>(); // .3
    }
    s.setu(0x43C,progressModifier==0.0f?0u:1u);
    float coefficient=p.table0C28451C;
    coefficient+=p.table0C28866C;
    coefficient+=progressModifier;
    coefficient+=p.global0CAA987C;
    s.setf(0x250,coefficient);
    s.setf(0x214,0.0f);s.setf(0x218,0.0f);
    return coefficient;
}

OriginalPreparationResult prepareOriginalDriveState(OriginalDriveState& s,
        OriginalPreparedControls& controls,float frameCoefficient,
        const OriginalRoadParameters& road,OriginalMath math) {
    if(!math.sinF32||!math.cosF32||!math.dot3F32)throw std::invalid_argument("Original math callbacks are required");
    if(road.path0C901728.size()<=road.lastIndex0C283E88 || road.lastIndex0C283E88<64)
        throw std::invalid_argument("Original inclusive course path and valid last index are required");
    OriginalPreparationResult out;
    // D124..D1E0: retain signed repeated-squaring powers, not ordinary x^n.
    if(s.u(0x1A8)!=0)controls.throttle=0.0f;
    s.setf(0x1CC,controls.steering);
    float power=controls.steering*controls.steering;
    for(const auto offset:{0x1D0,0x1D4,0x1D8,0x1DC,0x1E0}){
        s.setf(offset,controls.steering<0.0f?-1.0f*power:power);
        power*=power;
    }
    s.setf(0x1B8,controls.throttle);s.setf(0x1C4,controls.brake);
    s.setu(0x1B0,controls.brake>literal<0x3E4CCCCD>()?1u:0u);
    const auto positiveCounter=[](float v)->std::uint32_t{
        if(!(v>0.0f))return 0;
        if(v>=2147483648.0f)return 0x7fffffffu;
        return std::uint32_t(std::int32_t(v));
    };
    float brakeCounter=s.f(0x1C4)-literal<0x3F666666>();
    brakeCounter*=literal<0x437F0000>();brakeCounter*=s.f(0x248);
    s.setu(0x414,positiveCounter(brakeCounter));s.setu(0x418,positiveCounter(brakeCounter));
    float errorCounter=s.f(0x27C)-literal<0x3DCCCCCD>();
    if(errorCounter>0.0f)errorCounter*=literal<0x438D8000>();
    s.setu(0x41C,positiveCounter(errorCounter));s.setu(0x420,positiveCounter(errorCounter));
    s.setu(0x424,positiveCounter(brakeCounter));s.setu(0x428,positiveCounter(brakeCounter));
    errorCounter=s.f(0x27C)-literal<0x3E4CCCCD>();
    if(errorCounter>0.0f)errorCounter*=literal<0x439F0000>();
    s.setu(0x42C,positiveCounter(errorCounter));s.setu(0x430,positiveCounter(errorCounter));
    if(s.u(0x140)!=0)for(const auto offset:{0x424,0x428,0x42C,0x430})s.setu(offset,255);

    // D392..D408: +248 is NORMALIZED SPEED, not wheel slip or a drift weight.
    s.setf(0x1E8,s.f(0x1CC)-s.f(0x1EC));s.setf(0x1EC,s.f(0x1CC));
    const float speedDenominator=literal<0x3E8E5604>()*frameCoefficient; // .278
    s.setf(0x248,s.f(0x238)/speedDenominator);
    const float speedDelta=s.f(0x238)-s.f(0x23C);
    s.setf(0x234,speedDelta);s.setf(0x23C,s.f(0x238));
    float scaledDelta=literal<0x41200000>()*speedDelta;
    if(literal<0xBF000000>()>scaledDelta)scaledDelta=literal<0xBF000000>();
    else if(scaledDelta>literal<0x3F000000>())scaledDelta=literal<0x3F000000>();
    scaledDelta+=scaledDelta;s.setf(0x24C,scaledDelta);

    // D408..D57A: steering-heading vectors against original contact vectors.
    const float correctedHeading=originalWrap(mac(s.f(0x010),s.f(0x1D0),literal<0x3F060A92>()));
    const auto readVector=[&](std::size_t off){return std::array<float,3>{s.f(off),s.f(off+4),s.f(off+8)};};
    const auto writeVector=[&](std::size_t off,const std::array<float,3>& v){for(std::size_t i=0;i<3;++i)s.setf(off+4*i,v[i]);};
    const std::array<float,3> steeringDirection{math.sinF32(correctedHeading),0.0f,math.cosF32(correctedHeading)};
    const std::array<float,3> headingDirection{math.sinF32(s.f(0x010)),0.0f,math.cosF32(s.f(0x010))};
    writeVector(0x308,steeringDirection);writeVector(0x314,steeringDirection);
    writeVector(0x320,headingDirection);writeVector(0x32C,headingDirection);
    for(const auto off:{0x2DC,0x2E8,0x2F4,0x300})s.setf(off,literal<0x2EDBE6FF>()); // 1e-10
    const auto normalizeOriginal=[&](std::size_t offset){
        auto v=readVector(offset);const float squared=math.dot3F32(v,v);
        // Original FSRRA PR0 finite-positive contract: 1.f / sqrtf(x).
        const float reciprocalLength=1.0f/std::sqrt(squared);
        v[2]*=reciprocalLength;v[1]*=reciprocalLength;v[0]*=reciprocalLength;
        writeVector(offset,v);
    };
    for(const auto off:{0x308,0x314,0x320,0x32C,0x2D8,0x2E4,0x2F0,0x2FC})normalizeOriginal(off);
    s.setf(0x348,math.dot3F32(readVector(0x308),readVector(0x2D8)));
    s.setf(0x34C,math.dot3F32(readVector(0x314),readVector(0x2E4)));
    s.setf(0x350,math.dot3F32(readVector(0x320),readVector(0x2F0)));
    s.setf(0x354,math.dot3F32(readVector(0x32C),readVector(0x2FC)));
    s.setf(0x3DC,0.0f>s.f(0x348)?0.0f:s.f(0x348));
    s.setf(0x3E0,0.0f>s.f(0x350)?0.0f:s.f(0x350));

    // D57A..D62C: four brake/contact counters, saturating after wraparound.
    if(s.f(0x248)>literal<0x3DCCCCCD>()&&s.f(0x1C4)>literal<0x3E99999A>())
        for(const auto off:{0x174,0x178,0x17C,0x180})s.setu(off,s.u(off)+3u);
    for(const auto off:{0x174,0x178,0x17C,0x180}){
        const auto count=std::bit_cast<std::int32_t>(s.u(off)-2u);
        s.setu(off,count<0?0u:count>255?255u:std::uint32_t(count));
    }
    // The comparison at D638 is overwritten before use in original code.
    const bool negativeRate=s.f(0x0E0)<0.0f,positiveRate=s.f(0x0E0)>0.0f;
    float contactA=s.f(negativeRate?0x3B4:0x3B8),contactB=s.f(negativeRate?0x3BC:0x3C0);
    contactA+=literal<0x3D8F5C29>();contactA/=literal<0x3E0F5C29>();
    contactB+=literal<0x3D8F5C29>();contactB/=literal<0x3E0F5C29>();
    s.setf(0x07C,contactA);s.setf(0x080,contactB);
    s.setu(0x084,s.u(positiveRate?0x164:0x168));s.setu(0x088,s.u(positiveRate?0x16C:0x170));
    s.setu(0x08C,s.u(negativeRate?0x164:0x168));s.setu(0x090,s.u(negativeRate?0x16C:0x170));
    s.setu(0x148,0);
    if(road.condition0C9015CC-6u<=1u && ((s.u(0x08C)&0xF0u)==0x20u||(s.u(0x090)&0xF0u)==0x20u))s.setu(0x148,1);
    s.setu(0x140,(s.u(0x08C)&0xFFu)==24u?1u:0u);
    if(s.u(0x434)!=0||road.mode0C9015E0==24||road.mode0C9015E0==25||road.mode0C9015E0==31)s.setu(0x140,0);
    if(s.u(0x140)==0 && s.u(0x144)!=0)s.setu(0x144,s.u(0x144)+1u);
    if(s.u(0x140)!=0)s.setu(0x144,1);
    if(s.u(0x144)-2u<=117u && std::abs(s.f(0x258))+std::abs(s.f(0x25C))>literal<0x3C23D70A>())out.setServiceFlag0C91FB40=true;
    if(s.f(0x248)==0.0f)s.setf(0x0DC,0.0f);
    if(std::abs(s.f(0x0DC))>literal<0x3F490FDB>())s.setf(0x0DC,s.f(0x0DC)<0.0f?literal<0xBF490FDB>():literal<0x3F490FDB>());

    // D86E..D922: preserve the 128-candidate window and inclusive endpoint.
    std::uint32_t nearest=s.u(0x118);
    std::int32_t candidate=std::bit_cast<std::int32_t>(nearest-64u);
    if(candidate<0)candidate+=std::int32_t(road.lastIndex0C283E88);
    float bestSquared=literal<0x4CBEBC20>(); // 100000000
    for(int count=0;count<128;++count,++candidate){
        if(candidate>std::int32_t(road.lastIndex0C283E88))candidate=0;
        if(candidate<0)throw std::invalid_argument("Original path-search state is outside the valid index range");
        const auto& point=road.path0C901728[std::size_t(candidate)];
        const float dx=point[0]-s.f(0x000),dz=point[2]-s.f(0x008);
        const float squared=mac(dz*dz,dx,dx);
        if(bestSquared>squared){bestSquared=squared;nearest=std::uint32_t(candidate);}
    }
    s.setu(0x118,nearest==road.lastIndex0C283E88?0u:nearest);
    return out;
}

float computeOriginalMotionScale(const OriginalDriveState& s, const OriginalAngularParameters& p) {
    // 0C15D91C..0C15D9E6. s22C/s230 are original vector components, not
    // substitute wheel-slip estimates. The denominator uses a 44-byte car record.
    float motion = s.f(0x230) * s.f(0x230);
    motion = mac(motion, s.f(0x22C), s.f(0x22C));
    float scaleA = 1.0f - literal<0x3F333333>() * s.f(0x248); // 0.7
    scaleA *= scaleA;
    float scaleB = 1.0f - literal<0x3F7D70A4>() * s.f(0x248); // 0.99
    float scaleC = mac(1.0f, 1.0f - s.f(0x1BC), literal<0x3E4CCCCD>()); // 0.2
    motion = std::sqrt(motion);
    motion *= scaleA;
    motion *= scaleB;
    motion *= scaleC;
    motion /= p.carRecord0C283F18_08 + p.carRecord0C283F18_0C;
    return motion;
}

void updateOriginalSteeringMemory(OriginalDriveState& s,
        const OriginalSteeringMemoryParameters& p) {
    // 0C15D9EA..DA20: real downshift-edge contribution, separate from the
    // transmission's gear selection. This state must survive between frames.
    if (p.shiftDownPressed) {
        float contribution = s.f(0x248) * literal<0x42340000>(); // 45
        contribution *= s.f(0x0DC);
        contribution *= p.table0C2864D4;
        s.setf(0x288, mac(s.f(0x288), contribution, p.global0C90094C));
    }
    if ((p.mask0C900EBC & p.mask0CAA9CF0) == 0) {
        float contribution = s.f(0x248) * literal<0x41A00000>(); // 20
        contribution *= s.f(0x0DC);
        contribution *= p.table0C2864D4;
        s.setf(0x288, mac(s.f(0x288), contribution, p.global0C90094C));
    }
    if (s.u(0x15C) != 0 || s.u(0x150) != 0) {
        float contribution = s.f(0x248) * literal<0x3DCCCCCD>(); // .1
        contribution *= s.f(0x0DC);
        contribution *= p.table0C2864D4;
        s.setf(0x288, mac(s.f(0x288), contribution, p.global0C90094C));
    }
    if (s.f(0x274) * s.f(0x278) < 0.0f) {
        float contribution = s.f(0x248) * s.f(0x0DC);
        contribution *= p.table0C286EAC;
        contribution *= literal<0x425C0000>(); // 55
        s.setf(0x288, mac(s.f(0x288), contribution, p.global0C900E30));
    }

    // 0C15DB10..DC14: move only a limited signed portion from +288 to +284.
    const float memory = s.f(0x288);
    if (memory != 0.0f) {
        float transfer = memory;
        if (memory > 0.0f) {
            float positiveLimit = p.table0C287884 * literal<0x3F000000>();
            positiveLimit *= p.global0C900EF8;
            if (memory > positiveLimit) transfer = positiveLimit;
        } else {
            float negativeLimit = -p.table0C287884;
            negativeLimit *= literal<0x3F000000>();
            negativeLimit *= p.global0C900EF8;
            if (negativeLimit > memory) transfer = negativeLimit;
        }
        s.setf(0x288, memory - transfer);
        s.setf(0x284, s.f(0x284) + transfer);
    }

    // 0C15DC14..DCFE: brake and throttle-lift pathways. State+114==3 has
    // its own original two-term equation; it is not a chosen new drift mode.
    float brakeContribution = s.f(0x1C4) * s.f(0x248);
    brakeContribution *= s.f(0x0DC);
    brakeContribution *= p.table0C2864D4;
    s.setf(0x288, mac(s.f(0x288), brakeContribution, p.global0C90094C));
    if (s.f(0x248) > literal<0x3DCCCCCD>()) {
        if (s.u(0x114) == 3) {
            float lift = 1.0f - s.f(0x1B8);
            lift *= s.f(0x248);
            lift *= s.f(0x0DC);
            lift *= p.table0C2869C0;
            lift *= literal<0x3F000000>(); // .5
            float baseline = s.f(0x248) * s.f(0x0DC);
            baseline *= p.table0C2869C0;
            float memoryValue = mac(s.f(0x288), lift, p.global0C900E4C);
            baseline *= literal<0x3E4CCCCD>(); // .2
            memoryValue = mac(memoryValue, baseline, p.global0C900E4C);
            s.setf(0x288, memoryValue);
        } else {
            float lift = s.f(0x248) * (1.0f - s.f(0x1B8));
            lift *= s.f(0x0DC);
            lift *= p.table0C2869C0;
            s.setf(0x288, mac(s.f(0x288), lift, p.global0C900E4C));
        }
    }
    if (literal<0x3DCCCCCD>() > s.f(0x27C)) s.setf(0x284, s.f(0x284) * literal<0x3F4CCCCD>()); // .8
    float steeringMemory = s.f(0x284);
    if (literal<0xC28C0000>() > steeringMemory) steeringMemory = literal<0xC28C0000>();
    else if (steeringMemory > literal<0x428C0000>()) steeringMemory = literal<0x428C0000>();
    s.setf(0x284, steeringMemory); // [-70,+70]

    // 0C15DDA4..DEA8. Preserve the original upper/lower decision ordering,
    // including the case where parameter signs invert the apparent bounds.
    float upper = p.table0C287D70 * s.f(0x248);
    upper *= p.global0C900EB0;
    float lower = p.table0C287D70 * -s.f(0x248);
    lower *= p.global0C900EB0;
    float finalMemory = s.f(0x288);
    if ((lower > finalMemory && lower > upper) || finalMemory > upper) finalMemory = upper;
    else if (lower > finalMemory) finalMemory = lower;
    s.setf(0x288, finalMemory);
}

OriginalAngularResult updateOriginalAngular(OriginalDriveState& s,
        const OriginalAngularParameters& p, OriginalMath math, float motion) {
    if (!math.sinF32 || !math.cosF32 || !math.dot3F32) throw std::invalid_argument("Original math callbacks are required");
    OriginalAngularResult out;
    out.motionScale = motion;

    // 0C15DEA6..DEDE: steering opposing the accumulated correction.
    s.setf(0x28C, 0.0f);
    if (s.f(0x284) < 0.0f && s.f(0x1CC) > 0.0f) s.setf(0x28C, std::abs(s.f(0x1CC)));
    if (s.f(0x284) > 0.0f && s.f(0x1CC) < 0.0f) s.setf(0x28C, std::abs(s.f(0x1CC)));

    // Original uses OLD normalized direction error for these three factors.
    float oldErrorQuarter = s.f(0x27C) * literal<0x3F000000>();
    oldErrorQuarter *= literal<0x3F000000>();
    float secondaryGain = 1.0f - oldErrorQuarter; // live fr14
    const float primaryGain = oldErrorQuarter + 1.0f; // live fr7
    float oldErrorTerm = literal<0x3ECCCCCD>() * s.f(0x27C);
    oldErrorTerm = 0.0f * oldErrorTerm; // present in original; do not invent a nonzero coefficient
    const float sinusoidalGain = 1.0f - oldErrorTerm; // live fr13

    // 0C15DEDE..DFC8: drain only a clamped part of +284, but use the
    // UNCLAMPED quotient in steering correction, exactly as the original does.
    float correction = s.f(0x284) / literal<0x425C0000>(); // /55
    float correctionDrain = correction;
    if (literal<0xBF860A92>() > correctionDrain) correctionDrain = literal<0xBF860A92>();
    else if (correctionDrain > literal<0x3F860A92>()) correctionDrain = literal<0x3F860A92>();
    s.setf(0x284, s.f(0x284) - correctionDrain);
    const float a = s.f(0x1BC), b = s.f(0x1C4), c = s.f(0x248);
    if ((c > b && c > a) || b > a) correction *= c > b ? c : b;
    else correction *= a;
    out.steeringCorrection = correction;

    // 0C15DFC8..E06E: PRIMARY angle. Gain tables use a 140-byte condition
    // row plus vehicleIndex*4. No steering ratio, bicycle or force fit is added.
    float angle = s.f(0x1CC) * literal<0x3F060A92>();
    const float combined = angle + correction;
    if (literal<0xBF060A92>() > combined) angle = literal<0xBF060A92>();
    else if (combined > literal<0x3F060A92>()) angle = literal<0x3F060A92>();
    else angle += correction;
    angle *= motion;
    angle *= primaryGain;
    angle *= p.table0C285124;
    angle *= p.global0C900E40;
    s.setf(0x0D8, angle);
    float primaryRate = s.f(0x0DC) + angle;
    s.setf(0x0DC, primaryRate);
    const float primaryHeading = s.f(0x110) + primaryRate;
    s.setf(0x110, primaryHeading);
    s.setf(0x010, originalWrap(primaryHeading));
    out.primaryContribution = angle;

    // 0C15E06E..E0E0. Retention is applied AFTER heading integration.
    float retention = s.f(0x248) * s.f(0x248);
    retention *= p.table0C285AFC;
    retention *= p.global0C900EC0;
    if (!(literal<0x3F570A3D>() > retention)) retention = literal<0x3F570A3D>(); // cap .84
    primaryRate *= retention;
    s.setf(0x0DC, primaryRate);
    if (s.f(0x0D8) * primaryRate < 0.0f) {
        primaryRate *= 1.0f - s.f(0x27C);
        s.setf(0x0DC, primaryRate);
    }

    // 0C15E0E0..E184: SECONDARY heading error/history and normalized magnitude.
    float directionError = mac(s.f(0x110), s.f(0x1CC), literal<0x3F060A92>());
    directionError -= s.f(0x10C);
    s.setf(0x278, s.f(0x274));
    s.setf(0x274, directionError);
    const float absoluteError = std::abs(directionError);
    s.setf(0x27C, absoluteError > literal<0x3FC90FDB>() ? 1.0f : absoluteError / literal<0x3FC90FDB>());
    out.directionError = directionError;

    // 0C15E184..E2E8: second angular channel, including original mode factors.
    float sine = math.sinF32(s.f(0x27C) * literal<0x40490FDB>());
    sine *= literal<0x3F400000>(); // 0.75
    float secondaryContribution = mac(1.0f, sine, sinusoidalGain);
    float squared = s.f(0x248) * s.f(0x248);
    squared += squared;
    const float speedFactor = literal<0x40300000>() - squared; // 2.75 - 2*s248^2
    secondaryContribution *= directionError;
    secondaryContribution *= speedFactor;
    secondaryContribution *= p.table0C285610;
    secondaryContribution *= secondaryGain;
    secondaryContribution *= p.global0C8FF380;
    s.setf(0x0E4, secondaryContribution);
    if (s.u(0x434) != 0) {
        secondaryContribution *= s.u(0x114) == 3 ? literal<0x3F59999A>() : literal<0x3F4CCCCD>();
        s.setf(0x0E4, secondaryContribution);
    }
    if (s.u(0x148) != 0) {
        secondaryContribution *= p.table0C287398;
        s.setf(0x0E4, secondaryContribution);
    }
    if (s.u(0x140) != 0) {
        secondaryContribution *= literal<0x3ECCCCCD>(); // 0.4
        s.setf(0x0E4, secondaryContribution);
    }
    float secondaryRate = s.f(0x0E0) + secondaryContribution;
    s.setf(0x0E0, secondaryRate);
    const float secondaryHeading = s.f(0x10C) + secondaryRate;
    s.setf(0x10C, secondaryHeading);
    s.setf(0x108, originalWrap(secondaryHeading));
    float secondaryRetention = p.table0C285FE8 * p.global0C900E54;
    secondaryRate *= secondaryRetention;
    s.setf(0x0E0, secondaryRate);

    // 0C15E32E emits a non-force service request; expose it to the caller.
    out.feedbackArgument = s.u(0x434) + s.u(0x438) + s.u(0x438);
    out.feedbackStrength = s.f(0x27C);
    out.feedbackSpeed = s.f(0x238) * 3.6f;
    // Original 0C1F6B80 constructs two vec4s (fourth lane zero) and uses FIPR.
    // Keep its rounding contract explicit instead of inventing scalar rounding.
    const float directionDot = math.dot3F32(
        {math.sinF32(s.f(0x108)),0.0f,math.cosF32(s.f(0x108))},
        {s.f(0x260),0.0f,s.f(0x264)});
    s.setf(0x160, directionDot);
    return out;
}

void updateOriginalLongitudinalLoss(OriginalDriveState& s, OriginalLossState& loss,
        const OriginalLossParameters& p) {
    // 0C15E370..E452: sustained contact/angle state feeds persistent loss.
    if (s.f(0x248) > literal<0x3DCCCCCD>()) { // 0.1
        float factor = 1.0f - s.f(0x214);
        if (!(factor > literal<0x3F000000>())) factor = literal<0x3F000000>();
        float magnitude = std::abs(s.f(0x258));
        magnitude += std::abs(s.f(0x25C));
        magnitude += magnitude;
        if (!(p.condition0C9015CC > 1u)) factor *= literal<0x3E19999A>(); // 0.15
        const float coupling = factor * magnitude;
        loss.speedLoss0CAA9880 = mac(loss.speedLoss0CAA9880, coupling, literal<0x3E4CCCCD>());
        float persistent = mac(loss.persistentPenalty0CAA9884, coupling, p.growth0C284F84);
        if (!(p.cap0C284F80 > persistent)) persistent = p.cap0C284F80;
        if (!(p.condition0C9015CC > 1u) && !(literal<0x3F99999A>() > persistent)) persistent = literal<0x3F99999A>();
        loss.persistentPenalty0CAA9884 = persistent;
        s.setf(0x1F4, persistent / literal<0x41200000>()); // /10
    }

    // 0C15E452..E4E6: these are actual original per-frame loss equations.
    const float state248 = s.f(0x248);
    const float brake = p.normalizedBrake0CAA98A0 * (1.0f - state248);
    loss.speedLoss0CAA9880 = mac(loss.speedLoss0CAA9880, brake, literal<0x3E6B851F>()); // .23
    loss.speedLoss0CAA9880 = mac(loss.speedLoss0CAA9880, state248, literal<0x3A83126F>()); // .001
    if (s.u(0x1A8) != 0) {
        loss.speedLoss0CAA9880 += literal<0x3EB851EC>(); // .36
        if (state248 == 0.0f) s.setu(0x1AC, 1);
    }
    if (s.u(0x43C) != 0) loss.persistentPenalty0CAA9884 = 0.0f;
    if (state248 > literal<0x3F000000>()) loss.speedLoss0CAA9880 += loss.persistentPenalty0CAA9884 / literal<0x442F0000>();
    const float decay = loss.persistentPenalty0CAA9884 / literal<0x442F0000>(); // /700
    loss.persistentPenalty0CAA9884 -= decay;
}

void finishOriginalDriveState(OriginalDriveState& s,OriginalLossState& loss,
        OriginalTailState& h,const OriginalTailInputs& in,OriginalMath math) {
    if(!math.sinF32||!math.cosF32)throw std::invalid_argument("Original math callbacks are required");
    const auto throttleCount=s.u(0x1C0);
    if(throttleCount==0||throttleCount>h.throttleHistory0CAA99E0.size())
        throw std::invalid_argument("Original throttle history length must be in the backing record");
    // EA42..EA8A: preserve the original integer-speed cadence flag.
    std::uint32_t cadence=(literal<0x3DCCCCCD>()>s.f(0x1B8)&&in.transmissionFiltered18>literal<0x459C4000>())?1u:0u;
    if(cadence!=0){
        const float speed=s.f(0x238);
        std::uint32_t truncated;
        if(std::isnan(speed)||speed<=-2147483648.0f)truncated=0x80000000u;
        else if(speed>=2147483648.0f)truncated=0x7fffffffu;
        else truncated=std::uint32_t(std::int32_t(speed));
        if((truncated&3u)!=0)cadence=0;
    }
    s.setu(0x1B4,cadence);
    loss.speedLoss0CAA9880=0.0f;

    // Original history updates average in ascending storage order, not by
    // subtracting an old sample from a rolling total (which changes rounding).
    ++h.counter0CAA9CE4;
    h.history0CAA98E0[h.counter0CAA9CE4&63u]=in.transmissionDelta24;
    float sum=0.0f;for(const auto v:h.history0CAA98E0)sum+=v;
    h.mean0CAA9CE0=literal<0x3C800000>()*sum;
    ++h.counter0CAA9CE8;
    h.throttleHistory0CAA99E0[h.counter0CAA9CE8&(throttleCount-1u)]=s.f(0x1B8);
    sum=0.0f;for(std::uint32_t i=0;i<throttleCount;++i)sum+=h.throttleHistory0CAA99E0[i];
    s.setf(0x1BC,sum/float(std::bit_cast<std::int32_t>(throttleCount)));
    ++h.counter0CAA9CEC;
    h.steeringHistory0CAA9BE0[h.counter0CAA9CEC&63u]=s.f(0x1CC);
    sum=0.0f;for(const auto v:h.steeringHistory0CAA9BE0)sum+=v;
    s.setf(0x1E4,literal<0x3C800000>()*sum);
    if(0.0f>s.f(0x238))s.setf(0x238,0.0f);
    h.filteredDelta0CAA98B0=in.transmissionFiltered18-in.priorFiltered0CAA98AC;
    h.speedDelta0CAA9878=s.f(0x238)-h.previousSpeed0CAA9874;
    h.previousSpeed0CAA9874=s.f(0x238);

    // EB64..EC0A: original headings point opposite the host's forward vector.
    if(s.f(0x238)==0.0f){s.setf(0x22C,0.0f);s.setf(0x230,0.0f);}
    else{
        float dx=math.sinF32(s.f(0x108))*s.f(0x238);dx/=literal<0x42700000>();
        s.setf(0x22C,-dx);
        float dz=math.cosF32(s.f(0x108))*s.f(0x238);dz/=literal<0x42700000>();
        s.setf(0x230,-dz);
    }
    for(std::size_t axis=0;axis<3;++axis)s.setu(0x024+4*axis,s.u(0x030+4*axis));
    s.setf(0x030,s.f(0x22C));s.setf(0x034,0.0f);s.setf(0x038,s.f(0x230));
    if(!in.gearEnabled){s.setf(0x260,0.0f);s.setf(0x264,0.0f);}
    float dx=s.f(0x22C)+s.f(0x258);dx+=s.f(0x260);s.setf(0x000,s.f(0x000)+dx);
    float dz=s.f(0x230)+s.f(0x25C);dz+=s.f(0x264);s.setf(0x008,s.f(0x008)+dz);
    if(in.gear!=0)h.lastNonzeroGear0CAA9888=in.gear;
    h.previousGear0CAA988C=in.gear;

    // Complete called helper0C15ECE0: drive statistics, no omitted service.
    const float kph=s.f(0x238)*literal<0x40666666>(); // 3.6
    if(!(s.f(0x3E4)>kph))s.setf(0x3E4,kph);
    if(s.u(0x1A8)==0&&in.elapsedFrames0C900E84>360){
        s.setu(0x3F0,s.u(0x3F0)+1u);
        s.setf(0x3E8,s.f(0x3E8)+s.f(0x1B8));
        s.setf(0x3EC,s.f(0x3EC)+s.f(0x1C4));
        const float change=s.f(0x1CC)-s.f(0x40C);
        if(std::abs(change)>s.f(0x408))s.setf(0x408,std::abs(change));
    }
    s.setf(0x40C,s.f(0x1CC));
    const auto writeStatistic=[&](std::size_t offset,float value){h.statistics0C91FB0C[offset/4]=std::bit_cast<std::uint32_t>(value);};
    writeStatistic(0,s.f(0x3E4));
    const float count=float(std::bit_cast<std::int32_t>(s.u(0x3F0)));
    writeStatistic(4,s.f(0x3E8)/count);writeStatistic(8,s.f(0x3EC)/count);
    h.statistics0C91FB0C[4]=s.u(0x3F4);h.statistics0C91FB0C[8]=s.u(0x3F8);
    h.statistics0C91FB0C[12]=s.u(0x404);
    h.statistics0C91FB0C[11]=std::bit_cast<std::int32_t>(s.u(0x3FC))>std::bit_cast<std::int32_t>(s.u(0x3F8))?s.u(0x3F8):s.u(0x3FC);
    h.statistics0C91FB0C[14]=s.u(0x400);writeStatistic(60,s.f(0x408));
}

} // namespace idas3::original
