// A front-facing inspection of genuine night Akagi ending poses. The camera
// alone is arranged; the standard ending camera remains unchanged in game.
void prepareEndingGlareUnityFixture(App& app,unsigned scene){
    if(scene<660||scene>662)throw std::invalid_argument("Ending glare fixture");
    static OriginalEndingHeadlightGlare saved;
    static unsigned seed=0;static std::uint64_t physics=0;static bool prepared=false;
    const unsigned phase=scene-660;
    if(phase==0){
        prepareEndingWeatherUnityFixture(app,476);
        const auto& frame=app.endingReplay.playbackFrame(599);
        const auto& car=frame.cars[0];
        app.endingCamera.reset(car.body+forward(car.yaw)*12+Vec3{0,.7f,0},car.body,1);
        seed=app.endingRandom;physics=app.originalSession.rollbackDigest();saved=app.endingHeadlightGlare;prepared=true;
    }else{
        if(!prepared)throw std::logic_error("Glare comparison before preparation");
        if(phase==1)app.endingHeadlightGlare.reset();else app.endingHeadlightGlare=saved;
    }
    app.input={};if(!app.renderEnding(0))throw std::runtime_error(app.renderer.error);
    if(app.endingRandom!=seed||app.originalSession.rollbackDigest()!=physics)throw std::runtime_error("Glare repaint changed RNG/physics");
    Mesh mesh;const auto lamps=app.endingHeadlightGlare.append(mesh,app.endingReplay.playbackFrame(599),
        {unsigned(app.frontend.car),unsigned(app.loadedRivalCar)},app.rivalVisible,app.endingCamera,app.endingHeadlightGlareTextureBase);
    if((lamps>0)!=(phase!=1))throw std::runtime_error("Glare fixture missing lamps/removal");
    std::ofstream out(app.saveRoot.parent_path()/("ending-glare-"+std::to_string(phase)+".json"));
    out<<"{\"sample\":600,\"lamps\":"<<lamps<<",\"textureBase\":"<<app.endingHeadlightGlareTextureBase
       <<",\"ranges\":"<<mesh.ranges.size()<<",\"vertices\":"<<mesh.vertices.size()<<",\"random\":"<<seed<<"}\n";
}
