#pragma once
#if !defined(IDAS3_PORTABLE_SCENE)
#include <windows.h>
#include <mmsystem.h>
#endif
#include <array>
#include "original_audio.h"
#include "music_catalog.h"
#include "original_menu_audio.h"
#include "original_engine_playback.h"
#include "original_sfx_sequence.h"
#include "original_dynamics.h"
#include "original_attract_audio.h"
#include "original_stream_fade.h"
#include "original_legend_return.h"
#include "original_music_playback.h"
#include "original_audio_dsp_runtime.h"
#include "original_oneshot_playback.h"
#include <string>
#include <vector>
namespace idas3 {
struct OriginalEngineAudioStatistics {
    std::uint64_t simulationFrames=0,pcmFrames=0,releaseCues=0,backfireCues=0,droppedFrames=0,underflowFrames=0;
    double dryEnergy=0;
    double effectSendEnergy=0;
    std::uint64_t tireRequests=0,tireCues=0,tireSamples=0;
    double tireEnergy=0;
    unsigned tireCueMask=0;
    float maximumTireStrength=0,maximumTireSpeed=0;
};
struct AudioOutputGains {
    float master=1,music=1,engine=1,effects=1;
};
// Original SPSD music, race effects and native ICS engine playback.
class EngineAudio {
public:
    ~EngineAudio();
    bool open();
    void update(float rpm,float throttle,float speed,float slip,bool active);
    void configure(const std::filesystem::path& root);
    enum class FinishOutcome { Win, Loss, Pending, Draw };
    void scene(bool menu,bool finished,bool paused,bool timeUp=false,bool holdRaceMusic=false,bool muteVehicle=false,FinishOutcome outcome=FinishOutcome::Win);
    struct RaceTimingStatistics {
        unsigned flags=0; //1 held,2 source idle,4 paused,8 race stream loaded,16 hidden vehicle muted
        int scene=-1,track=-1;
        double streamFrame=0;
        std::uint64_t idlePcmFrames=0,idleControlFrames=0,drivingControlFrames=0;
    };
    RaceTimingStatistics raceTimingStatistics()const;
    // Call for each60Hz frontend tick, using the source counter BEFORE update.
    // Repeated render/capture calls at the same source counter are harmless.
    void attract(unsigned child,std::uint32_t sourceFrame,bool demoFinished=false,bool soundEnabled=true);
    struct AttractStatistics{std::uint64_t plays{},stops{},volumeChanges{};int stream=-1;double frame=0;unsigned volume=127;};
    AttractStatistics attractStatistics()const;
    void selection(const original::OriginalSelectionMusicCommand& command);
    const OriginalMusicPlaybackStatistics& selectionStatistics()const;
    // Diagnostic: the music voices sounding right now.
    std::vector<OriginalMusicPlayback::LiveVoice> selectionVoices()const;
    std::uint64_t selectionSamplePosition()const;
    bool selectionPlaying()const{return nativeMusic&&nativeMusic->playing();}
    // The race-end WIN/LOSE/TIMEUP stream plays once and stops; the announcement
    // holds the road view until it runs out. No clip loaded counts as over.
    bool raceMusicFinished()const{return music.frames()==0||musicFrame>=double(music.frames());}
    const OriginalAudioDspRuntime* dspRuntime()const{return nativeDsp.get();}
    // Diagnostic: how much the DSP wet return actually contributes.
    double dspWetEnergy=0,dspInputEnergy=0;
    void nextMusic();
    // Changes the race selection; menu/attract/result owners keep their music.
    // Valid same-track requests are no-ops. Invalid indices leave state intact.
    bool selectMusicTrack(int index);
    void playMenuCue(OriginalMenuCue cue);
    void playOriginalMenuCue(unsigned sourceCueId);
    void playTuningCue(unsigned sourceCueId);
    // The common result owner can explicitly skip its sound block. Preserve
    // the preceding stream/manager/sound set in that source branch.
    // The result screen was the first owner outside the menus to drive the
    // sequenced music; the dialogue scenes drive the same manager with their
    // own cue, so it takes the cue rather than assuming Result.
    void beginResultMusic(bool requestSoundSet=true){beginOriginalMusicCue(2,requestSoundSet);}
    void beginOriginalMusicCue(unsigned cue,bool requestSoundSet=true,int soundSet=1);
    void tickResultMusic();
    // Post-race Legend stream commands are separate from sequenced rival music.
    void applyLegendStreamCommand(const original::OriginalLegendReturnEvent&);
    void tickLegendStream();
    struct LegendStreamStatistics {int stream=-1;unsigned volume=127;bool playing=false;double frame=0;std::uint64_t starts=0,stops=0;};
    LegendStreamStatistics legendStreamStatistics()const{auto out=legendStreamStats;out.frame=musicFrame;return out;}

    void endResultMusic();
    // 1431E0 at the dialogue's exit: the source asks for a fade, not a stop.
    void fadeOriginalMusicCue();
    unsigned originalMusicCue()const{return originalMusicCue_;}
    // Development diagnostic only; ~0u is every channel, which is the game.
    void setDiagnosticMusicChannel(unsigned channel){if(nativeMusic)nativeMusic->setDiagnosticChannel(channel);}
    void playRaceCue(unsigned bank,unsigned cue);
    // Interrupt owner16CCC4 calls141F40(3,1): persistent PACK20 A9 cue3.
    void playChallengerCue(){if(nativeOneShots)nativeOneShots->play(20,3);}
    void resetRaceEffects();
    void selectOriginalEngine(const std::filesystem::path& root,const original::OriginalBattleProfile& profile);
    // Replay controls drive the existing car instrument on its audio clock,
    // independently of physics and the gameplay random seed.
    void setReplayEngine(const original::OriginalEngineControlInput& input){replayEngineInput=input;replayEngineEnabled=true;}
    void useDevelopmentEngine(){nativeEngine.reset();presentationEngine.reset();nativeTire.reset();}
    void stepOriginalEngine(const original::OriginalEngineControlInput& input,std::uint32_t& sharedSeed);
    void requestOriginalTire(const original::OriginalAngularResult& feedback);
    void finishSoundFrame(std::uint32_t& sharedSeed);
    void applyConfirmedOnlineAudio(std::span<const original::OriginalEngineCommand> engine,
        std::span<const original::OriginalTireCommand> tires);
    // Same mixer used by waveOut and bounded offline application checks.
    std::array<short,2> renderStereo(float rpm,float throttle,float speed,float slip,bool active);
    // Host output controls, not the original instrument/manager volume state.
    // Engine includes tire PCM. Shared wet tails remain until they decay;
    // master controls the final mix including those tails. No clock resets.
    void setOutputGains(const AudioOutputGains& gains);
    AudioOutputGains outputGains()const{return outputGains_;}
    // Host output queues follow exactly the existing stream/score reset cuts.
    std::uint64_t outputResetSerial()const{return outputResetSerial_;}
    const OriginalEngineAudioStatistics& engineStatistics()const{return engineStats;}
    OriginalOneShotStatistics oneShotStatistics()const{return nativeOneShots?nativeOneShots->statistics():OriginalOneShotStatistics{};}
    std::string musicName()const;
    int musicTrack=0;
    // Next-race host music, already decoded before selection. The running
    // race owns its own music buffer, so an idle lobby never changes it.
    std::shared_ptr<const OriginalAudioClip> customRaceMusic;
    bool enabled=true;
private:
    // Four short buffers keep menu feedback below the previous93ms queue.
    static constexpr int samples=512,buffers=4;
#if !defined(IDAS3_PORTABLE_SCENE)
    HWAVEOUT output=nullptr;
    std::array<WAVEHDR,buffers> headers{};
    std::array<std::array<short,samples*2>,buffers> data{};
#endif
    double phase=0;unsigned noise=912381;
    std::filesystem::path streamRoot;
    OriginalAudioClip music;
    double musicFrame=0;
    int musicScene=-1,loadedTrack=-1;
    bool musicPaused=false;
    bool raceMusicHeld=false;
    bool vehicleMuted=false;
    float musicGain=1;
    unsigned attractChild=~0u;
    std::uint32_t attractFrame=~0u;
    bool attractFinished=false,attractSoundEnabled=true;
    AttractStatistics attractStats;
    LegendStreamStatistics legendStreamStats;
    original::OriginalStreamFade legendStreamFade;
    bool legendStreamOwner=false;
    void stopLegendStream();
    std::unique_ptr<OriginalOneShotPlayback> nativeOneShots;
    std::unique_ptr<OriginalEnginePlayback> nativeEngine;
    // The showcase cannot tick the live engine controller: that consumes the
    // gameplay RNG. This separate source instrument has only an audio clock.
    std::unique_ptr<OriginalEnginePlayback> presentationEngine;
    bool replayEngineEnabled=false;
    original::OriginalEngineControlInput replayEngineInput{};
    std::uint32_t presentationSeed=1;
    std::uint64_t presentationPcmFrames=0,presentationControlFrames=0;
    std::unique_ptr<OriginalTirePlayback> nativeTire;
    std::unique_ptr<OriginalMusicPlayback> nativeMusic;
    std::unique_ptr<OriginalAudioDspRuntime> nativeDsp;
    int originalSoundSet=0;
    bool originalResultOwner=false;
    bool resultMusicRequested=false;
    unsigned originalMusicCue_=2;
    original::OriginalSelectionMusicState resultMusicManager;
    original::OriginalTireAudioState tireState;
    bool pendingEngineFrame=false;
    // PCM follows the60Hz simulation clock, independently of rendering FPS.
    // Bounded queue prevents diagnostics or a stalled output from growing RAM.
    // Keep the source DSP sends on the same clock as their dry samples. The
    // larger frames live on the heap instead of consuming the window stack.
    std::vector<OriginalIcsMixFrame> engineFrames=std::vector<OriginalIcsMixFrame>(8820);
    std::size_t engineRead=0,engineCount=0;
    bool enginePrimed=false;
    OriginalEngineAudioStatistics engineStats;
    void appendEngineFrame(const OriginalIcsMixFrame& frame);
    void renderOriginalEngineFrame();
    void clearOriginalDspSends();
    void selectOriginalSoundSet(int soundSet);
    void loadMusic();
    std::uint64_t outputResetSerial_=0;
    AudioOutputGains outputGains_;
    void resetOutputQueue();
};
}
