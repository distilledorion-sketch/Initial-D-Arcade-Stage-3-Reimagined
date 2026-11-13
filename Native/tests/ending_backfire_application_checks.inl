{
    const auto state=[](const OriginalEndingBackfire& owner){
        std::vector<unsigned> out{unsigned(owner.active()),owner.fields().updates,owner.fields().primarySoundRequest};
        for(const auto& car:owner.fields().cars)out.insert(out.end(),{unsigned(car.active),car.frame,std::bit_cast<unsigned>(car.variation),unsigned(car.presentedFrame)});
        return out;
    };
    for(unsigned scenario=0;scenario<4;++scenario){
        for(unsigned phase=0;phase<3;++phase){
            prepareEndingBackfireUnityFixture(*app,490+scenario*3+phase);
            check(app->renderer.saveBitmap((out/("ending-backfire-"+std::to_string(scenario)+"-"+std::to_string(phase)+".bmp")).wstring()),"Capture ending backfire comparison");
        }
        const auto clip=app->endingReplay;const auto physics=app->originalSession.rollbackDigest();const auto profile=app->battleProfile.words;
        const auto sample=app->ending.timeline.totalFrame;const auto fields=state(app->endingBackfire);const auto random=app->endingRandom;
        const auto& frame=clip.playbackFrame(sample-1);Mesh flame;
        check(app->endingBackfire.append(flame,app->backfire,frame,app->endingBackfireTextureBase)>0,"Ending backfire disappeared after restore");
        for(const auto& range:flame.ranges)check(range.original&&range.emissive&&range.viewMask==1&&!range.carLighting&&!range.courseLighting&&
            range.texture>=app->endingBackfireTextureBase&&range.texture<app->endingBackfireTextureBase+8,"Ending flame material/texture scope");
        for(const auto& vertex:flame.vertices)for(float x:{vertex.position.x,vertex.position.y,vertex.position.z})check(std::isfinite(x),"Nonfinite original backfire geometry");
        const auto lightCount=app->endingDrawLightSets->course.count;
        check(lightCount>app->endingLightSets->course.count,"Ending backfire omitted its point light");
        app->renderEnding(0);app->paused=true;app->renderEnding(.5);app->paused=false;
        check(fields==state(app->endingBackfire)&&random==app->endingRandom,"Repaint/pause advanced ending backfire");
        check(app->endingDrawLightSets->course.count==lightCount,"Repaint accumulated backfire lights");
        std::vector<unsigned> expected;unsigned expectedRandom=0;
        for(unsigned fps:{30u,60u,144u,240u}){
            app->endingReplay=clip;app->beginEnding();
            for(unsigned render=0;render<fps*10;++render)app->advanceEnding(1./fps);
            check(app->endingBackfire.fields().updates==600,"Ending backfire lost original cadence");
            if(fps==30){expected=state(app->endingBackfire);expectedRandom=app->endingRandom;}
            else check(expected==state(app->endingBackfire)&&expectedRandom==app->endingRandom,"Backfire depends on display FPS");
        }
        check(app->originalSession.rollbackDigest()==physics&&app->battleProfile.words==profile,"Backfire changed race/save state");
        app->start();check(!app->endingBackfire.active()&&!app->endingBackfire.fields().updates&&!app->endingDrawLightSets,"Restart retained ending backfire/light state");
    }
    auto config=original::OriginalCarAppearanceConfig(19);config.word=0x40080000u;
    check(BackfirePresentation::eligible(config),"Valid source exhaust rejected");
    config.word=0x80000u;check(!BackfirePresentation::eligible(config),"Exhaust without misfire upgrade admitted");
    config.word=0x40000000u;check(!BackfirePresentation::eligible(config),"Standard exhaust admitted");
    config.word=0x40200000u;check(!BackfirePresentation::eligible(config),"Corrupt exhaust mount admitted");
    config.car=0;config.word=0x40080000u;check(!BackfirePresentation::eligible(config),"Different car admitted");
}
