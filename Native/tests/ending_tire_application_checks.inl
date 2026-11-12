{
    const auto tirePhysics=app->originalSession.rollbackDigest();
    const auto tireState=[](const OriginalEndingTireMarks& marks){
        std::uint64_t hash=14695981039346656037ull;
        const auto word=[&](unsigned value){hash=(hash^value)*1099511628211ull;};
        const auto point=[&](const auto& p){for(float value:p)word(std::bit_cast<unsigned>(value));};
        word(marks.active());word(marks.updates());word(marks.system().acquired());
        for(auto slot:marks.system().order()){
            word(slot);const auto& stroke=marks.system().stroke(slot);word(stroke.started());word(stroke.hasEdges());word(stroke.hasStrip());
            point(stroke.previous());for(const auto& p:stroke.edges())point(p);
            for(const auto& strip:stroke.strips()){
                word(strip.cadence);word(unsigned(strip.samples.size()));point(strip.center);
                word(std::bit_cast<unsigned>(strip.radiusSquared));
                for(const auto& sample:strip.samples){for(const auto& p:sample.edges)point(p);point(sample.normal);word(sample.color);}
            }
        }
        return hash;
    };
    unsigned recordedMarks=0;
    for(unsigned i=0;i<leafFootage.highlightSize();++i)for(const auto& car:leafFootage.recentFrame(i).cars)
        for(unsigned rear=2;rear<4;++rear)recordedMarks+=car.tireMarkIntensity[rear]!=0;
    check(recordedMarks>0,"Real Legend capture omitted tyre-mark intensity");
    app->endingReplay=leafFootage;app->beginEnding();
    check(app->endingTireMarks.active()&&app->endingTireMarks.updates()==0,"Dry ending mark initialization");
    unsigned selected=0,changed=0,drawn=0;
    for(unsigned tick=1;tick<5000&&app->endingActive;++tick){
        app->advanceEnding(1./60);
        if(tick<120||tick%30||app->endingTireMarks.system().order().empty())continue;
        Mesh mesh;drawn=app->endingTireMarks.append(mesh,app->endingCamera);if(!drawn)continue;
        const auto saved=app->endingTireMarks;const auto state=tireState(saved);const auto random=app->endingRandom;
        const auto on=capture("tires-on");app->endingTireMarks.reset();const auto off=capture("tires-off");app->endingTireMarks=saved;
        check(on==capture("tires-restored"),"Restored tyre marks changed pixels");
        check(tireState(app->endingTireMarks)==state&&app->endingRandom==random,"Mark repaint changed state/RNG");
        changed=0;for(unsigned i=0;i<on.size();++i)changed+=(on[i]&0xffffffu)!=(off[i]&0xffffffu);
        if(changed>8){selected=tick;break;}
    }
    std::cout<<"Ending tyre marks: recorded="<<recordedMarks<<" selected="<<selected<<" visible strips="<<drawn<<" changed pixels="<<changed<<'\n'<<std::flush;
    check(selected>0,"Recorded rear-wheel slip produced no visible tyre marks");
    const auto before=tireState(app->endingTireMarks);app->paused=true;app->advanceEnding(3);app->renderEnding(0);app->paused=false;
    check(before==tireState(app->endingTireMarks),"Pause advanced tyre marks");
    std::uint64_t reference=0;
    for(unsigned fps:{30u,60u,144u,240u}){
        app->endingReplay=leafFootage;app->beginEnding();for(unsigned frame=0;frame<fps*22;++frame)app->advanceEnding(1./fps);
        check(app->endingTireMarks.updates()==1320&&!app->endingTireMarks.system().order().empty(),"Tyre tick-rate coverage");
        if(fps==30)reference=tireState(app->endingTireMarks);else check(reference==tireState(app->endingTireMarks),"Tyre state depends on rendering FPS");
    }
    check(app->originalSession.rollbackDigest()==tirePhysics,"Tyre playback changed live physics");
    app->wet=true;app->endingReplay=leafFootage;app->beginEnding();app->advanceEnding(1./60);
    check(!app->endingTireMarks.active()&&app->endingTireMarks.system().order().empty(),"Wet ending admitted tyre marks");app->wet=false;
    app->clearEndingPresentation();check(!app->endingTireMarks.active()&&app->endingTireMarks.updates()==0,"Tyre marks survived cleanup");
}
