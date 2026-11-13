// Record genuine Usui contact bytes, then replay the same frame with the leaf
// owner present, removed and restored. No contact/intensity overrides or saves.
void prepareEndingLeafUnityFixture(App& app,unsigned scene){
    if(scene<640||scene>642)throw std::invalid_argument("Ending leaf fixture");
    static OriginalEndingLeaves saved;
    static unsigned seed=0;
    static std::uint64_t physics=0;
    static bool prepared=false;
    const unsigned phase=scene-640;
    if(phase==0){
        if(app.multiplayer.active)throw std::logic_error("Ending diagnostic cannot use a peer");
        app.validationMode=true;app.replayPlaybackActive=false;app.replayRecordingFlags=0;
        auto& f=app.frontend;f.gameMode=original::OriginalGameMode::LegendOfTheStreets;
        f.battleProfile=original::makeOriginalFreshBattleProfile();f.car=0;f.automatic=true;
        f.battleProfile.setu(16,0);f.battleProfile.setByte(164,63);
        original::selectOriginalRival(f.battleProfile,original::originalLegendRivalId(1,0));
        f.battleProfile.setu(8,0);f.battleProfile.setu(32,0);app.start();
        if(app.courseIndex!=1||app.wet||app.night)throw std::runtime_error("Usui leaf conditions");
        EndingReplay clip;unsigned contacts=0;
        for(unsigned tick=0;tick<2200&&clip.highlightSize()<1400;++tick){
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
            if(app.endingReplay.size()>size){
                const auto& frame=app.endingReplay.recentFrame(unsigned(app.endingReplay.size()-1));
                for(auto surface:frame.cars[0].wheelSurfaces)contacts+=original::originalReplayLeafSurface(surface);
                clip.record(frame,*app.courseLightFsca,.7f,0);
            }
        }
        if(clip.highlightSize()!=1400||contacts==0)throw std::runtime_error("Leaf fixture did not record source contacts");
        app.endingReplay=clip;app.beginEnding();
        for(unsigned tick=0;tick<1305;++tick)app.advanceEnding(1./60);
        if(app.endingLeaves.system().order().empty())throw std::runtime_error("Leaf fixture has no source particles");
        prepared=true;seed=app.endingRandom;physics=app.originalSession.rollbackDigest();saved=app.endingLeaves;
    }else{
        if(!prepared)throw std::logic_error("Leaf comparison before preparation");
        if(phase==1)app.endingLeaves.reset();else app.endingLeaves=saved;
    }
    app.input={};if(!app.renderEnding(0))throw std::runtime_error(app.renderer.error);
    if(app.endingRandom!=seed||app.originalSession.rollbackDigest()!=physics)throw std::runtime_error("Leaf repaint changed RNG/physics");
    Mesh mesh;const auto leaves=app.endingLeaves.append(mesh,app.endingLeafTextureBase,*app.courseLightFsca);
    std::ofstream out(app.saveRoot.parent_path()/("ending-leaf-"+std::to_string(phase)+".json"));
    out<<"{\"sample\":1305,\"leaves\":"<<leaves<<",\"textureBase\":"<<app.endingLeafTextureBase
       <<",\"ranges\":"<<mesh.ranges.size()<<",\"vertices\":"<<mesh.vertices.size()<<",\"random\":"<<seed<<"}\n";
}
