int runSharedImportAppTests(App& app){
    const auto output=app.saveRoot.parent_path();
    if(app.saveRoot.filename()!="userdata"||!fs::is_regular_file(output/"ISOLATED_MODE_FLOW_TEST.txt"))throw std::runtime_error("Import tests require isolated saves");
    unsigned checks=0;auto require=[&](bool ok,const char* msg){++checks;if(!ok)throw std::runtime_error(msg);};
    const auto root=app.userdataRoot();LocalSaveSlots slots(root/"saves");
    auto profile=original::makeOriginalFreshBattleProfile();profile.setu(1180,129);profile.setu(16,0);profile.setu(44,162);
    std::array<std::uint32_t,3> splits{300000,600000,900000};
    original::registerOriginalPersonalTimeAttackRecord(profile,original::originalRecordPartition(6,false),1200000,0,splits);
    LocalDriverProfiles first(slots.profileDirectory(0));require(first.save(0,profile),"Write first private save");
    original::registerOriginalPersonalTimeAttackRecord(profile,original::originalRecordPartition(6,false),1100000,1,splits);profile.setu(44,163);
    LocalDriverProfiles second(slots.profileDirectory(1));require(second.save(0,profile),"Write second private save");
    LocalDriverProfiles legacy(root/"driver_profiles_v1");require(legacy.save(0,profile),"Write duplicate legacy copy");
    auto fresh=original::makeOriginalFreshBattleProfile();fresh.setu(16,2);require(first.save(2,fresh),"Write empty personal profile");
    TimeAttackRecords imported;TimeAttackEntry old{18,0,1,1300000};old.nameGlyphs={164,221,221,221,221};imported.record(old);
    require(imported.save(slots.profileDirectory(0)/"hakone_personal_v1.csv"),"Write old imported personal record without splits");
    TimeAttackRecords aggregate;aggregate.record({4,0,0,700000});require(aggregate.save(root/"time_attack_records_v1.csv"),"Write aggregate-only row that cannot prove completion");
    const auto stamp=fs::last_write_time(second.path(0));
    const auto text=sharedPersonalImportJson(root);
    require(text.find("1200000")==std::string::npos&&text.find("1100000")!=std::string::npos,"Choose fastest personal best across slots");
    require(text.find("700000")==std::string::npos,"Exclude unowned aggregate rows and timeouts");
    require(text.find("\"manual\":-1")!=std::string::npos&&text.find("\"points\":-1")!=std::string::npos,"Unknown historical details are explicit");
    require(text.find("\"splits\":[0,0,0,0]")!=std::string::npos,"Missing checkpoints remain missing");
    require(text==sharedPersonalImportJson(root)&&stamp==fs::last_write_time(second.path(0)),"Repeated read does not mutate saves");
    // Exercise the real save-selection and displayed-ranking owners, including
    // the legacy no-slot profile. These paths are all below private userdata.
    for(int slot=-1;slot<int(LocalSaveSlots::count);++slot){
        const auto directory=slot<0?root/"driver_profiles_v1":slots.profileDirectory(unsigned(slot));
        fs::create_directories(directory);const auto file=directory/"hakone_personal_v1.csv";
        const std::string oldCsv="condition,weather,car,finish_ticks6000\n18,0,1,1300000\n32,0,0,876541\n33,1,34,876542\n34,0,0,876543\n35,1,34,876544\n";
        {std::ofstream out(file,std::ios::binary);out<<oldCsv;}
        require(sharedPersonalImportJson(root).find("87654")==std::string::npos,"Historical import resurrected old Gunsai/Odawara handling");
        app.useSaveSlot(slot);
        for(unsigned condition=32;condition<36;++condition)
            require(app.displayedTimeAttackRecords().best(condition,condition&1,(condition&1)?34:0).model==0,"Old Gunsai/Odawara times still visible after loading save");
        require(app.displayedTimeAttackRecords().best(18,0,1).model==1300000,"Migration cleared Hakone");
        auto archive=file;archive+=".before-idzero-revision-1.bak";std::ifstream backup(archive,std::ios::binary);
        require(std::string(std::istreambuf_iterator<char>(backup),{})==oldCsv,"Save selection did not preserve exact legacy backup");
        for(unsigned condition=32;condition<36;++condition)
            app.importedPersonalRecords.record({condition,condition&1,(condition&1)?34u:0u,1500000+condition*1000});
        require(app.importedPersonalRecords.save(file),"Post-reset personal best save");
        app.useSaveSlot(slot<0?0:-1);app.useSaveSlot(slot);
        for(unsigned condition=32;condition<36;++condition)
            require(app.displayedTimeAttackRecords().best(condition,condition&1,(condition&1)?34:0).model==1500000+condition*1000&&!app.importedPersonalRecords.needsMigration(),"Returning to a save erased its new Gunsai/Odawara best");
    }
    app.useSaveSlot(0);
    require(app.displayedTimeAttackRecords().best(6,0,0).model==1200000,"Original driver-card records changed during migration");
    std::ofstream(output/"personal-import.json")<<text;
    std::ofstream(output/"shared-import-native.log")<<"PASS "<<checks<<" import selection, deduplication, read-only import and all-save Gunsai/Odawara migration checks\n";
    return 0;
}
