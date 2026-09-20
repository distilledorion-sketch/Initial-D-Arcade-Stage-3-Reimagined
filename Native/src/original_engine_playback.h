#pragma once
#include "original_battle_profile.h"
#include "original_engine_control.h"
#include "original_ics_player.h"
#include <functional>
#include <string>
namespace idas3 {
struct OriginalEngineSoundSelection {unsigned family=0;int level=0;};
//067A00: profile tuning bytes select the sound level and the special AE86 bank.
OriginalEngineSoundSelection selectOriginalEngineSound(const original::OriginalBattleProfile& profile);
struct OriginalEngineBankSelection {unsigned bank=0,instrument=0,effect=0;};
struct OriginalEngineBankSelections {
    std::array<std::array<OriginalEngineBankSelection,2>,36> families{};
    OriginalEngineBankSelection auxiliary;
    static OriginalEngineBankSelections load(const std::filesystem::path& root);
};
//0C3E20 loads the first family bank, then PACK10. All three effect setup
//branches reload group0 parameters+4, regardless of the voice handle.
struct OriginalEngineInitialization {
    std::array<unsigned,2> loadedBankIndices{};
    unsigned effectSelector=0;
    std::array<std::string,2> loadedBankNames()const;
};
//1435C0 and143680: one non-forced scene queue. Forced cues neither check the
// cooldown nor consume RNG. Tick once in the outer sound update, not per sample.
struct OriginalSoundCueQueue {
    std::int32_t cooldown=0;
    bool request(bool forced,std::uint32_t& sharedSeed);
    void tick();
};
class OriginalEnginePlayback {
public:
    explicit OriginalEnginePlayback(const std::filesystem::path& root);
    void select(const original::OriginalBattleProfile& profile);
    void step(const original::OriginalEngineControlInput& input,std::uint32_t& sharedSeed,
        const std::function<void(unsigned bank,unsigned cue)>& cueOutput);
    void finishSoundFrame(){releaseQueue_.tick();}
    // Apply already-decided continuous commands after network confirmation.
    // No control/RNG step and no re-emission of one-shot requests.
    void applyContinuous(std::span<const original::OriginalEngineCommand> commands);
    void clearDspSends(){player_.clearDspSends();}
    OriginalIcsMixFrame renderFrame(){return player_.renderFrame();}
    const original::OriginalEngineConfiguration& configuration()const{return configuration_;}
    const original::OriginalEngineControlState& state()const{return state_;}
    const OriginalSoundCueQueue& releaseQueue()const{return releaseQueue_;}
    const OriginalEngineInitialization& initialization()const{return initialization_;}
    const OriginalIcsVoiceControls& channelControls(unsigned channel)const{return controls_.at(channel);}
    unsigned activeVoices()const{return player_.activeVoices();}
private:
    std::filesystem::path root_;
    original::OriginalEngineTables tables_;
    OriginalEngineBankSelections selections_;
    OriginalIcsPlayer player_;
    std::array<std::shared_ptr<const OriginalIcsBank>,15> banks_;
    std::array<OriginalIcsVoiceControls,4> controls_{};
    original::OriginalEngineConfiguration configuration_;
    original::OriginalEngineControlState state_;
    OriginalSoundCueQueue releaseQueue_;
    OriginalEngineInitialization initialization_;
    std::shared_ptr<const OriginalIcsBank> bank(unsigned index);
};
}
