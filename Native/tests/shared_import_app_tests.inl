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
    std::ofstream(output/"personal-import.json")<<text;
    std::ofstream(output/"shared-import-native.log")<<"PASS "<<checks<<" import selection, deduplication, missing metadata and read-only checks\n";
    return 0;
}
