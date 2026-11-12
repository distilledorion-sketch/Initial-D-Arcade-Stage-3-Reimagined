#include "online_race_test_driver.h"

// Use the real App authority lifecycle with a numerical remote peer. All
// rendering remains offscreen, with no audio device or Internet connection.
unsigned runOnlineCarSmokeAppTests(App& app,const fs::path& out){
    unsigned checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    for(unsigned local=0;local<2;++local){
        Idas3MultiplayerConfig config{sizeof(config),2,7,0,1,0,local?19u:0u,local?0u:19u,local,1};
        app.startMultiplayer(config);app.enableAuthority(905001+local,true,false,true);
        app.setMultiplayerGo(true);app.loadingActive=app.preRaceDialogueActive=app.vsActive=false;
        const auto selected=app.authorityRace->setup();
        original::OnlineRaceSimulation baseline(app.root,selected),peer(app.root,selected);
        original::OnlineRaceLink link(peer,905001+local,local!=0);
        std::array<original::OriginalHostInputState,2> inputStates{};
        bool captured=false;
        const auto exchange=[&](){
            for(unsigned settle=0;settle<3;++settle){const auto a=app.authorityLink->packet(),b=link.packet();
                app.receiveAuthority(b);link.receive(a);link.reconcile();}
        };
        for(unsigned tick=0;tick<480;++tick){
            std::array<original::OriginalHostControls,2> controls;
            std::array<original::OriginalVehicleInputs,2> inputs;
            for(unsigned slot=0;slot<2;++slot){controls[slot]=onlineRaceTestControls(baseline,slot,tick);
                inputs[slot]=original::adaptOriginalHostInput(inputStates[slot],controls[slot],true,false,0);}
            baseline.step(inputs);
            DriverInput input;input.steer=controls[local].steering;input.throttle=controls[local].throttle;input.brake=controls[local].brake;
            input.shiftDown=controls[local].shiftDown;input.shiftUp=controls[local].shiftUp;input.automatic=true;
            app.simulate(input);check(!app.authorityStalled&&link.step(inputs[1-local]),"App smoke fixture stalled before its deliberate outage");
            check(app.carSmoke.frames()==0,"App advanced duplicate offline smoke during an online race");
            if(tick%17==16||tick==479){exchange();
                check(app.authorityRace->digest()==baseline.digest()&&peer.digest()==baseline.digest(),"App smoke fixture changed corrected physics");
                check(app.displayedCarSmoke().digest()==baseline.smoke(local).digest(),"App renders the wrong view or stale speculative smoke");
                check(peer.smoke(local).digest()==baseline.smoke(local).digest(),"Numerical peer smoke correction differs");
                if(!captured&&tick>240&&app.displayedCarSmoke().system().particleCount()>3){
                    check(app.render(0),"Online smoke scene could not render");
                    const auto saved=app.displayedCarSmoke();
                    const auto capture=[&](const char* suffix){
                        check(app.render(0),"Online smoke comparison render failed");
                        const auto path=out/("online-"+std::to_string(local)+suffix+".bmp");
                        check(app.renderer.saveBitmap(path.wstring()),"Online smoke screenshot failed");
                        std::ifstream in(path,std::ios::binary);BITMAPFILEHEADER header{};in.read(reinterpret_cast<char*>(&header),sizeof header);in.seekg(header.bfOffBits);
                        std::vector<unsigned> pixels(960*720);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);check(bool(in),"Online smoke screenshot truncated");return pixels;
                    };
                    const auto on=capture("-on");
                    // Isolated display comparison only, immediately restored
                    // before another simulation or packet can be processed.
                    auto& visual=const_cast<CarSmokeState&>(app.authorityRace->smoke(local));
                    visual.reset(1);const auto off=capture("-off");visual=saved;
                    const auto restored=capture("-restored");unsigned changed=0;
                    check(on==restored,"Online smoke restored frame differs");
                    for(unsigned i=0;i<on.size();++i)changed+=(on[i]&0xffffff)!=(off[i]&0xffffff);
                    check(app.displayedCarSmoke().digest()==saved.digest(),"Online repaint advanced particles or RNG");
                    if(changed){captured=true;std::cout<<"online slot="<<local<<" tick="<<tick<<" smokePixels="<<changed<<'\n'<<std::flush;}
                }
            }
        }
        check(captured,"Online App smoke never contributed visible race pixels");
        check(app.authorityLink->verifiedPeerFrames()==480&&link.verifiedPeerFrames()==480,"App peers failed to verify their input history");
        check(app.authorityLink->timeline().metrics().rollbacks>0,"App smoke fixture never rolled back");
        DriverInput held;held.automatic=true;held.throttle=1;
        for(unsigned i=0;i<80&&!app.authorityStalled;++i)app.simulate(held);
        check(app.authorityStalled,"Deliberate packet outage did not exhaust prediction history");
        const auto smoke=app.displayedCarSmoke().digest(),physics=app.authorityRace->digest();
        for(unsigned i=0;i<8;++i){app.simulate(held);check(app.render(1./60.),"Stalled online scene render failed");
            check(app.displayedCarSmoke().digest()==smoke&&app.authorityRace->digest()==physics,"Stalled online draw/input advanced smoke or physics");}
        app.leaveMultiplayer();check(!app.authorityRace,"Online smoke owner survived leaving its race");
    }
    std::cout<<"PASS "<<checks<<" App online smoke checks; both slots, corrected input, full race pixels, stalled repaints and room cleanup.\n";
    return checks;
}
