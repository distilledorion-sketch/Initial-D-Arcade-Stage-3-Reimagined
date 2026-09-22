// Isolated original-solver integration. No fabricated slip/contact samples.
int runDrivingEffectsAppTests(App& app){
    std::ofstream log(app.saveRoot.parent_path()/"driving-effects-live.txt");
    app.validationMode=true;app.validationHideDrivingEffects=false;
    app.replayPlaybackActive=false;app.paused=false;
    app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
    app.frontend.course=app.courseIndex=3;app.frontend.car=0;
    app.frontend.night=app.night=false;app.frontend.wet=app.wet=false;
    app.start();app.loadingActive=app.vsActive=app.preRaceDialogueActive=false;app.menu=false;
    unsigned peakMarks=0,peakSmoke=0,checks=0;float peakSlip=0,topSpeed=0;
    for(unsigned i=0;i<1800;++i){
        DriverInput input;input.automatic=true;input.throttle=1;
        if(i>650)input.steer=((i/100)&1)?.85f:-.85f;
        app.simulate(input);
        const auto before=app.presentedSession().vehicle().drive.words;
        Idas3UiBeginFrame(app.renderer.width,app.renderer.height);
        if(!app.render(1./60))throw std::runtime_error(app.renderer.error);
        if(before!=app.presentedSession().vehicle().drive.words)throw std::runtime_error("Driving effects changed original physics state");
        ++checks;peakSlip=std::max(peakSlip,std::abs(app.vehicle.slip));topSpeed=std::max(topSpeed,app.vehicle.speedKmh());
        peakMarks=std::max(peakMarks,app.drivingEffects.markCount());peakSmoke=std::max(peakSmoke,app.drivingEffects.smokeCount());
    }
    log<<"frames "<<checks<<" top_kmh "<<topSpeed<<" peak_slip "<<peakSlip<<" peak_marks "<<peakMarks<<" peak_smoke "<<peakSmoke<<'\n';log.flush();
    if(!peakMarks||!peakSmoke)throw std::runtime_error("Live original driving did not emit dry tire effects");
    app.paused=true;const auto marks=app.drivingEffects.markCount(),smoke=app.drivingEffects.smokeCount();
    app.render(.1);
    if(marks!=app.drivingEffects.markCount()||smoke!=app.drivingEffects.smokeCount())throw std::runtime_error("Paused live effects advanced");
    log<<"PASS original driving emits smoke and skid marks; every rendered frame preserves all320 original drive words; pause freezes effect state.\n";
    return 0;
}
