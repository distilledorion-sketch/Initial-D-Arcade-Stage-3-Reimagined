//Arranged snow-course replay, using source-driven poses. It does not change
//Legend course availability or claim a natural Legend finish on Akina Snow.
{
    prepareEndingWeatherUnityFixture(*app,482);
    const auto clip=app->endingReplay;const auto physics=app->originalSession.rollbackDigest();
    const auto profile=app->battleProfile.words;
    const auto state=[](const OriginalSnowPresentation& snow){
        std::vector<unsigned> result{unsigned(snow.active()),snow.updates(),snow.fields().cursor};
        for(unsigned i=0;i<320;++i){const auto& p=snow.fields().particles[i];
            for(const auto* array:{&p.position,&p.velocity,&p.normal})for(float v:*array)result.push_back(std::bit_cast<unsigned>(v));
            result.insert(result.end(),{unsigned(p.life),p.active,unsigned(snow.fields().visible[i])});}
        for(const auto& corner:snow.fields().quadOffsets)for(float v:corner)result.push_back(std::bit_cast<unsigned>(v));
        return result;
    };
    std::vector<unsigned> expected;unsigned expectedRandom=0;
    for(unsigned fps:{30u,60u,144u,240u}){
        app->endingReplay=clip;app->beginEnding();
        check(app->endingSnow.active()&&!app->endingRain.active(),"Snow/rain admission overlap");
        for(unsigned render=0;render<fps*10;++render)app->advanceEnding(1./fps);
        check(app->endingSnow.updates()==600,"Snow source cadence");
        if(fps==30){expected=state(app->endingSnow);expectedRandom=app->endingRandom;}
        else check(state(app->endingSnow)==expected&&app->endingRandom==expectedRandom,"Snow changes with host frame rate");
    }
    check(app->renderEnding(0),"Render snow ending");
    check(app->endingSnowTextureBase==app->endingSmokeTextureBase+app->smokeTextures.size(),"Snow texture overlap");
    Mesh mesh;const auto flakes=app->endingSnow.append(mesh,app->endingSnowTextureBase);
    check(flakes>0&&mesh.vertices.size()==flakes*6,"Snow geometry missing");
    for(const auto& range:mesh.ranges)check(range.texture==app->endingSnowTextureBase&&range.original&&range.emissive&&
        range.viewMask==1&&!range.carLighting&&!range.courseLighting,"Snow material binding");
    check(app->renderer.saveBitmap((out/"ending-snow.bmp").wstring()),"Capture snow ending");
    const auto saved=app->endingSnow;app->endingSnow.reset();check(app->renderEnding(0),"Snow visibility control");
    check(app->renderer.saveBitmap((out/"ending-snow-disabled.bmp").wstring()),"Capture disabled snow");
    app->endingSnow=saved;check(app->renderEnding(0),"Restore snow ending");
    check(app->renderer.saveBitmap((out/"ending-snow-restored.bmp").wstring()),"Capture restored snow");
    app->paused=true;app->advanceEnding(10);app->renderEnding(0);app->paused=false;
    check(state(app->endingSnow)==expected&&app->endingRandom==expectedRandom,"Snow repaint/pause changed state");
    check(app->originalSession.rollbackDigest()==physics&&app->battleProfile.words==profile,"Ending snow changed race/save state");
    std::cout<<"Snow ending600 ticks: "<<flakes<<" flakes / "<<mesh.ranges.size()<<" ranges / "<<mesh.vertices.size()<<" vertices\n";
    app->start();check(!app->endingSnow.active()&&!app->endingSnow.updates(),"Restart retained ending snow");
}
