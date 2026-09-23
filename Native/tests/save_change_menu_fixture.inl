// Reached only after Idas3SceneModeFlowFixture has verified isolated storage.
namespace {
struct SaveChangeFixtureState {
    std::map<std::string,std::vector<char>> files;
    std::array<std::map<std::string,std::vector<char>>,2> deleteFiles;
    std::map<std::string,std::vector<char>> lastUsedFiles,lastUsedOtherFiles;
    LocalSaveSlots::Slot lastUsedSlot{};
    original::OriginalBattleProfile first{},fitted{},stock{};
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
    saveChangeCheck(saveChangeFixture.seeded,"Save-change fixture was not seeded");
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
    throw std::invalid_argument("Unknown save-change menu fixture");
}
}
