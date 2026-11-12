int runSharedCacheAppChecks(App& app){
    const auto output=app.saveRoot.parent_path();
    if(app.saveRoot.filename()!="userdata"||!fs::is_regular_file(output/"ISOLATED_MODE_FLOW_TEST.txt"))
        throw std::runtime_error("Display cache checks require isolated saves");
    unsigned checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok){std::ofstream(output/"shared-cache-error.txt")<<why;throw std::runtime_error(why);}};
    auto same=[](const TimeAttackEntry& a,const TimeAttackEntry& b){return
        a.condition==b.condition&&a.weather==b.weather&&a.car==b.car&&a.ticks6000==b.ticks6000&&
        a.nameGlyphs==b.nameGlyphs&&a.manual==b.manual&&a.night==b.night&&
        a.intermediate6000==b.intermediate6000&&a.metadataUnknown==b.metadataUnknown;};
    const auto exact=[&](const TimeAttackRecords& actual,const TimeAttackRecords& expected){
        check(actual.entries().size()==expected.entries().size(),"Display row count changed");
        for(std::size_t i=0;i<actual.entries().size();++i)check(same(actual.entries()[i],expected.entries()[i]),"Display order or metadata changed");
    };
    // Frozen pre-batch display construction: the reference stays sequential
    // and scans all remote rows when suppressing a published personal best.
    const auto personalReference=[&]{
        TimeAttackRecords result;
        for(unsigned car=0;car<app.personalRecordProfiles.size();++car)if(app.personalRecordProfiles[car]){
            const auto& p=*app.personalRecordProfiles[car];
            for(unsigned condition=0;condition<18;++condition)for(unsigned wet=0;wet<2;++wet){
                if(condition>=16&&!wet)continue;
                const auto saved=original::originalPersonalTimeAttackRecord(p,original::originalRecordPartition(condition,wet!=0));
                if(!saved.ticks6000||saved.ticks6000>=10800000||saved.night>1)continue;
                TimeAttackEntry e{condition,wet,car,saved.ticks6000};e.night=saved.night!=0;e.metadataUnknown=true;
                for(unsigned n=0;n<5;++n){auto code=p.u(44+n*4);e.nameGlyphs[n]=code<=221?std::uint8_t(code):221;}
                result.record(e);
            }
        }
        for(const auto& e:app.importedPersonalRecords.entries())if(e.condition>=18)result.record(e);
        return result;
    };
    const auto combinedReference=[&](const TimeAttackRecords& personal){
        auto result=app.sharedRecords;
        for(const auto& own:personal.entries()){
            bool published=std::any_of(app.sharedRecords.entries().begin(),app.sharedRecords.entries().end(),[&](const auto& remote){
                return own.condition==remote.condition&&own.weather==remote.weather&&own.car==remote.car&&
                    own.ticks6000==remote.ticks6000&&own.nameGlyphs==remote.nameGlyphs;});
            if(!published)result.record(own);
        }
        return result;
    };
    std::ofstream samples(output/"shared-cache.csv"),golden(output/"shared-cache-rows.csv");
    samples<<"cars,phase,iteration,rows,milliseconds\n";samples.precision(10);
    for(unsigned cars:{1u,5u,35u}){
        app.personalRecordProfiles={};app.importedPersonalRecords={};app.records={};
        std::vector<TimeAttackEntry> imported;
        const std::array<std::uint32_t,3> splits{200000,400000,600000};
        for(unsigned car=0;car<cars;++car){
            auto p=original::makeOriginalFreshBattleProfile();p.setu(16,car);p.setu(1180,129);
            for(unsigned n=0;n<5;++n)p.setu(44+n*4,162+car+n);
            for(unsigned condition=0;condition<18;++condition)for(unsigned wet=0;wet<2;++wet){
                if(condition>=16&&!wet)continue;
                check(original::registerOriginalPersonalTimeAttackRecord(p,original::originalRecordPartition(condition,wet!=0),
                    1000000+condition*10000+wet*1000+car*100,car%2,splits),"Create source card record");
            }
            app.personalRecordProfiles[car]=p;
            for(unsigned condition=18;condition<supportedConditionCount;++condition)for(unsigned wet=0;wet<2;++wet){
                TimeAttackEntry e{condition,wet,car,1000000+condition*10000+wet*1000+car*100};
                for(unsigned n=0;n<5;++n)e.nameGlyphs[n]=std::uint8_t(p.u(44+n*4));
                e.night=car%2;e.manual=car%3;e.intermediate6000={200000,400000,600000};imported.push_back(e);
            }
        }
        app.importedPersonalRecords.recordBatch(imported);
        // A cabinet-only record must not leak into this driver's tables.
        app.records.record({0,0,0,1});
        const auto personal=personalReference();check(personal.entries().size()==cars*70,"Full personal fixture coverage");
        std::vector<TimeAttackEntry> remote;
        for(unsigned condition=0;condition<supportedConditionCount;++condition)for(unsigned wet=0;wet<2;++wet){
            if(condition>=16&&condition<18&&!wet)continue;
            for(unsigned place=0;place<10;++place){TimeAttackEntry e{condition,wet,0,900000+condition*10000+wet*1000+place};
                e.nameGlyphs={1,2,3,std::uint8_t(place),221};remote.push_back(e);}
            for(unsigned car=1;car<35;++car){
                auto e=personal.personalBest(condition,wet,car);
                if(!e.ticks6000){e={condition,wet,car,1000000+condition*10000+wet*1000+car*100};e.nameGlyphs={3,4,5,6,221};}
                // A mix of published rows, same time/different driver and
                // unpublished faster/slower personal records.
                if(car%4==1)e.nameGlyphs[0]=1;
                if(car%4==2)e.ticks6000-=50;
                if(car%4==3)e.ticks6000+=50;
                e.metadataUnknown=false;e.manual=true;e.night=true;e.intermediate6000={200000,400000,600000};remote.push_back(e);
            }
        }
        TimeAttackRecords shared;shared.recordBatch(remote);app.setSharedRecords(shared,true);
        check(shared.entries().size()==3080,"Full remote fixture coverage");const auto combined=combinedReference(personal);
        for(unsigned pass=0;pass<9;++pass)for(unsigned phase=0;phase<4;++phase){
            // personal only; fresh remote with cached personal; both dirty;
            // then an unchanged cached read. Timing excludes fixture/oracle.
            app.sharedRecordsEnabled=phase!=0;
            if(phase==0||phase==2)app.personalRecordsDirty=true;
            if(phase==1||phase==2)app.combinedRecordsDirty=true;
            const auto start=std::chrono::steady_clock::now();const auto& actual=app.displayedTimeAttackRecords();
            const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            samples<<cars<<','<<phase<<','<<pass<<','<<actual.entries().size()<<','<<ms<<'\n';
            exact(actual,phase?combined:personal);
            const auto* address=actual.entries().data();check(app.displayedTimeAttackRecords().entries().data()==address,"Unchanged cache rebuilt storage");
        }
        for(const auto& e:combined.entries()){
            golden<<cars<<','<<e.condition<<','<<e.weather<<','<<e.car<<','<<e.ticks6000;
            for(auto g:e.nameGlyphs)golden<<','<<unsigned(g);
            golden<<','<<e.manual<<','<<e.night<<','<<e.metadataUnknown;
            for(auto split:e.intermediate6000)golden<<','<<split;golden<<'\n';
        }
        app.setSharedRecords({},true);exact(app.displayedTimeAttackRecords(),personal);
        app.setSharedRecords(shared,false);exact(app.displayedTimeAttackRecords(),personal);
        app.setSharedRecords(shared,true);exact(app.displayedTimeAttackRecords(),combined);
        const auto before=*app.personalRecordProfiles[0];
        for(unsigned n=0;n<20;++n)app.displayedTimeAttackRecords();
        check(app.personalRecordProfiles[0]->words==before.words,"Reading a ranking mutated its card");
        auto updated=before;updated.setu(44,200);app.personalRecordProfiles[0]=updated;app.personalRecordsDirty=true;
        const auto changed=combinedReference(personalReference());exact(app.displayedTimeAttackRecords(),changed);
        app.personalRecordProfiles={};app.importedPersonalRecords={};app.personalRecordsDirty=true;
        app.setSharedRecords({},false);check(app.displayedTimeAttackRecords().entries().empty(),"New empty save inherited prior rows");
    }
    samples.close();golden.close();check(bool(samples)&&bool(golden),"Write private cache evidence");
    std::ofstream(output/"shared-cache-native.txt")<<"PASS "<<checks<<" checks: 1/5/35-car cards, full original/imported personal tables and 3080 remote rows; exact sequential reference, ties/names/metadata, dirty/cached paths, removals, sharing toggle, changed and empty profiles. No public service or personal save accessed.\n";
    return 0;
}
