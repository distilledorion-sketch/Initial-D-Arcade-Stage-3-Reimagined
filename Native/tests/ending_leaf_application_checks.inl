{
    // Myogi and Akina contain no leaf-admitting collision materials. Usui's
    // source collision includes material2. Seed only the highlight threshold,
    // never contact/effect data.
    auto originalApp=std::move(app);app=std::make_unique<App>();
    app->root=root;app->saveRoot=out/"leaf-userdata";app->validationMode=true;
    app->settings();app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);app->audio.enabled=false;
    check(app->renderer.initialize(nullptr,960,720,true),"Leaf offscreen renderer");
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    auto& f=app->frontend;f.gameMode=original::OriginalGameMode::LegendOfTheStreets;
    f.battleProfile=original::makeOriginalFreshBattleProfile();f.car=0;f.automatic=true;f.battleProfile.setu(16,0);f.battleProfile.setByte(164,63);
    original::selectOriginalRival(f.battleProfile,original::originalLegendRivalId(1,0));
    f.battleProfile.setu(8,0);f.battleProfile.setu(32,0);app->start();
    check(app->courseIndex==1&&!app->night&&!app->wet,"Usui leaf scene conditions");
    EndingReplay leafFootage;unsigned contactFrames=0;
    for(unsigned tick=0;tick<6800&&leafFootage.highlightSize()<5000;++tick){
        const auto size=app->endingReplay.size();app->simulate(drivingInput());
        if(app->endingReplay.size()>size||size==EndingReplay::capacity){
            const auto& frame=app->endingReplay.recentFrame(unsigned(app->endingReplay.size()-1));
            for(auto surface:frame.cars[0].wheelSurfaces)contactFrames+=original::originalReplayLeafSurface(surface);
            leafFootage.record(frame,*app->courseLightFsca,.7f,0);
        }
    }
    std::cout<<"Leaf clip frames="<<leafFootage.highlightSize()<<" eligible wheel contacts="<<contactFrames<<'\n'<<std::flush;
    check(contactFrames>0,"Usui drive did not cross original leaf surfaces");
    const auto leafPhysics=app->originalSession.rollbackDigest();
    app->endingReplay=leafFootage;app->beginEnding();
    check(app->endingLeaves.active()&&app->endingLeaves.updates()==0&&app->endingLeaves.system().order().empty(),"Ending leaf reset/admission");
    const auto leafState=[](const OriginalEndingLeaves& leaves){
        std::vector<unsigned> words{unsigned(leaves.active()),leaves.updates(),leaves.system().admitted()};
        const auto point=[&](const auto& values){for(float value:values)words.push_back(std::bit_cast<unsigned>(value));};
        const auto& wake=leaves.system().wake();point(wake.origin);point(wake.direction);words.push_back(std::bit_cast<unsigned>(wake.strength));
        for(auto index:leaves.system().order()){
            const auto& p=leaves.system().particle(index);words.push_back(index);words.push_back(p.model);words.push_back(p.remaining);
            point(p.position);point(p.velocity);point(p.angles);point(p.spin);point(p.origin);
        }
        return words;
    };
    const auto capture=[&](const char* suffix){
        check(app->renderEnding(0),"Ending leaf scene render");
        const auto path=out/(std::string("ending-leaves-")+suffix+".bmp");
        check(app->renderer.saveBitmap(path.wstring()),"Ending leaf readback");
        std::ifstream file(path,std::ios::binary);BITMAPFILEHEADER header{};file.read(reinterpret_cast<char*>(&header),sizeof header);file.seekg(header.bfOffBits);
        std::vector<unsigned> pixels(std::size_t(app->renderer.width)*app->renderer.height);
        file.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);check(bool(file),"Ending leaf image extent");return pixels;
    };
    unsigned selected=0,changed=0,maximum=0;
    for(unsigned tick=1;tick<=5000&&app->endingActive;++tick){
        app->advanceEnding(1./60);maximum=std::max(maximum,unsigned(app->endingLeaves.system().order().size()));
        if(tick<120||tick%15||app->endingLeaves.system().order().empty())continue;
        const auto saved=app->endingLeaves;const auto beforeState=leafState(saved);const auto random=app->endingRandom;
        const auto on=capture("on");app->endingLeaves.reset();const auto off=capture("off");app->endingLeaves=saved;
        const auto restored=capture("restored");check(on==restored,"Restored ending leaves changed pixels");
        check(leafState(app->endingLeaves)==beforeState&&app->endingRandom==random,"Leaf rendering advanced state/RNG");
        changed=0;for(unsigned i=0;i<on.size();++i)changed+=on[i]!=off[i];
        if(changed>8){selected=tick;break;}
    }
    std::cout<<"Ending leaves selected="<<selected<<" changed="<<changed<<" maximum="<<maximum<<'\n'<<std::flush;
    check(selected>0,"Captured Legend contacts produced no visible ending leaves");
    const auto beforeState=leafState(app->endingLeaves);const auto random=app->endingRandom;
    app->paused=true;app->advanceEnding(10);app->renderEnding(0);app->paused=false;
    check(leafState(app->endingLeaves)==beforeState&&app->endingRandom==random,"Paused ending advanced leaves");
    std::vector<unsigned> reference;unsigned referenceRandom=0;
    for(unsigned fps:{30u,60u,144u,240u}){
        app->endingReplay=leafFootage;app->beginEnding();
        for(unsigned render=0;render<fps*22;++render)app->advanceEnding(1./fps);
        check(app->endingLeaves.updates()==1320,"Leaf tick rate follows render rate");
        check(!app->endingLeaves.system().order().empty(),"Frame-rate comparison missed active leaves");
        if(fps==30){reference=leafState(app->endingLeaves);referenceRandom=app->endingRandom;}
        else check(reference==leafState(app->endingLeaves)&&referenceRandom==app->endingRandom,"Leaf state differs by host FPS");
    }
    check(app->originalSession.rollbackDigest()==leafPhysics,"Ending leaves changed live physics");
    app->clearEndingPresentation();
    check(!app->endingLeaves.active()&&app->endingLeaves.updates()==0&&app->endingLeaves.system().order().empty(),"Ending leaves survived cleanup");
    #include "ending_tire_application_checks.inl"
    app=std::move(originalApp);
}
