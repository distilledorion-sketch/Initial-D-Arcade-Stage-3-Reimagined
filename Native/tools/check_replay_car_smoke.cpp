#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include "../tests/roadside_dust_race_fixture.inl"
#include <iostream>
DriverInput replaySmokeInput(App& app,unsigned scenario,unsigned tick){
    auto input=roadsideDustDiagnosticInput(tick);
    if(scenario>=2){
        const auto projection=app.course.project(app.vehicle.position,app.segment);
        const float lookahead=std::max(12.f,app.vehicle.speed*.65f);
        const auto target=app.course.sample(projection.sample.distance+lookahead).center-app.vehicle.position;
        const auto angle=wrapAngle(std::atan2(target.x,target.z)-app.vehicle.yaw);
        input.steer=-std::clamp(std::atan2(2*app.config.wheelbase*std::sin(angle),lookahead)/recoveredSteeringLimit,-.75f,.75f);
        if(tick>=240&&tick%120<30){input.throttle=0;input.brake=1;}
        if(scenario==2&&tick>=300&&tick<320)input.steer=.8f;
    }
    return input;
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and NEW evidence directory required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    unsigned checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    check(app->renderer.initialize(nullptr,960,720,true),"Offscreen renderer failed");
    app->demoPreparationStarted=true;app->replayRecordingFlags=1;
    for(unsigned scenario=0;scenario<4;++scenario){
        prepareRoadsideDustRaceFixture(*app,scenario,500,scenario>=2?0.f:.55f);
        for(unsigned tick=0;tick<60;++tick)app->advanceRoadsideDust();
        std::vector<std::uint64_t> expected;unsigned emitted=0;
        for(unsigned tick=0;tick<720;++tick){app->simulate(replaySmokeInput(*app,scenario,tick));expected.push_back(app->carSmoke.digest());emitted+=app->carSmoke.system().particleCount()>0;}
        check(emitted>0,"Actual race fixture did not emit recorded effects");
        const auto raw=app->recording.sharedBytes(unsigned(app->recording.frames.back().tick*100));
        const auto word=[&](unsigned at){unsigned v;std::memcpy(&v,raw.data()+at,4);return v;};
        check(raw.size()>=96,"Recording incomplete");
        check(word(24)==4&&word(16)==100&&word(20)==168&&word(96)==136&&word(8)==720&&word(92)==780,"Ordinary race did not capture complete combined timeline");
        check(app->recording.dustFrames.empty(),"New replay duplicated old dust timeline");
        const auto tail=std::span(raw).subspan(word(16)+720*word(20));
        check(tail.size()==780*ReplayCarSmokeFrame::bytes,"Combined source input extent");
        check(std::memcmp(tail.data(),app->recording.smokeFrames.data(),tail.size())==0,"Live float bits changed in capture");
        CarSmokeState playback;ReplayCarSmokePlayback timeline;timeline.load(tail,playback);
        for(unsigned i=0;i<720;++i){timeline.seek(word(word(16)+i*word(20)+160),playback);check(playback.digest()==expected[i],"Live recording did not reproduce source state");}
        for(unsigned i:{719u,0u,299u,300u,600u,20u,719u}){timeline.seek(word(word(16)+i*word(20)+160),playback);check(playback.digest()==expected[i],"Live replay seek diverged");}
        std::ofstream file(out/("scene-"+std::to_string(scenario)+".idr2"),std::ios::binary);file.write(reinterpret_cast<const char*>(raw.data()),raw.size());check(bool(file),"Recording write failed");
    }
    unsigned totalChanged=0;
    for(unsigned scenario=0;scenario<4;++scenario){
        app->replayPlaybackActive=false;prepareRoadsideDustRaceFixture(*app,scenario,500,scenario>=2?0.f:.55f);
        // Frames independently observed by check_car_smoke's live camera
        // search after restoring source shared RNG. At the previous night
        // snapshot (312), the new source particles are behind the car.
        constexpr std::array<unsigned,4> visibleTicks{61,61,318,65};
        for(unsigned tick=0;tick<=visibleTicks[scenario];++tick)app->simulate(replaySmokeInput(*app,scenario,tick));
        const auto source=app->carSmoke;const auto physics=app->presentedSession().rollbackDigest();
        const auto raw=app->recording.sharedBytes(unsigned(app->recording.frames.back().tick*100));
        const auto tail=std::span(raw).subspan(100+168*app->recording.frames.size());
        app->replayPlaybackActive=true;app->replayDetailed=true;
        // Use the established unobstructed live camera for white-cloud
        // comparison. The fixed replay chase/orbit can enter the hillside
        // during this steering fixture. No particle/depth settings change.
        app->replayCameraMode=scenario>=2?4:0;
        app->replayFreeEye=app->previousCameraFrame.eye;app->replayFreeTarget=app->previousCameraFrame.target;
        if(scenario>=2){std::ofstream camera(out/("camera-"+std::to_string(scenario)+".json"));camera.precision(9);
            const auto vector=[&](Vec3 p){camera<<"{\"x\":"<<p.x<<",\"y\":"<<p.y<<",\"z\":"<<p.z<<'}';};
            camera<<"{\"eye\":";vector(app->replayFreeEye);camera<<",\"target\":";vector(app->replayFreeTarget);camera<<'}';}
        app->replaySmoke.load(tail,app->carSmoke);app->carSmoke=source;
        const auto render=[&](const char* name){
            check(app->render(0,false),"Combined replay scene render failed");
            const auto path=out/(std::to_string(scenario)+"-"+name+".bmp");check(app->renderer.saveBitmap(path.wstring()),"Replay pixel readback failed");
            std::ifstream in(path,std::ios::binary);BITMAPFILEHEADER header{};in.read(reinterpret_cast<char*>(&header),sizeof header);in.seekg(header.bfOffBits);
            std::vector<unsigned> pixels(960*720);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);check(bool(in),"Replay pixels truncated");return pixels;
        };
        const auto expectedPixels=render("source");
        app->replaySmoke.load(tail,app->carSmoke);app->replaySmoke.seek(unsigned(app->recording.smokeFrames.size()),app->carSmoke);
        check(app->carSmoke.digest()==source.digest(),"Restored combined replay state differs");
        const auto restoredPixels=render("restored");check(app->carSmoke.digest()==source.digest(),"Replay painting changed effects");
        app->carSmoke.reset(app->wet?1:0);const auto absentPixels=render("absent");unsigned changed=0;
        for(unsigned i=0;i<expectedPixels.size();++i){check(expectedPixels[i]==restoredPixels[i],"Restored combined replay pixels differ");changed+=expectedPixels[i]!=absentPixels[i];}
        check(changed>(scenario<2?100u:0u),"Combined replay has no visible contribution");totalChanged+=changed;
        std::cout<<"scenario="<<scenario<<" restored=true changed_pixels="<<changed<<'\n';
        check(app->drivingEffects.smokeCount()==0,"Generic smoke duplicates recorded smoke");
        check(app->presentedSession().rollbackDigest()==physics,"Replay painting changed physics");
    }
    std::cout<<"PASS checks="<<checks<<" recorded_ticks=3120 full_scene_pairs=4 changed_pixels="<<totalChanged<<"; actual race capture, all source ticks, exact seeks/render restoration, no duplicate smoke.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
