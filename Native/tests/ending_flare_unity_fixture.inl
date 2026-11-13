// Real moving Legend poses with an early diagnostic highlight. Positive
// daylight draw plus night/rain admission controls; no ordinary saves.
void prepareEndingFlareUnityFixture(App& app,unsigned scene){
    if(scene<510||scene>518)throw std::invalid_argument("Ending flare fixture");
    static OriginalEndingLensFlare saved;
    static unsigned prepared=~0u,seed=0,sample=0;
    static std::uint64_t physics=0;
    const unsigned scenario=(scene-510)/3,phase=(scene-510)%3;
    if(phase==0){
        if(app.multiplayer.active)throw std::logic_error("Ending diagnostic cannot use a peer");
        app.validationMode=true;app.replayPlaybackActive=false;app.replayRecordingFlags=0;
        auto& f=app.frontend;f.gameMode=original::OriginalGameMode::LegendOfTheStreets;
        f.battleProfile=original::makeOriginalFreshBattleProfile();f.car=0;f.automatic=true;
        f.battleProfile.setu(16,0);f.battleProfile.setByte(164,63);
        original::selectOriginalRival(f.battleProfile,0);
        f.battleProfile.setu(8,scenario==1);f.battleProfile.setu(32,scenario==2);app.start();
        EndingReplay clip;
        for(unsigned tick=0;tick<1400&&clip.highlightSize()<600;++tick){
            const auto& drive=app.originalSession.vehicle().drive;const auto& points=app.originalSession.path().points;
            std::size_t nearest=0;float best=1e30f;
            for(std::size_t p=0;p<points.size();++p){const float dx=points[p][0]-drive.f(0),dz=points[p][2]-drive.f(8),distance=dx*dx+dz*dz;
                if(distance<best){best=distance;nearest=p;}}
            auto target=nearest;float distance=0;const float look=std::max(10.f,drive.f(0x238)*.65f);
            while(distance<look&&target+1<points.size()){
                const float dx=points[target+1][0]-points[target][0],dz=points[target+1][2]-points[target][2];
                distance+=std::sqrt(dx*dx+dz*dz);++target;}
            const auto& point=points[target];const float wanted=std::atan2(-(point[0]-drive.f(0)),-(point[2]-drive.f(8)));
            DriverInput input;input.automatic=true;input.throttle=1;
            input.steer=std::clamp(-std::remainder(wanted-drive.f(0x10),2*pi)*.85f,-.8f,.8f);
            const auto size=app.endingReplay.size();app.simulate(input);
            if(app.endingReplay.size()>size)clip.record(app.endingReplay.recentFrame(unsigned(app.endingReplay.size()-1)),*app.courseLightFsca,.7f,0);
        }
        if(clip.highlightSize()!=600)throw std::runtime_error("Flare fixture did not record driving");
        app.endingReplay=clip;app.beginEnding();
        for(sample=1;sample<=600;++sample){app.advanceEnding(1./60);if(sample>=180&&(scenario!=0||app.endingFlare.visible()))break;}
        if(sample>600||app.endingFlare.visible()!=(scenario==0))throw std::runtime_error("Flare source admission fixture");
        prepared=scenario;seed=app.endingRandom;physics=app.originalSession.rollbackDigest();saved=app.endingFlare;
    }else{
        if(prepared!=scenario)throw std::logic_error("Flare comparison before preparation");
        if(phase==1)app.endingFlare.reset();else app.endingFlare=saved;
    }
    app.input={};if(!app.renderEnding(0))throw std::runtime_error(app.renderer.error);
    if(app.endingRandom!=seed||app.originalSession.rollbackDigest()!=physics)throw std::runtime_error("Flare repaint changed RNG/physics");
    Mesh flare;const auto pieces=app.endingFlare.append(flare,app.endingFlareTextureBase);
    std::ofstream out(app.saveRoot.parent_path()/("ending-flare-"+std::to_string(scenario)+"-"+std::to_string(phase)+".json"));
    out<<"{\"sample\":"<<sample<<",\"pieces\":"<<pieces<<",\"textureBase\":"<<app.endingFlareTextureBase
       <<",\"ranges\":"<<flare.ranges.size()<<",\"vertices\":"<<flare.vertices.size()<<",\"random\":"<<seed<<"}\n";
}
