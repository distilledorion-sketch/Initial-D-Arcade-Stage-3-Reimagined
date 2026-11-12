void prepareEndingTireUnityFixture(App& app,unsigned scene){
    if(scene<650||scene>652)throw std::invalid_argument("Ending tyre fixture");
    static OriginalEndingTireMarks saved;static unsigned seed=0;static std::uint64_t physics=0;static bool prepared=false;
    const unsigned phase=scene-650;
    if(phase==0){
        // Reuse the isolated genuine Usui recording, then rewind its ending.
        // No slip/contact override: only the highlight start was arranged.
        prepareEndingLeafUnityFixture(app,640);app.beginEnding();
        for(unsigned tick=0;tick<510;++tick)app.advanceEnding(1./60);
        Mesh mesh;if(app.endingTireMarks.append(mesh,app.endingCamera)==0)throw std::runtime_error("Recorded tyres produced no strips");
        saved=app.endingTireMarks;seed=app.endingRandom;physics=app.originalSession.rollbackDigest();prepared=true;
    }else{
        if(!prepared)throw std::logic_error("Tyre comparison before preparation");
        if(phase==1)app.endingTireMarks.reset();else app.endingTireMarks=saved;
    }
    app.input={};if(!app.renderEnding(0))throw std::runtime_error(app.renderer.error);
    if(app.endingRandom!=seed||app.originalSession.rollbackDigest()!=physics)throw std::runtime_error("Tyre repaint changed RNG/physics");
    Mesh mesh;const auto strips=app.endingTireMarks.append(mesh,app.endingCamera);
    std::ofstream out(app.saveRoot.parent_path()/("ending-tire-"+std::to_string(phase)+".json"));
    out<<"{\"sample\":510,\"strips\":"<<strips<<",\"ranges\":"<<mesh.ranges.size()
       <<",\"vertices\":"<<mesh.vertices.size()<<",\"random\":"<<seed<<"}\n";
}
