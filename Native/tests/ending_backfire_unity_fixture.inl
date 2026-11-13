//Arranged early Legend highlight with real, source-driven Evo III poses and
//published actor flags. No invented flame events or ordinary save changes.
//Scenario3 moves only the diagnostic camera to the rival's recorded pose:
//the normal ending camera follows the player and can hide a distant rival.
void prepareEndingBackfireUnityFixture(App& app,unsigned scene){
    if(scene<490||scene>501)throw std::invalid_argument("Ending backfire fixture");
    static OriginalEndingBackfire saved;
    static unsigned prepared=~0u,seed=0,sample=0;
    static std::uint64_t physics=0;
    const unsigned scenario=(scene-490)/3,phase=(scene-490)%3;
    if(phase==0){
        if(app.multiplayer.active)throw std::logic_error("Ending diagnostic cannot use a network peer");
        app.validationMode=true;app.replayPlaybackActive=false;app.replayRecordingFlags=0;
        auto& f=app.frontend;f.gameMode=original::OriginalGameMode::LegendOfTheStreets;
        f.battleProfile=original::makeOriginalFreshBattleProfile();f.car=19;f.automatic=true;
        f.battleProfile.setu(16,19);f.battleProfile.setByte(164,63);
        f.battleProfile.setByte(162,std::uint8_t(scenario%3+1));f.battleProfile.setByte(166,1);
        original::selectOriginalRival(f.battleProfile,11);app.start();
        EndingReplay clip;unsigned playerFlags=0,rivalFlags=0;
        for(unsigned tick=0;tick<3000&&clip.highlightSize()<1800;++tick){
            const auto& drive=app.originalSession.vehicle().drive;const auto& points=app.originalSession.path().points;
            std::size_t nearest=0;float best=1e30f;
            for(std::size_t p=0;p<points.size();++p){const float dx=points[p][0]-drive.f(0),dz=points[p][2]-drive.f(8),d=dx*dx+dz*dz;
                if(d<best){best=d;nearest=p;}}
            auto target=nearest;float distance=0;const float look=std::max(10.f,drive.f(0x238)*.65f);
            while(distance<look&&target+1<points.size()){
                const float dx=points[target+1][0]-points[target][0],dz=points[target+1][2]-points[target][2];
                distance+=std::sqrt(dx*dx+dz*dz);++target;}
            const auto& point=points[target];const float wanted=std::atan2(-(point[0]-drive.f(0)),-(point[2]-drive.f(8)));
            DriverInput input;input.automatic=true;input.throttle=tick%120<90?1.f:0.f;
            input.steer=std::clamp(-std::remainder(wanted-drive.f(0x10),2*pi)*.85f,-.8f,.8f);
            //05C13A captures the already-published actor before the next
            //driving tick; comparing its flag with the new tick is one late.
            const std::array<unsigned,2> recordedFlags{app.presentedSession().publishedActors().player0C8FF388[23],app.renderedRivalActor()[23]};
            const auto size=app.endingReplay.size();app.simulate(input);
            if(app.endingReplay.size()>size){
                const auto frame=app.endingReplay.recentFrame(unsigned(app.endingReplay.size()-1));
                if(frame.cars[0].actorFlags!=recordedFlags[0]||frame.cars[1].actorFlags!=recordedFlags[1])
                    throw std::runtime_error("Ending actor trigger capture differs from source publication");
                playerFlags+=bool(frame.cars[0].actorFlags&2);rivalFlags+=bool(frame.cars[1].actorFlags&2);
                clip.record(frame,*app.courseLightFsca,.7f,0);
            }
        }
        if(clip.highlightSize()!=1800||!playerFlags)throw std::runtime_error("Ending backfire fixture did not record trigger-bearing driving");
        app.endingReplay=clip;app.beginEnding();
        if(!app.endingBackfire.enabled()[0]||!app.endingBackfire.enabled()[1])throw std::runtime_error("Missing tuned Evo ending owner");
        unsigned best=0;sample=0;
        for(unsigned tick=0;tick<1800;++tick){
            app.advanceEnding(1./60);if(tick<180)continue;
            const auto& fields=app.endingBackfire.fields();
            const unsigned score=unsigned(fields.cars[0].presentedFrame>=0)*(scenario==3?1:2)+
                unsigned(fields.cars[1].presentedFrame>=0)*(scenario==3?2:1);
            if(score>best){best=score;sample=tick+1;}if(best==3)break;
        }
        if(best<2)throw std::runtime_error("Recorded ending never produced the selected driver's backfire");
        app.endingReplay=clip;app.beginEnding();for(unsigned tick=0;tick<sample;++tick)app.advanceEnding(1./60);
        if(scenario==3){
            const auto& frame=app.endingReplay.playbackFrame(sample-1);const auto& world=frame.cars[1].visualWorld;
            constexpr auto e=OriginalEndingCamera::eyeOffset,t=OriginalEndingCamera::targetOffset;
            const auto eye=original::transformOriginalPoint(world,{e.x,e.y,e.z});
            const auto target=original::transformOriginalPoint(world,{t.x,t.y,t.z});
            app.endingCamera.reset({eye[0],eye[1],eye[2]},{target[0],target[1],target[2]},app.endingRandom);
            app.updateEndingLighting(frame);
        }
        saved=app.endingBackfire;physics=app.originalSession.rollbackDigest();seed=app.endingRandom;prepared=scenario;
        std::ofstream(app.saveRoot.parent_path()/("ending-backfire-"+std::to_string(scenario)+"-recording.txt"))
            <<"sample="<<sample<<" recorded flags="<<playerFlags<<'/'<<rivalFlags<<'\n';
    }else{
        if(prepared!=scenario)throw std::logic_error("Ending backfire comparison before preparation");
        if(phase==1)app.endingBackfire.reset();else app.endingBackfire=saved;
    }
    app.input={};if(!app.renderEnding(0))throw std::runtime_error(app.renderer.error);
    if(app.originalSession.rollbackDigest()!=physics||app.endingRandom!=seed)throw std::runtime_error("Ending backfire repaint changed physics or RNG");
    const auto& frame=app.endingReplay.playbackFrame(sample-1);Mesh flame;
    const unsigned count=app.endingBackfire.append(flame,app.backfire,frame,app.endingBackfireTextureBase);
    if((phase==1)!=(count==0))throw std::runtime_error("Backfire comparison did not change submission");
    std::ofstream out(app.saveRoot.parent_path()/("ending-backfire-"+std::to_string(scenario)+"-"+std::to_string(phase)+".json"));
    out<<"{\"sample\":"<<sample<<",\"textureBase\":"<<app.endingBackfireTextureBase<<",\"flames\":"<<count
        <<",\"ranges\":"<<flame.ranges.size()<<",\"vertices\":"<<flame.vertices.size()<<",\"random\":"<<seed
        <<",\"updates\":"<<app.endingBackfire.fields().updates<<",\"playerFrame\":"<<app.endingBackfire.fields().cars[0].presentedFrame
        <<",\"rivalFrame\":"<<app.endingBackfire.fields().cars[1].presentedFrame<<"}\n";
}
