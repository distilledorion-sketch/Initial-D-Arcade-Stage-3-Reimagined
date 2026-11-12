// The reported parked-car mirror cloud must be checked through Unity as well
// as the native offscreen renderer. All scenarios use isolated saves.
void prepareStoppedMirrorUnityFixture(App& app,unsigned scene){
    if(scene<530||scene>547)throw std::invalid_argument("Stopped mirror fixture");
    static CarSmokeState savedSmoke;
    static RoadsideDustPresentation savedDust;
    static std::uint64_t physics=0;
    static unsigned prepared=~0u;
    const unsigned scenario=(scene-530)/3,phase=(scene-530)%3;
    if(phase==0){
        if(app.multiplayer.active)throw std::logic_error("Mirror diagnostic cannot use a peer");
        const unsigned condition=scenario==1?1u:scenario==2||scenario==5?4u:scenario==3?2u:0u;
        const unsigned course=scenario==2||scenario==3||scenario==5?3u:7u;
        prepareRoadsideDustRaceFixture(app,condition,scenario==1?500u:~0u,scenario==1?.55f:0.f,course);
        for(unsigned tick=0;tick<(scenario>=4?1200u:125u);++tick){
            DriverInput input;input.automatic=true;input.brake=1;
            if(scenario>=4&&tick<600){
                input.brake=0;input.throttle=1;
                const auto projection=app.course.project(app.vehicle.position,app.segment);
                const float ahead=std::max(12.f,app.vehicle.speed*.65f);
                const auto target=app.course.sample(projection.sample.distance+ahead).center-app.vehicle.position;
                const auto angle=wrapAngle(std::atan2(target.x,target.z)-app.vehicle.yaw);
                input.steer=-std::clamp(std::atan2(2*app.config.wheelbase*std::sin(angle),ahead)/recoveredSteeringLimit,-.75f,.75f);
            }
            app.simulate(input);
            // Host rain/spray and older effects advance during scene painting,
            // not simulate(). Exercise that clock while driving and stopping.
            if(!app.render(1./60.))throw std::runtime_error("Stopped mirror effect clock render");
        }
        if(std::abs(app.vehicle.speedKmh())>=.1f||!app.raceRearViewActive())throw std::runtime_error("Parked mirror fixture did not stop with a mirror");
        if(app.drivingEffects.smokeCount()!=0)throw std::runtime_error("Parked car retained legacy smoke");
        savedSmoke=app.carSmoke;savedDust=app.roadsideDust;physics=app.originalSession.rollbackDigest();prepared=scenario;
    }else{
        if(prepared!=scenario)throw std::logic_error("Mirror comparison before preparation");
        if(phase==1){app.carSmoke.reset(app.wet?1:0);app.roadsideDust.reset();}
        else{app.carSmoke=savedSmoke;app.roadsideDust=savedDust;}
    }
    app.input={};
    if(app.originalSession.rollbackDigest()!=physics)throw std::runtime_error("Mirror comparison changed physics");
    std::ofstream out(app.saveRoot.parent_path()/("parked-mirror-"+std::to_string(scenario)+"-"+std::to_string(phase)+".json"));
    out<<"{\"course\":"<<app.courseIndex<<",\"night\":"<<app.night<<",\"wet\":"<<app.wet
       <<",\"speedKmh\":"<<app.vehicle.speedKmh()<<",\"particles\":"<<app.carSmoke.system().particleCount()
       <<",\"dustParticles\":"<<app.roadsideDust.system().particleCount()<<"}\n";
}
