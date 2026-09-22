#include "audio.h"
#include "original_stream_gain.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace idas3 {
namespace {
constexpr std::array<unsigned,4> tuningCues{6,11,12,13};
}
#if defined(IDAS3_PORTABLE_SCENE)
EngineAudio::~EngineAudio()=default;
#else
EngineAudio::~EngineAudio(){if(output){waveOutReset(output);for(auto& h:headers)if(h.dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(output,&h,sizeof(h));waveOutClose(output);}}
#endif
void EngineAudio::setOutputGains(const AudioOutputGains& gains){
    for(const auto value:{gains.master,gains.music,gains.engine,gains.effects,gains.tires})
        if(!std::isfinite(value)||value<0.f||value>1.f)throw std::invalid_argument("Audio output gain must be finite in0..1");
    outputGains_=gains;
}
void EngineAudio::resetOutputQueue(){++outputResetSerial_;
#if !defined(IDAS3_PORTABLE_SCENE)
if(output)waveOutReset(output);
#endif
}
void EngineAudio::configure(const std::filesystem::path& root){
    streamRoot=root/"data/original_audio/streams";musicTrack=clampMusicTrack(musicTrack);
    nativeMusic=std::make_unique<OriginalMusicPlayback>(root);
    nativeDsp=std::make_unique<OriginalAudioDspRuntime>(root,[this]{clearOriginalDspSends();},true);
    //1412E0 uploads PACK20 first in8MiB mode; only that first registration
    // latches the ring size. Registration itself never chooses a DSP program.
    nativeDsp->registerBank(0,"PACK20");originalSoundSet=0;
    nativeMusic->setSourceCommandOutput([this](unsigned,std::uint32_t word){
        // The captured score used isolated slot0. The ordinary SH4 allocator
        // binds this bank to slot2 after persistent PACK20 and menu PACK21.
        // A019 is scene selection; the following A0000080 is a source no-op.
        if((word&0xffff0000u)==0xa0190000u)nativeDsp->selectScene(2,(word>>8)&127);
    });
    nativeOneShots=std::make_unique<OriginalOneShotPlayback>(root);
}
void EngineAudio::playMenuCue(OriginalMenuCue cue){
    playOriginalMenuCue(static_cast<std::uint32_t>(cue)>>16);
}
void EngineAudio::playOriginalMenuCue(unsigned sourceCueId){
    //141F80 submits nothing while the PACK21 scene manager is absent.
    if(nativeOneShots&&(originalSoundSet==1||originalSoundSet==2))nativeOneShots->play(21,sourceCueId);
}
void EngineAudio::playTuningCue(unsigned sourceCueId){
    if(sourceCueId==2){playMenuCue(OriginalMenuCue::Change);return;}
    if(sourceCueId==5){playMenuCue(OriginalMenuCue::ResultCount);return;}
    const auto found=std::find(tuningCues.begin(),tuningCues.end(),sourceCueId);
    if(found==tuningCues.end())throw std::out_of_range("Unhandled original tuning sound cue");
    if(nativeOneShots&&(originalSoundSet==1||originalSoundSet==2))nativeOneShots->play(21,sourceCueId);
}
void EngineAudio::playRaceCue(unsigned bank,unsigned cue){
    if((bank!=2&&bank!=4&&bank!=5)||cue>=(bank==2?8u:bank==4?6u:3u))return;
    if(nativeOneShots&&originalSoundSet==4)nativeOneShots->play(20+bank,cue);
}
void EngineAudio::resetRaceEffects(){if(nativeOneShots)for(unsigned bank:{22u,24u,25u})nativeOneShots->stopBank(bank);}
void EngineAudio::selectOriginalEngine(const std::filesystem::path& root,const original::OriginalBattleProfile& profile){
    replayEngineEnabled=false;
    endResultMusic();
    selectOriginalSoundSet(4);
    if(!nativeEngine)nativeEngine=std::make_unique<OriginalEnginePlayback>(root);
    if(!presentationEngine)presentationEngine=std::make_unique<OriginalEnginePlayback>(root);
    if(!nativeTire)nativeTire=std::make_unique<OriginalTirePlayback>(root);
    nativeEngine->select(profile);engineRead=engineCount=0;enginePrimed=false;engineStats={};
    presentationEngine->select(profile);presentationSeed=1;presentationPcmFrames=presentationControlFrames=0;
    if(nativeDsp){
        const auto& init=nativeEngine->initialization();
        for(unsigned i=0;i<2;++i)nativeDsp->registerBank(6+i,"PACK"+std::to_string(init.loadedBankIndices[i]));
        // The source repeats group0's bank-only70A4 followed by preset00A4
        // for both engine handles and the auxiliary handle. Preserve its
        // cache behavior even when the selected bank and live program differ.
        if(init.effectSelector)for(unsigned handle:{0u,1u,3u}){
            const unsigned bank=(init.effectSelector>>8)&127;
            nativeDsp->setBank(bank);nativeDsp->select(bank,init.effectSelector&127);
            //10A4/20A4 address global EFSDL/EFPAN returns; the instrument's
            // own source layer separately supplies its MIXS input bus.
            nativeDsp->setReturnLevel(handle,handle==3?5:10);
            nativeDsp->setReturnPan(handle,64);
        }
    }
    nativeTire->reset();original::resetOriginalTireAudio(tireState);pendingEngineFrame=false;
}
void EngineAudio::selectOriginalSoundSet(int soundSet){
    if(!nativeDsp||soundSet==originalSoundSet)return;
    //1416A0 unloads the preceding scene banks. Stop only PCM voices owned
    // by those banks; PACK21 persists across same-set result/menu visits.
    if(nativeOneShots){
        if(originalSoundSet==1||originalSoundSet==2)nativeOneShots->stopBank(21);
        else if(originalSoundSet==4)resetRaceEffects();
    }
    for(unsigned slot=1;slot<8;++slot)nativeDsp->unregisterBank(slot);
    // Source set2 (Bunta dialogue) shares the PACK21 registration with set1.
    if(soundSet==1||soundSet==2)nativeDsp->registerBank(1,"PACK21");
    else if(soundSet==4){
        constexpr std::array<const char*,5> banks{"PACK22","PACK23","PACK23","PACK24","PACK25"};
        for(unsigned i=0;i<banks.size();++i)nativeDsp->registerBank(i+1,banks[i]);
    }else if(soundSet!=0)throw std::invalid_argument("Unhandled native sound set");
    if(soundSet!=4){nativeEngine.reset();presentationEngine.reset();nativeTire.reset();engineRead=engineCount=0;enginePrimed=false;pendingEngineFrame=false;}
    originalSoundSet=soundSet;
}
void EngineAudio::appendEngineFrame(const OriginalIcsMixFrame& frame,std::int32_t tire){
    if(engineCount==engineFrames.size()){engineRead=(engineRead+1)%engineFrames.size();--engineCount;++engineStats.droppedFrames;}
    const auto at=(engineRead+engineCount)%engineFrames.size();engineFrames[at]=frame;tireFrames[at]=tire;++engineCount;
}
void EngineAudio::clearOriginalDspSends(){
    if(nativeMusic)nativeMusic->clearDspSends();
    if(nativeEngine)nativeEngine->clearDspSends();
    if(presentationEngine)presentationEngine->clearDspSends();
    if(nativeOneShots)nativeOneShots->clearDspSends();
    // PCM was synthesized ahead of the output clock. Apply the hardware
    // register clear to the pending sends at this same output boundary.
    for(std::size_t i=0;i<engineCount;++i)engineFrames[(engineRead+i)%engineFrames.size()].effects={};
}
void EngineAudio::stepOriginalEngine(const original::OriginalEngineControlInput& input,std::uint32_t& seed){
    if(!nativeEngine)return;
    if(pendingEngineFrame)throw std::logic_error("Original audio frame was not finished");
    nativeEngine->step(input,seed,[&](unsigned bank,unsigned cue){if(bank==5)++engineStats.releaseCues;else ++engineStats.backfireCues;playRaceCue(bank,cue);});
    pendingEngineFrame=true;++engineStats.simulationFrames;
}
void EngineAudio::requestOriginalTire(const original::OriginalAngularResult& feedback){
    if(!nativeTire)return;
    ++engineStats.tireRequests;original::requestOriginalTireAudio(tireState,0,feedback.feedbackSpeed,0,feedback.feedbackArgument,feedback.feedbackStrength);
    engineStats.maximumTireStrength=std::max(engineStats.maximumTireStrength,feedback.feedbackStrength);engineStats.maximumTireSpeed=std::max(engineStats.maximumTireSpeed,feedback.feedbackSpeed);
}
void EngineAudio::finishSoundFrame(std::uint32_t& seed){
    if(!nativeEngine)return;
    for(const auto& command:original::stepOriginalTireAudio(tireState,0,seed)){if(command.type==original::OriginalTireCommandType::Play)engineStats.tireCueMask|=1u<<unsigned(command.value);nativeTire->apply(command);}
    nativeEngine->finishSoundFrame();renderOriginalEngineFrame();
}
void EngineAudio::applyConfirmedOnlineAudio(std::span<const original::OriginalEngineCommand> engine,
        std::span<const original::OriginalTireCommand> tires){
    if(!nativeEngine)return;
    nativeEngine->applyContinuous(engine);
    for(const auto& command:engine){
        if(command.target==original::OriginalEngineCommandTarget::RaceCue1424A0)playRaceCue(2,unsigned(command.value));
        else if(command.target==original::OriginalEngineCommandTarget::RaceCue1424E0)playRaceCue(5,unsigned(command.value));
    }
    if(nativeTire)for(const auto& command:tires)nativeTire->apply(command);
    pendingEngineFrame=true;++engineStats.simulationFrames;renderOriginalEngineFrame();
}
void EngineAudio::renderOriginalEngineFrame(){
    if(!pendingEngineFrame)return;pendingEngineFrame=false;
    // Two waveOut blocks absorb ordinary host frame jitter. PCM synthesis
    // still advances exactly735 frames for each original60Hz driving frame.
    if(!enginePrimed){for(unsigned i=0;i<1024;++i)appendEngineFrame({});enginePrimed=true;}
    for(unsigned i=0;i<735;++i){auto pcm=nativeEngine->renderFrame();const auto tire=nativeTire->renderFrame();
        for(const auto value:pcm.dry)engineStats.dryEnergy+=double(value)*value;
        for(const auto value:pcm.effects)engineStats.effectSendEnergy+=double(value)*value;
        engineStats.tireEnergy+=double(tire)*tire;appendEngineFrame(pcm,std::int32_t(float(tire)*.45f));++engineStats.pcmFrames;
    }
    engineStats.tireCues=nativeTire->startedCues();engineStats.tireSamples=nativeTire->startedSamples();
}
std::string EngineAudio::musicName()const{return musicTrack==-2?"Custom music":raceMusicCatalog[clampMusicTrack(musicTrack)].title;}
bool EngineAudio::selectMusicTrack(int index){
    if(index<0||index>=int(raceMusicCatalog.size()))return false;
    if(index==musicTrack)return true;
    const int previous=musicTrack;musicTrack=index;
    try{if(musicScene==1)loadMusic();}catch(...){musicTrack=previous;throw;}
    return true;
}
void EngineAudio::nextMusic(){selectMusicTrack((clampMusicTrack(musicTrack)+1)%int(raceMusicCatalog.size()));}
void EngineAudio::loadMusic(){
    if(streamRoot.empty())return;
    stopLegendStream();
    musicGain=1;attractChild=~0u;attractStats.stream=-1;
    // Source stream commands belong to scene owners. Ordinary menus do not
    // replay the Rosso logo sound simply because they share a frontend.
    if(musicScene==0||musicScene==6){
        if(musicScene==6&&nativeMusic)nativeMusic->stop();
        music={};musicFrame=0;loadedTrack=-1;resetOutputQueue();return;
    }
    if(nativeMusic)nativeMusic->stop();
    const auto name=musicScene==2?"WIN.bin":musicScene==3?"TIMEUP.bin":musicScene==5?"LOSE.bin":raceMusicCatalog[clampMusicTrack(musicTrack)].relativePath;
    if((musicScene==1||musicScene==4)&&musicTrack==-2&&customRaceMusic)music=*customRaceMusic;
    else music=loadOriginalSpsd(streamRoot/name);
    musicFrame=0;loadedTrack=musicTrack;
    resetOutputQueue();
}
EngineAudio::AttractStatistics EngineAudio::attractStatistics()const{
    auto out=attractStats;out.frame=musicFrame;return out;
}
const OriginalMusicPlaybackStatistics& EngineAudio::selectionStatistics()const{
    if(!nativeMusic)throw std::logic_error("Original selection audio is not configured");return nativeMusic->statistics();
}
std::vector<OriginalMusicPlayback::LiveVoice> EngineAudio::selectionVoices()const{
    return nativeMusic?nativeMusic->liveVoices():std::vector<OriginalMusicPlayback::LiveVoice>{};
}
std::uint64_t EngineAudio::selectionSamplePosition()const{return nativeMusic?nativeMusic->samplePosition():0;}
void EngineAudio::selection(const original::OriginalSelectionMusicCommand& command){
    if(!nativeMusic)throw std::logic_error("Original selection audio is not configured");
    if(musicScene!=0){musicScene=0;loadMusic();}
    using Op=original::OriginalSelectionMusicOperation;
    if(nativeDsp){
        if(command.operation==Op::Load){
            // Another owner taking the music ends this one; the owner's own
            // load must not, or the request would cancel itself.
            if(!originalResultOwner||command.cue!=unsigned(resultMusicManager.selectedCue28)){
                endResultMusic();selectOriginalSoundSet(1);
            }
            const auto& descriptor=original::originalMusicCueDescriptor(command.cue);
            nativeDsp->registerBank(2,std::filesystem::path(descriptor.filename).stem().string());
        }
        else if(command.operation==Op::Unload)nativeDsp->unregisterBank(2);
    }
    nativeMusic->apply(command);
    if(command.operation==Op::Start){
        music={};musicFrame=0;musicGain=1;attractStats.stream=-1;attractChild=~0u;
    }
    if(command.operation==Op::Start||command.operation==Op::Unload||
        (command.operation==Op::Control&&command.word==0x001200a0))resetOutputQueue();
}
void EngineAudio::beginOriginalMusicCue(unsigned cue,bool requestSoundSet,int soundSet){
    stopLegendStream();
    if(!nativeMusic)throw std::logic_error("Original sequenced music is not configured");
    // A repeat of the cue already playing is the source's no-op; a different
    // cue replaces it, which is how one dialogue follows another.
    if(originalResultOwner&&originalMusicCue_==cue)return;
    if(originalResultOwner)endResultMusic();
    originalResultOwner=true;resultMusicRequested=requestSoundSet;originalMusicCue_=cue;
    if(!requestSoundSet)return;
    resultMusicManager={};selectOriginalSoundSet(soundSet);
    for(const auto& command:original::requestOriginalSelectionMusic(resultMusicManager,
        static_cast<original::OriginalSelectionMusicCue>(cue)))selection(command);
}
void EngineAudio::fadeOriginalMusicCue(){
    if(!originalResultOwner||!resultMusicRequested)return;
    for(const auto& command:original::exitOriginalSelectionMusic(resultMusicManager))selection(command);
}
void EngineAudio::tickResultMusic(){
    if(originalResultOwner&&resultMusicRequested)for(const auto& command:original::tickOriginalSelectionMusic(resultMusicManager))selection(command);
}
void EngineAudio::endResultMusic(){
    if(!originalResultOwner)return;
    originalResultOwner=false;
    if(resultMusicRequested){
        for(const auto& command:original::changeOriginalSelectionMusicScene(resultMusicManager,0))selection(command);
        resultMusicManager={};
    }
    resultMusicRequested=false;
}
void EngineAudio::stopLegendStream(){
    if(!legendStreamOwner)return;
    legendStreamOwner=false;legendStreamStats.playing=false;legendStreamStats.stream=-1;
    legendStreamFade={};music={};musicFrame=0;musicGain=1;++legendStreamStats.stops;resetOutputQueue();
}
void EngineAudio::applyLegendStreamCommand(const original::OriginalLegendReturnEvent& event){
    using C=original::OriginalLegendReturnCommand;
    switch(event.command){
    case C::SoundSet:endResultMusic();selectOriginalSoundSet(int(event.a));break;
    case C::StreamStart:{
        endResultMusic();stopLegendStream();
        if(nativeMusic)nativeMusic->stop();
        const auto& descriptor=original::originalStreamDescriptor(event.a);
        music=loadOriginalSpsd(streamRoot/descriptor.filename);music.looping=descriptor.loop!=0;
        musicFrame=0;musicScene=0;raceMusicHeld=false;loadedTrack=-1;attractChild=~0u;attractStats.stream=-1;
        legendStreamOwner=true;legendStreamStats.stream=int(event.a);legendStreamStats.playing=false;
        legendStreamStats.volume=unsigned(descriptor.volume);legendStreamFade={};
        musicGain=original::originalStreamGain(std::uint8_t(legendStreamStats.volume));resetOutputQueue();break;
    }
    case C::StreamVolume:
        legendStreamStats.volume=event.a&127;legendStreamFade={};
        if(legendStreamOwner)musicGain=original::originalStreamGain(std::uint8_t(legendStreamStats.volume));
        break;
    case C::StreamPlay:
        if(legendStreamOwner&&!legendStreamStats.playing){legendStreamStats.playing=true;++legendStreamStats.starts;resetOutputQueue();}break;
    case C::StreamFade:
        if(legendStreamOwner)legendStreamFade.begin(legendStreamStats.volume,event.a);break;
    case C::StreamStop:stopLegendStream();break;
    default:break;
    }
}
void EngineAudio::tickLegendStream(){
    if(!legendStreamOwner||!legendStreamFade.active||musicPaused)return;
    legendStreamStats.volume=legendStreamFade.tick();
    musicGain=original::originalStreamGain(std::uint8_t(legendStreamStats.volume));
}
void EngineAudio::attract(unsigned child,std::uint32_t frame,bool finished,bool soundEnabled){
    if(streamRoot.empty())return;
    if(musicScene!=0){musicScene=0;loadMusic();}
    if(child==attractChild&&frame==attractFrame&&finished==attractFinished&&soundEnabled==attractSoundEnabled)return;
    if(child!=~0u){endResultMusic();stopLegendStream();}
    if(child!=~0u)selectOriginalSoundSet(0);
    if(child!=~0u&&nativeMusic&&nativeMusic->playing())nativeMusic->stop();
    const auto stopStream=[&]{music={};musicFrame=0;attractStats.stream=-1;++attractStats.stops;resetOutputQueue();};
    //02DF60 invokes141D80 independently of which child is selected next.
    if(child!=attractChild)stopStream();
    attractChild=child;attractFrame=frame;attractFinished=finished;attractSoundEnabled=soundEnabled;
    for(const auto& command:original::originalAttractSoundCommands(child,frame,soundEnabled,finished)){
        switch(command.operation){
        case original::OriginalAttractSoundOperation::Play:{
            const auto& descriptor=original::originalStreamDescriptor(command.value);
            music=loadOriginalSpsd(streamRoot/descriptor.filename);musicFrame=0;
            // Attract streams are source one-shots. The original descriptor,
            // not the desktop race-song repeat policy, controls this owner.
            music.looping=descriptor.loop!=0;
            attractStats.stream=int(command.value);attractStats.volume=unsigned(descriptor.volume);++attractStats.plays;
            // Original A2 volume table and AICA attenuation precede the
            // desktop mix headroom; command125 is not a linear125/127 gain.
            musicGain=original::originalStreamGain(std::uint8_t(attractStats.volume));
            resetOutputQueue();
            break;
        }
        case original::OriginalAttractSoundOperation::Stop:stopStream();break;
        case original::OriginalAttractSoundOperation::Volume:
            attractStats.volume=command.value&127;musicGain=original::originalStreamGain(std::uint8_t(attractStats.volume));++attractStats.volumeChanges;break;
        }
    }
}
EngineAudio::RaceTimingStatistics EngineAudio::raceTimingStatistics()const{
    return {(raceMusicHeld?1u:0u)|(raceMusicHeld&&presentationEngine&&!vehicleMuted?2u:0u)|(musicPaused?4u:0u)|
        ((musicScene==1||musicScene==4)&&music.frames()?8u:0u)|(vehicleMuted?16u:0u),musicScene,loadedTrack,musicFrame,
        presentationPcmFrames,presentationControlFrames,engineStats.simulationFrames};
}
void EngineAudio::scene(bool menu,bool finished,bool paused,bool timeUp,bool holdRaceMusic,bool muteVehicle,FinishOutcome outcome){
    if(vehicleMuted!=muteVehicle){vehicleMuted=muteVehicle;resetOutputQueue();}
    musicPaused=paused;
    const bool wasHeld=raceMusicHeld;
    raceMusicHeld=holdRaceMusic&&!menu&&!finished;
    if(menu){endResultMusic();stopLegendStream();}
    if(legendStreamOwner)return;
    //07042A..07044E may omit every result sound operation. The owner then
    // keeps the preceding music state; finished is not a new stream command.
    if(originalResultOwner)return;
    // Native scene mapping; original sound-command sequencing remains separate.
    // Decode during loading, not on the visible countdown boundary. Scene4
    // retains the selected race stream at sample0 while the source idle plays.
    // Finishing is not a win. Online races wait for the settled result, and a
    // draw has no winner announcement. Later sequenced result owners retain
    // their source-defined music through the guard above.
    const int finishScene=outcome==FinishOutcome::Pending||outcome==FinishOutcome::Draw?6:
        timeUp?3:outcome==FinishOutcome::Loss?5:2;
    const int next=menu?0:finished?finishScene:raceMusicHeld?4:1;
    if(next==1||next==4)selectOriginalSoundSet(4);
    if(next==1&&musicScene==4&&loadedTrack==musicTrack){musicScene=next;resetOutputQueue();return;}
    if(next!=musicScene||(next==1&&loadedTrack!=musicTrack)||(raceMusicHeld&&!wasHeld)){musicScene=next;loadMusic();}
}
#if defined(IDAS3_PORTABLE_SCENE)
// Unity consumes the unchanged mixer through unity_audio_output.cpp.
bool EngineAudio::open(){return false;}
void EngineAudio::update(float,float,float,float,bool){}
#else
bool EngineAudio::open(){
    WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=2;fmt.nSamplesPerSec=44100;fmt.wBitsPerSample=16;fmt.nBlockAlign=4;fmt.nAvgBytesPerSec=176400;
    if(waveOutOpen(&output,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){output=nullptr;return false;}
    for(int i=0;i<buffers;i++){auto& h=headers[i];h.lpData=reinterpret_cast<char*>(data[i].data());h.dwBufferLength=samples*4;if(waveOutPrepareHeader(output,&h,sizeof(h))!=MMSYSERR_NOERROR){enabled=false;return false;}}return true;
}
void EngineAudio::update(float rpm,float throttle,float speed,float slip,bool active){
    if(!output)return;
    for(int i=0;i<buffers;i++){auto& h=headers[i];if((h.dwFlags&WHDR_INQUEUE)&&!(h.dwFlags&WHDR_DONE))continue;
        for(int j=0;j<samples;j++){const auto stereo=renderStereo(rpm,throttle,speed,slip,active);data[i][j*2]=stereo[0];data[i][j*2+1]=stereo[1];}
        waveOutWrite(output,&h,sizeof(h));
    }
}
#endif
std::array<short,2> EngineAudio::renderStereo(float rpm,float throttle,float speed,float slip,bool active){
            std::array<short,2> result{};std::array<float,2> engine{};std::array<std::int32_t,16> dspInput{};
            const auto gains=outputGains_;float tires=0;
            const auto scaleSend=[](std::int32_t value,float gain){return gain==1.f?value:std::int32_t(float(value)*gain);};
            if(nativeEngine){
                if(active&&!musicPaused&&!vehicleMuted){
                    if((raceMusicHeld||replayEngineEnabled)&&presentationEngine){
                        if(presentationPcmFrames%735==0){
                            original::OriginalEngineControlInput idle;
                            // 15E8FA's stationary tach floor, zero throttle.
                            // No car state, tire audio, race cues or shared RNG
                            // may advance during the held presentation.
                            idle.rpm=800.f;idle.gear=1;idle.suppressShiftRelease=true;
                            if(replayEngineEnabled){
                                presentationEngine->step(replayEngineInput,presentationSeed,[&](unsigned bank,unsigned cue){playRaceCue(bank,cue);});
                                replayEngineInput.suppressShiftRelease=false;
                            }else presentationEngine->step(idle,presentationSeed,{});
                            presentationEngine->finishSoundFrame();++presentationControlFrames;
                        }
                        const auto pcm=presentationEngine->renderFrame();
                        for(unsigned c=0;c<2;++c)engine[c]=pcm.dry[c]/32768.f;
                        dspInput=pcm.effects;++presentationPcmFrames;
                    }else if(engineCount){const auto& pcm=engineFrames[engineRead];
                        // Preserve the old integer summation exactly when both sliders match.
                        const auto tire=tireFrames[engineRead];
                        for(unsigned c=0;c<2;++c)engine[c]=(pcm.dry[c]+(gains.engine==gains.tires?tire:0))/32768.f;
                        if(gains.engine!=gains.tires)tires=tire/32768.f;
                        dspInput=pcm.effects;engineRead=(engineRead+1)%engineFrames.size();--engineCount;}
                    else if(enginePrimed)++engineStats.underflowFrames;
                }
            }else{
            phase+=double(std::clamp(rpm,600.f,10000.f))/30/44100*6.283185307;phase=std::fmod(phase,6.283185307);noise=noise*1664525u+1013904223u;float hiss=(int(noise>>16)-32768)/32768.f;
            const float tone=float(std::sin(phase)*.55+std::sin(phase*2)*.28+std::sin(phase*3)*.1);
            const float tire=std::clamp((std::abs(slip)-.09f)*3,0.f,.8f)*std::clamp(speed/15,0.f,1.f);
            if(gains.engine==gains.tires)engine.fill(active&&!vehicleMuted?(tone*(.07f+.11f*throttle)+hiss*(.012f*speed/50+.06f*tire)):0.f);
            else if(active&&!vehicleMuted){engine.fill(tone*(.07f+.11f*throttle)+hiss*.012f*speed/50);tires=hiss*.06f*tire;}
            }
            if(gains.engine!=1.f)for(auto& value:dspInput)value=scaleSend(value,gains.engine);
            const auto frame=std::size_t(musicFrame);const bool play=!musicPaused&&!raceMusicHeld&&(!legendStreamOwner||legendStreamStats.playing)&&frame<music.frames();
            const auto loopEnd=music.looping&&music.loopEnd?music.loopEnd:music.frames();
            std::array<std::int32_t,2> selectionPcm{};
            if(!musicPaused&&musicScene==0&&nativeMusic){const auto pcm=nativeMusic->renderFrame();selectionPcm=pcm.dry;for(unsigned bus=0;bus<16;++bus)dspInput[bus]+=scaleSend(pcm.effects[bus],gains.music);}
            std::array<std::int32_t,2> effectPcm{};
            if(!musicPaused&&nativeOneShots){const auto pcm=nativeOneShots->renderFrame();effectPcm=pcm.dry;for(unsigned bus=0;bus<16;++bus)dspInput[bus]+=scaleSend(pcm.effects[bus],gains.effects);}
            std::array<std::int32_t,2> wet{};
            // One shared44100Hz processor receives the actual source MIXS.
            // Zero-input frames retain reverb tails; pause freezes this clock.
            if(!musicPaused&&nativeDsp)wet=nativeDsp->render(dspInput).wet;
            for(const auto bus:dspInput)dspInputEnergy+=double(bus)*bus;
            for(const auto value:wet)dspWetEnergy+=double(value)*value;
            for(unsigned channel=0;channel<2;++channel){float song=0;
                if(play){const auto next=music.looping&&music.loopEnd&&frame+1>=loopEnd?music.loopStart:std::min(frame+1,music.frames()-1);
                    const auto a=frame*music.channels+std::min(channel,music.channels-1),b=next*music.channels+std::min(channel,music.channels-1);const float fraction=float(musicFrame-frame);song=(music.samples[a]+(music.samples[b]-music.samples[a])*fraction)/32768.f;}
                // Desktop gain/headroom is applied AFTER original effects.
                // The shared return gain stays constant across scenes so a
                // surviving tail cannot jump in level at the menu/race boundary.
                // Desktop output headroom covers layered engine PCM, complete
                // skid sequences, music and one-shots playing together.
                // Keep the default arithmetic and summation order unchanged.
                float mixed;
                if(gains.music==1.f&&gains.engine==1.f&&gains.effects==1.f&&gains.tires==1.f)
                    mixed=(engine[channel]+song*.38f*musicGain+(selectionPcm[channel]/32768.f)*.38f+
                        (wet[channel]/32768.f)*.38f+(effectPcm[channel]/32768.f)*.45f)*.60f;
                else mixed=(engine[channel]*gains.engine+tires*gains.tires+song*.38f*musicGain*gains.music+
                    (selectionPcm[channel]/32768.f)*.38f*gains.music+(wet[channel]/32768.f)*.38f+
                    (effectPcm[channel]/32768.f)*.45f*gains.effects)*.60f;
                if(gains.master!=1.f)mixed*=gains.master;
                result[channel]=short(std::clamp(enabled&&!musicPaused?mixed:0.f,-1.f,1.f)*32767);
            }
            if(play){musicFrame+=double(music.sampleRate)/44100;if(musicFrame>=loopEnd){
                if(music.looping)musicFrame=music.loopStart+std::fmod(musicFrame-loopEnd,double(loopEnd-music.loopStart));
                else if(musicScene==1)musicFrame=std::fmod(musicFrame,double(music.frames()));
            }}
            return result;
}
}
