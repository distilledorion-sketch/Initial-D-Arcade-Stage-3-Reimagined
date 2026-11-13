// Regression investigation for smoke reported in the mirror while stopped.
// Private save, actual source driver inputs, offscreen renderer and no audio.
int checkStoppedMirrorSmoke(const fs::path& root,const fs::path& out){
    if(fs::exists(out))throw std::runtime_error("Preserve prior mirror evidence");
    fs::create_directories(out);
    unsigned checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    check(app->renderer.initialize(nullptr,960,720,true),"Mirror offscreen renderer");
    app->demoPreparationStarted=true;app->replayRecordingFlags=0;
    std::ofstream trace(out/"trace.csv");
    trace<<"scenario,tick,speed,intensity,emitters,particles,hostParticles,smokeFrames\n";
    for(unsigned scenario:{0u,2u,3u}){
        prepareRoadsideDustRaceFixture(*app,scenario,500,0);
        // First observe an idle car, then drive, brake to a complete stop and
        // leave all controls released. No particle/actor state is arranged.
        for(unsigned tick=0;tick<1500;++tick){
            DriverInput input;input.automatic=true;
            if(tick>=300&&tick<600){
                input.throttle=1;
                const auto projection=app->course.project(app->vehicle.position,app->segment);
                const float ahead=std::max(12.f,app->vehicle.speed*.65f);
                const auto target=app->course.sample(projection.sample.distance+ahead).center-app->vehicle.position;
                const auto angle=wrapAngle(std::atan2(target.x,target.z)-app->vehicle.yaw);
                input.steer=-std::clamp(std::atan2(2*app->config.wheelbase*std::sin(angle),ahead)/recoveredSteeringLimit,-.75f,.75f);
                if(tick>=520&&tick<540)input.steer=.8f;
            }
            if(tick<300||(tick>=600&&tick<1100))input.brake=1;
            app->simulate(input);
            const auto& smoke=app->displayedCarSmoke();
            const auto& actor=app->presentedSession().publishedActors().player0C8FF388;
            trace<<scenario<<','<<tick<<','<<app->vehicle.speedKmh()<<','<<actor[21]<<','<<smoke.system().order().size()<<','
                 <<smoke.system().particleCount()<<','<<app->drivingEffects.smokeCount()<<','<<smoke.frames()<<'\n';
            if(tick==299||tick==599||tick==1099||tick==1499){
                const auto prefix=std::to_string(scenario)+"-"+std::to_string(tick);
                const auto capture=[&](const char* suffix){
                    check(app->render(0),"Stopped mirror scene render");
                    const auto path=out/(prefix+suffix+".bmp");
                    check(app->renderer.saveBitmap(path.wstring()),"Stopped mirror readback");
                    std::ifstream file(path,std::ios::binary);BITMAPFILEHEADER header{};
                    file.read(reinterpret_cast<char*>(&header),sizeof header);file.seekg(header.bfOffBits);
                    std::vector<unsigned> pixels(960*720);file.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);
                    check(bool(file),"Stopped mirror image truncated");return pixels;
                };
                const auto before=smoke;const auto on=capture("-on");
                app->carSmoke.reset(app->wet?1:0);const auto off=capture("-off");
                app->carSmoke=before;const auto restored=capture("-restored");
                check(on==restored,"Mirror repaint modified original state or geometry");
                check(app->carSmoke.digest()==before.digest(),"Mirror repaint advanced smoke/RNG");
                unsigned changed=0,mirrorChanged=0;
                for(unsigned y=0;y<720;++y)for(unsigned x=0;x<960;++x)if(on[y*960+x]!=off[y*960+x]){
                    // BMP rows are bottom-up; the mirror occupies top48..144.
                    ++changed;mirrorChanged+=x>=240&&x<720&&y>=576&&y<672;
                }
                std::cout<<"mirror scenario="<<scenario<<" tick="<<tick<<" speed="<<app->vehicle.speedKmh()
                    <<" particles="<<before.system().particleCount()<<" intensity="<<actor[21]<<" pixels="<<changed
                    <<" mirrorPixels="<<mirrorChanged<<" mirrorActive="<<app->raceRearViewActive()<<'\n'<<std::flush;
                if(tick==299||tick>=1099){
                    if(tick!=1499)check(std::abs(app->vehicle.speedKmh())<1,"Stationary fixture did not stop");
                    check(before.system().particleCount()==0,"Stopped car retained source smoke particles");
                    check(changed==0,"Stopped car published smoke pixels without an emitter");
                }
            }
        }
    }
    std::ofstream(out/"PASS.txt")<<checks<<" checks: idle start, acceleration, turning, brake to stop and released controls; dry day/night/rain. Actual App simulation and paired offscreen pixels. No user save or audio.\n";
    return 0;
}

// A parked shoulder still owns a source dust emitter with zero lifetime.
// Exercise its entire five-frame emission cycle rather than only road centre.
int checkStationaryShoulderSmoke(const fs::path& root,const fs::path& out){
    if(fs::exists(out))throw std::runtime_error("Preserve prior shoulder evidence");
    fs::create_directories(out);
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    if(!app->renderer.initialize(nullptr,960,720,true))throw std::runtime_error("Shoulder offscreen renderer");
    app->demoPreparationStarted=true;app->replayRecordingFlags=0;
    std::ofstream trace(out/"trace.csv");trace<<"scenario,tick,speed,contacts,intensity,particles,pixels,mirrorPixels\n";
    for(unsigned scenario:{0u,1u,4u}){
        prepareRoadsideDustRaceFixture(*app,scenario,scenario==4?~0u:500u,scenario==4?0.f:.55f,scenario==4?3u:7u);
        for(unsigned tick=0;tick<125;++tick){
            DriverInput input;input.automatic=true;input.brake=1;app->simulate(input);
            if(tick<120)continue;
            const auto prefix=std::to_string(scenario)+"-"+std::to_string(tick);
            const auto capture=[&](const char* suffix){
                if(!app->render(0))throw std::runtime_error("Shoulder scene render");
                const auto path=out/(prefix+suffix+".bmp");
                if(!app->renderer.saveBitmap(path.wstring()))throw std::runtime_error("Shoulder readback");
                std::ifstream file(path,std::ios::binary);BITMAPFILEHEADER header{};
                file.read(reinterpret_cast<char*>(&header),sizeof header);file.seekg(header.bfOffBits);
                std::vector<unsigned> pixels(960*720);file.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);
                if(!file)throw std::runtime_error("Shoulder bitmap truncated");return pixels;
            };
            const auto saved=app->carSmoke;const auto on=capture("-on");
            app->carSmoke.reset(app->wet?1:0);const auto off=capture("-off");
            app->carSmoke=saved;const auto restored=capture("-restored");
            if(on!=restored||app->carSmoke.digest()!=saved.digest())throw std::runtime_error("Shoulder repaint changed state");
            unsigned changed=0,mirrorChanged=0;
            for(unsigned y=0;y<720;++y)for(unsigned x=0;x<960;++x)if(on[y*960+x]!=off[y*960+x]){
                ++changed;mirrorChanged+=x>=240&&x<720&&y>=576&&y<672;
            }
            const auto& actor=app->presentedSession().publishedActors().player0C8FF388;
            trace<<scenario<<','<<tick<<','<<app->vehicle.speedKmh()<<','<<actor[29]<<','<<actor[21]<<','<<saved.system().particleCount()<<','<<changed<<','<<mirrorChanged<<'\n';
            std::cout<<"shoulder scenario="<<scenario<<" tick="<<tick<<" speed="<<app->vehicle.speedKmh()
                <<" particles="<<saved.system().particleCount()<<" pixels="<<changed<<" mirrorPixels="<<mirrorChanged<<'\n'<<std::flush;
            if(std::abs(app->vehicle.speedKmh())>=.1f)throw std::runtime_error("Stationary smoke fixture moved");
            if(changed)throw std::runtime_error("Stationary source smoke contributed visible pixels");
        }
    }
    std::ofstream(out/"PASS.txt")<<"15 stationary captures across a complete five-frame emitter cycle: Tsuchisaka forward/reverse shoulder and Akina night/rain. No smoke pixels in either view; restored pixels and simulation digest match.\n";
    return 0;
}

// Sample the approach and stop, not only distant endpoints. Positive mirror
// coverage is reported separately: an empty mirror cannot prove cloud cleanup.
int checkMirrorStopSequence(const fs::path& root,const fs::path& out){
    if(fs::exists(out))throw std::runtime_error("Preserve prior mirror sequence evidence");
    fs::create_directories(out);
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);app->audio.enabled=false;
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    if(!app->renderer.initialize(nullptr,960,720,true))throw std::runtime_error("Mirror sequence renderer");
    app->demoPreparationStarted=true;app->replayRecordingFlags=0;
    std::ofstream trace(out/"sequence.csv");
    trace<<"scenario,course,wet,night,tick,speed,particles,hostSmoke,spray,leaves,changed,mirrorChanged\n";
    unsigned positiveScenes=0,stoppedScenes=0;
    for(unsigned scenario=0;scenario<6;++scenario){
        const unsigned course=scenario<3?7u:3u,condition=scenario%3==0?0u:scenario%3==1?2u:4u;
        prepareRoadsideDustRaceFixture(*app,condition,~0u,0,course);
        unsigned bestMirror=0;bool sawStop=false;
        const auto capture=[&](const char* suffix){
            if(!app->render(0))throw std::runtime_error("Mirror sequence repaint");
            const auto path=out/(std::string("sample-")+suffix+".bmp");
            if(!app->renderer.saveBitmap(path.wstring()))throw std::runtime_error("Mirror sequence capture");
            std::ifstream file(path,std::ios::binary);BITMAPFILEHEADER header{};file.read(reinterpret_cast<char*>(&header),sizeof header);file.seekg(header.bfOffBits);
            std::vector<unsigned> pixels(960*720);file.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);
            if(!file)throw std::runtime_error("Mirror sequence bitmap extent");return pixels;
        };
        for(unsigned tick=0;tick<1500;++tick){
            DriverInput input;input.automatic=true;
            if(tick<900){
                input.throttle=1;
                const auto projection=app->course.project(app->vehicle.position,app->segment);
                const float ahead=std::max(12.f,app->vehicle.speed*.65f);
                const auto target=app->course.sample(projection.sample.distance+ahead).center-app->vehicle.position;
                const auto angle=wrapAngle(std::atan2(target.x,target.z)-app->vehicle.yaw);
                input.steer=-std::clamp(std::atan2(2*app->config.wheelbase*std::sin(angle),ahead)/recoveredSteeringLimit,-.75f,.75f);
                if(tick>=720&&tick<840){input.steer=.8f;input.brake=tick%60<15?1.f:0.f;}
            }else input.brake=1;
            app->simulate(input);
            if(!app->render(1./60))throw std::runtime_error("Mirror sequence effect clock");
            if(tick<600||tick%15)continue;
            const auto smoke=app->carSmoke;const auto physics=app->originalSession.rollbackDigest();
            const auto on=capture("on");app->carSmoke.reset(app->wet?1:0);
            const auto off=capture("off");app->carSmoke=smoke;const auto restored=capture("restored");
            if(on!=restored||app->carSmoke.digest()!=smoke.digest()||app->originalSession.rollbackDigest()!=physics)
                throw std::runtime_error("Mirror sequence repaint changed pixels or simulation");
            unsigned changed=0,mirror=0;
            for(unsigned y=0;y<720;++y)for(unsigned x=0;x<960;++x)if(on[y*960+x]!=off[y*960+x]){
                ++changed;mirror+=x>=240&&x<720&&y>=576&&y<672;
            }
            trace<<scenario<<','<<course<<','<<app->wet<<','<<app->night<<','<<tick<<','<<app->vehicle.speedKmh()<<','
                 <<smoke.system().particleCount()<<','<<app->drivingEffects.smokeCount()<<','<<app->wetWeather.liveSpray()<<','
                 <<app->environment.leafCount()<<','<<changed<<','<<mirror<<'\n';
            if(mirror>bestMirror){
                bestMirror=mirror;
                for(const char* suffix:{"on","off","restored"})fs::copy_file(out/(std::string("sample-")+suffix+".bmp"),
                    out/("scene-"+std::to_string(scenario)+"-visible-"+suffix+".bmp"),fs::copy_options::overwrite_existing);
            }
            if(tick>=1200){
                if(std::abs(app->vehicle.speedKmh())>=.1f)throw std::runtime_error("Mirror sequence did not stop");
                if(smoke.system().particleCount()||app->drivingEffects.smokeCount()||app->wetWeather.liveSpray()||changed)
                    throw std::runtime_error("Stopped sequence retained tyre effects");
                sawStop=true;
                if(tick==1485)fs::copy_file(out/"sample-on.bmp",out/("scene-"+std::to_string(scenario)+"-stopped.bmp"));
            }
        }
        positiveScenes+=bestMirror>0;stoppedScenes+=sawStop;
        std::cout<<"Mirror stop scenario="<<scenario<<" maximumMirrorPixels="<<bestMirror<<" stopped="<<sawStop<<'\n'<<std::flush;
    }
    std::ofstream(out/"result.json")<<"{\"stoppedScenes\":"<<stoppedScenes<<",\"visibleMirrorScenes\":"<<positiveScenes
        <<",\"reportedArtifactReproduced\":false,\"scope\":\"Isolated solo Time Attack; no ordinary save or audio; actual inputs, simulation, render clock and paired smoke pixels.\"}\n";
    return 0;
}
