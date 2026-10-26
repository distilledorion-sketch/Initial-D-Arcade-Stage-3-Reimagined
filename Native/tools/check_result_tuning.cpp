#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
#include <map>

// Application regression for a resumed upgraded car with renewed basic tuning.
// Earlier upgrades and the completed-race award are explicit fixtures; the
// package-menu commit, result owner, car renderer and saves are production code.
// All writes stay in the required new evidence directory; no device/window opens.
int main(int argc,char** argv)try{
    if(argc<3||argc>4)throw std::runtime_error("Native root, NEW evidence directory, optional arcade language required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");
    fs::create_directories(out);setOriginalUiLanguage(argc==4?std::stoi(argv[3]):0);
    unsigned checks=0;auto check=[&](bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);};
    const auto tables=original::OriginalTuningData::load(root);
    auto fixture=original::makeOriginalFreshBattleProfile();fixture.setu(16,0);fixture.setu(0,1);
    fixture.setu(1180,3);fixture.setu(72,999999);fixture.setByte(152,1);
    for(unsigned i=0;!(fixture.u(1180)&0x400)&&i<128;++i)original::applyOriginalTuningCommand(fixture,tables,1);
    for(unsigned i=0;!(fixture.u(1180)&0x800)&&i<128;++i)original::applyOriginalTuningCommand(fixture,tables,2);
    check(fixture.byte(164)==63&&(fixture.u(1180)&0xc00)==0xc00,"Completed package-B prerequisite");
    fixture.setByte(1192,1);
    original::OriginalTuningCourseMenu packageMenu;
    original::initializeOriginalTuningCourseMenu(packageMenu,fixture,tables);
    bool committed=false;
    for(unsigned i=0;i<200&&!committed;++i){
        const auto event=original::tickOriginalTuningCourseMenu(packageMenu,fixture,{0,packageMenu.phase464==1,4});
        committed=event.profileCommitted;
    }
    check(committed&&fixture.byte(152)==0&&fixture.byte(153)==0&&fixture.byte(164)==63&&
        (fixture.u(1180)&0xc00)==0,"Package renewal did not preserve the high engine and restart basic tuning");
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);
    app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    check(app->renderer.initializeSceneCapture(640,480),"Scene capture renderer unavailable");
    Idas3UiEnable(1);app->demoPreparationStarted=true;
    check(app->profiles.save(0,fixture),"Isolated renewed profile did not save");
    app->frontend.car=0;app->frontend.gameMode=original::OriginalGameMode::TimeAttack;
    app->frontend.course=app->courseIndex=3;app->frontend.automatic=app->automatic=true;
    app->frontend.wet=app->wet=app->frontend.reverse=app->reverse=app->frontend.night=app->night=false;
    std::ofstream report(out/"transition.csv");
    report<<"visit,frame,result_phase,child_phase,child_frame,basic_step,engine,flags,points,focus,vertices\n";
    const auto render=[&](double dt){
        Idas3UiBeginFrame(640,480);check(app->render(dt),"Actual application render failed");
    };
    bool completed=false,sawCompletionFocus=false;unsigned visits=0,previousStep=0;
    for(;visits<12&&!completed;++visits){
        app->start();
        check(!app->menu&&!app->battle&&app->originalSession.ready(),"Isolated Time Attack did not initialize");
        const auto balance=app->battleProfile.u(72)+1000;
        app->race.phase=RacePhase::Finished;app->race.timeUp=true;app->race.elapsed6000=1000;
        app->finishBannerDone=true;app->timeAttackLectureDone=true;app->pendingResultSetup.reset();
        app->battleResults={};app->battleResults.profileMode=1;app->battleResults.resultStatus=2;
        app->battleResults.totalTicks6000=1000;app->battleResults.sectionCapacity=4;
        app->battleResults.points={1000,0,0,1000,balance};
        app->beginResultVisit({1000,balance,false});
        check(app->resultVisit.tuning.kind==original::OriginalTuningChildKind::basic,"Renewed package did not receive its basic child");
        unsigned frames=0,lastPhase=~0u;
        for(;frames<1800&&!app->timeAttackVisitActive;++frames){
            // Exercise the real result advance and paint on every owner tick.
            const bool completionPreview=app->resultVisit.child.phase==7&&app->resultVisit.child.frame==1;
            const auto priorProfile=app->battleProfile.words;
            render(1./60.);
            if(completionPreview)check(app->battleProfile.words==priorProfile,"Completion preview changed saved points, upgrades or records");
            const auto& capture=app->renderer.sceneCapture()->frame();
            check(capture.vertexCount>0&&capture.rangeCount>0,"Result/Continue car geometry disappeared");
            check(app->battleProfile.u(72)==balance&&app->battleProfile.byte(164)==63,"Result changed the award or downgraded the retained engine");
            if(app->resultVisit.child.phase==7&&app->resultVisit.child.frame>=2){
                check(app->tuningPreview->visibleState().focus==0,"Completed basic tuning did not use completion camera focus");
                sawCompletionFocus=true;
            }
            if(lastPhase!=app->resultVisit.child.phase||frames%60==0){
                report<<visits+1<<','<<frames<<','<<app->resultVisit.animation.phase<<','<<app->resultVisit.child.phase<<','
                    <<app->resultVisit.child.frame<<','<<unsigned(app->battleProfile.byte(153))<<','<<unsigned(app->battleProfile.byte(164))<<','
                    <<app->battleProfile.u(1180)<<','<<app->battleProfile.u(72)<<','<<app->tuningPreview->visibleState().focus<<','<<capture.vertexCount<<'\n';
                report.flush();lastPhase=app->resultVisit.child.phase;
            }
        }
        check(app->timeAttackVisitActive&&app->resultAnimationFrame.finished,"Result child did not complete through the Time Attack handoff");
        check(app->timeAttackVisit.stage()==original::OriginalTimeAttackVisit::Stage::Continue,"Time-up result did not open Continue");
        completed=(app->battleProfile.u(1180)&0x400)!=0;
        check(completed||app->battleProfile.byte(153)>previousStep,"Basic tuning made no progress");
        previousStep=app->battleProfile.byte(153);
        // Decline the actual Continue owner, then verify persisted progression.
        app->input.pressed[VK_RIGHT]=true;render(1./60.);app->input.pressed[VK_RIGHT]=false;
        app->input.pressed[VK_RETURN]=true;render(1./60.);app->input.pressed[VK_RETURN]=false;
        bool saving=false,retrieval=false,gameOver=false;
        for(unsigned i=0;i<600&&!app->menu;++i){
            render(1./60.);
            using Exit=original::OriginalSessionExit::Stage;
            saving|=app->sessionExit.stage==Exit::SavingCard;
            retrieval|=app->sessionExit.stage==Exit::RetrieveCard;
            gameOver|=app->sessionExit.stage==Exit::GameOver;
        }
        check(saving&&retrieval&&gameOver,"Declining Continue skipped save/card retrieval/Game Over");
        check(app->menu&&app->frontend.stage==FrontendStage::Title,"Game Over did not return to the title");
        const auto saved=app->profiles.load(0);
        check(saved.origin==LocalDriverProfiles::Origin::Saved&&saved.profile.u(72)==balance&&saved.profile.byte(164)==63,
            "Result award/upgrades did not persist to the isolated profile");
        check(saved.profile.byte(153)==previousStep&&(saved.profile.u(1180)&0xc00)==(app->battleProfile.u(1180)&0xc00),
            "Result tuning progress did not persist");
        const auto retained=saved.profile.words;render(0);render(0);
        check(app->profiles.load(0).profile.words==retained,"Frontend repaint changed result progression");
    }
    check(completed&&sawCompletionFocus,"High-engine renewal never reached the problematic basic-completion preview");
    check(app->battleProfile.byte(153)==57&&(app->battleProfile.u(1180)&0xc00)==0xc00,"Completion lost retained maximum performance progress");
    check(app->battleProfile.u(72)==999999+1000*visits,"Result points were awarded more than once");
    // Regress the user's screen-order report through the actual App, including
    // the finish -> points handoff. The earned-rank flag is an explicit fixture;
    // the source finish clock, HUD selection and painters remain production code.
    const auto noticeBank=NativeTextureBank::load(root/"data/original_assets/session_exit/levelup_dr/textures/textures.idastex");
    const auto noticeModel=NativeModel::load(root/"data/original_assets/session_exit/levelup_dr/levelup_dr.idasmesh");
    const auto& notice=noticeBank.at(noticeModel.chunks.at(0).batches.at(0).material[9]);
    std::vector<std::uint8_t> noticeRgba;
    for(auto pixel:notice.argb){noticeRgba.push_back(std::uint8_t(pixel>>16));noticeRgba.push_back(std::uint8_t(pixel>>8));
        noticeRgba.push_back(std::uint8_t(pixel));noticeRgba.push_back(std::uint8_t(pixel>>24));}
    std::map<unsigned,bool> noticeTextures;
    const auto noticeDraws=[&](){UnityUiFrame frame{sizeof(frame)};
        check(Idas3UiGetFrame(&frame)==1&&frame.unresolvedSurfaces==0,"Finish UI frame is unresolved");
        std::vector<UnityUiDraw> draws(frame.drawCount);
        check(Idas3UiCopyDraws(draws.data(),int(draws.size()))==int(draws.size()),"Cannot inspect finish UI draws");
        unsigned count=0;
        for(const auto& draw:draws){auto found=noticeTextures.find(draw.texture);
            if(found==noticeTextures.end()){UnityUiTextureInfo info{sizeof(info)};bool match=false;
                if(Idas3UiGetTextureInfo(draw.texture,&info)==1&&info.width==notice.width&&info.height==notice.height&&info.bytes==noticeRgba.size()){
                    std::vector<std::uint8_t> rgba(info.bytes);
                    check(Idas3UiCopyTextureRGBA(draw.texture,rgba.data(),int(rgba.size()))==int(rgba.size()),"Cannot inspect notice artwork");
                    match=rgba==noticeRgba;
                }
                found=noticeTextures.emplace(draw.texture,match).first;
            }
            if(found->second&&draw.opacity>0&&draw.count>=3)++count;
        }return count;
    };
    app->frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
    app->frontend.battleProfile.setu(0,0);original::selectOriginalRival(app->frontend.battleProfile,0);
    app->start();
    app->loadingActive=app->vsActive=app->preRaceDialogueActive=false;
    app->race.phase=RacePhase::Finished;app->race.timeUp=false;
    app->battleProgressApplied=true;app->battleResult=original::OriginalLegendResult::Win;
    app->battleResults={};app->battleResults.profileMode=0;
    app->pendingResultSetup=original::OriginalBattleResultAnimationSetup{0,app->battleProfile.u(72),false};
    app->finishAudioSkipped=true;
    render(0);check(noticeDraws()==0,"A non-promoted race displayed Racer Level Up");
    app->battleResults.levelNoticeFlags=0x200000;
    render(0);check(noticeDraws()>0&&!app->resultVisit.initialized,"Racer Level Up is missing from the finish road view");
    const auto clock=app->finishBannerTicks;render(0);
    check(noticeDraws()>0&&app->finishBannerTicks==clock,"Repaint advanced the finish notice");
    unsigned finishNoticeFrames=0;
    for(unsigned frame=0;frame<330&&!app->resultVisit.initialized;++frame){
        app->simulate({});render(0);
        if(!app->resultVisit.initialized){check(noticeDraws()>0,"Rank notice vanished before the points transition");++finishNoticeFrames;}
    }
    check(finishNoticeFrames>=300&&app->resultVisit.initialized,"Finish owner did not hand off to points");
    check(noticeDraws()==0,"Racer Level Up carried over onto the points screen");
    Idas3UiEnable(0);
    std::ofstream(out/"PASS.txt")<<checks<<" checks, "<<visits<<" application result visits; package renewal, real car rendering, Continue, isolated persistence, rank notice on finish before points\n";
    std::cout<<"PASS "<<checks<<" result tuning application checks across "<<visits<<" visits\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
