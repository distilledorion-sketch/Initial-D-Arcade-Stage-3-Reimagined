#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include "original_tuning_candidate.h"
#include <bit>
#include <iostream>

namespace {
// Hash explicit fields rather than object memory/padding. These fingerprints
// detect accidental mutation of the saved car's private material/assembly data
// while the package selector displays its separately built candidate.
struct AppearanceFingerprint {
    std::uint64_t value=1469598103934665603ull;
    void add(std::uint32_t word){for(unsigned i=0;i<4;++i){value^=(word>>(8*i))&255u;value*=1099511628211ull;}}
    void add(float number){add(std::bit_cast<std::uint32_t>(number));}
};
std::uint64_t modelFingerprint(const NativeModel& model){
    AppearanceFingerprint h;h.add(std::uint32_t(model.chunks.size()));
    for(const auto& chunk:model.chunks){h.add(chunk.index);h.add(chunk.sourceOffset);h.add(chunk.sourceSize);for(auto word:chunk.header)h.add(word);
        h.add(std::uint32_t(chunk.batches.size()));for(const auto& batch:chunk.batches){h.add(batch.sourceOffset);for(auto word:batch.ich)h.add(word);for(auto word:batch.material)h.add(word);
            h.add(std::uint32_t(batch.vertices.size()));for(const auto& v:batch.vertices){h.add(v.header);h.add(v.position.x);h.add(v.position.y);h.add(v.position.z);h.add(v.normal.x);h.add(v.normal.y);h.add(v.normal.z);h.add(v.u);h.add(v.v);h.add(v.color0);h.add(v.color1);}
            h.add(std::uint32_t(batch.indices.size()));for(auto index:batch.indices)h.add(index);
        }
    }return h.value;
}
std::uint64_t assemblyFingerprint(const NativeAssembly& assembly){
    AppearanceFingerprint h;h.add(std::uint32_t(assembly.instances.size()));
    for(const auto& instance:assembly.instances){h.add(instance.chunk);for(auto number:instance.transform)h.add(number);h.add(std::uint32_t(instance.billboard));}return h.value;
}
std::array<std::uint32_t,307> profileExceptCountdown(original::OriginalBattleProfile profile){
    profile.setu(1176,0);return profile.words;
}
}

int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("isolated-root output-directory required");
    const fs::path isolated=fs::canonical(argv[1]),output=fs::absolute(argv[2]);
    const auto workspaceWork=fs::canonical(fs::current_path()/"work");
    if(isolated.parent_path()!=workspaceWork||!fs::exists(isolated/"DRIVER_ENTRY_TEST_ROOT.txt"))
        throw std::runtime_error("Refusing writes outside explicitly marked isolated work root");
    if(fs::exists(isolated/"userdata")&&(GetFileAttributesW((isolated/"userdata").c_str())&FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Isolated userdata must not be a junction");
    fs::create_directories(output);
    auto app=std::make_unique<App>();app->root=isolated;app->settings();
    app->originalCamera=OriginalChaseCamera::load(isolated);app->bumperCamera=OriginalChaseCamera::load(isolated,OriginalDrivingView::Bumper);
    app->frontend.initialize(isolated,true);app->hud.loadOriginal(isolated);app->audio.configure(isolated);app->audio.enabled=true;app->audio.scene(true,false,false);app->load();
    if(!app->renderer.initialize(nullptr,960,720,true))throw std::runtime_error(app->renderer.error);
    unsigned checks=0,ticks=0;std::uint64_t pcmFrames=0,clipped=0;
    auto check=[&](bool x,const char* message){++checks;if(!x)throw std::runtime_error(message);};
    std::ofstream csv(output/"application.csv");csv<<"label,car,stage,source_frame,name_length,points,flags,setup_status,pending_profile,pending_setup,cue,songs_started,samples,candidate_active,candidate_package,candidate_revision,candidate_word,base_model,base_assembly,candidate_model,candidate_assembly,entry_active,entry_revision,entry_word,entry_model,entry_assembly,entry_material_variant,color\n";
    auto tick=[&](int key=0,bool pressed=false,double dt=1./60.){
        app->input.down={};app->input.pressed={};if(key){app->input.down[key]=true;app->input.pressed[key]=pressed;}
        app->commands(dt);if(!app->render(dt))throw std::runtime_error(app->renderer.error);
        app->audio.scene(true,false,false);for(unsigned i=0;i<(dt?735u:0u);++i){const auto pcm=app->audio.renderStereo(800,0,0,0,false);for(auto s:pcm)if(s==32767||s==-32767)++clipped;++pcmFrames;}
        ++ticks;
    };
    auto capture=[&](const char* name){
        const auto car=unsigned(app->frontend.car);const auto& p=app->frontend.battleProfile;
        csv<<name<<','<<car<<','<<int(app->frontend.stage)<<','<<app->frontend.nameEntryState().frame572<<','<<p.u(76)<<','<<p.u(72)<<','<<p.u(1180)<<','<<int(app->driverSetup.load(car).status)<<','<<bool(app->pendingProfiles[car])<<','<<app->pendingSetupCompletion[car]<<','<<app->audio.selectionStatistics().cue<<','<<app->audio.selectionStatistics().songsStarted<<','<<app->audio.selectionSamplePosition()<<','<<app->tuningCoursePreviewActive<<','<<app->tuningCoursePreviewPackage<<','<<app->tuningCoursePreviewRevision<<','<<(app->tuningCoursePreviewAppearance?app->tuningCoursePreviewAppearance->word:0)<<','<<modelFingerprint(app->originalModel)<<','<<assemblyFingerprint(app->carPresentation.assembly())<<','<<modelFingerprint(app->tuningCoursePreviewModel)<<','<<assemblyFingerprint(app->tuningCoursePreviewCar.assembly())<<','<<app->driverEntryPreviewActive<<','<<app->driverEntryPreviewRevision<<','<<(app->driverEntryPreviewAppearance?app->driverEntryPreviewAppearance->word:0)<<','<<modelFingerprint(app->driverEntryPreviewModel)<<','<<assemblyFingerprint(app->driverEntryPreviewCar.assembly())<<','<<(app->driverEntryPreviewAppearance?app->driverEntryPreviewAppearance->materialVariant:~0u)<<','<<app->frontend.selectedColor()<<'\n';csv.flush();
        if(!app->renderer.saveBitmap((output/(std::string(name)+".bmp")).wstring()))throw std::runtime_error(app->renderer.error);
    };
    auto ready=[&]{for(unsigned n=0;!app->frontend.inputReady()&&n<600;++n)tick();check(app->frontend.inputReady(),"Source menu never became interactive");};
    auto confirmTo=[&](FrontendStage expected){ready();const auto previous=app->frontend.stage;tick(VK_RETURN,true);for(unsigned n=0;app->frontend.stage==previous&&n<600;++n)tick();check(app->frontend.stage==expected,"Unexpected actual application driver-entry route");};
    auto metadataMissing=[&](unsigned car){return app->driverSetup.load(car).status==LocalDriverSetup::Status::Missing;};
    auto fileBytes=[](const fs::path& path){std::ifstream file(path,std::ios::binary);return std::vector<char>(std::istreambuf_iterator<char>(file),{});};
    const auto sourceEnvironment=NativeTextureBank::load(isolated/"data/original_assets/tuning/environment/textures.idastex");
    auto verifyDriverEntry=[&]{
        const unsigned car=unsigned(app->frontend.car);
        check((app->frontend.stage==FrontendStage::Car||app->frontend.stage==FrontendStage::Transmission||app->frontend.stage==FrontendStage::Name)&&app->driverEntryPreviewActive,"Ordinary driver-entry owner did not activate its private car");
        check(!app->tuningCoursePreviewActive&&app->driverEntryPreviewAppearance.has_value(),"Ordinary driver entry retained the candidate or omitted its source appearance");
        auto profile=app->frontend.battleProfile;profile.setu(16,car);profile.setu(64,app->frontend.selectedColor());
        const auto expectedAppearance=original::originalPlayerAppearanceConfig(profile,app->frontend.stage==FrontendStage::Car?1u:0u);
        const auto& actual=*app->driverEntryPreviewAppearance;
        check(actual.car==expectedAppearance.car&&actual.word==expectedAppearance.word&&actual.materialVariant==expectedAppearance.materialVariant&&actual.variants==expectedAppearance.variants,"Car/Transmission/Name displayed candidate parts or the wrong source material variant");
        check(app->driverEntryPreviewCondition==profile.u(32),"Driver-entry preview lost the source material condition");
        check(app->driverEntryPreviewName[0]==profile.u(76),"Driver-entry preview cached the wrong name length");
        for(unsigned n=0;n<5;++n)check(app->driverEntryPreviewName[n+1]==profile.u(44+4*n),"Driver-entry preview altered an active or inactive stored name word");
        check(app->tuningCoursePreviewEnvironmentLoaded&&app->tuningCoursePreviewEnvironment.size()==1&&sourceEnvironment.size()==1,"Driver-entry parent did not load the source selector1/3 environment image");
        const auto& environment=app->tuningCoursePreviewEnvironment.at(0);
        check(environment.width==128&&environment.height==128&&environment.argb==sourceEnvironment.at(0).argb,"Driver-entry environment differs from the original shared selector1/3 image");
        const unsigned environmentIndex=unsigned(app->originalTextures.size()+app->numberPlate.textures.size());
        const auto folder=std::string(originalCarFolders.at(car));
        const auto modelFile=isolated/"data/original_models"/folder/(folder+".idasmesh");
        auto expectedModel=NativeModel::load(modelFile);
        auto expectedCar=CarPresentation::loadConfiguredAppearance(isolated,profile,expectedAppearance,2,environmentIndex);
        expectedCar.applyMaterials(expectedModel);expectedCar.pose({},false,false);
        check(modelFingerprint(app->driverEntryPreviewModel)==modelFingerprint(expectedModel),"Driver-entry model differs from source profile appearance with parent layer2");
        check(assemblyFingerprint(app->driverEntryPreviewCar.assembly())==assemblyFingerprint(expectedCar.assembly()),"Driver-entry submitted assembly differs from source profile appearance with parent layer2");
        check(assemblyFingerprint(app->driverEntryPreviewCar.profilePlateAssembly())==assemblyFingerprint(expectedCar.profilePlateAssembly()),"Driver-entry preview changed source name/number-plate geometry");
        // Independently rebuild the ordinary race car without the parent's
        // display-only layer. Both the saved profile and this separate model
        // must remain eligible for the race path after leaving the menu.
        auto plainModel=NativeModel::load(modelFile);
        auto plainCar=CarPresentation::loadPlayerProfile(isolated,profile);plainCar.applyMaterials(plainModel);
        check(modelFingerprint(app->originalModel)==modelFingerprint(plainModel),"Driver-entry display layer leaked into the ordinary race material model");
        check(app->driverEntryPreviewCar.assembly().instances.size()>plainCar.assembly().instances.size(),"Driver-entry layer2 did not add any original secondary geometry");
        const auto materials=original::OriginalCarMaterialRebuild::load(isolated/"data/original_models"/folder/"material_layout.bin",car);
        const auto& slots=materials.semanticChunks();unsigned secondaryInstances=0;
        for(const auto& instance:app->driverEntryPreviewCar.assembly().instances){
            bool secondary=false;for(unsigned slot=140;slot<=186;++slot)secondary|=slots[slot]>=0&&unsigned(slots[slot])==instance.chunk;
            if(secondary)++secondaryInstances;
        }
        check(secondaryInstances>0,"Driver-entry displayed car omitted secondary semantics140..186");
        Mesh submitted;submitted.originalCar(app->driverEntryPreviewModel,app->driverEntryPreviewCar.assembly(),{},0);
        unsigned environmentRanges=0;for(const auto& range:submitted.ranges){
            environmentRanges+=range.texture==environmentIndex;
            check(range.texture==0xffffffffu||range.texture<environmentIndex+1,"Driver-entry car references an image outside uploaded car/plate/environment banks");
        }
        check(environmentRanges>0,"Driver-entry secondary geometry never samples the source environment");
        Mesh ordinary;plainCar.pose({},false,false);ordinary.originalCar(plainModel,plainCar.assembly(),{},0);
        for(const auto& range:ordinary.ranges)check(range.texture!=environmentIndex,"Menu-only environment leaked into the ordinary race car");
    };
    auto verifyEntryRepaint=[&](const char* label){
        verifyDriverEntry();
        const auto car=unsigned(app->frontend.car);
        const auto stage=app->frontend.stage;
        const auto profileWords=app->frontend.battleProfile.words,savedWords=app->profiles.load(car).profile.words;
        const auto profileTime=fs::last_write_time(app->profiles.path(car));
        const auto revision=app->driverEntryPreviewRevision,model=modelFingerprint(app->driverEntryPreviewModel),assembly=assemblyFingerprint(app->driverEntryPreviewCar.assembly());
        const auto ordinaryModel=modelFingerprint(app->originalModel),ordinaryAssembly=assemblyFingerprint(app->carPresentation.assembly());
        const auto sourceFrame=app->frontend.nameEntryState().frame572;
        const auto samples=app->audio.selectionSamplePosition();
        const auto prefix=std::string(label);
        const auto before=output/(prefix+"-before-repaint.bmp"),paused=output/(prefix+"-after-repaint.bmp"),restored=output/(prefix+"-textures-restored.bmp");
        check(app->renderer.saveBitmap(before.wstring()),"Could not capture driver-entry repaint baseline");
        // Menus have no separate pause command. A zero-time repaint is the
        // actual App contract for displaying a frozen owner without ticks.
        for(unsigned n=0;n<4;++n)tick(0,false,0);
        check(app->renderer.saveBitmap(paused.wstring()),"Could not capture frozen driver-entry repaint");
        check(fileBytes(before)==fileBytes(paused),"Frozen driver-entry repaint changed actual WARP pixels");
        check(app->driverEntryPreviewRevision==revision&&modelFingerprint(app->driverEntryPreviewModel)==model&&assemblyFingerprint(app->driverEntryPreviewCar.assembly())==assembly,"Frozen driver-entry repaint rebuilt or mutated private geometry");
        check(app->frontend.stage==stage&&app->frontend.nameEntryState().frame572==sourceFrame&&app->audio.selectionSamplePosition()==samples,"Frozen driver-entry repaint advanced its owner or audio");
        // Deliberate cache-loss fixture, separate from the natural owner
        // transitions below: remove just the environment GPU texture and
        // deactivate the preview. The same owner must restore identical pixels.
        check(app->renderer.loadTextures(app->originalTextures)&&app->renderer.loadTextures(app->numberPlate.textures,true),"Could not evict the driver-entry environment texture");
        app->menuTexturesLoaded=true;app->driverEntryPreviewActive=false;app->loadDriverEntryPreview();
        check(!app->menuTexturesLoaded&&app->driverEntryPreviewRevision==revision+1,"Driver-entry cache recovery did not invalidate uploads and rebuild exactly once");
        check(app->renderMenu(0)&&app->menuTexturesLoaded,"Driver-entry cache recovery did not upload its complete texture set");
        check(app->renderer.saveBitmap(restored.wstring()),"Could not capture recovered driver-entry textures");
        check(fileBytes(before)==fileBytes(restored),"Driver-entry pixels changed after texture-cache recovery");
        check(app->frontend.battleProfile.words==profileWords&&app->profiles.load(car).profile.words==savedWords&&fs::last_write_time(app->profiles.path(car))==profileTime,"Driver-entry repaint/cache recovery changed or rewrote the driver profile");
        check(modelFingerprint(app->originalModel)==ordinaryModel&&assemblyFingerprint(app->carPresentation.assembly())==ordinaryAssembly,"Driver-entry repaint/cache recovery mutated the separate race car");
        verifyDriverEntry();
    };
    // Begin with the source Car owner. Only the starting screen is assigned;
    // subsequent navigation/confirmation uses actual App inputs. The source
    // parent uses material variant1 here, then variant0 in Transmission/Name.
    app->frontend.stage=FrontendStage::Car;tick(0,false,0);ready();capture("fresh-car");
    verifyEntryRepaint("fresh-car");
    const auto carProfileBefore=app->profiles.load(0).profile;
    const auto initialColor=app->frontend.selectedColor();
    const auto initialCarModel=modelFingerprint(app->driverEntryPreviewModel),initialCarAssembly=assemblyFingerprint(app->driverEntryPreviewCar.assembly());
    const auto firstCarRevision=app->driverEntryPreviewRevision;
    check(original::originalCarColorCounts.at(0)>1,"AE86 fixture has no alternate authored paint");
    tick('E',true);tick();
    check(app->frontend.selectedColor()!=initialColor&&app->driverEntryPreviewRevision==firstCarRevision+1,"Actual E command did not switch the source paint and rebuild once");
    verifyEntryRepaint("fresh-car-alternate-color");capture("fresh-car-alternate-color");
    check(modelFingerprint(app->driverEntryPreviewModel)!=initialCarModel,"Alternate source paint did not change any displayed material");
    tick('Q',true);tick();
    check(app->frontend.selectedColor()==initialColor,"Actual Q command did not restore source paint");
    check(modelFingerprint(app->driverEntryPreviewModel)==initialCarModel&&assemblyFingerprint(app->driverEntryPreviewCar.assembly())==initialCarAssembly,"Restoring paint did not recover the exact original display model/assembly");
    const auto roster=Frontend::carsForMake(app->frontend.make);
    const auto initialCar=std::find(roster.begin(),roster.end(),app->frontend.car);
    check(initialCar!=roster.end()&&roster.size()>1,"Car fixture lacks another authored car in this manufacturer");
    const int nextCar=roster[(std::size_t(initialCar-roster.begin())+1)%roster.size()];
    const auto beforeCarSwitch=app->driverEntryPreviewRevision;
    tick('D',true);tick();
    check(app->frontend.car==nextCar&&app->driverEntryPreviewRevision==beforeCarSwitch+1,"Actual D command did not switch car resources exactly once");
    verifyEntryRepaint("fresh-car-next");capture("fresh-car-next");
    tick('A',true);tick();
    check(app->frontend.car==0&&app->frontend.selectedColor()==initialColor,"Actual A command did not restore AE86 and its remembered paint");
    verifyEntryRepaint("fresh-car-returned");capture("fresh-car-returned");
    check(modelFingerprint(app->driverEntryPreviewModel)==initialCarModel&&assemblyFingerprint(app->driverEntryPreviewCar.assembly())==initialCarAssembly,"Returning to AE86 did not recover its original private model/assembly");
    const auto carProfileAfter=app->profiles.load(0).profile;
    check(carProfileAfter.u(72)==carProfileBefore.u(72)&&carProfileAfter.u(76)==carProfileBefore.u(76),"Car/paint browsing changed saved points or name length");
    for(unsigned n=0;n<5;++n)check(carProfileAfter.u(44+4*n)==carProfileBefore.u(44+4*n),"Car/paint browsing altered saved name words");
    for(unsigned offset=152;offset<=166;++offset)check(carProfileAfter.byte(offset)==carProfileBefore.byte(offset),"Car/paint browsing changed saved package or earned parts");
    check(app->driverEntryPreviewAppearance->materialVariant==1,"Car preview lost source material variant1");
    const auto carExitRevision=app->driverEntryPreviewRevision;
    confirmTo(FrontendStage::Transmission);ready();capture("fresh-transmission");
    check(app->driverEntryPreviewActive&&app->driverEntryPreviewAppearance->materialVariant==0&&app->driverEntryPreviewRevision==carExitRevision+1,"Car to Transmission did not replace variant1 with variant0 exactly once");
    verifyEntryRepaint("fresh-transmission");
    check(metadataMissing(0)&&original::originalDriverSetupRequested(app->frontend.battleProfile),"Fresh driver skipped explicit setup request");
    confirmTo(FrontendStage::TuningCourse);ready();capture("fresh-tuning");
    check(!app->driverEntryPreviewActive,"Transmission's private car stayed active inside the package selector");
    const auto tuningProfile=app->frontend.battleProfile;
    const auto tuningSaved=app->profiles.load(0).profile;
    const auto tuningSaveTime=fs::last_write_time(app->profiles.path(0));
    const auto baseModel=modelFingerprint(app->originalModel),baseAssembly=assemblyFingerprint(app->carPresentation.assembly());
    const auto tuningData=original::OriginalTuningData::load(isolated);
    auto verifyCandidate=[&]{
        const unsigned car=unsigned(app->frontend.car),selected=app->frontend.tuningCourseState().selected496;
        check(app->frontend.stage==FrontendStage::TuningCourse&&app->tuningCoursePreviewActive,"Tuning selector did not activate its private preview");
        check(app->tuningCoursePreviewCarId==int(car)&&app->tuningCoursePreviewPackage==int(selected)&&app->tuningCoursePreviewAppearance.has_value(),"Private preview cached a different car or source package");
        auto profile=app->frontend.battleProfile;profile.setu(16,car);profile.setu(64,app->frontend.selectedColor());
        auto expectedAppearance=original::originalPlayerAppearanceConfig(profile);
        const auto candidate=original::originalTuningCandidateAppearance(tuningData,car,selected);
        original::applyOriginalTuningCandidateAppearance(expectedAppearance,candidate);
        const auto& actual=*app->tuningCoursePreviewAppearance;
        check(actual.car==expectedAppearance.car&&actual.word==expectedAppearance.word&&actual.materialVariant==expectedAppearance.materialVariant&&actual.variants==expectedAppearance.variants,"Displayed package appearance differs from source12B440/12B520 recipe");
        check(app->tuningCoursePreviewCarState==candidate.carState224&&app->tuningCoursePreviewCondition==profile.u(32),"Candidate lost source car-state or material-condition boundary");
        check(app->tuningCoursePreviewEnvironmentLoaded&&app->tuningCoursePreviewEnvironment.size()==1&&sourceEnvironment.size()==1,"Candidate failed to bind source selector3's single environment image");
        const auto& environment=app->tuningCoursePreviewEnvironment.at(0);const auto& authoredEnvironment=sourceEnvironment.at(0);
        check(environment.width==128&&environment.height==128&&environment.argb==authoredEnvironment.argb,"Candidate environment differs from original j_env_select128_b image");
        const unsigned environmentIndex=unsigned(app->originalTextures.size()+app->numberPlate.textures.size());
        const auto folder=std::string(originalCarFolders.at(car));
        auto expectedModel=NativeModel::load(isolated/"data/original_models"/folder/(folder+".idasmesh"));
        auto expectedCar=CarPresentation::loadConfiguredAppearance(isolated,profile,expectedAppearance,candidate.carState224,environmentIndex);expectedCar.applyMaterials(expectedModel);expectedCar.pose({},false,false);
        check(modelFingerprint(app->tuningCoursePreviewModel)==modelFingerprint(expectedModel),"Cached preview material/geometry differs from the selected source appearance consumer");
        check(assemblyFingerprint(app->tuningCoursePreviewCar.assembly())==assemblyFingerprint(expectedCar.assembly()),"Rendered candidate assembly differs from selected source parts");
        check(assemblyFingerprint(app->tuningCoursePreviewCar.profilePlateAssembly())==assemblyFingerprint(expectedCar.profilePlateAssembly()),"Candidate preview lost current driver's number-plate assembly");
        auto plainCar=CarPresentation::loadConfiguredAppearance(isolated,profile,expectedAppearance);plainCar.pose({},false,false);
        check(app->tuningCoursePreviewCar.assembly().instances.size()>plainCar.assembly().instances.size(),"Source layer2 remained metadata without additional submitted geometry");
        const auto materials=original::OriginalCarMaterialRebuild::load(isolated/"data/original_models"/folder/"material_layout.bin",car);
        const auto& slots=materials.semanticChunks();unsigned secondaryInstances=0;
        for(const auto& instance:app->tuningCoursePreviewCar.assembly().instances){
            bool secondary=false;for(unsigned slot=140;slot<=186;++slot)secondary|=slots[slot]>=0&&unsigned(slots[slot])==instance.chunk;
            if(secondary)++secondaryInstances;
        }
        check(secondaryInstances>0,"Rendered candidate lacks source secondary semantics140..186");
        Mesh submitted;submitted.originalCar(app->tuningCoursePreviewModel,app->tuningCoursePreviewCar.assembly(),{},0);
        unsigned environmentRanges=0;for(const auto& range:submitted.ranges){
            if(range.texture==environmentIndex)++environmentRanges;
            check(range.texture==0xffffffffu||range.texture<environmentIndex+app->tuningCoursePreviewEnvironment.size(),"Candidate draw references an image outside its uploaded car/plate/environment banks");
        }
        check(environmentRanges>0,"Secondary pass never references the source environment image");
    };
    verifyCandidate();
    auto unchangedDuringPreview=[&]{
        check(profileExceptCountdown(app->frontend.battleProfile)==profileExceptCountdown(tuningProfile),"Browsing a candidate changed the live driver profile beyond its source countdown");
        check(app->profiles.load(0).profile.words==tuningSaved.words&&fs::last_write_time(app->profiles.path(0))==tuningSaveTime,"Browsing a candidate rewrote the saved driver");
        check(!app->pendingProfiles[0]&&!app->pendingSetupCompletion[0]&&metadataMissing(0),"Browsing a candidate queued persistence or completed setup");
        check(modelFingerprint(app->originalModel)==baseModel&&assemblyFingerprint(app->carPresentation.assembly())==baseAssembly,"Candidate preview mutated the base car model or posed assembly");
    };
    const auto package=app->frontend.tuningCourseState().selected496;
    const auto firstCandidateModel=modelFingerprint(app->tuningCoursePreviewModel),firstCandidateAssembly=assemblyFingerprint(app->tuningCoursePreviewCar.assembly());
    const auto firstRevision=app->tuningCoursePreviewRevision;tick('D',true);tick();
    check(app->frontend.tuningCourseState().selected496!=package,"Actual D command failed to change tuning selection");
    check(app->tuningCoursePreviewRevision==firstRevision+1,"Source package change did not rebuild the candidate exactly once");verifyCandidate();unchangedDuringPreview();
    check(modelFingerprint(app->tuningCoursePreviewModel)!=firstCandidateModel||assemblyFingerprint(app->tuningCoursePreviewCar.assembly())!=firstCandidateAssembly,"Different AE86 source packages did not change any displayed material or assembly");
    tick('A',true);tick();check(app->frontend.tuningCourseState().selected496==package,"Actual A command failed to restore tuning selection");
    verifyCandidate();unchangedDuringPreview();
    check(modelFingerprint(app->tuningCoursePreviewModel)==firstCandidateModel&&assemblyFingerprint(app->tuningCoursePreviewCar.assembly())==firstCandidateAssembly,"Returning to the first package did not restore its exact model and assembly");
    // Visit every authored AE86 package, including the final NORMAL entry and
    // the wrap back to the original choice, using only actual App input.
    for(unsigned n=1;n<=app->frontend.tuningCourseState().count504;++n){
        const auto previousRevision=app->tuningCoursePreviewRevision;tick('D',true);tick();
        check(app->frontend.tuningCourseState().selected496==(package+n)%app->frontend.tuningCourseState().count504,"Actual package navigation did not wrap in source order");
        check(app->tuningCoursePreviewRevision==previousRevision+1,"One source navigation rebuilt the preview more or less than once");verifyCandidate();unchangedDuringPreview();
    }
    const auto selectedPackage=(package+1u)%app->frontend.tuningCourseState().count504;
    tick('D',true);tick();check(app->frontend.tuningCourseState().selected496==selectedPackage,"Fresh fixture failed to choose a different package before confirmation");
    verifyCandidate();unchangedDuringPreview();capture("fresh-tuning-candidate");
    const auto beforeRepaint=app->frontend.tuningCourseState().frame492;
    const auto beforeRepaintRevision=app->tuningCoursePreviewRevision;
    const auto beforeRepaintModel=modelFingerprint(app->tuningCoursePreviewModel),beforeRepaintAssembly=assemblyFingerprint(app->tuningCoursePreviewCar.assembly());
    for(unsigned n=0;n<4;++n)tick(0,false,0);
    check(app->frontend.tuningCourseState().frame492==beforeRepaint,"Repeated render advanced the source tuning selector");
    check(app->tuningCoursePreviewRevision==beforeRepaintRevision&&modelFingerprint(app->tuningCoursePreviewModel)==beforeRepaintModel&&assemblyFingerprint(app->tuningCoursePreviewCar.assembly())==beforeRepaintAssembly,"Repeated render rebuilt or mutated the cached candidate");unchangedDuringPreview();
    // Explicit texture-cache recovery fixture: evict the environment image
    // and deactivate the cached preview while retaining this exact source
    // owner/frame. This is not proof of natural parent re-entry; natural
    // owner exit/restoration is exercised separately below. Changing stage
    // even for a zero-dt render would reinitialize the source owner/fade.
    const auto beforeTextureReturn=output/"candidate-textures-before-return.bmp",afterTextureReturn=output/"candidate-textures-after-return.bmp";
    check(app->renderer.saveBitmap(beforeTextureReturn.wstring()),"Could not capture candidate texture baseline");
    check(app->renderer.loadTextures(app->originalTextures)&&app->renderer.loadTextures(app->numberPlate.textures,true),"Could not simulate the ordinary car/plate texture table");
    app->menuTexturesLoaded=true;app->tuningCoursePreviewActive=false;app->loadTuningCoursePreview();
    check(!app->menuTexturesLoaded,"Candidate cache recovery failed to invalidate the ordinary texture table");
    check(app->tuningCoursePreviewRevision==beforeRepaintRevision+1,"Candidate cache recovery did not rebuild exactly once");
    check(app->renderMenu(0)&&app->menuTexturesLoaded,"Candidate cache recovery did not upload the full texture set");
    check(app->renderer.saveBitmap(afterTextureReturn.wstring()),"Could not capture restored candidate texture table");
    check(fileBytes(beforeTextureReturn)==fileBytes(afterTextureReturn),"Candidate image changed after its car/plate/environment textures were restored");
    check(app->frontend.stage==FrontendStage::TuningCourse&&app->frontend.tuningCourseState().frame492==beforeRepaint,"Texture-cache recovery diagnostic changed the source owner/frame");verifyCandidate();unchangedDuringPreview();
    const auto confirmedPreviewRevision=app->tuningCoursePreviewRevision;
    confirmTo(FrontendStage::Name);ready();capture("fresh-name-empty");check(metadataMissing(0),"Package selection prematurely marked driver ready");
    verifyEntryRepaint("fresh-name-empty");
    check(app->frontend.battleProfile.byte(152)==selectedPackage&&app->profiles.load(0).profile.byte(152)==selectedPackage,"Confirmed source package was not committed to live and saved driver profiles");
    for(unsigned offset=153;offset<=166;++offset)check(app->frontend.battleProfile.byte(offset)==tuningProfile.byte(offset),"Choosing a package granted installed parts or performance progress");
    check(modelFingerprint(app->originalModel)==baseModel&&assemblyFingerprint(app->carPresentation.assembly())==baseAssembly,"Name entry failed to restore the saved base car after package preview");
    check(!app->tuningCoursePreviewActive&&app->tuningCoursePreviewRevision==confirmedPreviewRevision,"Leaving package selection retained the active candidate or rebuilt it outside its owner");
    const auto tables=original::OriginalNameEntryTables::load(isolated);
    constexpr std::array<unsigned,5> tiles{2,7,17,8,18};
    std::array<unsigned,5> expected{};
    for(unsigned letter=0;letter<tiles.size();++letter){
        const auto target=tiles[letter];unsigned guard=0;
        while(app->frontend.nameEntryState().selected460!=target&&guard++<1000){const int direction=app->frontend.nameEntryState().selected460<target?'D':'A';tick(direction);}
        check(app->frontend.nameEntryState().selected460==target,"Held A/D failed to reach CHRIS glyph");
        while(app->frontend.nameEntryState().selector.phase)tick();
        tick(VK_RETURN,true);expected[letter]=tables.keyboard[122+target].words[0];
        check(app->frontend.nameEntryState().length528==letter+1,"Enter did not append original selected glyph");
        check(app->frontend.nameEntryState().glyphIds480[letter]==expected[letter],"Original glyph mapping mismatch");
    }
    capture("fresh-name-CHRIS");check(metadataMissing(0),"Typing a name prematurely marked driver ready");verifyEntryRepaint("fresh-name-CHRIS");
    tick(VK_RETURN,true);
    verifyDriverEntry();
    auto saved=app->profiles.load(0);check(saved.origin==LocalDriverProfiles::Origin::Saved&&saved.profile.u(76)==5,"Name commit did not save before child exit");
    for(unsigned n=0;n<5;++n)check(saved.profile.u(44+4*n)==expected[n],"Saved CHRIS glyph mismatch");
    check(metadataMissing(0),"Name commit incorrectly marked setup before owner exit");
    const auto profileTime=fs::last_write_time(app->profiles.path(0));const auto nameFrame=app->frontend.nameEntryState().frame572;
    for(unsigned n=0;n<4;++n)tick(0,false,0);
    check(app->frontend.nameEntryState().frame572==nameFrame&&fs::last_write_time(app->profiles.path(0))==profileTime&&metadataMissing(0),"Repeated render changed committed setup or rewrote profile");
    for(unsigned n=0;app->frontend.stage==FrontendStage::Name&&n<100;++n)tick();
    check(app->frontend.stage==FrontendStage::Mode&&app->driverSetup.load(0).status==LocalDriverSetup::Status::Complete,"Name exit did not complete persisted native setup");
    check(!app->driverEntryPreviewActive,"Name's private car remained active in Mode selection");
    capture("fresh-mode");check(app->audio.selectionStatistics().songsStarted==2&&app->audio.selectionStatistics().cue==1,"TYPE restarted through entry or Mode did not switch to SELECT");
    app->loadedProfileCar=-1;app->loadSelectedProfile();app->frontend.stage=FrontendStage::Transmission;tick(0,false,0);ready();verifyEntryRepaint("reload-transmission-CHRIS");confirmTo(FrontendStage::Mode);
    check(app->frontend.battleProfile.u(76)==5&&!original::originalDriverSetupRequested(app->frontend.battleProfile),"Reload lost established CHRIS setup");
    check(app->frontend.battleProfile.byte(152)==selectedPackage,"Reload lost the confirmed tuning package");
    check(!app->tuningCoursePreviewActive,"Established reload reactivated a stale tuning candidate");
    for(unsigned offset=153;offset<=166;++offset)check(app->frontend.battleProfile.byte(offset)==tuningProfile.byte(offset),"Reload installed preview-only parts or changed earned tuning progress");capture("reload-skips-setup");
    // Explicit valid old-save fixture: car29 skips the package child; kind2
    // imports SEGA without inferring readiness from the existing profile.
    auto migrated=original::makeOriginalFreshBattleProfile();migrated.setu(16,29);migrated.setu(72,134567);migrated.setByte(153,5);migrated.setByte(164,6);migrated.setByte(156,1);migrated.setu(1180,129);migrated.setu(76,4);
    const std::array<unsigned,5> sega{180,166,168,162,0xdeadbeef};for(unsigned n=0;n<5;++n)migrated.setu(44+4*n,sega[n]);
    check(app->profiles.save(29,migrated),"Could not create isolated migrated fixture");
    app->frontend.car=29;app->loadedProfileCar=-1;app->loadSelectedCar();app->frontend.stage=FrontendStage::Transmission;tick(0,false,0);ready();verifyEntryRepaint("migrated-transmission-SEGA");confirmTo(FrontendStage::Name);ready();capture("migrated-SEGA");verifyEntryRepaint("migrated-name-SEGA");
    check(metadataMissing(29)&&app->frontend.battleProfile.byte(1192)==2,"Saved profile was incorrectly considered setup complete");
    // Deny replacement of only this isolated profile. Atomic save must fail,
    // while queued profile and completion marker survive for a later retry.
    HANDLE lock=CreateFileW(app->profiles.path(29).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(lock==INVALID_HANDLE_VALUE)throw std::runtime_error("Could not lock isolated profile fixture");
    tick(VK_RETURN,true);check(bool(app->pendingProfiles[29])&&metadataMissing(29),"Failed name save lost pending profile or wrote marker");
    for(unsigned n=0;app->frontend.stage==FrontendStage::Name&&n<100;++n)tick();
    check(app->frontend.stage==FrontendStage::Mode&&app->pendingSetupCompletion[29]&&app->pendingProfiles[29]&&metadataMissing(29),"Failed final profile save incorrectly completed setup");
    capture("failed-save-retained");CloseHandle(lock);app->flushProfiles();
    check(!app->pendingProfiles[29]&&!app->pendingSetupCompletion[29]&&app->driverSetup.load(29).status==LocalDriverSetup::Status::Complete,"Retry did not save profile before completion marker");
    const auto restored=app->profiles.load(29).profile;check(restored.u(72)==134567&&restored.byte(153)==5&&restored.byte(164)==6&&restored.byte(156)==1,"Migration reset earned points/parts");
    check(restored.u(76)==4,"Migration changed imported name length");for(unsigned n=0;n<5;++n)check(restored.u(44+4*n)==sega[n],"Migration changed imported name or inactive tail");
    capture("retry-save-complete");
    // A completion marker describes a previously completed setup, but cannot
    // make a missing/corrupt replacement profile ready. Loading the fallback
    // must persist its unfinished bit before a later load sees the marker.
    auto reloadSelected=[&](unsigned car){app->frontend.car=int(car);app->loadedProfileCar=-1;app->loadSelectedProfile();};
    auto checkUnfinishedReplacement=[&](unsigned car){
        check(app->driverSetup.load(car).status==LocalDriverSetup::Status::Complete,"Stale marker fixture unexpectedly disappeared");
        check(original::originalDriverSetupRequested(app->frontend.battleProfile)&&app->frontend.battleProfile.byte(1192)==0,"Stale Complete accepted a missing/corrupt replacement as established");
        const auto replacement=app->profiles.load(car);
        check(replacement.origin==LocalDriverProfiles::Origin::Saved&&original::originalDriverSetupRequested(replacement.profile),"Fresh replacement was not persisted with its unfinished setup flag");
        check(!app->pendingProfiles[car]&&!app->pendingSetupCompletion[car],"Successful fallback save left unexpected pending persistence");
        for(unsigned retry=0;retry<2;++retry){reloadSelected(car);
            check(original::originalDriverSetupRequested(app->frontend.battleProfile)&&app->frontend.battleProfile.byte(1192)==0,"Forced reload let a stale Complete marker clear replacement setup");
            check(original::originalDriverSetupRequested(app->profiles.load(car).profile),"Forced reload lost the persisted unfinished setup bit");
        }
        app->frontend.stage=FrontendStage::Transmission;tick(0,false,0);confirmTo(FrontendStage::TuningCourse);
        check(original::originalDriverSetupRequested(app->frontend.battleProfile),"Transmission route consumed replacement setup without completing it");
    };
    constexpr unsigned missingCar=2,corruptCar=3,backupCar=4;
    check(app->profiles.load(missingCar).origin==LocalDriverProfiles::Origin::Fresh,"Missing-profile fixture was not fresh");
    check(app->driverSetup.markComplete(missingCar),"Could not create isolated stale Complete marker");
    reloadSelected(missingCar);checkUnfinishedReplacement(missingCar);capture("stale-complete-missing-profile");
    auto corruptIsolated=[&](const fs::path& path){
        check(fs::canonical(path.parent_path())==fs::canonical(isolated/"userdata/driver_profiles_v1"),"Corrupt fixture escaped isolated profile directory");
        std::ofstream damaged(path,std::ios::binary|std::ios::trunc);damaged<<"deliberately invalid isolated driver-entry fixture";damaged.close();check(bool(damaged),"Could not write isolated corrupt-profile fixture");
    };
    check(app->profiles.load(corruptCar).origin==LocalDriverProfiles::Origin::Fresh,"Corrupt-profile fixture was not fresh");
    check(app->driverSetup.markComplete(corruptCar),"Could not create corrupt-profile stale marker");
    corruptIsolated(app->profiles.path(corruptCar));corruptIsolated(fs::path(app->profiles.path(corruptCar).wstring()+L".previous"));
    check(app->profiles.load(corruptCar).origin==LocalDriverProfiles::Origin::Unreadable,"Both corrupted profile copies unexpectedly decoded");
    reloadSelected(corruptCar);checkUnfinishedReplacement(corruptCar);capture("stale-complete-corrupt-profile");
    // A valid previous copy really is stored driver data. Complete+Backup must
    // keep the established route and earned state, without importing/editing
    // the name merely because the primary file was damaged.
    auto backupProfile=original::makeOriginalFreshBattleProfile();backupProfile.setu(16,backupCar);backupProfile.setu(72,246810);
    backupProfile.setu(1180,129);backupProfile.setByte(152,1);backupProfile.setByte(153,5);backupProfile.setByte(164,6);backupProfile.setByte(156,1);backupProfile.setu(76,4);
    for(unsigned n=0;n<5;++n)backupProfile.setu(44+4*n,sega[n]);
    check(app->profiles.save(backupCar,backupProfile),"Could not save isolated valid backup source");
    auto displacedPrimary=backupProfile;displacedPrimary.setu(72,999999);
    check(app->profiles.save(backupCar,displacedPrimary)&&app->driverSetup.markComplete(backupCar),"Could not establish valid backup and Complete fixture");
    corruptIsolated(app->profiles.path(backupCar));
    const auto backupBefore=app->profiles.load(backupCar);
    check(backupBefore.origin==LocalDriverProfiles::Origin::Backup&&backupBefore.profile.words==backupProfile.words,"Fixture did not select its valid previous profile");
    for(unsigned retry=0;retry<2;++retry){reloadSelected(backupCar);
        check(!original::originalDriverSetupRequested(app->frontend.battleProfile)&&app->frontend.battleProfile.byte(1192)==0,"Valid Backup+Complete incorrectly requested setup/import");
        check(app->frontend.battleProfile.u(72)==246810&&app->frontend.battleProfile.u(76)==4,"Backup recovery lost earned points or name length");
        for(unsigned n=0;n<5;++n)check(app->frontend.battleProfile.u(44+4*n)==sega[n],"Backup recovery altered established name or inactive tail");
        for(unsigned offset=152;offset<=166;++offset)check(app->frontend.battleProfile.byte(offset)==backupProfile.byte(offset),"Backup recovery altered package, earned parts or performance");
    }
    app->frontend.stage=FrontendStage::Transmission;tick(0,false,0);confirmTo(FrontendStage::Mode);
    check(!app->tuningCoursePreviewActive&&!original::originalDriverSetupRequested(app->frontend.battleProfile),"Established backup route visited a stale tuning candidate");
    capture("valid-backup-complete-skips-setup");check(!clipped,"Application setup audio clipped");
    std::cout<<"PASS actual driver entry: "<<checks<<" checks, "<<ticks<<" application updates/repaints, "<<pcmFrames<<" stereo frames; Transmission/Name source appearance and layer2/environment, private race-car isolation, pixel-exact frozen repaint/GPU texture restoration, source package previews, CHRIS held A/D+Enter, save-before-marker, established reload, migrated car29 SEGA/earned parts, failed-write retry, stale Complete with missing/corrupt profiles across forced reloads, valid Backup+Complete route. Isolated root: "<<isolated.string()<<". No audio device or visible window.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
