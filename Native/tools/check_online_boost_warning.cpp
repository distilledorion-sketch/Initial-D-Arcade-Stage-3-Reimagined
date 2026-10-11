#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
#include <map>

// Real App showcase and command-renderer regression. Physics is held at its
// initialized boundary so presentation must not alter either numerical car.
// All storage belongs to the required new evidence directory.
int main(int argc,char** argv)try{
    if(argc<3||argc>4)throw std::runtime_error("Native root, NEW evidence directory, optional arcade language required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");
    fs::create_directories(out);setOriginalUiLanguage(argc==4?std::stoi(argv[3]):0);
    unsigned checks=0;const auto check=[&](bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);};
    const auto warningBank=NativeTextureBank::load(root/"data/original_assets/hud/start2d/textures/textures.idastex");
    const auto& warning=warningBank.at(11);std::vector<std::uint8_t> warningRgba;
    for(auto pixel:warning.argb){warningRgba.push_back(std::uint8_t(pixel>>16));warningRgba.push_back(std::uint8_t(pixel>>8));
        warningRgba.push_back(std::uint8_t(pixel));warningRgba.push_back(std::uint8_t(pixel>>24));}
    // Only start2d chunk81 uses source texture11. Match actual submitted image
    // bytes, not capture texture IDs, which vary with preceding screens.
    std::map<unsigned,bool> warningTextures;
    const auto warningDraws=[&](){
        UnityUiFrame frame{sizeof(frame)};check(Idas3UiGetFrame(&frame)==1,"UI frame unavailable");
        check(frame.unresolvedSurfaces==0,"Scene submitted an unresolved UI surface");
        std::vector<UnityUiDraw> draws(frame.drawCount);
        check(Idas3UiCopyDraws(draws.data(),int(draws.size()))==int(draws.size()),"UI draw copy failed");
        unsigned count=0;
        for(const auto& draw:draws){
            auto found=warningTextures.find(draw.texture);
            if(found==warningTextures.end()){
                UnityUiTextureInfo info{sizeof(info)};bool match=false;
                if(Idas3UiGetTextureInfo(draw.texture,&info)==1&&info.width==warning.width&&info.height==warning.height&&info.bytes==warningRgba.size()){
                    std::vector<std::uint8_t> pixels(info.bytes);
                    check(Idas3UiCopyTextureRGBA(draw.texture,pixels.data(),int(pixels.size()))==int(pixels.size()),"Warning texture copy failed");
                    match=pixels==warningRgba;
                }
                found=warningTextures.emplace(draw.texture,match).first;
            }
            if(found->second&&draw.opacity>0&&draw.count>=3)++count;
        }
        return count;
    };
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);app->loadStartPresentation();
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    check(app->renderer.initializeSceneCapture(640,480),"Scene capture renderer unavailable");
    Idas3UiEnable(1);app->demoPreparationStarted=true;app->replayRecordingFlags=0;
    std::array<original::OriginalBattleProfile,2> profiles;
    for(unsigned slot=0;slot<2;++slot){
        auto& p=profiles[slot];p=original::makeOriginalFreshBattleProfile();p.setu(16,slot?8:0);p.setu(72,45000+slot*1234);
        p.setu(1180,0xc01);p.setByte(153,57);p.setByte(164,63);p.setByte(116,0x21);p.setu(1080,7);
        check(app->profiles.save(p.u(16),p),"Isolated profile fixture failed");
    }
    app->frontend.car=0;app->loadedProfileCar=0;app->frontend.battleProfile=profiles[0];app->battleProfile=profiles[0];
    app->validationMode=false;
    std::ofstream report(out/"showcase.csv");
    report<<"slot,match,boost,phase,source_tick,vs_active,warning_chunks,warning_draws,authority_frame,authority_digest\n";
    const auto hasWarning=[&](){const auto chunks=app->vsBanner.chunks();return std::find(chunks.begin(),chunks.end(),81)!=chunks.end();};
    const auto render=[&](){
        Idas3UiBeginFrame(640,480);check(app->render(0),"Actual App scene render failed");
        const auto& scene=app->renderer.sceneCapture()->frame();
        check(scene.vertexCount>0&&scene.rangeCount>0,"Showcase/driving scene geometry disappeared");
        return warningDraws();
    };
    std::uint64_t raceId=900000;unsigned matches=0;
    for(unsigned slot=0;slot<2;++slot){
        // Reuse the actual App and banner across rematches to catch stale Off
        // state in both directions, independently of which client is local.
        unsigned match=0;
        for(bool boost:{false,true,false}){
            Idas3MultiplayerConfig config{sizeof(config),2,0,0,0,0,profiles[slot].u(16),profiles[1-slot].u(16),slot,1};
            app->startMultiplayer(config,&profiles[slot],&profiles[1-slot]);
            check(!app->authorityRace&&!app->vsActive&&app->multiplayer.waiting,"Rematch retained previous authority or showcase");
            app->multiplayer.localName=slot?"GUEST":"HOST";app->multiplayer.remoteName=slot?"HOST":"GUEST";
            app->multiplayer.records[0]={120,60,1,0};app->multiplayer.records[1]={24,6,1,0};
            app->enableAuthority(++raceId,true,boost);
            check(app->authorityRace->setup().boost==boost,"Native authority did not own the selected boost rule");
            const auto digest=app->authorityRace->digest(),authorityFrame=app->authorityRace->frame();
            const auto raceProfile=app->battleProfile.words,frontendProfile=app->frontend.battleProfile.words;
            const auto remoteProfile=app->multiplayer.remoteProfile.words,savedProfile=app->multiplayer.savedProfile.words;
            const auto authorityProfiles=app->authorityRace->setup().profiles;
            const auto unchanged=[&](){
                check(app->authorityRace->digest()==digest&&app->authorityRace->frame()==authorityFrame,"Showcase changed the authority simulation");
                check(app->battleProfile.words==raceProfile&&app->frontend.battleProfile.words==frontendProfile&&
                    app->multiplayer.remoteProfile.words==remoteProfile&&app->multiplayer.savedProfile.words==savedProfile,"Showcase changed profile points, tuning or records");
                for(unsigned side=0;side<2;++side)check(app->authorityRace->setup().profiles[side].words==authorityProfiles[side].words,"Showcase changed authority profile inputs");
            };
            check(render()==0,"Held lobby submitted a prior Boost Off warning");unchanged();
            app->setMultiplayerGo(true);
            check(app->vsActive&&app->vsBanner.sourceTick()==0,"Released online race did not restart showcase");
            check(hasWarning()==!boost,"Boost Off warning disagrees with native authority");
            check(app->vsBanner.displayName(0)=="HOST"&&app->vsBanner.displayName(1)=="GUEST","Warning changed slot-ordered player labels");
            const auto capture=[&](const char* phase,bool expected){
                const auto draws=render();check((draws>0)==expected,"Submitted warning artwork disagrees with showcase state");unchanged();
                report<<slot<<','<<match<<','<<boost<<','<<phase<<','<<app->vsBanner.sourceTick()<<','<<app->vsActive<<','
                    <<hasWarning()<<','<<draws<<','<<app->authorityRace->frame()<<','<<app->authorityRace->digest()<<'\n';report.flush();
            };
            capture("intro-start",!boost);
            for(unsigned tick=0;tick<120;++tick)app->advanceStartPresentation(1./60.);
            check(app->vsActive&&app->vsBanner.sourceTick()==120,"First showcase shot ended early");capture("shot-one",!boost);
            app->advanceStartPresentation(1./60.);check(app->vsShot==1,"Second showcase shot did not begin");capture("shot-two",!boost);
            for(unsigned tick=121;tick<240;++tick)app->advanceStartPresentation(1./60.);
            check(app->vsActive&&app->vsBanner.sourceTick()==240,"Showcase lost its terminal display frame");capture("intro-last",!boost);
            app->advanceStartPresentation(1./60.);check(!app->vsActive,"Showcase warning owner survived its completion");capture("countdown",false);
            capture("driving-repaint",false);
            app->leaveMultiplayer();check(!app->authorityRace&&!app->multiplayer.active&&!app->vsActive,"Mode return retained warning authority/owner");
            for(unsigned side=0;side<2;++side)check(app->profiles.load(profiles[side].u(16)).profile.words==profiles[side].words,"Showcase or rematch changed an isolated saved profile");
            ++match;++matches;
        }
        // Old snapshot-only multiplayer has no negotiated boost truth. Its
        // default/no-authority state must not masquerade as explicit Boost Off.
        Idas3MultiplayerConfig config{sizeof(config),1,0,0,0,0,profiles[slot].u(16),profiles[1-slot].u(16),slot,1};
        app->startMultiplayer(config,&profiles[slot],&profiles[1-slot]);app->setMultiplayerGo(true);
        check(app->vsActive&&!app->authorityRace&&!hasWarning(),"No-authority multiplayer inferred Boost Off");
        check(render()==0,"No-authority race submitted Boost Off artwork");app->leaveMultiplayer();
    }
    // Reuse the initialized race at the offline VS-owner boundary. This keeps
    // an actual versus banner, so suppression is not merely a hidden HUD.
    app->menu=false;app->battle=true;app->bunta=false;app->beginVsBanner();
    check(app->vsActive&&!hasWarning(),"Offline versus inherited the last online warning");
    check(render()==0,"Offline versus submitted Boost Off artwork");
    // The replay bridge owns an isolated scene, sets these same playback flags
    // before start(), and suppresses the showcase independently of saved races.
    app->replayPlaybackActive=true;app->validationMode=true;app->frontend.gameMode=original::OriginalGameMode::TimeAttack;
    app->frontend.battleProfile=profiles[0];app->frontend.car=0;app->frontend.course=app->courseIndex=0;app->start();
    check(!app->multiplayer.active&&!app->authorityRace&&!app->vsActive,"Replay retained an online showcase");
    app->beginVsBanner();check(!app->vsActive,"Replay began a live showcase");check(render()==0,"Replay submitted Boost Off artwork");
    for(unsigned side=0;side<2;++side)check(app->profiles.load(profiles[side].u(16)).profile.words==profiles[side].words,"Offline/replay suppression changed saved profile data");
    std::ofstream(out/"PASS.txt")<<checks<<" application checks; "<<matches<<" authority matches across both slots, Off/On/Off rematches, exact source texture submission, intro-only removal, no-authority/offline/replay suppression; authority digests and profiles unchanged\n";
    std::cout<<"PASS "<<checks<<" online Boost Off application checks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
