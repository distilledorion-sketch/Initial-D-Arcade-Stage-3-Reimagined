// Private diagnostic setup shared by native and packaged-player checks.
// Only initial pose/input are arranged; contacts and particles use live owners.
void prepareRoadsideDustRaceFixture(App& app,unsigned scenario,unsigned pathIndex=500,float shoulder=.55f,unsigned courseIndex=7){
    if(scenario>4||courseIndex>=9)throw std::invalid_argument("Dust diagnostic scenario");
    app.validationMode=true;app.replayPlaybackActive=false;app.validationHideDrivingEffects=false;
    app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();app.frontend.car=0;
    app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
    app.frontend.course=app.courseIndex=courseIndex;app.frontend.reverse=app.reverse=scenario==1;
    app.frontend.night=app.night=scenario==2||scenario==4;app.frontend.wet=app.wet=scenario>=3;
    app.frontend.automatic=app.automatic=true;app.start();
    app.loadingActive=app.preRaceDialogueActive=app.vsActive=app.menu=app.paused=false;
    if(!app.roadsideDust.system().order().empty())throw std::runtime_error("Race start retained dust");
    // Path500 is open in BOTH direction-specific collision files. Path100 is
    // closed in the forward race's _1 file: material0x800e is a barrier there.
    // UINT_MAX retains the real race's starting grid on other courses.
    if(pathIndex!=~0u){
        const Vec3 initial=app.course.points.at(pathIndex)*(1.f-shoulder)+app.course.right.at(pathIndex)*shoulder+Vec3{0,.03f,0};
        const auto direction=app.course.project(initial).sample.tangent;
        const float yaw=std::atan2(direction.x,direction.z)+(app.reverse?pi:0.f);
        // An ordinary reset retains source caches. Relocation needs a fresh owner.
        original::OriginalDrivingSession arranged;
        arranged.reset(app.root,app.originalSession.selection(),{initial.x,initial.y,initial.z},{0,wrapAngle(yaw-pi),0});
        app.originalSession=std::move(arranged);
        original::warmupOriginalRaceSession(app.originalSession,{},app.originalSession.platformFrame(),0);
    }
    app.originalRaceStart.reset(2);for(unsigned i=0;i<240;++i)app.originalRaceStart.step();
    app.originalSession.enableRaceStart(2);app.originalRace.start();app.race.phase=RacePhase::Running;
    app.projectOriginalPose(false);app.advanceOriginalCamera();app.drivingView=OriginalDrivingView::Chase;
}
DriverInput roadsideDustDiagnosticInput(unsigned tick){
    DriverInput input;input.automatic=true;input.throttle=1;
    input.steer=tick>=30&&tick<100?-.15f:0.f;return input;
}
void prepareRoadsideDustUnityFixture(App& app,unsigned scene){
    const bool sequence=scene>=550&&scene<=633;
    if(!sequence&&(scene<450||scene>461))throw std::invalid_argument("Dust Unity fixture");
    static std::optional<RoadsideDustPresentation> saved;
    static std::optional<CarSmokeState> savedSmoke;
    static unsigned prepared=~0u;
    static std::uint64_t physics=0;
    const unsigned key=(scene-(sequence?550u:450u))/3,phase=(scene-(sequence?550u:450u))%3;
    // Include the complete five-tick emission interval, plus later samples.
    // A freshly emitted cloud can be wholly occluded by the player's car;
    // sampling only every 30 ticks repeatedly observes the same phase.
    constexpr unsigned samples[]{60,61,62,63,64,90,120};
    constexpr unsigned sampleCount=unsigned(std::size(samples));
    const unsigned scenario=sequence?key/sampleCount:key;
    const unsigned sample=sequence?samples[key%sampleCount]:60;
    if(phase==0){
        if(app.multiplayer.active)throw std::logic_error("Dust diagnostic cannot use a network peer");
        prepareRoadsideDustRaceFixture(app,scenario);
        for(unsigned tick=0;tick<=sample;++tick)app.simulate(roadsideDustDiagnosticInput(tick));
        const auto count=app.roadsideDust.system().particleCount();
        if(!sequence&&(scenario<2?count<6:count!=0))throw std::runtime_error("Dust live contact/condition gate failed");
        saved.reset();savedSmoke.reset();prepared=key;physics=app.originalSession.rollbackDigest();
    }else{
        if(prepared!=key)throw std::logic_error("Dust comparison before its race fixture");
        if(phase==1){saved=app.roadsideDust;savedSmoke=app.carSmoke;app.roadsideDust.reset();app.carSmoke.reset(app.wet?1:0);}
        else{if(!saved||!savedSmoke)throw std::logic_error("Dust restore before baseline");app.roadsideDust=*saved;app.carSmoke=*savedSmoke;}
    }
    app.input={};
    if(app.originalSession.rollbackDigest()!=physics)throw std::runtime_error("Dust comparison changed physics");
    if(sequence){
        unsigned brown=0,white=0;const auto& system=app.carSmoke.system();
        for(auto index:system.order()){brown+=system.cloud(index).color==0xff8b5439u;white+=system.cloud(index).color==0xffffffffu;}
        std::ofstream out(app.saveRoot.parent_path()/("dust-sequence-"+std::to_string(key)+"-"+std::to_string(phase)+".json"));
        out<<"{\"scenario\":"<<scenario<<",\"tick\":"<<sample<<",\"brown\":"<<brown<<",\"white\":"<<white
           <<",\"particles\":"<<system.particleCount()<<",\"speedKmh\":"<<app.vehicle.speedKmh()<<",\"digest\":\""<<app.carSmoke.digest()<<"\"}\n";
    }
}

// Moving smoke regression: restore recorded source smoke and poses in a
// private fixture, plus live Tsuchisaka dirt. Repaints never run the solver.
void prepareMovingMirrorUnityFixture(App& app,unsigned scene){
    if(scene<670||scene>687)throw std::invalid_argument("Moving mirror fixture");
    static std::optional<CarSmokeState> saved;
    static unsigned prepared=~0u;
    static std::uint64_t physics=0;
    const unsigned key=(scene-670)/3,phase=(scene-670)%3;
    if(phase==0){
        if(app.multiplayer.active)throw std::logic_error("Mirror fixture requires an isolated race");
        const bool dirt=key>=4;
        prepareRoadsideDustRaceFixture(app,dirt?0u:2u,dirt?500u:~0u,dirt?.55f:0.f,dirt?7u:3u);
        if(dirt)for(unsigned tick=0;tick<=61;++tick)app.simulate(roadsideDustDiagnosticInput(tick));
        else{
            const auto path=app.saveRoot.parent_path().parent_path()/"mirror-recorded.idr2";
            const auto size=fs::file_size(path);
            if(size<100||size>34000000)throw std::runtime_error("Bounded private IDR2 fixture required");
            std::vector<std::uint8_t> raw(std::size_t(size),0);std::ifstream file(path,std::ios::binary);
            if(!file.read(reinterpret_cast<char*>(raw.data()),std::streamsize(size)))throw std::runtime_error("Mirror recording read failed");
            const auto word=[&](unsigned at){unsigned value;std::memcpy(&value,raw.data()+at,4);return value;};
            // Recorded turns at 33.75s and 158.4s have particles actually
            // inside the rear camera; low-intensity straight samples do not.
            const unsigned frame=key<2?2025u:9504u;
            if(word(0)!=0x32524449||word(16)!=100||word(20)!=168||word(24)!=4||word(8)<=frame||word(8)>108000||
               std::uint64_t(100)+std::uint64_t(word(8))*168+std::uint64_t(word(92))*136!=size)
                throw std::runtime_error("Mirror fixture requires the recorded revision4 race");
            const unsigned at=100+frame*168;
            ReplayDetail d;std::memcpy(&d,raw.data()+at+28,sizeof d);
            std::memcpy(&app.vehicle.position,raw.data()+at+4,12);std::memcpy(&app.vehicle.yaw,raw.data()+at+16,4);
            std::memcpy(&app.vehicle.speed,raw.data()+at+20,4);app.vehicle.gear=int(word(at+24));
            app.vehicle.rpm=d.rpm;app.vehicle.throttle=d.throttle;app.vehicle.brake=d.brake;app.previous=app.vehicle;
            app.previousPlayerBodyWorld=app.playerBodyWorld=d.bodyPosition;
            app.previousPitch=app.bodyPitch=d.pitch;app.previousRoll=app.bodyRoll=d.roll;
            app.wheelPose.steeringRadians=d.steering;app.wheelPose.suspensionY=d.suspension;app.wheelPose.rotationRadians=d.rotation;
            app.previousWheelPose=app.wheelPose;
            const auto projection=app.course.project(app.vehicle.position);app.segment=projection.segment;app.progress=projection.sample.distance;
            app.originalCoordinate.index=int(projection.segment);app.originalCoordinate.fraction=0;
            app.originalPath.project({app.vehicle.position.x,app.vehicle.position.y,app.vehicle.position.z},app.originalCoordinate,true);
            app.courseLightPathIndex=app.originalCoordinate.index;
            app.race.elapsed6000=d.elapsed;app.race.remaining6000=d.remaining;app.race.sectionTimes6000=d.sections;app.race.sector=int(d.sector);
            const Vec3 angles{-d.pitch,wrapAngle(app.vehicle.yaw-pi),-d.roll};
            app.originalCamera.reset();app.previousCameraFrame=app.originalCamera.update(app.vehicle.position,angles);
            app.bumperCamera.reset();app.previousBumperFrame=app.bumperCamera.update(app.vehicle.position,angles);
            app.previousRearCameraFrame=app.rearCameraFrame=app.originalCamera.rearView(app.vehicle.position,angles);
            ReplayCarSmokePlayback timeline;timeline.load(std::span(raw).subspan(100+word(8)*168),app.carSmoke);
            timeline.seek(word(at+160),app.carSmoke);
        }
        app.drivingView=key%2?OriginalDrivingView::Chase:OriginalDrivingView::Bumper;
        if(!app.carSmoke.system().particleCount())throw std::runtime_error("Moving mirror fixture needs source smoke");
        saved.reset();prepared=key;physics=app.originalSession.rollbackDigest();
    }else{
        if(prepared!=key)throw std::logic_error("Moving mirror comparison before fixture");
        if(phase==1){saved=app.carSmoke;app.carSmoke.reset(app.wet?1:0);}
        else{if(!saved)throw std::logic_error("Mirror smoke restore before removal");app.carSmoke=*saved;}
    }
    app.input={};
    if(app.originalSession.rollbackDigest()!=physics)throw std::runtime_error("Mirror smoke comparison changed physics");
}
