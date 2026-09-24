// Reached only after Idas3SceneModeFlowFixture has verified isolated storage.
namespace {
struct SaveChangeFixtureState {
    std::map<std::string,std::vector<char>> files;
    std::array<std::map<std::string,std::vector<char>>,2> deleteFiles;
    std::map<std::string,std::vector<char>> lastUsedFiles,lastUsedOtherFiles;
    LocalSaveSlots::Slot lastUsedSlot{};
    original::OriginalBattleProfile first{},fitted{},stock{};
    original::OriginalBattleProfile fullTuneLevin{},fullTuneCandidateBefore{},fullTuneCandidateAfter{};
    std::map<std::string,std::vector<char>> fullTuneOtherSlot;
    unsigned fullTuneCandidate=2;
    std::map<std::string,std::vector<char>> transmissionFiles,transmissionOtherSlot;
    std::array<LocalDriverProfiles::Loaded,35> transmissionProfiles{};
    LocalSaveSlots::Slot transmissionSlot{};
    bool transmissionSnapshot=false;
    bool seeded=false;
};
SaveChangeFixtureState saveChangeFixture;
void saveChangeCheck(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
std::map<std::string,std::vector<char>> saveDeleteFiles(const App& app,unsigned slot){
    std::map<std::string,std::vector<char>> result;
    const auto directory=app.saveSlots.profileDirectory(slot).parent_path();
    if(!fs::exists(directory))return result;
    for(const auto& entry:fs::recursive_directory_iterator(directory))if(entry.is_regular_file()){
        std::ifstream input(entry.path(),std::ios::binary);
        result.emplace(fs::relative(entry.path(),directory).generic_string(),std::vector<char>{std::istreambuf_iterator<char>(input),{}});
    }
    return result;
}
std::map<std::string,std::vector<char>> saveChangeFiles(const App& app){
    std::map<std::string,std::vector<char>> result;
    const auto directory=app.userdataRoot()/"saves";
    if(!fs::exists(directory))return result;
    for(const auto& entry:fs::recursive_directory_iterator(directory))if(entry.is_regular_file()){
        // Open-save time is legitimately banked while visiting menus. Compare
        // its identity/car/wins separately, without treating the clock as a write.
        const auto filename=entry.path().filename().string();
        if(filename=="save.txt"||filename=="save.txt.bak")continue;
        std::ifstream input(entry.path(),std::ios::binary);
        result.emplace(fs::relative(entry.path(),directory).generic_string(),std::vector<char>{std::istreambuf_iterator<char>(input),{}});
    }
    return result;
}
bool saveChangeNameIntact(const original::OriginalBattleProfile& profile){
    constexpr std::array<unsigned,5> name{164,169,179,170,180}; // CHRIS
    if(profile.u(76)!=name.size())return false;
    for(unsigned i=0;i<name.size();++i)if(profile.u(44+4*i)!=name[i])return false;
    return true;
}
void prepareSaveChangeMenuFixture(App& app,unsigned scene){
    const auto reopen=[&]{
        app.returnToCourseSelection(true);app.useSaveSlot(-1);
        app.validationMode=false;app.menu=true;app.paused=false;
        app.fullTuneActive=app.fullTuneSelecting=false;
        auto& f=app.frontend;f.stage=FrontendStage::SaveSelect;f.saveSelected=0;f.automatic=true;
        f.saveActionsEnabled=true;f.saveActionsOpen=false;f.saveActionSelected=0;
        f.saveDeleteOpen=f.saveDeleteFailed=false;f.saveDeleteSelected=0;f.saveDeleteRequested=-1;
        f.changingSavedCar=f.savedDriverSelected=false;f.saveFileChosen=f.saveCarChangeRequested=false;
        app.saveFilesShown=false;app.browsedSaveSlot=-2;app.browsingSaveFiles=false;
        app.menuTexturesLoaded=false;app.input={};
    };
    if(scene==350){
        app.validationMode=false;
        saveChangeFixture.transmissionSnapshot=false;
        if(!app.tuningTables)app.tuningTables=original::OriginalTuningData::load(app.root);
        const auto makeProfile=[&](unsigned car,bool fitted){
            auto p=original::makeOriginalFreshBattleProfile();p.setu(16,car);
            original::applyOriginalAcceptedCardFlag(p);original::finishOriginalDriverSetupFlag(p);
            p.setu(68,1);p.setu(72,123456);p.setu(76,5);
            constexpr std::array<unsigned,5> name{164,169,179,170,180};
            for(unsigned i=0;i<name.size();++i)p.setu(44+4*i,name[i]);
            if(fitted){
                p.setByte(152,car==22?1:0);
                unsigned steps=0;
                while(!(p.u(1180)&0x400)&&steps++<128)original::applyOriginalTuningCommand(p,*app.tuningTables,1);
                while(!(p.u(1180)&0x800)&&steps++<256)original::applyOriginalTuningCommand(p,*app.tuningTables,2);
                saveChangeCheck((p.u(1180)&0xc00)==0xc00,"Save-change fixture tuning failed");
            }
            return p;
        };
        app.useSaveSlot(0);
        saveChangeFixture.first=makeProfile(0,true);
        saveChangeFixture.fitted=makeProfile(22,true);
        saveChangeFixture.fitted.setu(64,1);
        saveChangeFixture.stock=makeProfile(1,false);
        // An alternate profile's stale identity must not replace the slot name.
        saveChangeFixture.fitted.setu(44,175);
        for(const auto* p:{&saveChangeFixture.first,&saveChangeFixture.fitted,&saveChangeFixture.stock}){
            saveChangeCheck(app.profiles.save(p->u(16),*p),"Could not write isolated save-change profile");
            saveChangeCheck(app.driverSetup.markComplete(p->u(16)),"Could not write isolated setup marker");
        }
        saveChangeCheck(app.saveSlots.adopt(0,saveChangeFixture.first),"Could not write isolated slot");
        auto slot=app.saveSlots.at(0);slot.playedSeconds=169928;slot.wins=31;
        saveChangeCheck(app.saveSlots.write(0,slot),"Could not write isolated slot details");
        app.useSaveSlot(1);auto other=makeProfile(8,true);other.setu(76,3);
        constexpr std::array<unsigned,5> ren{179,166,175,220,220};
        for(unsigned i=0;i<ren.size();++i)other.setu(44+4*i,ren[i]);
        saveChangeCheck(app.profiles.save(8,other)&&app.driverSetup.markComplete(8)&&app.saveSlots.adopt(1,other),"Could not write second isolated slot");
        reopen();saveChangeFixture.files=saveChangeFiles(app);saveChangeFixture.seeded=true;return;
    }
    saveChangeCheck(saveChangeFixture.seeded||scene==365,"Save-change fixture was not seeded");
    if(scene==351){reopen();return;}
    if(scene==353){
        saveChangeCheck(app.frontend.stage==FrontendStage::SaveSelect,"Cancellation failed to return to Save Select");
        saveChangeCheck(saveChangeFiles(app)==saveChangeFixture.files,"Cancelled car browsing changed save files");
        app.saveSlots.reload();const auto& slot=app.saveSlots.at(0);
        saveChangeCheck(slot.car==0&&slot.wins==31&&slot.nameLength==5&&
            slot.nameGlyphs==std::array<std::uint32_t,5>{164,169,179,170,180},"Cancellation changed save identity, car or wins");
        return;
    }
    if(scene==352||scene==354||scene==356){
        const unsigned car=scene==352?22:scene==354?1:2;
        saveChangeCheck(app.frontend.stage==FrontendStage::Mode&&unsigned(app.frontend.car)==car,"Selected car did not reach mode selection");
        saveChangeCheck(app.activeSaveSlot==0&&app.saveSlots.at(0).car==car,"Selected car did not persist in its save slot");
        const LocalDriverProfiles profiles(app.saveSlots.profileDirectory(0));const auto saved=profiles.load(car);
        saveChangeCheck(saved.origin==LocalDriverProfiles::Origin::Saved&&saveChangeNameIntact(saved.profile),"Selected car lost the saved driver identity");
        saveChangeCheck(LocalDriverSetup(app.saveSlots.profileDirectory(0)).load(car).status==LocalDriverSetup::Status::Complete,"Selected car setup was not committed");
        saveChangeCheck(profiles.load(0).profile.words==saveChangeFixture.first.words,"Changing cars altered the original car");
        if(scene==352){
            for(unsigned offset=152;offset<168;++offset)saveChangeCheck(saved.profile.byte(offset)==saveChangeFixture.fitted.byte(offset),"Saved fitted parts or tuning changed");
            for(unsigned offset:{64u,68u,72u})saveChangeCheck(saved.profile.u(offset)==saveChangeFixture.fitted.u(offset),"Saved car paint, transmission or points changed");
        }else{
            const auto expected=scene==354?saveChangeFixture.stock:original::makeOriginalFreshBattleProfile();
            saveChangeCheck((saved.profile.u(1180)&0xc00)==0,"Changing to a stock car silently applied tuning");
            for(unsigned offset=152;offset<168;++offset)saveChangeCheck(saved.profile.byte(offset)==expected.byte(offset),"Changing to a stock car added parts or performance upgrades");
            saveChangeCheck(saved.profile.u(68)==1,"Selected stock car lost the driver's manual transmission");
            if(scene==354)for(unsigned offset:{64u,72u})saveChangeCheck(saved.profile.u(offset)==expected.u(offset),"Existing stock car paint or points changed");
            else saveChangeCheck(saved.profile.u(72)==expected.u(72),"Fresh car acquired tuning points from another car");
        }
        return;
    }
    if(scene==355){
        saveChangeCheck(app.frontend.stage==FrontendStage::Make&&app.activeSaveSlot==2&&!app.frontend.changingSavedCar,"Empty slot did not start ordinary driver creation");
        return;
    }
    if(scene==357){
        saveChangeCheck(app.frontend.stage==FrontendStage::SaveSelect,"Delete fixture must begin on Save Select");
        app.saveSlots.reload();
        saveChangeCheck(app.saveSlots.at(0).used&&app.saveSlots.at(1).used,"Delete fixture requires two existing isolated saves");
        for(unsigned slot=0;slot<2;++slot)saveChangeFixture.deleteFiles[slot]=saveDeleteFiles(app,slot);
        saveChangeCheck(!saveChangeFixture.deleteFiles[0].empty()&&!saveChangeFixture.deleteFiles[1].empty(),"Delete fixture did not snapshot both save directories");
        return;
    }
    if(scene==358){
        for(unsigned slot=0;slot<2;++slot)
            saveChangeCheck(saveDeleteFiles(app,slot)==saveChangeFixture.deleteFiles[slot],"Cancelled deletion or blocked input modified an isolated save");
        app.saveSlots.reload();
        saveChangeCheck(app.saveSlots.at(0).used&&app.saveSlots.at(1).used,"Cancelled deletion lost a save slot");
        return;
    }
    if(scene==359){
        saveChangeCheck(!fs::exists(app.saveSlots.profileDirectory(0).parent_path()),"Deleted save directory or a per-car profile was retained or recreated");
        saveChangeCheck(saveDeleteFiles(app,1)==saveChangeFixture.deleteFiles[1],"Deleting one save modified the other save's files");
        app.saveSlots.reload();
        saveChangeCheck(!app.saveSlots.at(0).used&&app.saveSlots.at(1).used,"Deleted slot was restored or another slot was lost");
        saveChangeCheck(!app.frontend.saveFiles[0].used&&app.frontend.saveFiles[1].used,"Save Select summaries did not reflect just the deleted slot");
        return;
    }
    if(scene==360){
        saveChangeFixture.lastUsedFiles=saveChangeFiles(app);
        saveChangeFixture.lastUsedOtherFiles=saveDeleteFiles(app,1);
        app.saveSlots.reload();auto expected=app.saveSlots.at(0);
        saveChangeCheck(expected.used,"Last-used-car fixture requires a saved driver");
        saveChangeCheck(app.rememberSaveCar(0,22),"Post-race last-used-car helper failed to persist another owned car");
        expected.car=22;expected.lastPlayed=LocalSaveSlots::today();
        const LocalSaveSlots persisted(app.userdataRoot()/"saves");
        saveChangeCheck(persisted.at(0)==expected,"Post-race last-used-car helper changed driver identity, wins or time");
        saveChangeCheck(saveChangeFiles(app)==saveChangeFixture.lastUsedFiles,"Remembering the raced car modified a per-car profile or tuning");
        saveChangeCheck(saveDeleteFiles(app,1)==saveChangeFixture.lastUsedOtherFiles,"Remembering the raced car modified another save");
        saveChangeCheck(!app.rememberSaveCar(-1,22)&&!app.rememberSaveCar(int(LocalSaveSlots::count),22)&&!app.rememberSaveCar(0,35),"Last-used-car helper must reject invalid slot or car identifiers");
        saveChangeFixture.lastUsedSlot=expected;reopen();return;
    }
    if(scene==361){
        const LocalSaveSlots persisted(app.userdataRoot()/"saves");
        saveChangeCheck(persisted.at(0)==saveChangeFixture.lastUsedSlot&&persisted.at(0).car==22,"Fresh save-store reload lost the last raced car");
        saveChangeCheck(saveChangeFiles(app)==saveChangeFixture.lastUsedFiles,"Reopening last-used-car selection changed tuning files");
        saveChangeCheck(saveDeleteFiles(app,1)==saveChangeFixture.lastUsedOtherFiles,"Reopening last-used-car selection changed another save");
        return;
    }
    if(scene==362||scene==363){
        app.saveSlots.reload();const auto& slot=app.saveSlots.at(0);
        saveChangeCheck(app.frontend.stage==FrontendStage::SaveSelect&&slot.car==(scene==362?22u:1u),"Save selection did not remember the car just chosen");
        const LocalDriverProfiles profiles(app.saveSlots.profileDirectory(0));
        saveChangeCheck(profiles.load(0).profile.words==saveChangeFixture.first.words,"Changing the last-used car modified another car's tune");
        const auto selected=profiles.load(slot.car);
        saveChangeCheck(selected.origin==LocalDriverProfiles::Origin::Saved&&saveChangeNameIntact(selected.profile),"Remembered car lost its saved driver identity");
        const auto& expected=scene==362?saveChangeFixture.fitted:saveChangeFixture.stock;
        for(unsigned offset=152;offset<168;++offset)saveChangeCheck(selected.profile.byte(offset)==expected.byte(offset),"Remembered car changed fitted parts or tuning");
        for(unsigned offset:{64u,68u,72u})saveChangeCheck(selected.profile.u(offset)==expected.u(offset),"Remembered car changed paint, transmission or points");
        saveChangeFixture.lastUsedSlot=slot;saveChangeFixture.lastUsedFiles=saveChangeFiles(app);
        saveChangeFixture.lastUsedOtherFiles=saveDeleteFiles(app,1);return;
    }
    if(scene==364){
        app.saveSlots.reload();const auto& slot=app.saveSlots.at(0);
        saveChangeCheck(app.frontend.stage==FrontendStage::SaveSelect&&slot.car==saveChangeFixture.lastUsedSlot.car,"Cancelled browsing replaced the last-used car");
        saveChangeCheck(slot.nameGlyphs==saveChangeFixture.lastUsedSlot.nameGlyphs&&slot.nameLength==saveChangeFixture.lastUsedSlot.nameLength&&slot.wins==saveChangeFixture.lastUsedSlot.wins,"Cancelled browsing changed the remembered driver's identity or wins");
        saveChangeCheck(saveChangeFiles(app)==saveChangeFixture.lastUsedFiles,"Cancelled browsing changed any per-car profile");
        saveChangeCheck(saveDeleteFiles(app,1)==saveChangeFixture.lastUsedOtherFiles,"Cancelled browsing changed another save");
        return;
    }
    if(scene==365){
        // Reproduce the report through the actual Change Car commit. The
        // driver's first Levin already uses A; the next car has never had a
        // tuning-course choice, despite being ready to drive from this save.
        prepareSaveChangeMenuFixture(app,350);
        app.useSaveSlot(0);
        auto levin=saveChangeFixture.stock;levin.setByte(152,0);
        unsigned steps=0;
        while(!(levin.u(1180)&0x400)&&steps++<128)original::applyOriginalTuningCommand(levin,*app.tuningTables,1);
        while(!(levin.u(1180)&0x800)&&steps++<256)original::applyOriginalTuningCommand(levin,*app.tuningTables,2);
        saveChangeCheck((levin.u(1180)&0xc00)==0xc00,"Levin A regression seed did not finish mandatory tuning");
        saveChangeCheck(app.profiles.save(1,levin)&&app.driverSetup.markComplete(1)&&app.saveSlots.adopt(0,levin),"Could not save the first Levin's A tune");
        saveChangeFixture.fullTuneLevin=levin;
        saveChangeFixture.fullTuneCandidate=2;
        saveChangeCheck(app.profiles.load(2).origin==LocalDriverProfiles::Origin::Fresh,"Full Tune regression needs an unused second car");
        app.openSaveFile(0,true);
        app.frontend.car=2;app.frontend.make=App::originalCarMake(2);app.loadedProfileCar=-1;
        app.loadSelectedProfile();app.frontend.stage=FrontendStage::Transmission;
        saveChangeCheck(app.finishSavedCarSelection(),"Change Car failed to create the independent stock car");
        saveChangeCheck(app.frontend.stage==FrontendStage::Mode&&!app.fullTuneActive,"Choosing a fresh car unexpectedly entered upgrades");
        const auto stock=app.profiles.load(2);
        saveChangeCheck(stock.origin==LocalDriverProfiles::Origin::Saved&&saveChangeNameIntact(stock.profile),"Changed car lost the driver's name");
        saveChangeCheck((stock.profile.u(1180)&0xc00)==0&&stock.profile.u(68)==levin.u(68),"Changed car inherited upgrades or lost transmission");
        for(unsigned offset=152;offset<168;++offset)
            saveChangeCheck(stock.profile.byte(offset)==original::makeOriginalFreshBattleProfile().byte(offset),"Changed car inherited the first car's tuning");
        saveChangeFixture.fullTuneCandidateBefore=stock.profile;
        saveChangeFixture.fullTuneOtherSlot=saveDeleteFiles(app,1);
        app.beginFullTune();
        saveChangeCheck(app.fullTuneSelecting&&app.frontend.stage==FrontendStage::SaveSelect,"Independent route regression did not open Full Tune save selection");
        return;
    }
    if(scene==366){
        const auto car=saveChangeFixture.fullTuneCandidate;
        saveChangeCheck(app.fullTuneSelecting&&!app.fullTuneActive&&app.activeSaveSlot==0&&unsigned(app.frontend.car)==car,
            "Stock car began Full Tune before selecting its own tuning route");
        // Also call after previewing B and cancelling back to Car. A cancelled
        // visit must leave the stored A/default route and all progress intact.
        const bool cancelled=app.frontend.stage==FrontendStage::Car;
        saveChangeCheck((cancelled||app.frontend.stage==FrontendStage::TuningCourse)&&app.frontend.inputReady(),
            "Stock car skipped its independent tuning-course choice or failed to cancel");
        if(!cancelled)saveChangeCheck(app.frontend.tuningCourseState().count504>1,"Regression car does not offer an independent non-A route");
        saveChangeCheck(saveChangeNameIntact(app.frontend.battleProfile),"Route selection replaced the saved driver's name");
        for(unsigned offset:{64u,68u,72u})saveChangeCheck(app.frontend.battleProfile.u(offset)==saveChangeFixture.fullTuneCandidateBefore.u(offset),
            "Entering route selection changed paint, transmission or earned points");
        const LocalDriverProfiles profiles(app.saveSlots.profileDirectory(0));
        const auto saved=profiles.load(car).profile;
        for(unsigned offset=152;offset<168;++offset)saveChangeCheck(saved.byte(offset)==saveChangeFixture.fullTuneCandidateBefore.byte(offset),
            "Browsing or cancelling the new route saved tuning changes");
        for(unsigned offset:{64u,68u,72u})saveChangeCheck(saved.u(offset)==saveChangeFixture.fullTuneCandidateBefore.u(offset),
            "Browsing or cancelling the new route saved paint, transmission or points");
        saveChangeCheck(profiles.load(1).profile.words==saveChangeFixture.fullTuneLevin.words,"Choosing another car's route changed the Levin A profile");
        saveChangeCheck(profiles.load(0).profile.words==saveChangeFixture.first.words&&profiles.load(22).profile.words==saveChangeFixture.fitted.words,
            "Choosing another car's route changed an established car");
        saveChangeCheck(saveDeleteFiles(app,1)==saveChangeFixture.fullTuneOtherSlot,"Choosing a route changed another driver's save");
        return;
    }
    if(scene==367){
        const auto car=saveChangeFixture.fullTuneCandidate;
        saveChangeCheck(app.fullTuneActive&&app.activeSaveSlot==0&&unsigned(app.frontend.car)==car&&app.battleProfile.byte(152)==1,
            "The second car did not start upgrades using its independently selected B route");
        saveChangeCheck(saveChangeNameIntact(app.battleProfile),"Confirming the car's own route changed driver identity");
        unsigned visits=0;
        while(app.fullTuneActive&&visits++<256){
            unsigned frames=0;
            while(!app.resultAnimationFrame.finished&&frames++<10000){
                app.resultSelectionAxis=1;app.resultConfirmPending=frames==20;app.advanceResultVisit();
            }
            saveChangeCheck(frames<10000,"Independent route upgrade visit stalled");
            saveChangeCheck(app.render(0),"Independent route upgrade render failed");
        }
        saveChangeCheck(!app.fullTuneActive&&app.menu&&app.frontend.stage==FrontendStage::Mode,"Independent car tuning did not finish at Mode");
        const LocalDriverProfiles profiles(app.saveSlots.profileDirectory(0));const auto saved=profiles.load(car);
        saveChangeCheck(saved.origin==LocalDriverProfiles::Origin::Saved&&saved.profile.byte(152)==1&&(saved.profile.u(1180)&0xc00)==0xc00,
            "The selected B route or mandatory upgrades did not persist per car");
        saveChangeCheck(saveChangeNameIntact(saved.profile),"Finished tuning changed the driver's name");
        for(unsigned offset:{64u,68u})saveChangeCheck(saved.profile.u(offset)==saveChangeFixture.fullTuneCandidateBefore.u(offset),
            "Full Tune changed the selected car's paint or transmission");
        saveChangeCheck(profiles.load(1).profile.words==saveChangeFixture.fullTuneLevin.words,"Full Tune on a second car changed the Levin A route or progress");
        saveChangeCheck(profiles.load(0).profile.words==saveChangeFixture.first.words&&profiles.load(22).profile.words==saveChangeFixture.fitted.words,
            "Full Tune changed another established car's route or progress");
        saveChangeCheck(saveDeleteFiles(app,1)==saveChangeFixture.fullTuneOtherSlot,"Full Tune changed another driver's save");
        saveChangeFixture.fullTuneCandidateAfter=saved.profile;
        return;
    }
    if(scene==368){
        const auto car=saveChangeFixture.fullTuneCandidate;
        saveChangeCheck(!app.fullTuneSelecting&&app.activeSaveSlot==0&&unsigned(app.frontend.car)==car&&app.frontend.stage!=FrontendStage::TuningCourse,
            "An already tuned car requested another route choice");
        saveChangeCheck(app.fullTuneActive||(app.menu&&app.frontend.stage==FrontendStage::Mode),"Established car did not enter or finish Full Tune");
        for(unsigned offset:{152u,153u,156u,157u,158u,159u,160u,161u,162u,163u,164u,165u,166u})saveChangeCheck(app.battleProfile.byte(offset)==saveChangeFixture.fullTuneCandidateAfter.byte(offset),
            "Revisiting the tuned car replaced its route or existing upgrades");
        saveChangeCheck((app.battleProfile.u(1180)&0xc00)==0xc00&&saveChangeNameIntact(app.battleProfile),"Revisiting the tuned car lost progress or driver identity");
        if(app.fullTuneActive)app.finishFullTune();
        const LocalDriverProfiles profiles(app.saveSlots.profileDirectory(0));
        const auto saved=profiles.load(car).profile;
        for(unsigned offset:{152u,153u,156u,157u,158u,159u,160u,161u,162u,163u,164u,165u,166u})saveChangeCheck(saved.byte(offset)==saveChangeFixture.fullTuneCandidateAfter.byte(offset),
            "Revisiting Full Tune failed to retain the saved route and parts");
        saveChangeCheck(profiles.load(1).profile.words==saveChangeFixture.fullTuneLevin.words&&profiles.load(0).profile.words==saveChangeFixture.first.words&&
            profiles.load(22).profile.words==saveChangeFixture.fitted.words,"Revisiting Full Tune changed an unselected car");
        saveChangeCheck(saveDeleteFiles(app,1)==saveChangeFixture.fullTuneOtherSlot,"Revisiting Full Tune changed another driver's save");
        return;
    }
    if(scene==369){
        // Old releases wrote Complete for a stock Change Car profile. Import
        // that observable state without using the updated Change Car owner.
        app.useSaveSlot(0);saveChangeFixture.fullTuneCandidate=3;
        auto stock=original::makeOriginalFreshBattleProfile();stock.setu(16,3);stock.setu(68,1);stock.setu(72,54321);
        original::applyOriginalAcceptedCardFlag(stock);original::finishOriginalDriverSetupFlag(stock);app.applySaveDriverName(stock);
        // Earned race data and cosmetics do not answer the package question;
        // even one installed upgrade does. These cases protect migrated saves
        // without requiring every old release to have written new metadata.
        stock.setu(64,1);app.frontend.battleProfile=stock;
        saveChangeCheck(app.needsFullTuneCourseSelection(),"Legacy stock car's paint, transmission, name or earned points hid route selection");
        for(unsigned offset:{176u,1080u,1184u}){
            app.frontend.battleProfile=stock;app.frontend.battleProfile.setu(offset,12345);
            saveChangeCheck(app.needsFullTuneCourseSelection(),"Stock car's race history suppressed route selection");
        }
        for(unsigned offset:{153u,156u,157u,158u,159u,160u,161u,162u,163u,164u,165u,166u}){
            app.frontend.battleProfile=stock;app.frontend.battleProfile.setByte(152,1);app.frontend.battleProfile.setByte(offset,1);
            const auto before=app.frontend.battleProfile.words;
            saveChangeCheck(!app.needsFullTuneCourseSelection(),"A partially tuned car was offered a destructive replacement route");
            saveChangeCheck(app.frontend.battleProfile.words==before,"Checking a partial tune changed its route or progress");
        }
        for(unsigned flags:{0x400u,0x800u,0xc00u}){
            app.frontend.battleProfile=stock;app.frontend.battleProfile.setu(1180,stock.u(1180)|flags);
            saveChangeCheck(!app.needsFullTuneCourseSelection(),"Existing completed tuning was offered another route");
        }
        app.frontend.battleProfile=stock;app.frontend.battleProfile.setu(16,29);
        saveChangeCheck(!app.needsFullTuneCourseSelection(),"Single-route car was offered a nonexistent package choice");
        // The car selector uses its own remembered paint while browsing. Seed
        // its default for this route test; paint1 is covered by the predicate
        // checks above and the frontend route-only preservation test.
        stock.setu(64,0);
        saveChangeCheck(app.profiles.save(3,stock)&&app.driverSetup.markComplete(3)&&app.saveSlots.adopt(0,stock),"Could not seed legacy stock car with completed driver setup");
        saveChangeFixture.fullTuneCandidateBefore=stock;
        app.frontend.car=3;app.loadedProfileCar=-1;app.loadSelectedProfile();app.frontend.stage=FrontendStage::Mode;
        app.beginFullTune();return;
    }
    if(scene==370){
        saveChangeCheck(app.activeSaveSlot==0,"Transmission snapshot requires the isolated saved driver");
        const LocalSaveSlots slots(app.userdataRoot()/"saves");
        saveChangeFixture.transmissionSlot=slots.at(0);
        const LocalDriverProfiles profiles(app.saveSlots.profileDirectory(0));
        for(unsigned car=0;car<35;++car)saveChangeFixture.transmissionProfiles[car]=profiles.load(car);
        saveChangeFixture.transmissionFiles=saveChangeFiles(app);
        saveChangeFixture.transmissionOtherSlot=saveDeleteFiles(app,1);
        saveChangeFixture.transmissionSnapshot=true;return;
    }
    if(scene==371||scene==375){
        saveChangeCheck(saveChangeFixture.transmissionSnapshot,"Transmission preview was not snapshotted");
        saveChangeCheck(saveChangeFiles(app)==saveChangeFixture.transmissionFiles,"Previewing or cancelling transmission wrote a driver profile or setup marker");
        saveChangeCheck(saveDeleteFiles(app,1)==saveChangeFixture.transmissionOtherSlot,"Previewing or cancelling transmission modified another save");
        const LocalSaveSlots slots(app.userdataRoot()/"saves");const auto& before=saveChangeFixture.transmissionSlot;const auto& after=slots.at(0);
        saveChangeCheck(after.car==before.car&&after.nameGlyphs==before.nameGlyphs&&after.nameLength==before.nameLength&&after.wins==before.wins&&after.lastPlayed==before.lastPlayed,
            "Unconfirmed transmission changed the remembered car, identity or progression");
        if(scene==375){
            const LocalDriverProfiles profiles(app.saveSlots.profileDirectory(0));
            saveChangeCheck(profiles.load(2).origin==LocalDriverProfiles::Origin::Fresh,"Unconfirmed fresh-car transmission created a driver profile");
            saveChangeCheck(LocalDriverSetup(app.saveSlots.profileDirectory(0)).load(2).status==LocalDriverSetup::Status::Missing,
                "Unconfirmed fresh-car transmission marked the car configured");
        }
        return;
    }
    if(scene==372||scene==373){
        saveChangeCheck(saveChangeFixture.transmissionSnapshot,"Transmission commit was not snapshotted");
        saveChangeCheck(app.frontend.stage==FrontendStage::Mode&&app.activeSaveSlot==0,"Transmission confirmation did not reach mode selection");
        const unsigned car=unsigned(app.frontend.car),transmission=scene==372?0u:1u;
        const LocalDriverProfiles profiles(app.saveSlots.profileDirectory(0));const auto saved=profiles.load(car);
        saveChangeCheck(saved.origin==LocalDriverProfiles::Origin::Saved&&saved.profile.u(68)==transmission,
            "Confirmed Automatic or Manual selection was not persisted for the selected car");
        saveChangeCheck(app.frontend.automatic==(transmission==0)&&app.frontend.battleProfile.u(68)==transmission,
            "Confirmed transmission and the live selected-car profile disagree");
        auto expected=saveChangeFixture.transmissionProfiles.at(car).profile;
        original::applyOriginalAcceptedCardFlag(expected);original::finishOriginalDriverSetupFlag(expected);expected.setByte(1192,0);
        const auto& identity=saveChangeFixture.transmissionSlot;
        expected.setu(76,identity.nameLength);
        for(unsigned i=0;i<5;++i)expected.setu(44+4*i,i<identity.nameLength?identity.nameGlyphs[i]:220);
        expected.setu(68,transmission);
        if(car!=identity.car){
            // Confirming Change Car records its manufacturer-local roster
            // index. Continue must preserve the stored index without rewriting it.
            const auto roster=Frontend::carsForMake(App::originalCarMake(car));
            const auto found=std::find(roster.begin(),roster.end(),int(car));
            saveChangeCheck(found!=roster.end(),"Confirmed car is missing from its manufacturer roster");
            expected.setu(1168,unsigned(found-roster.begin()));
        }
        // The original transmission owner banks its menu countdown when it
        // exits. Identity, paint, points, records and tuning stay independent.
        expected.setu(1176,saved.profile.u(1176));
        for(std::size_t i=0;i<expected.words.size();++i)
            if(saved.profile.words[i]!=expected.words[i])throw std::runtime_error("Transmission confirmation changed unrelated selected-car progress, paint, points or tuning at offset "+
                std::to_string(i*4)+" (expected "+std::to_string(expected.words[i])+", got "+std::to_string(saved.profile.words[i])+")");
        const auto prefix=fs::relative(profiles.path(car).parent_path(),app.userdataRoot()/"saves").generic_string()+"/"+profiles.path(car).stem().string()+".";
        auto beforeFiles=saveChangeFixture.transmissionFiles,afterFiles=saveChangeFiles(app);
        const auto removeSelected=[&](auto& files){for(auto i=files.begin();i!=files.end();)if(i->first.starts_with(prefix))i=files.erase(i);else ++i;};
        removeSelected(beforeFiles);removeSelected(afterFiles);
        saveChangeCheck(beforeFiles==afterFiles,"Confirming transmission modified an unselected car's files");
        saveChangeCheck(saveDeleteFiles(app,1)==saveChangeFixture.transmissionOtherSlot,"Confirming transmission modified another saved driver");
        const LocalSaveSlots slots(app.userdataRoot()/"saves");const auto& slot=slots.at(0);
        saveChangeCheck(slot.car==car&&slot.nameGlyphs==identity.nameGlyphs&&slot.nameLength==identity.nameLength&&slot.wins==identity.wins,
            "Confirmed transmission lost the selected car or changed driver identity and wins");
        return;
    }
    if(scene==374){
        saveChangeCheck(app.frontend.stage==FrontendStage::SaveSelect,"Transmission baseline refresh requires Save Select");
        const LocalDriverProfiles profiles(app.saveSlots.profileDirectory(0));
        const auto saved=profiles.load(0);
        saveChangeCheck(saved.origin==LocalDriverProfiles::Origin::Saved&&saved.profile.u(68)==1,"Continue regression did not restore the original Manual selection");
        auto expected=saveChangeFixture.first;expected.setu(1176,saved.profile.u(1176));
        saveChangeCheck(saved.profile.words==expected.words,"Continue changed the original car beyond its transmission and menu timer");
        saveChangeFixture.first=saved.profile;saveChangeFixture.files=saveChangeFiles(app);return;
    }
    throw std::invalid_argument("Unknown save-change menu fixture");
}
}
