// Included inside App. Version2 is opt-in and owns a separate two-car race.
std::unique_ptr<original::OnlineRaceSimulation> authorityRace;
std::unique_ptr<original::OnlineRaceLink> authorityLink;
original::OriginalHostInputState authorityInput;
std::array<Vec3,2> authorityVisualOffset{};
std::array<float,2> authorityYawOffset{};
std::array<std::uint64_t,2> authorityFinishFrame{},authorityFinishTicks{};
std::array<bool,2> authorityFinished{},authorityTimeUp{};
bool authorityStalled=false;
int authorityConfirmedWinner=-3;

const original::OriginalDrivingSession& presentedSession()const {
    return authorityRace?authorityRace->car(multiplayer.config.localSlot):originalSession;
}
void clearAuthority(){
    authorityLink.reset();authorityRace.reset();authorityInput={};
    authorityVisualOffset={};authorityYawOffset={};authorityFinishFrame={};authorityFinishTicks={};
    authorityFinished={};authorityTimeUp={};authorityStalled=false;authorityConfirmedWinner=-3;
}
void confirmedAuthorityFrame(const original::OnlineRaceFrame& frame){
    const auto local=multiplayer.config.localSlot;
    for(unsigned slot=0;slot<2;++slot){const auto& state=frame.states[slot];
        if(!authorityFinished[slot]&&(state.phase==original::OriginalRacePhase::Finished||state.phase==original::OriginalRacePhase::TimeUp)){
            authorityFinished[slot]=true;authorityTimeUp[slot]=state.phase==original::OriginalRacePhase::TimeUp;
            authorityFinishFrame[slot]=frame.frame+1;
            authorityFinishTicks[slot]=(authorityTimeUp[slot]?state.elapsed.value:state.times.finishTime)/100u;
        }
    }
    authorityConfirmedWinner=original::resolveOnlineRaceWinner(frame.states);
    // All commands were decided within the captured numerical RNG boundary.
    // Playback occurs once on confirmation and cannot feed back into physics.
    if(frame.start.cue2)audio.playRaceCue(4,2);
    if(frame.start.cue3)audio.playRaceCue(4,3);
    for(auto cue:frame.driving[local].feedback142460)audio.playRaceCue(2,cue);
    if(frame.driving[local].completion.requestCue4)audio.playRaceCue(2,4);
    audio.applyConfirmedOnlineAudio(frame.engine[local],frame.tires[local]);
    if(frame.rules[local].timeExtension)raceFeedback.extend(std::bit_cast<std::int32_t>(frame.states[local].remaining.value)-frame.rules[local].secondsAdded*6000);
}
void enableAuthority(std::uint64_t raceId,bool remoteAutomatic,bool boost){
    if(!multiplayer.active||multiplayer.config.version!=2||!multiplayer.waiting||authorityRace)
        throw std::logic_error("Authority mode requires a fresh, held version2 race");
    original::OnlineRaceSetup setup;setup.condition=unsigned(courseIndex)*2+unsigned(reverse);setup.wet=wet;setup.boost=boost;
    if(importedCourse)setup.imported=std::make_shared<const ImportedCourse>(*importedCourse);
    const auto local=multiplayer.config.localSlot;
    setup.profiles[local]=battleProfile;setup.profiles[1-local]=multiplayer.remoteProfile;
    setup.automatic[local]=automatic;setup.automatic[1-local]=remoteAutomatic;archiveOpponentAutomatic=remoteAutomatic;
    clearAuthority();authorityRace=std::make_unique<original::OnlineRaceSimulation>(root,setup);
    authorityLink=std::make_unique<original::OnlineRaceLink>(*authorityRace,raceId,local==0,
        [this](const auto& frame,auto){confirmedAuthorityFrame(frame);});
    originalSession.setEngineOutput({});
    projectOriginalPose(false);projectAuthorityRemote();clock.reset();
}
void projectAuthorityRemote(bool advance=false){
    if(!authorityRace)return;
    const auto oldVehicle=rivalVehicle;const auto oldBody=rivalBodyWorld;const auto oldWheels=rivalWheels;
    const float oldPitch=rivalPitch,oldRoll=rivalRoll;
    const auto remote=1-multiplayer.config.localSlot;const auto& car=authorityRace->car(remote);const auto& actor=car.actor();
    auto& pose=multiplayer.remote;pose={sizeof(pose),1};pose.flags=Idas3MpActive|(multiplayer.waiting?Idas3MpWaiting:0)|(night?Idas3MpHeadlights:0);
    if(authorityFinished[remote])pose.flags|=Idas3MpFinished;
    if(authorityTimeUp[remote])pose.flags|=Idas3MpTimeUp;
    if(car.vehicle().controls.brake>.05f)pose.flags|=Idas3MpBrake;
    pose.car=multiplayer.config.remoteCar;pose.sequence=authorityRace->frame()+1;
    const auto& state=authorityRace->rules(remote).state();pose.raceTicks=state.elapsed.value/100u;
    const Vec3 position=Vec3{actor.f(0),actor.f(4),actor.f(8)}+authorityVisualOffset[remote];
    pose.actorPosition[0]=position.x;pose.actorPosition[1]=position.y;pose.actorPosition[2]=position.z;
    const auto body=rivalBody.update(car.collision(),pose.car,position);
    pose.bodyPosition[0]=body.x;pose.bodyPosition[1]=body.y;pose.bodyPosition[2]=body.z;
    pose.yaw=wrapAngle(actor.f(28)+pi+authorityYawOffset[remote]);pose.pitch=-actor.f(24);pose.roll=-actor.f(32);
    pose.steering=actor.f(60);for(unsigned i=0;i<4;++i){pose.suspension[i]=actor.f(64+i*4);pose.wheelRotation[i]=actor.f(96+i*4);}
    pose.speed=car.vehicle().drive.f(0x238);pose.rpm=car.vehicle().transmission.tach1c;
    pose.progress=float(state.progress.index)+state.progress.fraction;
    const auto& lights=rivalPresentation.headlightState();pose.headlightPhase=lights.phase;pose.headlightCounter=lights.counter;pose.headlightVisible=lights.visible;
    projectMultiplayerRemote();multiplayer.received=true;
    rivalVehicle.gear=int(car.vehicle().transmission.gear00);rivalVehicle.throttle=car.vehicle().controls.throttle;
    // Authority poses are fixed-step samples, unlike the already-interpolated
    // legacy network snapshots. Retain the previous sample for render interpolation.
    if(advance){previousRival=oldVehicle;previousRivalBodyWorld=oldBody;previousRivalWheels=oldWheels;previousRivalPitch=oldPitch;previousRivalRoll=oldRoll;}
}
void receiveAuthority(std::span<const std::uint8_t> packet){
    if(!authorityLink)throw std::logic_error("No authority race");
    std::array<Vec3,2> positions;std::array<float,2> yaw;
    for(unsigned i=0;i<2;++i){const auto& a=authorityRace->car(i).actor();positions[i]={a.f(0),a.f(4),a.f(8)};yaw[i]=a.f(28);}
    authorityLink->receive(packet);authorityLink->reconcile();
    for(unsigned i=0;i<2;++i){const auto& a=authorityRace->car(i).actor();
        authorityVisualOffset[i]=authorityVisualOffset[i]+positions[i]-Vec3{a.f(0),a.f(4),a.f(8)};
        const auto distance=length(authorityVisualOffset[i]);if(distance>4)authorityVisualOffset[i]=authorityVisualOffset[i]*(4/distance);
        authorityYawOffset[i]=wrapAngle(authorityYawOffset[i]+yaw[i]-a.f(28));
    }
}
void simulateAuthority(const DriverInput& d){
    auto inputState=authorityInput;
    const auto input=original::adaptOriginalHostInput(inputState,{d.steer,d.throttle,d.brake,d.shiftDown,d.shiftUp},automatic,false,0);
    if(!authorityLink->step(input)){authorityStalled=true;return;}
    authorityStalled=false;authorityInput=inputState;
    for(unsigned i=0;i<2;++i){authorityVisualOffset[i]=authorityVisualOffset[i]*.875f;authorityYawOffset[i]*=.875f;}
    previous=vehicle;previousPitch=bodyPitch;previousRoll=bodyRoll;previousWheelPose=wheelPose;
    const auto local=multiplayer.config.localSlot;const auto& state=authorityRace->rules(local).state();const auto& frame=authorityRace->lastFrame();
    originalRaceStart=authorityRace->start();originalRace.restoreNumericalState(state);originalCoordinate=state.previousCoordinate;originalRaceOwnerFrame=std::uint32_t(authorityRace->frame());
    race.originalStartDigit=frame.start.countdownDigit;race.originalStartElapsed=240-frame.start.countdownRemaining;race.countdown=int(frame.start.countdownRemaining>60?frame.start.countdownRemaining-60:0);
    race.phase=authorityFinished[local]?RacePhase::Finished:frame.start.runRules?RacePhase::Running:RacePhase::Countdown;
    race.timeUp=authorityFinished[local]&&authorityTimeUp[local];race.ticks=state.elapsed.value/100u;
    race.elapsed6000=authorityRace->rules(local).displayedElapsed();race.remaining6000=std::bit_cast<std::int32_t>(state.remaining.value);
    race.progress=float(state.progress.index)+state.progress.fraction;race.furthest=std::max(race.furthest,race.progress);
    race.sector=int(state.times.sectionCount);race.sectionTimes6000=state.times.sectionTimes;
    for(unsigned i=0;i<4;++i)race.splits[i]=race.sectionTimes6000[i]/100;
    projectOriginalPose(true);projectAuthorityRemote(true);advanceOriginalCamera();
    projectedLightPriorAdvantage=battleMetrics.advantage(state.progress,authorityRace->rules(1-local).state().progress);
    advanceProjectedHeadlights();
    const auto projected=projectRacePosition(vehicle.position);segment=projected.segment;progress=projected.sample.distance;
    if(raceFeedback.tick(race.remaining6000))audio.playRaceCue(4,1);
}
int authorityWinner()const {
    if(!authorityLink||!authorityFinished[0]||!authorityFinished[1]||authorityLink->verifiedPeerFrames()<std::max(authorityFinishFrame[0],authorityFinishFrame[1]))return -3;
    return authorityConfirmedWinner;
}
