{
    prepareEndingWeatherUnityFixture(*app,470);
    const auto clip=app->endingReplay;const auto physics=app->originalSession.rollbackDigest();
    const auto profile=app->battleProfile.words;
    const std::array<unsigned,2> models{unsigned(app->frontend.car),unsigned(app->loadedRivalCar)};
    const auto state=[](const OriginalEndingLampTrails& trails){
        std::vector<unsigned> result{unsigned(trails.active()),trails.updates()};
        if(!trails.active())return result;
        for(const auto& car:trails.samples().frame(0))for(const auto& s:car){
            for(float v:s.center)result.push_back(std::bit_cast<unsigned>(v));
            result.insert(result.end(),{std::bit_cast<unsigned>(s.height1000),unsigned(s.opacity),unsigned(s.ribbon)});
        }
        for(const auto& car:trails.samples().history())for(const auto& lamp:car)for(const auto& p:lamp.points)
            for(float v:p)result.push_back(std::bit_cast<unsigned>(v));
        return result;
    };
    std::vector<unsigned> expected;unsigned random=0,peak=0,peakTick=0,peakVertices=0;
    std::array<unsigned,2> visibleSamples{},ribbonSamples{};
    OriginalDemoLampTrails cached;cached.load(app->root,600);
    for(unsigned fps:{30u,60u,144u,240u}){
        app->endingReplay=clip;app->beginEnding();
        check(app->endingLampTrails.active(),"Night ending omitted lamp owner");
        for(unsigned render=0;render<fps*10;++render){
            app->advanceEnding(1./fps);
            if(fps!=60)continue;
            const auto& frame=app->endingReplay.playbackFrame(render);
            const auto eye=app->endingCamera.eye(),target=app->endingCamera.target();
            const auto view=original::originalSmokeView(app->endingCamera.worldToView(),OriginalEndingCamera::fieldOfViewPhase());
            const auto camera=original::originalLookAtCameraWorld({eye.x,eye.y,eye.z},{target.x,target.y,target.z});
            cached.beginFrame(false);Mesh reference,streaming;
            for(unsigned slot=0;slot<2;++slot){const auto& car=frame.cars[slot];
                cached.prepare(render,slot,models[slot],car.visible&&car.lights,car.visualWorld,view,eye,app->originalSession.collision());
                cached.append(reference,render,slot,camera.elements,eye,0);
                for(const auto& sample:app->endingLampTrails.samples().frame(0)[slot]){
                    visibleSamples[slot]+=sample.opacity!=0;ribbonSamples[slot]+=sample.opacity&&sample.ribbon;
                }
            }
            const auto ribbons=app->endingLampTrails.append(streaming,0);
            if(render>=120&&ribbons>0&&streaming.vertices.size()>peakVertices){
                peak=ribbons;peakTick=render+1;peakVertices=unsigned(streaming.vertices.size());}
            // After nine samples both storage paths contain the same complete
            // projected history, including any visibility collapse boundaries.
            if(render>=9){
                check(reference.vertices.size()==streaming.vertices.size()&&reference.ranges.size()==streaming.ranges.size(),"Cached/streamed lamp geometry size");
                for(unsigned i=0;i<reference.vertices.size();++i)
                    check(std::memcmp(&reference.vertices[i],&streaming.vertices[i],sizeof(Vertex))==0,"Streaming changed original lamp geometry");
            }
        }
        check(app->endingLampTrails.updates()==600,"Ending lamp source cadence");
        if(fps==30){expected=state(app->endingLampTrails);random=app->endingRandom;}
        else check(state(app->endingLampTrails)==expected&&app->endingRandom==random,"Ending lamps depend on host FPS");
    }
    check(visibleSamples[0]>0&&visibleSamples[1]>0&&peak>0,"Recorded ending produced no rear lamps/trails");
    app->endingReplay=clip;app->beginEnding();
    for(unsigned tick=0;tick<peakTick;++tick)app->advanceEnding(1./60);
    check(app->renderEnding(0),"Render ending lamp trails");
    Mesh mesh;const auto ribbons=app->endingLampTrails.append(mesh,app->endingLampTrailTextureBase);
    check(ribbons==peak&&!mesh.vertices.empty(),"Ending lost peak lamp ribbons");
    for(const auto& range:mesh.ranges)check(range.original&&range.emissive&&range.viewMask==1&&!range.carLighting&&!range.courseLighting&&
        range.texture>=app->endingLampTrailTextureBase&&range.texture<app->endingLampTrailTextureBase+2,"Ending lamp material/texture scope");
    for(const auto& v:mesh.vertices)for(float f:{v.position.x,v.position.y,v.position.z,v.u,v.v,v.color.a})
        check(std::isfinite(f),"Nonfinite ending lamp geometry");
    check(app->renderer.saveBitmap((out/"ending-lamps.bmp").wstring()),"Capture ending lamps");
    const auto saved=app->endingLampTrails;const auto savedState=state(saved);const auto seed=app->endingRandom;
    app->endingLampTrails.reset();check(app->renderEnding(0),"Render ending without lamp trails");
    check(app->renderer.saveBitmap((out/"ending-lamps-disabled.bmp").wstring()),"Capture missing lamps control");
    app->endingLampTrails=saved;check(app->renderEnding(0),"Restore ending lamps");
    check(app->renderer.saveBitmap((out/"ending-lamps-restored.bmp").wstring()),"Capture restored lamps");
    app->paused=true;app->advanceEnding(5);app->renderEnding(0);app->paused=false;
    check(state(app->endingLampTrails)==savedState&&app->endingRandom==seed,"Lamp repaint/pause advanced history or RNG");
    auto frame=clip.playbackFrame(peakTick-1);
    for(unsigned slot=0;slot<2;++slot){auto hidden=frame;hidden.cars[slot].lights=false;
        auto probe=saved;probe.advance(hidden,models,true,app->endingCamera,app->originalSession.collision());
        for(const auto& sample:probe.samples().frame(0)[slot])check(!sample.opacity&&!sample.ribbon,"Blind attack retained stale lamp sample");
        const auto& histories=probe.samples().history()[slot];
        for(const auto& history:histories)for(const auto& point:history.points)check(point==history.points[0],"Blind attack did not collapse history");
    }
    auto cut=saved;
    for(unsigned tick=0;tick<9;++tick){cut.advance(frame,models,true,app->endingCamera,app->originalSession.collision(),tick==0);
        for(const auto& car:cut.samples().frame(0))for(const auto& sample:car)check(!sample.opacity,"Camera cut retained old lamp");}
    cut.advance(frame,models,true,app->endingCamera,app->originalSession.collision());
    unsigned restored=0;for(const auto& car:cut.samples().frame(0))for(const auto& sample:car)restored+=sample.opacity!=0;
    check(restored>0,"Camera cut did not release lamp visibility after nine ticks");
    OriginalEndingLampTrails day;day.begin(app->root,0,1);day.advance(frame,models,true,app->endingCamera,app->originalSession.collision());
    Mesh daylight;check(day.active()&&day.append(daylight,0)==0&&daylight.vertices.empty(),"Wet daytime ending incorrectly enabled rear lamps");
    day.begin(app->root,0,0);check(!day.active(),"Dry daytime created an ending lamp owner");
    check(app->originalSession.rollbackDigest()==physics&&app->battleProfile.words==profile,"Ending lamps changed race or save data");
    std::cout<<"Ending lamps600 ticks: visible "<<visibleSamples[0]<<'/'<<visibleSamples[1]<<" ribbons "<<ribbonSamples[0]<<'/'<<ribbonSamples[1]
        <<" peak="<<peak<<" at="<<peakTick<<" vertices="<<mesh.vertices.size()<<'\n';
    app->start();check(!app->endingLampTrails.active()&&!app->endingLampTrails.updates(),"Restart retained ending lamps");
}
