#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include "../tests/online_scored_driver.h"
#include <iomanip>
#include <iostream>

// Report-specific observation: Irohazaka, R32 versus CT9A, normal controls.
// Uses the real App prediction/presentation/recording path and a numerical
// second peer. No public services, audio device, actor placement or timer edits.
int main(int argc,char** argv)try{
    if(argc!=6)throw std::runtime_error("Native root, NEW output, collisions 0/1, local slot 0/1, packet interval 1..32 required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    const auto collisions=std::stoul(argv[3]),local=std::stoul(argv[4]),interval=std::stoul(argv[5]);
    if(collisions>1||local>1||interval<1||interval>32||fs::exists(out))throw std::runtime_error("Invalid fixture or existing output");
    fs::create_directories(out);
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    if(!app->renderer.initialize(nullptr,960,720,true))throw std::runtime_error("Offscreen setup failed");
    // Keep source HUD blends on the game's command path. These native BMPs
    // inspect the 3D cars; the command-only HUD is not composited into them.
    Idas3UiEnable(1);
    app->demoPreparationStarted=true;app->replayRecordingFlags=2;
    std::array<original::OriginalBattleProfile,2> profiles;
    for(unsigned slot=0;slot<2;++slot){auto& p=profiles[slot];p=original::makeOriginalFreshBattleProfile();p.setu(16,slot?21:7);p.setByte(164,63);}
    Idas3MultiplayerConfig config{sizeof(config),2,5,0,0,1,profiles[local].u(16),profiles[1-local].u(16),unsigned(local),1};
    app->startMultiplayer(config,&profiles[local],&profiles[1-local]);
    constexpr std::uint64_t raceId=921101;
    app->enableAuthority(raceId,true,true,collisions!=0);
    app->setMultiplayerGo(true);app->loadingActive=app->preRaceDialogueActive=app->vsActive=false;
    app->drivingView=OriginalDrivingView::Chase;
    const auto selected=app->authorityRace->setup();
    original::OnlineRaceSimulation baseline(root,selected),peer(root,selected);
    unsigned checks=0;std::size_t verifiedPoses=0;
    const auto require=[&](bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);};
    require(selected.condition==10&&selected.night&&!selected.wet&&selected.collisions==(collisions!=0),"Wrong report conditions");
    require(selected.profiles[0].u(16)==7&&selected.profiles[1].u(16)==21,"Wrong car order");
    std::vector<std::array<ReplayFrame,2>> expectedPoses;
    std::vector<std::uint64_t> digests;
    original::OnlineRaceLink link(peer,raceId,local!=0,[&](const auto& frame,auto digest){
        require(frame.frame<digests.size()&&digest==digests[frame.frame],"Peer confirmed a different physical state");
    });
    const auto verifyRecording=[&]{
        require(app->recording.frames.size()==app->rivalRecording.frames.size(),"Paired replay clocks diverged");
        while(verifiedPoses<app->recording.frames.size()){
            const auto& wanted=expectedPoses.at(verifiedPoses);
            for(unsigned slot=0;slot<2;++slot){
                const auto& actual=(slot==local?app->recording:app->rivalRecording).frames[verifiedPoses];
                require(actual.tick==verifiedPoses+1&&std::memcmp(&actual.position,&wanted[slot].position,sizeof(Vec3))==0&&actual.yaw==wanted[slot].yaw,
                    "Replay retained predicted or visual-correction positions");
            }
            ++verifiedPoses;
        }
    };
    const auto exchange=[&]{
        for(unsigned attempt=0;attempt<4;++attempt){const auto a=app->authorityLink->packet(),b=link.packet();
            app->receiveAuthority(b);link.receive(a);link.reconcile();
            app->receiveAuthority(b);link.receive(a);link.reconcile();
        }
        require(app->authorityRace->digest()==baseline.digest()&&peer.digest()==baseline.digest(),"Peers diverged after packet drain");
        verifyRecording();
    };
    std::ofstream csv(out/"samples.csv");csv<<std::setprecision(9)
        <<"tick,run,contact,phase0,phase1,progress0,progress1,canonical_distance,predicted_distance,displayed_distance,offset0,offset1,response0,response1";
    for(const auto* owner:{"canonical0","canonical1","predicted0","predicted1","displayed0","displayed1"})for(const auto* axis:{"x","y","z"})csv<<','<<owner<<'_'<<axis;
    csv<<'\n';
    std::array<original::OriginalHostInputState,2> adapters{};
    unsigned ticks=0,physicalResponses=0;int winner=-3;
    for(;ticks<20000;++ticks){
        std::array<original::OriginalHostControls,2> controls;std::array<original::OriginalVehicleInputs,2> inputs;
        for(unsigned slot=0;slot<2;++slot){controls[slot]=onlineScoredDriver(baseline,slot);inputs[slot]=original::adaptOriginalHostInput(adapters[slot],controls[slot],true,false,0);}
        const auto frame=baseline.step(inputs);digests.push_back(baseline.digest());
        if(frame.start.runRules)expectedPoses.push_back(frame.replayPoses);
        DriverInput input;input.steer=controls[local].steering;input.throttle=controls[local].throttle;input.brake=controls[local].brake;input.automatic=true;
        app->simulate(input);require(!app->authorityStalled&&link.step(inputs[1-local]),"Bounded connection unexpectedly stalled");
        std::array<Vec3,2> canonical,predicted,displayed;
        for(unsigned slot=0;slot<2;++slot){canonical[slot]=frame.replayPoses[slot].position;
            const auto& actor=app->authorityRace->car(slot).actor();predicted[slot]={actor.f(0),actor.f(4),actor.f(8)};
            displayed[slot]=slot==local?app->vehicle.position:app->rivalVehicle.position;
        }
        const auto response=[&](unsigned slot){const auto& d=baseline.car(slot).vehicle().drive;return d.u(0x15c)?std::hypot(d.f(0x260),d.f(0x264)):0.f;};
        physicalResponses+=(response(0)>0||response(1)>0);
        csv<<ticks<<','<<frame.start.runRules<<','<<frame.driving[0].bodyCollision.active;
        for(const auto& state:frame.states)csv<<','<<unsigned(state.phase);
        for(const auto& state:frame.states)csv<<','<<float(state.progress.index)+state.progress.fraction;
        csv<<','<<length(canonical[0]-canonical[1])<<','<<length(predicted[0]-predicted[1])<<','<<length(displayed[0]-displayed[1]);
        for(const auto& offset:app->authorityVisualOffset)csv<<','<<length(offset);
        csv<<','<<response(0)<<','<<response(1);
        for(const auto& pair:{canonical,predicted,displayed})for(const auto& position:pair)csv<<','<<position.x<<','<<position.y<<','<<position.z;
        csv<<'\n';
        if(ticks==1900||ticks==7130||ticks==7819||ticks==11000||ticks==12600){
            const auto before=app->authorityRace->digest();
            Idas3UiBeginFrame(960,720);
            require(app->render(0),"Offscreen race capture failed");
            require(app->renderer.saveBitmap((out/("race-"+std::to_string(ticks)+".bmp")).wstring()),"Cannot save offscreen race capture");
            require(before==app->authorityRace->digest(),"Rendering changed race physics");
        }
        if(ticks%interval==interval-1)exchange();
        // A device-free fixture must still clock the mixer, otherwise finished
        // one-shots never release their physical voices during a long race.
        app->updateAudioScene();
        for(unsigned sample=0;sample<735;++sample)app->audio.renderStereo(800,0,0,0,true);
        winner=original::resolveOnlineRaceWinner(frame.states);
        if(ticks%1800==0)std::cout<<"tick="<<ticks<<" contacts="<<baseline.contactFrames()<<" progress="<<frame.states[0].progress.index<<'/'<<frame.states[1].progress.index<<'\n'<<std::flush;
        if(winner!=-3){++ticks;break;}
    }
    exchange();
    require(verifiedPoses==expectedPoses.size(),"Final confirmed replay is incomplete");
    require(app->authorityLink->verifiedPeerFrames()==ticks&&link.verifiedPeerFrames()==ticks,"Peer history not mutually verified");
    require(!collisions?baseline.contactFrames()==0:true,"Disabled collisions produced a contact");
    for(const auto* replay:{&app->recording,&app->rivalRecording}){
        const auto bytes=replay->sharedBytes(unsigned(replay->frames.back().tick*100));
        std::ofstream file(out/(replay==&app->recording?"player.idr2":"opponent.idr2"),std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        require(bool(file),"Cannot save paired replay evidence");
    }
    std::ofstream summary(out/"result.json");summary<<"{\"passed\":true,\"checks\":"<<checks<<",\"ticks\":"<<ticks
        <<",\"collisions\":"<<collisions<<",\"localSlot\":"<<local<<",\"packetInterval\":"<<interval<<",\"winner\":"<<winner
        <<",\"contacts\":"<<baseline.contactFrames()<<",\"physicalResponseFrames\":"<<physicalResponses<<",\"verifiedReplayFrames\":"<<verifiedPoses
        <<",\"rollbacks\":"<<app->authorityLink->timeline().metrics().rollbacks<<",\"progress\":["<<baseline.rules(0).state().progress.index<<','<<baseline.rules(1).state().progress.index
        <<"],\"goalIndex\":"<<baseline.rules(0).rules().goalIndex<<",\"scope\":\"Local App and numerical second peer; ordinary path-following controls; no Internet or pixel-overlap assertion.\"}\n";
    app->leaveMultiplayer();std::cout<<"PASS checks="<<checks<<" ticks="<<ticks<<" contacts="<<baseline.contactFrames()<<" winner="<<winner<<'\n';
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
