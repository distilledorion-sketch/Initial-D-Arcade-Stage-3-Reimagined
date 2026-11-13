// Reach an ending from real source-driven poses in an isolated diagnostic.
// The early highlight threshold is the only arranged recording condition;
// this does not claim a completed second Legend race or use ordinary saves.
void prepareEndingWeatherUnityFixture(App& app,unsigned scene){
    if(scene<470||scene>487)throw std::invalid_argument("Ending weather fixture");
    static std::optional<OriginalRainPresentation> saved;
    static std::optional<OriginalSnowPresentation> savedSnow;
    static unsigned prepared=~0u,seed=0;
    static std::uint64_t physics=0;
    const unsigned scenario=(scene-470)/3,phase=(scene-470)%3;
    const bool snow=scenario>=4,wet=snow||(scenario&1)!=0;
    const unsigned sample=(scenario<2||scenario==4)?180:600;
    if(phase==0){
        if(app.multiplayer.active)throw std::logic_error("Ending diagnostic cannot use a network peer");
        app.validationMode=true;app.replayPlaybackActive=false;app.replayRecordingFlags=0;
        app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
        app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app.frontend.car=0;app.frontend.automatic=true;
        app.frontend.battleProfile.setu(16,0);app.frontend.battleProfile.setByte(164,63);
        original::selectOriginalRival(app.frontend.battleProfile,8);
        //An arranged snow-course replay exercises the separate owner; this
        //does not add snow to the Legend menu or claim a natural snow ending.
        if(snow){app.frontend.battleProfile.setu(4,8);app.frontend.battleProfile.setu(28,16);app.frontend.battleProfile.setu(12,0);}
        app.frontend.battleProfile.setu(32,wet?1:0);app.start();
        if(app.courseIndex!=(snow?8:2)||!app.night||app.wet!=wet)throw std::runtime_error("Wrong weather fixture conditions");
        EndingReplay clip;
        for(unsigned tick=0;tick<1400&&clip.highlightSize()<600;++tick){
            const auto& drive=app.originalSession.vehicle().drive;
            const auto& points=app.originalSession.path().points;
            std::size_t nearest=0;float best=1e30f;
            for(std::size_t p=0;p<points.size();++p){
                const float dx=points[p][0]-drive.f(0),dz=points[p][2]-drive.f(8),d=dx*dx+dz*dz;
                if(d<best){best=d;nearest=p;}
            }
            auto target=nearest;float distance=0;const float look=std::max(10.f,drive.f(0x238)*.65f);
            while(distance<look&&target+1<points.size()){
                const float dx=points[target+1][0]-points[target][0],dz=points[target+1][2]-points[target][2];
                distance+=std::sqrt(dx*dx+dz*dz);++target;
            }
            const auto& point=points[target];const float wanted=std::atan2(-(point[0]-drive.f(0)),-(point[2]-drive.f(8)));
            DriverInput input;input.automatic=true;input.throttle=1;
            input.steer=std::clamp(-std::remainder(wanted-drive.f(0x10),2*pi)*.85f,-.8f,.8f);
            const auto size=app.endingReplay.size();app.simulate(input);
            if(app.endingReplay.size()>size)clip.record(app.endingReplay.recentFrame(unsigned(app.endingReplay.size()-1)),*app.courseLightFsca,.7f,0);
        }
        if(clip.highlightSize()!=600)throw std::runtime_error("Ending fixture did not record real driving");
        app.endingReplay=clip;app.beginEnding();
        for(unsigned tick=0;tick<sample;++tick)app.advanceEnding(1./60);
        if(!app.endingActive||app.endingRain.active()!=(wet&&!snow)||app.endingSnow.active()!=snow)throw std::runtime_error("Ending weather admission");
        physics=app.originalSession.rollbackDigest();seed=app.endingRandom;
        saved.reset();savedSnow.reset();prepared=scenario;
    }else{
        if(prepared!=scenario)throw std::logic_error("Ending comparison before preparation");
        if(phase==1){saved=app.endingRain;savedSnow=app.endingSnow;app.endingRain.reset();app.endingSnow.reset();}
        else{if(!saved||!savedSnow)throw std::logic_error("Ending restore before comparison");app.endingRain=*saved;app.endingSnow=*savedSnow;}
    }
    app.input={};
    if(!app.renderEnding(0))throw std::runtime_error(app.renderer.error);
    if(app.originalSession.rollbackDigest()!=physics||app.endingRandom!=seed)
        throw std::runtime_error("Ending diagnostic render advanced physics or RNG");
    Mesh weather;const auto counts=app.endingRain.append(weather,app.endingRainTextureBase,*app.courseLightFsca);
    const unsigned flakes=app.endingSnow.append(weather,app.endingSnowTextureBase);
    Mesh lamps;const unsigned ribbons=app.endingLampTrails.append(lamps,app.endingLampTrailTextureBase);
    const bool enabled=wet&&phase!=1;
    if(enabled&&(snow?!flakes:(!counts.streaks||!counts.splashes||!counts.mist)))throw std::runtime_error("Missing weather particle family");
    for(const auto& range:weather.ranges)
        if(!range.original||!range.emissive||range.viewMask!=1||range.carLighting||range.courseLighting||
           range.texture<app.endingRainTextureBase||range.texture>=app.endingRainTextureBase+11)
            throw std::runtime_error("Rain material/texture binding");
    std::ofstream out(app.saveRoot.parent_path()/("ending-weather-"+std::to_string(scenario)+"-"+std::to_string(phase)+".json"));
    out<<"{\"sample\":"<<sample<<",\"wet\":"<<(wet?"true":"false")<<",\"snow\":"<<(snow?"true":"false")<<",\"flakes\":"<<flakes<<",\"enabled\":"<<(enabled?"true":"false")
       <<",\"textureBase\":"<<app.endingRainTextureBase<<",\"streaks\":"<<counts.streaks<<",\"splashes\":"<<counts.splashes
       <<",\"mist\":"<<counts.mist<<",\"glare\":"<<counts.glare<<",\"ranges\":"<<weather.ranges.size()
       <<",\"vertices\":"<<weather.vertices.size()<<",\"random\":"<<seed<<",\"updates\":"<<(snow?app.endingSnow.updates():app.endingRain.updates())<<"}\n";
    std::ofstream lampOut(app.saveRoot.parent_path()/("ending-lamps-"+std::to_string(scenario)+"-"+std::to_string(phase)+".json"));
    lampOut<<"{\"sample\":"<<sample<<",\"lampBase\":"<<app.endingLampTrailTextureBase<<",\"lampRanges\":"<<lamps.ranges.size()
        <<",\"lampVertices\":"<<lamps.vertices.size()<<",\"lampRibbons\":"<<ribbons<<",\"lampUpdates\":"<<app.endingLampTrails.updates()<<"}\n";
}
