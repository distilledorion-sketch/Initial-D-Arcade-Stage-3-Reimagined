#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>

// CPU-only live App simulation and its actual mixer. Each case is a fresh
// native audio boot followed by App::start; the companion selection harness
// covers the inherited SELECT->race DSP cache path. Never opens waveOut.
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("game-root output-directory required");
    const fs::path root=argv[1],output=argv[2];fs::create_directories(output);
    std::ofstream report(output/"application.csv");
    report<<"car,upgrade,condition,wet,night,simulation_frames,pcm_frames,dry_rms,mix_rms,clipped,release_cues,backfire_cues,tire_cues,tire_mask,dropped,underflow,travel,dsp_frames,ring_code,active_bank,active_preset,cache_bank,cache_preset,loads,clears,input_peak,wet_energy,wet_rms,mean_sim_ms,max_sim_ms,mean_mix_ms,max_mix_ms,mix_blocks_over_16_67ms,one_shot_cues,one_shot_notes,one_shot_peak,one_shot_dropped,one_shot_stolen\n";
    if(!report)throw std::runtime_error("Cannot write driving DSP report");
    struct Case{unsigned car,upgrade,course;bool wet,night;};
    constexpr std::array<Case,3> cases{{{0,0,3,false,false},{8,2,3,true,false},{19,2,8,true,true}}};
    constexpr unsigned frames=1500,samplesPerFrame=735;
    const auto selections=OriginalEngineBankSelections::load(root);
    for(const auto test:cases){
        App app;app.root=root;app.validationMode=true;app.settings();
        app.originalCamera=OriginalChaseCamera::load(root);app.bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
        app.frontend.initialize(root,true);app.audio.configure(root);
        app.courseIndex=int(test.course);app.reverse=false;app.wet=test.wet;app.night=test.night;app.automatic=true;
        app.frontend.course=app.courseIndex;app.frontend.reverse=false;app.frontend.wet=test.wet;app.frontend.night=test.night;app.frontend.automatic=true;
        app.frontend.car=int(test.car);app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        auto& profile=app.frontend.battleProfile;profile.setu(16,test.car);profile.setByte(164,std::uint8_t(test.upgrade));profile.setByte(162,1);profile.setByte(166,1);
        app.start();app.audio.enabled=true;app.audio.scene(false,false,false);
        const auto initialRemaining=app.race.remaining6000;
        if(app.audio.engineStatistics().simulationFrames||app.audio.engineStatistics().pcmFrames||app.race.ticks)
            throw std::runtime_error("Initialization warmup incorrectly advanced sound or race timers");
        auto dsp=[&](){if(!app.audio.dspRuntime())throw std::runtime_error("Driving App lacks DSP runtime");return app.audio.dspRuntime()->diagnostics();};
        const auto initial=dsp();const auto family=selectOriginalEngineSound(profile).family;
        const auto expectedEffect=selections.families[family][0].effect;
        if(!initial.ringLatched||initial.ringCode!=2||!initial.activeProgram||initial.activeBank!=((expectedEffect>>8)&127)||initial.activePreset!=(expectedEffect&127))
            throw std::runtime_error("Fresh driving App did not install source engine DSP preset");
        constexpr std::array<unsigned,6> staticIds{46,48,49,49,50,51};
        for(unsigned slot=0;slot<staticIds.size();++slot){const auto* bank=app.audio.dspRuntime()->registeredBank(slot);
            if(!bank||bank->bankId!=staticIds[slot])throw std::runtime_error("Driving App resource registry order differs from source");}
        if(!app.audio.dspRuntime()->registeredBank(6)||!app.audio.dspRuntime()->registeredBank(7))throw std::runtime_error("Driving App missing engine/auxiliary bank registration");
        std::vector<short> recording;recording.reserve(frames*samplesPerFrame*2);
        double energy=0,simMs=0,mixMs=0,maxSimMs=0,maxMixMs=0;std::uint64_t clipped=0,slowMixBlocks=0;
        for(unsigned frame=0;frame<frames;++frame){
            DriverInput input;input.automatic=true;input.throttle=frame%180<130?1.f:0.f;input.brake=frame%180>=160?.4f:0.f;
            const auto projection=app.course.project(app.vehicle.position,app.segment);
            const float lookahead=std::max(12.f,app.vehicle.speed*.65f);
            const auto target=app.course.sample(projection.sample.distance+lookahead).center-app.vehicle.position;
            const float angle=wrapAngle(std::atan2(target.x,target.z)-app.vehicle.yaw);
            input.steer=-std::clamp(std::atan2(2*app.config.wheelbase*std::sin(angle),lookahead)/recoveredSteeringLimit,-.7f,.7f);
            // Exercise live steering/road-contact changes after the source
            // countdown, without substituting tire or engine sound events.
            if(frame>430){input.throttle=1;input.brake=0;input.steer=frame%240<100?.8f:frame%240<200?-.8f:0.f;}
            auto begin=std::chrono::steady_clock::now();app.simulate(input);
            if(app.audio.engineStatistics().simulationFrames!=frame+1)
                throw std::runtime_error("Countdown/race omitted an engine update");
            if(frame<179&&(app.race.phase!=RacePhase::Countdown||app.race.ticks||app.race.remaining6000!=initialRemaining||app.originalSession.vehicle().transmission.gear00))
                throw std::runtime_error("Countdown advanced race timing or enabled driving early");
            if(frame==179&&(app.race.phase!=RacePhase::Running||app.race.ticks!=1||app.race.originalStartDigit!=1))
                throw std::runtime_error("GO180 did not run its same-frame rules and solver");
            if(frame==180&&app.race.originalStartDigit!=0)throw std::runtime_error("Countdown tail did not publish GO");
            if(frame==239&&app.race.originalStartDigit!=-1)throw std::runtime_error("Countdown tail did not finish at240");
            auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();simMs+=elapsed;maxSimMs=std::max(maxSimMs,elapsed);
            begin=std::chrono::steady_clock::now();
            for(unsigned i=0;i<samplesPerFrame;++i){const auto sample=app.audio.renderStereo(app.vehicle.rpm,app.vehicle.throttle,app.vehicle.speed,app.vehicle.slip,true);
                for(auto value:sample){recording.push_back(value);energy+=double(value)*value;if(value>=32767||value<=-32767)++clipped;}}
            elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();mixMs+=elapsed;maxMixMs=std::max(maxMixMs,elapsed);if(elapsed>1000./60.)++slowMixBlocks;
            if(frame==700){
                const auto before=dsp();const auto engineBefore=app.audio.engineStatistics().pcmFrames;app.audio.scene(false,false,true);
                for(unsigned i=0;i<44100;++i)if(app.audio.renderStereo(0,0,0,0,true)!=std::array<short,2>{})throw std::runtime_error("Paused driving App mixed audio");
                const auto after=dsp();if(after.frames!=before.frames||after.wetEnergy!=before.wetEnergy||after.inputPeak!=before.inputPeak||after.loadCount!=before.loadCount||app.audio.engineStatistics().pcmFrames!=engineBefore)
                    throw std::runtime_error("Paused driving App advanced DSP/engine state");
                app.audio.scene(false,false,false);
            }
        }
        const auto& stats=app.audio.engineStatistics();const auto effect=dsp();const auto oneShot=app.audio.oneShotStatistics();
        const double dryRms=stats.pcmFrames?std::sqrt(stats.dryEnergy/double(stats.pcmFrames*2)):0;
        const double mixRms=std::sqrt(energy/double(recording.size()));
        const auto dspFrames=effect.frames-initial.frames;const double wetEnergy=effect.wetEnergy-initial.wetEnergy;
        const double wetRms=dspFrames?std::sqrt(wetEnergy/double(dspFrames*2)):0;
        report<<test.car<<','<<test.upgrade<<','<<test.course*2<<','<<test.wet<<','<<test.night<<','<<stats.simulationFrames<<','<<stats.pcmFrames<<','<<dryRms<<','<<mixRms<<','<<clipped<<','
            <<stats.releaseCues<<','<<stats.backfireCues<<','<<stats.tireCues<<','<<stats.tireCueMask<<','<<stats.droppedFrames<<','<<stats.underflowFrames<<','<<app.vehicle.travel<<','
            <<dspFrames<<','<<effect.ringCode<<','<<effect.activeBank<<','<<effect.activePreset<<','<<effect.cacheBank<<','<<effect.cachePreset<<','<<effect.loadCount<<','<<effect.clearCount<<','
            <<effect.inputPeak<<','<<wetEnergy<<','<<wetRms<<','<<simMs/frames<<','<<maxSimMs<<','<<mixMs/frames<<','<<maxMixMs<<','<<slowMixBlocks<<','
            <<oneShot.cues<<','<<oneShot.notes<<','<<oneShot.peakVoices<<','<<oneShot.dropped<<','<<oneShot.stolen<<'\n';report.flush();
        if(!app.originalHandling||stats.simulationFrames<600||stats.pcmFrames!=stats.simulationFrames*samplesPerFrame||dryRms<100||mixRms<100||clipped||stats.droppedFrames||stats.underflowFrames||app.vehicle.travel<10)
            throw std::runtime_error("Live driving/audio regression for car "+std::to_string(test.car));
        if(!effect.activeProgram||effect.ringCode!=2||!effect.inputPeak||wetEnergy<=0||!dspFrames)
            throw std::runtime_error("Live driving DSP did not receive/render source effects for car "+std::to_string(test.car));
        if(oneShot.cues<4||oneShot.notes<oneShot.cues||oneShot.dropped||oneShot.stolen)
            throw std::runtime_error("Live source one-shot cues missing or exhausted their voice partition");
        std::ofstream wav(output/("car"+std::to_string(test.car)+"-condition"+std::to_string(test.course*2)+"-dsp.wav"),std::ios::binary);
        const auto u16=[&](unsigned v){wav.put(char(v));wav.put(char(v>>8));};const auto u32=[&](unsigned v){u16(v);u16(v>>16);};
        wav.write("RIFF",4);u32(36+unsigned(recording.size()*2));wav.write("WAVEfmt ",8);u32(16);u16(1);u16(2);u32(44100);u32(176400);u16(4);u16(16);wav.write("data",4);u32(unsigned(recording.size()*2));
        wav.write(reinterpret_cast<const char*>(recording.data()),std::streamsize(recording.size()*2));if(!wav)throw std::runtime_error("Cannot save driving DSP preview");
    }
    std::cout<<"PASS three fresh-boot native App driving cases: AE86 dry Akina, R32 wet Akina, FD3S snow Akina;60 silent initialization steps, live countdown engine, neutral/timer gates, same-frame GO180 and240-frame HUD tail; original resource slots, DSP input/wet output, dry/mix levels, clipping, pause clock and CPU measured. No renderer or audio device opened; no saved driver profile writes.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
