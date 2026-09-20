#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("game-root output-directory required");
    App app;app.root=argv[1];app.validationMode=true;app.settings();app.drivingView=OriginalDrivingView::Bumper;
    const fs::path output=argv[2];fs::create_directories(output);
    app.originalCamera=OriginalChaseCamera::load(app.root);app.bumperCamera=OriginalChaseCamera::load(app.root,OriginalDrivingView::Bumper);
    app.frontend.initialize(app.root,true);app.hud.loadOriginal(app.root);app.audio.configure(app.root);app.audio.enabled=true;app.audio.scene(true,false,false);app.load();
    if(!app.renderer.initialize(nullptr,1280,720,false))throw std::runtime_error(app.renderer.error);
    std::ofstream report(output/"application.csv");report<<"stage,menu,cue,songs_started,sample_position,peak_voices,fades,rms,clipped,dsp_frames,ring_code,active_bank,active_preset,cache_bank,cache_preset,dsp_loads,dsp_clears,input_peak,wet_energy,interval_wet_energy\n";
    auto dsp=[&](){if(!app.audio.dspRuntime())throw std::runtime_error("Application DSP runtime missing");return app.audio.dspRuntime()->diagnostics();};
    std::array<std::uint64_t,2> firstSendSample{};
    for(unsigned cue=0;cue<2;++cue){OriginalMusicSequence sequence;sequence.load(app.root,cue);bool found=false;
        for(const auto& event:sequence.events())if(event.kind==OriginalMusicSequenceEventKind::NoteOn&&(event.parameters.effectSend>>4)){
            firstSendSample[cue]=std::uint64_t(event.tick)*44;found=true;break;}
        if(!found)throw std::runtime_error("Source selection score has no authored effect sends");}
    double previousWetEnergy=0;
    double energy=0,mixMs=0;std::uint64_t sampleCount=0,clipped=0,mixBlocks=0;
    auto tick=[&]{
        if(!app.menu&&!app.paused)app.simulate({});
        if(!app.render(1./60.))throw std::runtime_error(app.renderer.error);
        app.audio.scene(app.menu,app.race.phase==RacePhase::Finished,app.paused,app.race.timeUp);
        const auto begin=std::chrono::steady_clock::now();
        for(unsigned i=0;i<735;++i){const auto frame=app.audio.renderStereo(app.vehicle.rpm,app.vehicle.throttle,app.vehicle.speed,0,!app.menu&&!app.paused);
            for(auto value:frame){energy+=double(value)*value;++sampleCount;if(value==32767||value==-32767)++clipped;}
        }
        mixMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();++mixBlocks;
    };
    auto capture=[&](const char* name){
        const auto& stats=app.audio.selectionStatistics();
        const auto effect=dsp();const auto wetDelta=effect.wetEnergy-previousWetEnergy;previousWetEnergy=effect.wetEnergy;
        const double rms=sampleCount?std::sqrt(energy/sampleCount):0;
        report<<name<<','<<app.menu<<','<<stats.cue<<','<<stats.songsStarted<<','<<app.audio.selectionSamplePosition()<<','<<stats.peakVoices<<','<<stats.fades<<','
            <<rms<<','<<clipped<<','<<effect.frames<<','<<effect.ringCode<<','<<effect.activeBank<<','<<effect.activePreset<<','<<effect.cacheBank<<','<<effect.cachePreset<<','
            <<effect.loadCount<<','<<effect.clearCount<<','<<effect.inputPeak<<','<<effect.wetEnergy<<','<<wetDelta<<'\n';report.flush();
        if(!app.renderer.saveBitmap((output/(std::string(name)+".bmp")).wstring()))throw std::runtime_error(app.renderer.error);
        if(clipped)throw std::runtime_error("Application selection mix clipped");energy=0;sampleCount=clipped=0;
        if(app.menu&&(!app.audio.selectionPlaying()||rms<1))throw std::runtime_error("Application selection music is silent");
        if(!effect.ringLatched||effect.ringCode!=2)throw std::runtime_error("Application lost PACK20 boot ring configuration");
        if(app.menu){
            const unsigned expectedBank=stats.cue==0?1u:2u;
            if(!effect.activeProgram||effect.activeBank!=expectedBank||effect.activePreset!=0||effect.cacheBank!=expectedBank||effect.cachePreset!=0)
                throw std::runtime_error("Application selection score did not select its original DSP program/cache");
            // Source TYPE starts dry: first authored send is sample55484;
            // SELECT's is8448. Full-score comparison found the first nonzero
            // input/return one sample later in each. Keep early captures dry.
            if(app.audio.selectionSamplePosition()>firstSendSample[unsigned(stats.cue)]+1&&(!effect.inputPeak||wetDelta<=0))
                throw std::runtime_error("Application selection DSP has no authored input/real wet return");
        }
    };
    auto ready=[&]{unsigned guard=0;while(!app.frontend.inputReady()&&guard++<600)tick();if(!app.frontend.inputReady())throw std::runtime_error("Selection input never became ready");};
    auto confirmTo=[&](FrontendStage expected){ready();const auto previous=app.frontend.stage;app.frontend.confirm();unsigned guard=0;
        while(app.menu&&app.frontend.stage==previous&&guard++<600)tick();
        if(!app.menu||app.frontend.stage!=expected)throw std::runtime_error("Unexpected actual application selection transition");
    };
    if(!app.render(0))throw std::runtime_error(app.renderer.error);
    confirmTo(FrontendStage::Make);for(unsigned i=0;i<60;++i)tick();capture("make-type");
    if(app.audio.selectionStatistics().songsStarted!=1||app.audio.selectionStatistics().cue!=0)throw std::runtime_error("Maker did not start TYPE");
    confirmTo(FrontendStage::Car);for(unsigned i=0;i<60;++i)tick();capture("car-type");
    if(app.audio.selectionStatistics().songsStarted!=1)throw std::runtime_error("Car menu restarted TYPE");
    confirmTo(FrontendStage::Transmission);for(unsigned i=0;i<60;++i)tick();capture("transmission-type");
    confirmTo(FrontendStage::Mode);for(unsigned i=0;i<60;++i)tick();capture("mode-select");
    if(app.audio.selectionStatistics().songsStarted!=2||app.audio.selectionStatistics().cue!=1)throw std::runtime_error("Mode did not switch to SELECT");
    const auto pausedSample=app.audio.selectionSamplePosition();const auto pausedDsp=dsp();app.audio.scene(true,false,true);
    for(unsigned i=0;i<44100;++i)if(app.audio.renderStereo(800,0,0,0,false)!=std::array<short,2>{})throw std::runtime_error("Paused selection mixed audio");
    if(app.audio.selectionSamplePosition()!=pausedSample)throw std::runtime_error("Paused selection advanced song");
    {const auto after=dsp();if(after.frames!=pausedDsp.frames||after.wetEnergy!=pausedDsp.wetEnergy||after.inputPeak!=pausedDsp.inputPeak||after.loadCount!=pausedDsp.loadCount||after.clearCount!=pausedDsp.clearCount)
        throw std::runtime_error("Paused application advanced DSP state");}
    app.audio.scene(true,false,false);
    ready();for(unsigned i=0;app.frontend.gameMode!=original::OriginalGameMode::TimeAttack&&i<4;++i)app.frontend.change(1);
    confirmTo(FrontendStage::Course);app.frontend.course=3;for(unsigned i=0;i<60;++i)tick();capture("course-select");
    confirmTo(FrontendStage::Route);for(unsigned i=0;i<20;++i)tick();
    confirmTo(FrontendStage::Weather);for(unsigned i=0;i<20;++i)tick();
    confirmTo(FrontendStage::Time);for(unsigned i=0;i<20;++i)tick();capture("time-select");
    if(app.audio.selectionStatistics().songsStarted!=2)throw std::runtime_error("Course/time screens restarted SELECT");
    ready();const auto fadesBeforeExit=app.audio.selectionStatistics().fades;app.frontend.confirm();unsigned guard=0;bool fadedInMenu=false;
    while(app.menu&&guard++<600){tick();if(app.menu&&app.audio.selectionStatistics().fades>fadesBeforeExit)fadedInMenu=true;}
    if(!fadedInMenu||app.audio.selectionStatistics().fades!=fadesBeforeExit+1)throw std::runtime_error("Final selection skipped or duplicated the source music fade");
    if(app.menu||app.audio.selectionPlaying())throw std::runtime_error("Race retained selection playback");
    constexpr std::array<unsigned,6> raceBanks{46,48,49,49,50,51};
    for(unsigned slot=0;slot<raceBanks.size();++slot){const auto* bank=app.audio.dspRuntime()->registeredBank(slot);
        if(!bank||bank->bankId!=raceBanks[slot])throw std::runtime_error("Outgoing selection unloaded a new race sound bank");}
    for(unsigned i=0;i<360;++i)tick();capture("bumper-race");
    const auto oneShot=app.audio.oneShotStatistics();
    if(oneShot.cues<4||oneShot.notes<oneShot.cues||oneShot.dropped||oneShot.stolen)throw std::runtime_error("Countdown/GO sound cues missing after selection");
    if(app.drivingView!=OriginalDrivingView::Bumper)throw std::runtime_error("Default bumper view changed");
    std::ofstream timing(output/"mixer-time.json");timing<<"{\"blocks\":"<<mixBlocks<<",\"frames_per_block\":735,\"mean_cpu_ms\":"<<mixMs/mixBlocks<<"}\n";
    std::cout<<"PASS actual game Start, TYPE/SELECT score and DSP program/cache continuity, real wet returns, frozen song/DSP pause, menu mix without clipping, race stop and bumper gameplay. No audio device or saved driver profile writes. Mean735-frame mixer CPU "<<mixMs/mixBlocks<<"ms\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
