#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace idas3::original {
// Source HLecture inputs: source15ECE0 statistics plus the current and prior
// personal-record timing arrays. Do not substitute host trace percentages.
struct OriginalTimeAttackAnalysisInput {
    unsigned course=0,car=0;
    bool manual=false;
    std::uint32_t resultStatus=0,convertedEventCount=0;
    float acceleratorFraction=0,brakeFraction=0,maxSteeringDelta=0;
    std::uint32_t wallCount=0,ditchCount=0,maxGearUsed=0;
    std::uint32_t finishTicks6000=0,previousBestTicks6000=0;
    //07CC60 turns cumulative intermediate clocks into per-section durations.
    std::array<std::uint32_t,4> currentSections6000{},previousSections6000{};
};
struct OriginalTimeAttackAdviceText {
    std::uint32_t rowAddress=0;
    std::array<std::uint32_t,4> addresses{};
    std::array<std::string_view,4> text{}; // source prefix and three visible lines
};
struct OriginalTimeAttackAnalysis {
    unsigned kind=0,adviceIndex=0,worstSection=0,countdownTicks=1279;
    std::uint32_t rowAddress=0;
    std::array<std::uint32_t,3> lineAddresses{};
    std::array<std::string_view,3> lines{};
    bool usedRandom=false;
};
//18BB80,18FD80..190080,18CA60,18D0E0. Only the two kind2 praise
// branches consume one1F9E60 RNG draw; sharedSeed is unchanged otherwise.
OriginalTimeAttackAnalysis analyzeOriginalTimeAttack(const OriginalTimeAttackAnalysisInput&,std::uint32_t& sharedSeed);
unsigned originalTimeAttackAnalysisKind(unsigned course,std::uint32_t resultStatus,std::uint32_t convertedEventCount);
unsigned originalTimeAttackWorstSection(unsigned course,const std::array<std::uint32_t,4>& current,const std::array<std::uint32_t,4>& previous);
bool originalTimeAttackAdvicePredicate(const OriginalTimeAttackAnalysisInput&,unsigned kind,unsigned index);
const OriginalTimeAttackAdviceText& originalTimeAttackAdviceText(unsigned kind,unsigned index);
std::uint32_t originalTimeAttackAdvicePredicateAddress(unsigned kind,unsigned index);
}
