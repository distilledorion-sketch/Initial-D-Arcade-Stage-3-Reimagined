#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
#include "../tests/ending_weather_unity_fixture.inl"
#include "../tests/ending_backfire_unity_fixture.inl"
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and NEW evidence directory required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    unsigned checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    const auto projectionMesh=[](const original::OriginalHeadlightProjection& light){
        std::vector<unsigned> words;
        for(const auto& v:light.model().chunks.at(0).batches.at(0).vertices){
            for(float x:{v.position.x,v.position.y,v.position.z,v.u,v.v})words.push_back(std::bit_cast<unsigned>(x));
            words.push_back(v.color0);words.push_back(v.color1);
        }
        return words;
    };
    const auto projectionState=[&](const OriginalHeadlightPresentation& light){
        auto words=projectionMesh(light.projection());words.push_back(unsigned(light.enabled()));
        for(const auto& record:light.projection().records())words.insert(words.end(),record.begin(),record.end());
        for(const auto& query:light.projection().queries())words.insert(words.end(),query.words.begin(),query.words.end());
        return words;
    };
    const auto smokeState=[](const original::OriginalReplayCarSmokeSystem& system){
        std::vector<unsigned> words{system.freeParticles(),unsigned(system.order().size())};
        const auto f=[&](float value){words.push_back(std::bit_cast<unsigned>(value));};
        for(auto slot:system.slots())words.push_back(unsigned(slot));
        for(auto kind:system.kinds())words.push_back(kind);
        for(auto index:system.order()){
            const auto& cloud=system.cloud(index);const auto& e=cloud.emitter;
            words.insert(words.end(),{index,cloud.car,cloud.effect,cloud.color,e.flags,e.frame,e.stopped,unsigned(e.particles.size())});
            f(cloud.gain);f(e.scale);f(e.force);f(e.lifetimeScale);
            for(float v:cloud.footprint.elements)f(v);
            for(const auto* values:{&e.position,&e.previousPosition,&e.velocity,&e.spread})for(float v:*values)f(v);
            for(const auto& p:e.particles){for(float v:p.position)f(v);for(float v:p.velocity)f(v);f(p.size);words.push_back(p.remaining);words.push_back(p.flags);}
        }
        return words;
    };
    const auto rainState=[](const OriginalRainPresentation& rain){
        const auto& owner=rain.owner();const auto& fields=owner.fields;
        std::vector<unsigned> words{unsigned(rain.active()),rain.updates(),owner.preserveStreakMotion,
            fields.streakCursor,fields.splashCursor,fields.wheelMistCursor};
        const auto f=[&](float value){words.push_back(std::bit_cast<unsigned>(value));};
        const auto point=[&](const auto& values){for(float value:values)f(value);};
        point(owner.previousVisualPosition);point(owner.streakMotion);
        for(unsigned i=0;i<2;++i){point(owner.previousActorPosition[i]);point(owner.actorMotion[i]);
            point(owner.previousWorld[i].elements);words.push_back(owner.glareAllowed[i]);words.push_back(owner.weatherAllowed[i]);}
        for(unsigned i=0;i<fields.streaks.size();++i){const auto& p=fields.streaks[i];
            point(p.position);point(p.direction);point(p.normal);for(const auto& offset:p.offsets)point(offset);
            words.insert(words.end(),{unsigned(p.life),p.active,unsigned(fields.visibleStreaks[i])});}
        for(unsigned i=0;i<fields.splashes.size();++i){const auto& p=fields.splashes[i];point(p.position);
            words.insert(words.end(),{unsigned(p.life),p.active,unsigned(fields.visibleSplashes[i])});}
        for(const auto* pool:{&fields.wheelMist,&fields.drawnWheelMist})for(const auto& p:*pool){
            point(p.position);f(p.velocityX);f(p.drag);f(p.velocityZ);words.push_back(unsigned(p.life));words.push_back(p.active);}
        words.insert(words.end(),rain.query().words.begin(),rain.query().words.end());
        for(const auto& lamp:rain.glare()){
            words.insert(words.end(),{lamp.car,lamp.lamp,lamp.chunk,lamp.draw.alpha,unsigned(lamp.draw.draw)});
            point(lamp.draw.position);point(lamp.draw.viewMatrix.elements);
        }
        return words;
    };
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;
    app->settings();app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->audio.enabled=false; // Never open an output device in this offscreen test.
    check(app->renderer.initialize(nullptr,960,720,true),"Ending renderer");
    app->originalCamera=OriginalChaseCamera::load(root);
    app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    // Exercise real Legend capture with the optional replay archive disabled.
    app->frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
    app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();
    app->frontend.car=0;app->frontend.battleProfile.setu(16,0);app->frontend.automatic=true;
    app->frontend.battleProfile.setByte(164,63);
    // The simple numerical driver races Itsuki for real moving footage. The
    // separate31-result flow below verifies entry after the final rival;
    // this is not a claim that this test driver defeats Bunta or all31 rivals.
    original::selectOriginalRival(app->frontend.battleProfile,0);
    app->replayRecordingFlags=0;app->start();
    // Ordinary path-following controls, no teleports or progress writes.
    // Drive far enough to exercise the original43% highlight trigger.
    const auto drivingInput=[&]{
        const auto& drive=app->originalSession.vehicle().drive;
        const auto& points=app->originalSession.path().points;
        std::size_t nearest=0;float best=1e30f;
        for(std::size_t p=0;p<points.size();++p){const float dx=points[p][0]-drive.f(0),dz=points[p][2]-drive.f(8),distance=dx*dx+dz*dz;
            if(distance<best){best=distance;nearest=p;}}
        auto target=nearest;float distance=0;const float look=std::max(10.f,drive.f(0x238)*.65f);
        while(distance<look&&target+1<points.size()){
            const float dx=points[target+1][0]-points[target][0],dz=points[target+1][2]-points[target][2];
            distance+=std::sqrt(dx*dx+dz*dz);++target;
        }
        const auto& p=points[target];const float wanted=std::atan2(-(p[0]-drive.f(0)),-(p[2]-drive.f(8)));
        DriverInput input;input.automatic=true;
        input.steer=std::clamp(-std::remainder(wanted-drive.f(0x10),2*pi)*.85f,-.8f,.8f);
        input.throttle=1;
        return input;
    };
    for(unsigned i=0;i<18000&&app->race.phase!=RacePhase::Finished;++i){
        const auto beforeBody=app->playerBodyWorld;
        const auto beforeActor=app->presentedSession().publishedActors().player0C8FF388;
        const bool wasRunning=app->race.phase==RacePhase::Running;
        app->simulate(drivingInput());
        if(wasRunning){
            const auto recorded=app->endingReplay.recentFrame(unsigned(app->endingReplay.size()-1)).cars[0].body;
            check(recorded.x==beforeBody.x&&recorded.y==beforeBody.y&&recorded.z==beforeBody.z,
                "Ending capture did not preserve source pre-solver pose");
            const auto& smoke=app->endingReplay.recentFrame(unsigned(app->endingReplay.size()-1)).cars[0];
            for(unsigned axis=0;axis<3;++axis)check(std::bit_cast<unsigned>((&smoke.actor.x)[axis])==beforeActor[axis],"Ending lost published raw actor XYZ for weather");
            for(unsigned i=0;i<3;++i)check(std::bit_cast<unsigned>(smoke.displacement[i])==beforeActor[3+i],"Ending changed recorded actor displacement");
            for(unsigned i=0;i<4;++i){
                check(smoke.smokeIntensity[i]==std::uint8_t(beforeActor[21]>>(8*i)),"Ending lost actor smoke intensity");
                check(smoke.wheelSurfaces[i]==std::uint8_t(beforeActor[29]>>(8*i)),"Ending lost actor wheel contacts");
            }
        }
        if(i%1200==0)std::cout<<"Driving "<<i<<" progress "<<app->race.progress
            <<" fraction "<<app->battleMetrics.positionFraction(app->originalCoordinate)
            <<" highlight "<<app->endingReplay.highlightSize()<<" km/h "<<app->vehicle.speedKmh()
            <<" gear "<<app->vehicle.gear<<'\n'<<std::flush;
    }
    check(app->race.phase==RacePhase::Finished&&!app->race.timeUp,"Recording fixture did not cross the finish line");
    std::cout<<"Recorded finish: "<<app->race.ticks<<" running ticks, "<<app->race.elapsed6000
        <<" timer units, "<<app->endingReplay.highlightSize()<<" highlight frames\n"<<std::flush;
    check(!app->archiveThisRace&&app->endingReplay.size()>600,"Ending depends on optional replay saving");
    check(length(app->endingReplay.recentFrame(0).cars[0].body-app->endingReplay.recentFrame(600).cars[0].body)>10,"Ending did not record moving cars");
    check(app->endingReplay.recentFrame(600).cars[1].visible,"Ending lost opponent");
    auto footage=app->endingReplay;footage.seal();
    check(footage.ready()&&footage.highlightSize()==EndingReplay::highlightCapacity,"Live race did not fill highlight recording");
    check(length(footage.playbackFrame(4600).cars[0].body-footage.playbackFrame(4599).cars[0].body)>.01f,
        "Driving stopped before the late credits");
    const auto physics=app->originalSession.rollbackDigest();
    const auto raceLightPacket=original::originalCourseLightingPacket(*app->raceLighting,original::originalLightIdentityMatrix);
    const auto raceTicks=app->race.ticks;
    // Boundary coverage: overwrite oldest, preserve chronological order,
    // stop capture after sealing. Playback ordering has its own source oracle.
    EndingReplay ring;
    for(unsigned i=0;i<4001;++i){auto f=footage.recentFrame(0);f.pathIndex=int(i);ring.record(f,*app->courseLightFsca,.7f,0);}
    ring.seal();check(ring.ready()&&ring.size()==3600,"Ending buffer bound");
    check(ring.recentFrame(0).pathIndex==401&&ring.recentFrame(3599).pathIndex==4000,"Ending ring ordering");
    ring.record(footage.recentFrame(0),*app->courseLightFsca,.7f,0);check(ring.recentFrame(3599).pathIndex==4000,"Sealed ending accepted live data");
    app->menu=true;
    const auto before=app->battleProfile.words;
    #include "../tests/ending_leaf_application_checks.inl"
    #include "../tests/ending_flare_application_checks.inl"
    std::array<Vec3,3> referenceTargets{};
    std::array<std::vector<unsigned>,3> referenceSmoke;
    unsigned smokeTicks=0,smokeDraws=0;
    for(unsigned fps:{30u,60u,144u,240u}){
        app->endingReplay=footage;
        app->beginEnding();check(app->extraModeVisitActive(),"Ending input ownership");
        app->commands(1);check(app->endingActive,"Generic command interrupted ending");
        check(app->endingCamera.updates()==0,"Ending camera inherited earlier playback state");
        auto expectedCamera=app->endingCamera;
        auto expectedSmoke=app->endingSmoke;auto expectedRandom=app->endingRandom;
        auto expectedLeaves=app->endingLeaves;
        unsigned renders=0;
        while(app->endingActive&&renders<fps*100){
            const auto beforeCamera=app->endingCamera.updates();
            app->advanceEnding(1./fps);++renders;
            if(app->endingActive){
                for(unsigned i=beforeCamera;i<app->endingCamera.updates();++i){
                    const auto& frame=footage.playbackFrame(i);expectedCamera.advanceShared(frame.eye,frame.target,expectedRandom);
                    std::array<original::OriginalCarSmokeInput,2> cars;
                    for(unsigned car=0;car<2;++car){const auto& recorded=frame.cars[car];
                        cars[car].available=true;cars[car].owner=original::originalRoadsideDustOwner(recorded.visualWorld,
                            app->endingCars[car].originalEffectOrigins(),recorded.displacement);
                        cars[car].intensity=recorded.smokeIntensity;cars[car].wheelSurfaces=recorded.wheelSurfaces;
                    }
                    expectedSmoke.advance(unsigned(app->courseIndex)*2u+unsigned(app->night),app->wet,2,cars,[&]{
                        expectedRandom=expectedRandom*0x41c64e6du+12345u;return float((expectedRandom>>16)&32767u)/32768.f;
                    });
                    expectedLeaves.advance(frame,app->endingCars[0].originalEffectOrigins(),*app->courseLightFsca,expectedRandom);
                }
                const auto actual=app->endingCamera.target(),expected=expectedCamera.target();
                check(actual.x==expected.x&&actual.y==expected.y&&actual.z==expected.z,"Camera advanced on render time instead of original ticks");
                check(app->endingRandom==expectedRandom,"Ending split camera/smoke/leaf random streams");
                check(app->endingLeaves.updates()==app->endingCamera.updates(),"Ending leaves did not advance once per source tick");
                smokeTicks+=!app->endingSmoke.order().empty();
                if(renders==fps*10||renders==fps*40||renders==fps*75){
                    const unsigned slot=renders==fps*10?0:renders==fps*40?1:2;
                    const auto state=smokeState(app->endingSmoke);
                    check(state==smokeState(expectedSmoke),"Ending did not replay captured smoke inputs");
                    if(fps==30){referenceTargets[slot]=actual;referenceSmoke[slot]=state;}
                    else {const auto ref=referenceTargets[slot];check(actual.x==ref.x&&actual.y==ref.y&&actual.z==ref.z,"Camera differs by host FPS");
                        check(state==referenceSmoke[slot],"Ending smoke differs by host FPS");}
                }
            }
            if(fps==60&&(renders==600||renders==2400||renders==4600||renders==5100)){
                check(app->renderEnding(0),"Ending frame");
                const auto smokeBefore=smokeState(app->endingSmoke);const auto seedBefore=app->endingRandom;
                Mesh smokeMesh;
                smokeDraws+=app->endingSmokePresentation.appendSystem(app->endingSmoke,smokeMesh,app->endingCamera.eye(),app->endingCamera.target(),
                    {0,1,0},OriginalEndingCamera::fieldOfView(),4.f/3.f,app->endingSmokeTextureBase,1,480.f);
                check(smokeState(app->endingSmoke)==smokeBefore&&app->endingRandom==seedBefore,"Drawing ending smoke mutated its state");
                check(app->ending.timeline.finalCard||app->endingTexturesLoaded,"Driving backdrop was not rendered");
                if(!app->ending.timeline.finalCard){
                    check(app->endingLightSets.has_value(),"Ending lighting was not initialized");
                    check(app->endingDrawLightSets&&app->renderer.courseLighting==&app->endingDrawLightSets->course&&
                        app->renderer.playerLighting==&app->endingDrawLightSets->player&&
                        app->renderer.rivalLighting==&app->endingDrawLightSets->rival,"Ending lost distinct course/car lighting scopes");
                    check(app->renderer.courseFog==&app->raceFog,"Ending omitted course fog");
                    const auto ambient=original::originalCourseCarAmbient({unsigned(app->courseIndex),0,app->night,app->wet});
                    check(app->endingDrawLightSets->player.ambient==ambient&&app->endingDrawLightSets->rival.ambient==ambient,
                        "Ending used race-parent ambient instead of replay ambient");
                    if(app->night)check(app->endingLightSets->course.lights[app->endingLightSets->course.count-1].sourceSlot==0x3f8,
                        "Night ending omitted camera spotlight");
                }else check(!app->renderer.courseLighting&&!app->renderer.playerLighting&&!app->renderer.rivalLighting&&!app->renderer.courseFog,
                    "Final artwork inherited 3D scene lighting");
                check(app->renderer.saveBitmap((out/("ending-"+std::to_string(renders)+".bmp")).wstring()),"Ending capture");
                const auto updates=app->endingCamera.updates();app->renderEnding(0);
                check(app->endingCamera.updates()==updates,"Repeated render aged ending camera");
                const auto currentRaceLights=original::originalCourseLightingPacket(*app->raceLighting,original::originalLightIdentityMatrix);
                check(currentRaceLights.glm==raceLightPacket.glm&&currentRaceLights.lights==raceLightPacket.lights,
                    "Ending lighting mutated the completed race owner");
            }
            if(renders==fps*10){
                check(app->audio.legendStreamStatistics().stream==12&&app->audio.legendStreamStatistics().playing,"Ending music not playing");
                const auto tick=app->ending.timeline.frame,updates=app->endingCamera.updates();
                const auto smokeBefore=smokeState(app->endingSmoke);const auto randomBefore=app->endingRandom;
                app->paused=true;app->advanceEnding(.2);app->paused=false;
                check(app->ending.timeline.frame==tick,"Paused credits moved");
                check(app->endingCamera.updates()==updates,"Paused credits advanced camera");
                check(smokeState(app->endingSmoke)==smokeBefore&&app->endingRandom==randomBefore,"Paused credits advanced smoke/random state");
            }
        }
        check(app->legendRunCompleted&&!app->endingActive&&app->frontend.stage==FrontendStage::Title,"Ending did not reach title");
        check(app->ending.timeline.totalFrame==5460,"Ending depends on render FPS");
        check(!app->audio.legendStreamStatistics().playing,"Ending stream leaked into title");
        check(app->battleProfile.words==before,"Ending re-awarded or changed profile");
        check(app->originalSession.rollbackDigest()==physics&&app->race.ticks==raceTicks,"Ending stepped race simulation");
        check(!app->endingReplay.ready()&&app->endingReplay.size()==0,"Ending retained race recording after exit");
        check(app->endingCamera.updates()==0,"Ending retained camera state after exit");
        check(app->endingSmoke.order().empty()&&app->endingSmoke.freeParticles()==60&&app->endingRandom==1,"Ending retained smoke after exit");
        check(!app->endingLightSets&&!app->endingCourseLighting&&!app->endingCourseAnimation.active(),"Ending retained lighting/animation owner after exit");
    }
    check(smokeTicks>0,"Natural Legend recording never emitted replay smoke");
    std::cout<<"Ending smoke active samples "<<smokeTicks<<", submitted clouds "<<smokeDraws<<'\n'<<std::flush;
    app->endingReplay=footage;
    app->beginEnding();app->input.down[VK_ESCAPE]=true;
    check(app->renderEnding(1./60)&&app->ending.timeline.frame==1,"Held dialogue skip skipped credits");
    app->input.down[VK_ESCAPE]=false;app->renderEnding(0);
    app->input.down[VK_ESCAPE]=true;app->renderEnding(1./60);
    check(app->ending.timeline.phase==6,"Start did not skip roll");
    for(unsigned i=0;i<250;++i)app->advanceEnding(1./60);
    check(app->endingActive&&app->ending.timeline.finalCard,"Skipping roll lost final card");
    app->input.down[VK_ESCAPE]=false;app->renderEnding(0);
    app->input.down[VK_ESCAPE]=true;app->renderEnding(1./60);
    check(!app->endingActive&&app->legendRunCompleted,"Start did not skip final card");
    // A separate night/wet Akagi presentation fixture exercises the camera
    // spotlight and animated course prop that the natural Myogi day finish
    // cannot cover. Poses come from actual simulation; only the highlight
    // trigger is seeded here. This is not a completed second Legend race.
    for(bool rain:{false,true}){
        app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app->frontend.car=0;app->frontend.battleProfile.setu(16,0);app->frontend.battleProfile.setByte(164,63);
        original::selectOriginalRival(app->frontend.battleProfile,8);
        app->frontend.battleProfile.setu(32,rain?1:0);app->start();
        check(app->courseIndex==2&&app->night&&app->wet==rain,"Night Akagi fixture conditions");
        check(!app->endingLightSets&&!app->endingCourseLighting&&!app->endingCourseAnimation.active(),"New race retained ending owners");
        EndingReplay nightClip;
        for(unsigned tick=0;tick<1200&&nightClip.highlightSize()<600;++tick){
            const auto old=app->endingReplay.size();app->simulate(drivingInput());
            if(app->endingReplay.size()>old)nightClip.record(app->endingReplay.recentFrame(unsigned(app->endingReplay.size()-1)),*app->courseLightFsca,.7f,0);
        }
        check(nightClip.highlightSize()==600,"Night fixture did not record real driving poses");
        const auto nightPhysics=app->originalSession.rollbackDigest();
        const auto nightProfile=app->battleProfile.words;
        const auto raceAnimationPhase=app->courseAnimation.phase();
        const std::array raceHeadlights{projectionState(app->playerProjectedHeadlight),projectionState(app->rivalProjectedHeadlight)};
        app->endingReplay=nightClip;app->beginEnding();
        check(app->endingCourseAnimation.active()&&app->endingCourseAnimation.phase()==0,"Ending did not create a fresh Akagi animation owner");
        // Geometry must retain its original publish boundary. Checking only
        // the final working records would miss an early publish in render.
        for(unsigned tick=0;tick<3;++tick){
            std::array<std::vector<unsigned>,2> expected;
            for(unsigned i=0;i<2;++i){auto copy=app->endingProjectedHeadlights[i].projection();copy.publish();expected[i]=projectionMesh(copy);}
            app->advanceEnding(1./60);
            for(unsigned i=0;i<2;++i)check(projectionMesh(app->endingProjectedHeadlights[i].projection())==expected[i],"Ending published current working geometry one frame too early");
        }
        std::array<std::vector<unsigned>,2> fixedRateProjection;
        std::vector<unsigned> fixedRateSmoke,fixedRateRain;unsigned fixedRateRandom=0,totalGlareDraws=0;
        for(unsigned fps:{30u,60u,144u,240u}){
            app->endingReplay=nightClip;app->beginEnding();
            check(app->endingRain.active()==rain,"Wrong ending rain admission");
            if(rain){check(app->endingRain.owner().preserveStreakMotion==1,"Ending lost original rain motion mode");
                for(const auto& p:app->endingRain.owner().previousActorPosition)for(float value:p)check(value==0,"Rain inherited live actor history");}
            for(unsigned render=0;render<fps*10;++render){app->advanceEnding(1./fps);
                if(fps==60)totalGlareDraws+=unsigned(app->endingRain.glare().size());}
            check(app->endingCourseAnimation.phase()==600,"Night ending tick count differs by host FPS");
            if(fps==30){fixedRateSmoke=smokeState(app->endingSmoke);fixedRateRandom=app->endingRandom;fixedRateRain=rainState(app->endingRain);}
            else {check(smokeState(app->endingSmoke)==fixedRateSmoke&&app->endingRandom==fixedRateRandom,"Night/wet ending smoke depends on host FPS");
                check(rainState(app->endingRain)==fixedRateRain,"Rain records, glare or collision cache depend on host FPS");}
            for(unsigned i=0;i<2;++i){const auto state=projectionState(app->endingProjectedHeadlights[i]);
                if(fps==30)fixedRateProjection[i]=state;
                else check(state==fixedRateProjection[i],"Ending projected headlights depend on host FPS");
            }
        }
        check(app->endingCourseAnimation.phase()==600,"Night Akagi prop did not advance on original ticks");
        check(app->renderEnding(0),"Night ending render");
        if(rain){
            check(app->endingRainTextureBase==app->endingSmokeTextureBase+app->smokeTextures.size(),"Rain texture bank overlap");
            check(totalGlareDraws>0,"Actual road queries never accepted ending rear glare");
            const auto before=rainState(app->endingRain);const auto seed=app->endingRandom;
            Mesh weather;const auto drawn=app->endingRain.append(weather,app->endingRainTextureBase,*app->courseLightFsca);
            check(drawn.streaks>0&&drawn.splashes>0&&drawn.mist>0,"Ending omitted one rain particle family");
            check(weather.vertices.size()==6u*(drawn.streaks+drawn.splashes+drawn.mist+drawn.glare),"Rain source quads not submitted");
            for(const auto& vertex:weather.vertices)for(float value:{vertex.position.x,vertex.position.y,vertex.position.z,vertex.u,vertex.v,vertex.color.a})
                check(std::isfinite(value),"Nonfinite rain vertex");
            for(const auto& range:weather.ranges){check(range.original&&range.emissive&&range.viewMask==1&&!range.courseLighting&&!range.carLighting,"Rain inherited car lighting or rear view");
                check(range.texture>=app->endingRainTextureBase&&range.texture<app->endingRainTextureBase+11,"Rain texture outside authored bank");}
            app->renderEnding(0);app->paused=true;app->advanceEnding(.5);app->paused=false;
            check(rainState(app->endingRain)==before&&app->endingRandom==seed,"Rain rendering or pause advanced queries/state/RNG");
            std::ofstream(out/"rain-summary.txt")<<"streaks="<<drawn.streaks<<" splashes="<<drawn.splashes<<" mist="<<drawn.mist
                <<" glare="<<drawn.glare<<" accepted_glare_over_600_ticks="<<totalGlareDraws<<"\n";
        }
        check(app->endingSmokeTextureBase==app->endingProjectedHeadlightTextureBase+app->projectedHeadlightTextures.size(),"Ending smoke texture bank overlap");
        if(rain)check(app->endingSmoke.particleCount()>0,"Wet ending omitted rear-wheel spray");
        Mesh smokeMesh;
        const auto nightSmokeDraws=app->endingSmokePresentation.appendSystem(app->endingSmoke,smokeMesh,
            app->endingCamera.eye(),app->endingCamera.target(),{0,1,0},OriginalEndingCamera::fieldOfView(),4.f/3.f,
            app->endingSmokeTextureBase,1,480.f);
        smokeDraws+=nightSmokeDraws;
        std::cout<<"Night rain="<<rain<<" smoke particles="<<app->endingSmoke.particleCount()
            <<" clouds drawn="<<nightSmokeDraws<<" vertices="<<smokeMesh.vertices.size()<<'\n'<<std::flush;
        const auto& shown=app->endingReplay.playbackFrame(599);
        Mesh projected;app->appendEndingHeadlights(projected,shown);
        const auto verticesPerLight=app->endingProjectedHeadlights[0].model().chunks.at(0).batches.at(0).indices.size();
        // Native meshes expand the source's12 strip vertices into triangles.
        check(verticesPerLight>0&&projected.vertices.size()==2*verticesPerLight&&!projected.ranges.empty(),"Ending lost either car's projected headlights");
        for(const auto& range:projected.ranges)check(range.emissive&&!range.courseLighting&&!range.carLighting,"Projected ending headlights inherited body/course shading");
        check(app->endingProjectedHeadlightTextureBase==app->originalCourseTextures.size()+app->originalBackgroundTextures.size()+
            app->originalTextures.size()+app->numberPlate.textures.size()+app->rivalTextures.size()+app->rivalPlate.textures.size(),"Ending headlight texture bank overlaps car/plate textures");
        check(app->renderer.courseFog==&app->raceFog,"Night ending omitted fog");
        check(app->renderer.playerLighting&&app->renderer.rivalLighting&&app->renderer.courseLighting,"Night ending lost one lighting scope");
        for(const auto* scope:{app->renderer.courseLighting,app->renderer.playerLighting,app->renderer.rivalLighting}){
            const auto& camera=scope->lights[scope->count-1];
            check(camera.sourceSlot==0x3f8&&camera.enabled&&camera.color[0]==(rain?.2f:.5f),"Night ending camera light missing or wrong weather intensity");
        }
        check(app->renderer.saveBitmap((out/(rain?"ending-akagi-night-wet.bmp":"ending-akagi-night-dry.bmp")).wstring()),"Night ending image");
        if(rain){
            const auto savedRain=app->endingRain;app->endingRain.reset();
            check(app->renderEnding(0),"Ending without rain control");
            check(app->renderer.saveBitmap((out/"ending-akagi-night-wet-no-rain.bmp").wstring()),"Rain comparison image");
            app->endingRain=savedRain;
            check(app->renderEnding(0),"Restored ending rain");
            check(app->renderer.saveBitmap((out/"ending-akagi-night-wet-rain-restored.bmp").wstring()),"Restored rain image");
            // Matched image control: suppress only the private smoke owner,
            // without stepping the camera, lights, timeline or live race.
            const auto savedSmoke=app->endingSmoke;
            app->endingSmoke.reset(1);
            check(app->renderEnding(0),"Matched no-smoke rendering control");
            check(app->renderer.saveBitmap((out/"ending-akagi-night-wet-no-smoke.bmp").wstring()),"No-smoke control image");
            app->endingSmoke=savedSmoke;check(app->renderEnding(0),"Restore ending smoke after control");
        }
        const auto phase=app->endingCourseAnimation.phase();app->renderEnding(0);
        app->paused=true;app->renderEnding(.25);app->paused=false;
        check(smokeState(app->endingSmoke)==fixedRateSmoke&&app->endingRandom==fixedRateRandom,"Ending repaint advanced smoke/random state");
        for(unsigned i=0;i<2;++i)check(projectionState(app->endingProjectedHeadlights[i])==fixedRateProjection[i],"Ending repaint advanced headlight geometry/queries");
        check(projectionState(app->playerProjectedHeadlight)==raceHeadlights[0]&&projectionState(app->rivalProjectedHeadlight)==raceHeadlights[1],"Ending changed live race headlight owners");
        check(app->endingCourseAnimation.phase()==phase&&app->courseAnimation.phase()==raceAnimationPhase,"Ending repaint changed presentation/race animation clocks");
        check(app->originalSession.rollbackDigest()==nightPhysics&&app->battleProfile.words==nightProfile,"Night ending changed race physics/profile");
        #include "../tests/ending_glare_application_checks.inl"
        // Seed only the recorded lamp bits to cover blind-attack playback.
        // The moving poses remain real captured race frames.
        EndingReplay blind;
        for(unsigned tick=0;tick<12;++tick){auto frame=nightClip.recentFrame(tick);
            frame.cars[0].lights=tick<3||tick>=7;frame.cars[1].lights=tick<5||tick>=9;
            blind.record(frame,*app->courseLightFsca,.7f,0);}
        app->endingReplay=blind;app->beginEnding();
        for(unsigned tick=0;tick<12;++tick){app->advanceEnding(1./60);
            const auto& frame=app->endingReplay.playbackFrame(tick);Mesh lightMesh;app->appendEndingHeadlights(lightMesh,frame);
            unsigned count=0;for(unsigned i=0;i<2;++i){check(app->endingProjectedHeadlights[i].enabled()==frame.cars[i].lights,"Ending ignored recorded blind attack");count+=frame.cars[i].lights;}
            check(lightMesh.vertices.size()==count*verticesPerLight,"Ending blind attack draw retained hidden beam");
        }
        // Starting the next race while credits own lighting must release it.
        app->start();
        check(!app->endingActive&&!app->endingLightSets&&!app->endingCourseLighting&&!app->endingCourseAnimation.active(),"Restart retained ending lighting");
        for(const auto& light:app->endingProjectedHeadlights)check(!light.enabled(),"Restart retained ending projected headlights");
        check(!app->endingRain.active()&&app->endingRain.updates()==0&&app->endingRain.glare().empty(),"Restart retained ending rain");
    }
    #include "../tests/ending_snow_application_checks.inl"
    #include "../tests/ending_lamp_application_checks.inl"
    #include "../tests/ending_backfire_application_checks.inl"
    app->load();
    check(smokeDraws>0,"No recorded ending smoke was submitted to rendering");
    check(runLegendRunSmoke(*app,out/"legend-run")==0,"Full Legend result-to-ending flow");
    std::ofstream(out/"PASS.txt")<<checks<<" checks; real two-car Legend recording with archives disabled,43% highlight trigger and bounded two-buffer playback/cleanup, driving backdrop, source ending camera and fixed-tick spring/shake, ending roll/final artwork/music,30/60/144/240 FPS, pause, repeated render, held-input guard, skip, unchanged physics/profile; separate simulated Akagi night dry/wet clips with seeded highlights, camera lighting, fog, independent prop animation, original projected headlights with publication delay, render-rate independence, separate road queries, recorded blind attack and restart cleanup; captured actor smoke inputs, replay-specific rear-wheel smoke/spray/dust, private shared camera/smoke RNG, exact particle state across host FPS and paused/repeated renders, submitted original smoke geometry.\n";
    std::cout<<"PASS "<<checks<<" ending application checks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
