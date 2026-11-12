// Included inside the challenger mixer test's anonymous namespace.
unsigned queuedCases=0,onlineCases=0,outputPhaseDiscriminators=0,resetAndGateCases=0;
void prepareRace(EngineAudio& a,const std::filesystem::path& root,const OriginalBattleProfile& profile){
    a.configure(root);a.selectOriginalEngine(root,profile);
    auto music=std::make_shared<OriginalAudioClip>();music->sampleRate=44100;music->channels=2;music->looping=true;
    music->samples.assign(4410*2,0);a.customRaceMusic=music;a.musicTrack=-2;a.scene(false,false,false);
}
OriginalEngineControlInput outputCheckInput(unsigned frame){
    return {1100.f+float((frame*997)%6900),frame%7<5?1.f:0.f,int(1+(frame/4)%4),frame==0};
}
void compareRelease(EngineAudio& interrupted,EngineAudio& uninterrupted,const std::filesystem::path& root,
    std::unique_ptr<OriginalIcsPlayer> expectedVoices,const char* context){
    const auto oldStream=interrupted.raceTimingStatistics().streamFrame;
    const auto oldControls=interrupted.engineStatistics().simulationFrames;
    const auto oldPcm=interrupted.engineStatistics().pcmFrames;
    const auto oldCueFrames=interrupted.oneShotStatistics().frames;
    const auto oldDsp=interrupted.dspRuntime()->diagnostics().frames;
    CueReference expected(root,interrupted.dspRuntime()->processor(),true,std::move(expectedVoices));
    interrupted.beginChallenger();
    for(unsigned frame=0;frame<2048;++frame){
        AudioOutputGains gains;
        // Exercise the release itself, not only gains after it ends.
        if(frame<23){gains.engine=2;gains.effects=0;gains.master=.4f;}
        else if(frame<67){gains.engine=0;gains.effects=2;}
        else if(frame<159){gains.engine=.31f;gains.effects=.7f;}
        interrupted.setOutputGains(gains);interrupted.enabled=frame<75||frame>93;
        const auto actual=interrupted.renderStereo(8000,1,50,1,true);
        const auto want=expected.render(gains,interrupted.enabled);
        if(actual!=want)throw std::runtime_error(std::string(context)+" release differs at sample "+std::to_string(frame)+
            " expected="+std::to_string(want[0])+","+std::to_string(want[1])+" actual="+std::to_string(actual[0])+","+std::to_string(actual[1]));
        ++compared;
    }
    check(interrupted.raceTimingStatistics().streamFrame==oldStream,"Release advanced suspended stream");
    check(interrupted.engineStatistics().simulationFrames==oldControls&&interrupted.engineStatistics().pcmFrames==oldPcm,"Release advanced engine control or synthesis statistics");
    check(interrupted.oneShotStatistics().frames==oldCueFrames&&interrupted.dspRuntime()->diagnostics().frames==oldDsp,"Release advanced suspended cue/DSP state");
    interrupted.setOutputGains({});interrupted.enabled=true;interrupted.endChallenger();
    for(unsigned i=0;i<1024;++i){
        check(interrupted.renderStereo(6500,1,40,0,true)==uninterrupted.renderStereo(6500,1,40,0,true),"Release changed resumed output phase/queued PCM");
        ++resumedFrames;
    }
}
void queuedOutput(const std::filesystem::path& root,unsigned car,unsigned blocks,unsigned consume,bool clearSends){
    auto profile=makeOriginalFreshBattleProfile();profile.setu(16,car);
    EngineAudio interrupted,uninterrupted;
    for(auto* a:{&interrupted,&uninterrupted})prepareRace(*a,root,profile);
    OriginalEnginePlayback source(root);source.select(profile);
    const unsigned written=1024+735*blocks,dropped=written>8820?written-8820:0;
    const unsigned available=written-dropped,head=dropped+std::min(consume,available);
    const bool sounding=consume&&consume<=available&&head>1024;
    std::unique_ptr<OriginalIcsPlayer> outputState;
    unsigned seedA=1,seedB=1,seedSource=1,at=1024;
    for(unsigned block=0;block<blocks;++block){
        const auto controls=outputCheckInput(block);
        interrupted.stepOriginalEngine(controls,seedA);interrupted.finishSoundFrame(seedA);
        uninterrupted.stepOriginalEngine(controls,seedB);uninterrupted.finishSoundFrame(seedB);
        source.step(controls,seedSource,{});source.finishSoundFrame();
        // The reference has no ring or checkpoints: capture it while rendering
        // the desired sample in sequence, before any later controls are applied.
        for(unsigned i=0;i<735;++i,++at){
            if(sounding&&at==head)outputState=std::make_unique<OriginalIcsPlayer>(source.snapshotVoices());
            source.renderFrame();
        }
    }
    if(sounding&&at==head)outputState=std::make_unique<OriginalIcsPlayer>(source.snapshotVoices());
    check(bool(outputState)==sounding,"Reference output boundary was not captured");
    check(interrupted.engineStatistics().droppedFrames==dropped,"Queue overflow fixture differs");
    check(seedA==seedB,"Identical queued engines consumed different RNG");
    if(clearSends){
        for(auto* a:{&interrupted,&uninterrupted})const_cast<OriginalAudioDspRuntime*>(a->dspRuntime())->select(22,127);
        if(outputState)outputState->clearDspSends();
    }
    for(unsigned i=0;i<consume;++i)
        check(interrupted.renderStereo(6500,1,40,0,true)==uninterrupted.renderStereo(6500,1,40,0,true),"Queued baseline PCM differs");
    if(outputState&&head<written){
        auto actualPhase=*outputState,futurePhase=source.snapshotVoices();
        if(clearSends)futurePhase.clearDspSends();
        bool different=false;
        for(unsigned i=0;i<64;++i)different|=actualPhase.renderFrame().dry!=futurePhase.renderFrame().dry;
        outputPhaseDiscriminators+=different;
    }
    compareRelease(interrupted,uninterrupted,root,std::move(outputState),"Queued engine");++queuedCases;
}
void confirmedOutput(const std::filesystem::path& root,unsigned car){
    auto profile=makeOriginalFreshBattleProfile();profile.setu(16,car);
    EngineAudio interrupted,uninterrupted;
    for(auto* a:{&interrupted,&uninterrupted})prepareRace(*a,root,profile);
    OriginalEnginePlayback source(root);source.select(profile);
    const auto tables=OriginalEngineTables::load(root);const auto config=configureProfileEngineSound(profile);
    OriginalEngineControlState controller;resetOriginalEngineControl(controller);unsigned seed=1;
    for(unsigned block=0;block<12;++block){
        const auto commands=stepOriginalEngineControl(tables,config,controller,outputCheckInput(block),seed);
        interrupted.applyConfirmedOnlineAudio(commands,{});uninterrupted.applyConfirmedOnlineAudio(commands,{});source.applyContinuous(commands);
        // Sustain output during a delayed packet; invent no control/RNG ticks.
        for(unsigned i=0;i<(block==11?11039u:509u);++i){
            check(interrupted.renderStereo(6500,1,40,0,true)==uninterrupted.renderStereo(6500,1,40,0,true),"Online baseline PCM differs");source.renderFrame();
        }
    }
    compareRelease(interrupted,uninterrupted,root,std::make_unique<OriginalIcsPlayer>(source.snapshotVoices()),"Confirmed online engine");++onlineCases;
}
void checkOutputClocks(const std::filesystem::path& root){
    for(unsigned car:{0u,3u,19u}){
        for(const auto [blocks,consume]:std::array<std::pair<unsigned,unsigned>,12>{{
            {1,0},{1,1023},{1,1024},{1,1025},{1,1759},{1,1760},
            {2,1758},{2,1759},{9,3301},{15,1},{25,2207},{25,8820}}})queuedOutput(root,car,blocks,consume,false);
        queuedOutput(root,car,9,3301,true);confirmedOutput(root,car);
    }
    check(queuedCases==39&&onlineCases==3&&outputPhaseDiscriminators>=18,"Output-clock cases did not distinguish synthesis ahead from heard PCM");
}
void checkReleaseGates(const std::filesystem::path& root){
    for(unsigned gate=0;gate<4;++gate){
        EngineAudio interrupted,uninterrupted;
        for(auto* a:{&interrupted,&uninterrupted}){
            prepareRace(*a,root,makeOriginalFreshBattleProfile());unsigned seed=1;
            for(unsigned block=0;block<4;++block){a->stepOriginalEngine(outputCheckInput(block),seed);a->finishSoundFrame(seed);}
            for(unsigned i=0;i<2100;++i)a->renderStereo(6500,1,40,0,true);
            if(gate==0)a->scene(false,false,false,false,false,true);
            else if(gate==1)a->renderStereo(6500,1,40,0,false);
            else if(gate==2){
                a->scene(false,false,true);a->renderStereo(6500,1,40,0,true);a->scene(false,false,false);
            }else{
                // Reconfiguration must discard old PCM as well as its sample
                // origin. Otherwise stale motor PCM survives without history.
                a->configure(root);
                for(unsigned i=0;i<1800;++i)check(a->renderStereo(6500,1,40,0,true)==std::array<short,2>{},"Reconfiguration retained old engine PCM");
            }
        }
        compareRelease(interrupted,uninterrupted,root,{},"Silent/reset engine");++resetAndGateCases;
    }
    // Pause during the authored release, then resume from the exact sample.
    EngineAudio paused,continuous;
    for(auto* a:{&paused,&continuous}){
        prepareRace(*a,root,makeOriginalFreshBattleProfile());a->setReplayEngine({6500,1,3,true});
        for(unsigned i=0;i<12000;++i)a->renderStereo(6500,1,40,0,true);
        a->beginChallenger();
    }
    for(unsigned i=0;i<13;++i)check(paused.renderStereo(0,0,0,0,false)==continuous.renderStereo(0,0,0,0,false),"Pause fixture initial release differs");
    paused.scene(false,false,true);
    for(unsigned i=0;i<2048;++i)check(paused.renderStereo(0,0,0,0,false)==std::array<short,2>{},"Paused challenger produced output");
    paused.scene(false,false,false);
    for(unsigned i=0;i<2048;++i)check(paused.renderStereo(0,0,0,0,false)==continuous.renderStereo(0,0,0,0,false),"Pause advanced challenger release/cue/DSP");
    ++resetAndGateCases;
}
