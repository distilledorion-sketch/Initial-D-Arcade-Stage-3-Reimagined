#include "original_transmission.h"
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace idas3 {
namespace {
constexpr float f(std::uint32_t bits) { return std::bit_cast<float>(bits); }
constexpr std::int32_t signedWord(std::uint32_t word) { return std::bit_cast<std::int32_t>(word); }
std::uint32_t read32(std::span<const std::byte> bytes,std::size_t offset) {
    return std::uint32_t(std::to_integer<unsigned char>(bytes[offset]))
        | (std::uint32_t(std::to_integer<unsigned char>(bytes[offset+1]))<<8)
        | (std::uint32_t(std::to_integer<unsigned char>(bytes[offset+2]))<<16)
        | (std::uint32_t(std::to_integer<unsigned char>(bytes[offset+3]))<<24);
}
std::uint32_t ftrc(float value) {
    // Saturating SH-4 FTRC result, not an undefined out-of-range C++ cast.
    if(std::isnan(value)||value<=-2147483648.0f)return 0x80000000u;
    if(value>=2147483648.0f)return 0x7fffffffu;
    return std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(value));
}
}

float OriginalTransmissionProfile::atByteOffset(std::size_t offset) const {
    if(offset%4||offset>=88)throw std::out_of_range("Original transmission profile offset");
    return f(words[offset/4]);
}
OriginalTransmissionProfile decodeOriginalTransmissionProfile(std::span<const std::byte> record) {
    if(record.size()!=88)throw std::invalid_argument("Original transmission profile must be exactly 88 bytes");
    OriginalTransmissionProfile result;
    for(std::size_t i=0;i<result.words.size();++i)result.words[i]=read32(record,i*4);
    return result;
}
OriginalPowertrainRow decodeOriginalPowertrainRow(std::span<const std::byte> record) {
    if(record.size()!=24)throw std::invalid_argument("Original powertrain row must be exactly 24 bytes");
    return {read32(record,0),read32(record,4),f(read32(record,8)),f(read32(record,12)),f(read32(record,16)),f(read32(record,20))};
}
OriginalTransmissionParameters selectOriginalTransmissionParameters(
    const OriginalPowertrainRow& ordinary,const OriginalPowertrainRow& overrideRow,bool overrideMode) {
    // 0C15E4EE..0C15E52E. Override row's order in source is split; the
    // adapter must assemble profile/max from table+348/+34C and floats+350.
    const auto& row=overrideMode?overrideRow:ordinary;
    float base=row.base08;
    if(!overrideMode)base-=f(0x43fa0000u); // literal 0C15E5A4 = 500
    return {row.profileIndex,row.maximumGear,base,row.lower0c,row.upper10,row.divisor14};
}

void decideOriginalTransmissionGear(
    OriginalTransmissionState& s,OriginalTransmissionDrive& d,
    OriginalTransmissionGlobals& g,const OriginalTransmissionInputs& in,
    const OriginalTransmissionParameters& p,const OriginalTransmissionProfile& profile) {
    // Guard the typed adapter's valid record domain; do not turn an invalid
    // profile/table request into guessed gear data or an out-of-bounds access.
    if(p.maximumGear<1||p.maximumGear>6||s.gear00>6)
        throw std::invalid_argument("Original transmission gear outside captured table domain");
    const auto gearValue=[&](std::uint32_t gear){return profile.atByteOffset(4+4*gear);};

    // 0C15E52E..0C15E614: snapshot then automatic threshold scan. The
    // comparison is against drive+248, not longitudinal velocity+238.
    s.snapshot04=s.gear00;
    if(in.automaticMode) {
        std::uint32_t candidate=1;
        while(signedWord(candidate)<=signedWord(p.maximumGear)) {
            float threshold=gearValue(candidate);
            threshold*=profile.atByteOffset(0);
            if(threshold>d.selection248)break;
            ++candidate;
        }
        if(signedWord(candidate)>signedWord(p.maximumGear))candidate=p.maximumGear;
        if(signedWord(s.gear00)>signedWord(candidate)) {
            float threshold=gearValue(candidate);
            threshold*=profile.atByteOffset(0);
            threshold*=f(0x3f733333u); // literal 0C15E750 = 0.95
            if(d.selection248>threshold)candidate=s.gear00;
        }
        s.gear00=candidate;
    } else {
        // 0C15E620..0C15E682. Raw byte already contains press edges.
        const std::uint32_t request=(in.pressedByte>>4)&3u;
        if(request==1) {
            s.gear00-=1u;
            if(signedWord(s.gear00)<0)s.gear00=1;
        }
        if(request==2) {
            s.gear00+=1u;
            if(signedWord(s.gear00)>signedWord(p.maximumGear))s.gear00=p.maximumGear;
            if(s.gear00==p.maximumGear)d.field400=1;
            if(!(p.upper>s.filtered18))g.flag91fb4c=1;
        }
        if(s.gear00==0)s.gear00=1;
    }

    // 0C15E682..0C15E6CC: the SAME automatic-mode flag enables this
    // original 60-tick downshift hold, including a combined 0x30 edge byte.
    if(in.automaticMode) {
        g.flag91fb4c=1;
        if(in.pressedByte&0x10u)g.downCounter9cfc=60;
        if(g.downCounter9cfc!=0) {
            auto gear=s.gear00-1u;
            if(signedWord(gear)<1)gear=1;
            if(signedWord(gear)>6)gear=6;
            s.gear00=gear;
            --g.downCounter9cfc;
        }
    }
    // 0C15E6CC..0C15E6F6. This last gate may deliberately produce gear 0.
    if(!in.gearEnabled)s.gear00=0;
    s.snapshot08=s.gear00;
    if(0.0f>s.filtered18)s.filtered18=0.0f;
}

void stepOriginalTransmission(
    OriginalTransmissionState& s,OriginalTransmissionDrive& d,
    OriginalTransmissionGlobals& g,const OriginalTransmissionInputs& in,
    const OriginalTransmissionParameters& p,const OriginalTransmissionProfile& profile,
    OriginalTransmissionSine sine,void* sineContext) {
    decideOriginalTransmissionGear(s,d,g,in,p,profile);
    const float base=p.workingBase;
    const auto gearValue=[&](std::uint32_t gear){return profile.atByteOffset(4+4*gear);};
    const auto risingDivisor=[&](std::uint32_t gear){return profile.atByteOffset(32+4*gear);};
    const auto fallingDivisor=[&](std::uint32_t gear){return profile.atByteOffset(60+4*gear);};

    // 0C15E6F6..0C15E790: transition snapshots. Keep multiplication order.
    if(g.previousGear988c!=s.gear00&&s.gear00!=0) {
        float denominator=f(0x3e8e5604u); // literal 0C15E770 = original .278
        denominator*=in.coefficientFr15;
        float scaled=gearValue(s.gear00);
        scaled*=denominator;
        float coupling=d.velocity238;
        coupling/=scaled;
        g.coupling98d0=coupling;
        float desired=base;
        desired*=coupling;
        float difference=s.filtered18;
        difference-=desired;
        // Preserve original compare/select behavior, including unordered input.
        if(0.0f>difference)difference=0.0f;
        else {difference=s.filtered18;difference-=desired;}
        g.shiftDifference98a8=difference;
        if(g.shiftDifference98a8>base)g.shiftDifference98a8=base;
    }

    // 0C15E790..0C15E7AE: conditional cancellation of accumulated loss.
    if(d.field12c==1&&d.field130==1) {g.loss9880=0.0f;d.field220=0.0f;}
    d.velocity238-=g.loss9880;
    if(s.gear00!=0) {
        float denominator=f(0x3e8e5604u);
        denominator*=in.coefficientFr15;
        float scaled=gearValue(s.gear00);
        scaled*=denominator;
        float coupling=d.velocity238;
        coupling/=scaled;
        g.coupling98d0=coupling;
    } else {
        float coupling=s.filtered18;
        coupling/=base;
        g.coupling98d0=coupling;
    }

    // 0C15E7F2..0C15E856: engine-like target and asymmetric convergence.
    float coupled=g.coupling98d0;
    coupled*=base;
    s.filtered18=coupled;
    g.coupledSnapshot98ac=coupled;
    float targetSpan=base;
    targetSpan-=f(0x44480000u); // 800, 0C15E944
    targetSpan-=f(0x43fa0000u); // 500, 0C15E948
    float target=std::fma(g.throttle9898,targetSpan,f(0x44480000u));
    s.target14=target;
    s.rangeFlag0c=0;
    float rise=risingDivisor(s.gear00);
    float fall=fallingDivisor(s.gear00);
    if(s.filtered18>p.lower&&p.upper>s.filtered18) {
        s.rangeFlag0c=1;
        rise=risingDivisor(s.gear00);
        rise/=p.divisor;
    }
    // 0C15E856..0C15E888. The extracted generated function was missing
    // E884 F476 and E886 F410; the exact binary proves BOTH loads + addition.
    if(f(0x455ac000u)>s.filtered18&&s.gear00!=1) { // 3500
        const float originalFirst=risingDivisor(s.gear00);
        rise=risingDivisor(s.gear00);
        rise+=originalFirst;
    }

    // 0C15E88A..0C15E8FA: filtered engine state feeds back into velocity.
    float difference=s.target14;
    difference-=s.filtered18;
    if(difference>0.0f)difference/=rise;
    else difference/=fall;
    s.filtered18+=difference;
    float normalized=s.filtered18;
    normalized/=base;
    float normalizedChange=normalized;
    normalizedChange-=g.coupling98d0;
    s.delta24=normalizedChange;
    g.coupling98d0=normalized;
    if(s.gear00!=0) {
        float velocityTarget=gearValue(s.gear00);
        velocityTarget*=normalized;
        float scaledCoefficient=in.coefficientFr15;
        scaledCoefficient*=f(0x3e8e5604u);
        velocityTarget*=scaledCoefficient;
        velocityTarget-=d.velocity238;
        velocityTarget*=f(0x3f000000u); // 0.5
        d.delta240=velocityTarget;
        d.velocity238+=velocityTarget;
    }

    // 0C15E8FA..0C15EA42: tach output. This remains separate from any
    // claim about the engine-audio unit being real RPM.
    float adjustment=d.field080;
    adjustment-=f(0x3f000000u);
    adjustment*=f(0x43fa0000u);
    float tachBase;
    if(f(0x44480000u)>s.filtered18)tachBase=f(0x44480000u);
    else tachBase=s.filtered18;
    tachBase+=adjustment;
    float ignoredDifference=d.field224;
    ignoredDifference-=d.field228;
    ignoredDifference*=f(0x428c0000u); // 70, then original FLDI0/FMUL
    ignoredDifference*=0.0f;
    float candidate=tachBase;
    candidate+=ignoredDifference;
    float tachTarget=f(0x44480000u);
    if(candidate>tachTarget) {tachTarget=tachBase;tachTarget+=ignoredDifference;}
    tachTarget-=s.tach1c;
    s.tach1c=std::fma(tachTarget,f(0x3d4ccccdu),s.tach1c); // original FMAC, 0.05
    float overshootThreshold=f(0x42480000u); // 50
    overshootThreshold+=base;
    if(s.tach1c>overshootThreshold) {
        float excess=s.tach1c;
        excess-=base;
        float phase=float(signedWord(g.phase9870));
        if(excess>f(0x457a0000u)) {excess+=phase;g.phase9870=ftrc(excess);}
        else {phase+=f(0x457a0000u);g.phase9870=ftrc(phase);}
        if(!sine)throw std::runtime_error("Original transmission tach branch requires original sine helper");
        float wave=sine(float(signedWord(g.phase9870)),sineContext);
        float amplitude=s.tach1c;
        amplitude-=base;
        amplitude*=f(0x3f000000u);
        if(f(0x43480000u)>amplitude)wave*=amplitude; // original 200 cap
        else {float capped=f(0x43480000u);capped*=wave;wave=capped;}
        float displayed=f(0x42c80000u); // 100
        displayed+=base;
        displayed+=wave;
        s.tach1c=displayed;
    }
    float normalizedTach=s.tach1c;
    normalizedTach/=base;
    s.normalized20=normalizedTach;
}
} // namespace idas3
