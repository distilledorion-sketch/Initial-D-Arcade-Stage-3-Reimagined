// Included inside App. All artwork below is rendered by the existing native
// result/Continue painters and captured by Unity's normal scene UI pipeline.
OnlineResultScreen onlineResult;
FixedClock onlineResultClock;
bool onlineFinishTickActive=false,onlineFinishTickDriving=false,onlineFinishTickViews=false;
bool onlineResultPagePending=false;
OriginalBattleResultsState onlineResultPoints;
original::OriginalBattleProfile onlineResultProfile;
original::OriginalRivalDialogScene onlineContinueArtwork;
MenuFont onlineResultFont;
std::unique_ptr<original::OriginalTuningPreviewPresentation> onlineResultPreview;
bool onlineResultTextures=false;
int onlineResultBattlePoints=0,onlineResultLevel=1;
std::vector<std::uint32_t> onlineResultPixels;

void resetOnlineResult(){
    onlineResult={};onlineResultClock.reset();onlineResultPreview.reset();
    onlineFinishTickActive=onlineFinishTickDriving=onlineFinishTickViews=onlineResultPagePending=false;
    onlineResultTextures=false;onlineResultBattlePoints=0;onlineResultLevel=1;
}
void beginOnlineResult(){
    onlineResultProfile=battleProfile;
    if(multiplayer.rewardSelection>=0){
        original::OriginalBattleProfile saved;
        if(onlineCarProfile(multiplayer.rewardSelection,saved))onlineResultProfile=saved;
    }
    onlineResultProfile.setu(16,multiplayer.config.localCar);
    // Linked067C8C invokes068520 on the remote goal packet too, propagating
    // outer+624. This is a shared goal outcome, not only this car's crossing.
    const bool goal=multiplayer.finishWinner>=0;
    onlineResult.begin(multiplayer.pointsEarned,onlineResultProfile.u(72),goal,
        multiplayer.finishWinner!=int(multiplayer.config.localSlot));
    onlineResultClock.reset();onlineResultPagePending=false;
    onlineFinishTickActive=false;onlineFinishTickDriving=onlineFinishTickViews=true;
    onlineResultPoints={};onlineResultPoints.profileMode=2;
    const bool won=multiplayer.finishWinner==int(multiplayer.config.localSlot);
    onlineResultPoints.resultStatus=multiplayer.finishWinner<0?2:won?0:1;
    onlineResultPoints.draw=multiplayer.finishWinner==2;
    onlineResultPoints.totalTicks6000=race.elapsed6000;
    onlineResultPoints.sectionCapacity=race.sectionCapacity;
    onlineResultPoints.sectionCount=std::min(unsigned(std::max(0,race.sector)),race.sectionCapacity);
    onlineResultPoints.sectionTimes6000=race.sectionTimes6000;
    if(won)completeOriginalResultSections(onlineResultPoints);
    onlineResultPoints.signedAdvantage=multiplayer.hudLocalMetres-multiplayer.hudRemoteMetres;
    const auto participation=std::min(1000u,multiplayer.pointsEarned);
    onlineResultPoints.points={participation,multiplayer.pointsEarned-participation,0,
        multiplayer.pointsEarned,onlineResult.points.displayedBalance};
}
// Called before each source-cadence simulation attempt, sharing App::clock's
// phase and catch-up batch. This owner does not load assets or mutate rewards.
// EOS deliberately permits settled presentation to finish when post-winner
// authority stalls: its watchdog no longer runs after a winner. Never invent
// accepted inputs, authority frames, checkpoints or solver steps to move UI.
void advanceOnlineFinishSourceTick(){
    onlineFinishTickDriving=onlineResult.page<0||
        (onlineResult.page==0&&!onlineResult.finish.done());
    onlineFinishTickViews=onlineResult.page==0&&!onlineResult.blocked&&
        (onlineResult.finish.phase==original::OriginalLinkedFinishPresentation::Phase::AwaitOutcome||
         onlineResult.finish.phase==original::OriginalLinkedFinishPresentation::Phase::Hold);
    if(onlineResult.page!=0)return;
    // Actual modal/focus/recovery ownership freezes this presentation clock.
    // A normal save in flight only holds the later EOS handoff, not driving.
    onlineResult.advance(false);
    if(onlineResult.page>0)onlineResultPagePending=true;
}
bool onlineFinishAcceptsDriving()const{
    if(onlineResult.page<0)return true;
    return onlineResult.page==0&&!onlineResult.blocked&&(onlineFinishTickActive?
        onlineFinishTickDriving:!onlineResult.finish.done());
}
void materializeOnlineResultPage(){
    if(!tuningTables)tuningTables=original::OriginalTuningData::load(root);
    onlineResultPreview=std::make_unique<original::OriginalTuningPreviewPresentation>();
    onlineResultPreview->load(root,onlineResultProfile,false,onlineResult.page==2);
    onlineResultTextures=false;texturesPending=true;
    if(!onlineResultFont.ready())onlineResultFont=MenuFont::load(root);
    if(onlineResult.page==1)audio.beginResultMusic(true);
    else if(!onlineContinueArtwork.loaded())onlineContinueArtwork.load(root);
}
void advanceOnlineResult(double dt){
    if(onlineResultPagePending){
        onlineResultPagePending=false;onlineResultClock.reset();
        materializeOnlineResultPage();
        // This host sample began in Finish. Its elapsed time cannot also
        // advance the new points owner, even after several catch-up ticks.
        return;
    }
    if(onlineResult.page<=0)return;
    if(onlineResult.blocked){onlineResultClock.reset();return;}
    onlineResultClock.advance(dt,[&]{
        const auto page=onlineResult.page;
        const auto cue=onlineResult.advance(false);
        if(cue)audio.playOriginalMenuCue(cue);
        if(page!=onlineResult.page)materializeOnlineResultPage();
        if(onlineResult.page==1&&page==1){
            ++onlineResultPoints.frame60;
            onlineResultPoints.points[4]=onlineResult.pointsFrame.displayedBalance;
            onlineResultPoints.balanceHighlighted=onlineResult.pointsFrame.highlightBalance;
            onlineResultPoints.balanceVisible=onlineResult.pointsFrame.balanceVisible;
            for(unsigned i=0;i<onlineResult.pointsFrame.cueCount;++i)audio.playTuningCue(onlineResult.pointsFrame.cueIds[i]);
        }
        if(onlineResultPreview){
            onlineResultPreview->consume({},onlineResultProfile,*tuningTables,original::OriginalTuningChildKind::none);
            audio.tickResultMusic();
        }
    });
}
bool renderOnlineResult(){
    if(!onlineResultPreview)throw std::logic_error("Online result preview is missing");
    auto& preview=*onlineResultPreview;
    if(!onlineResultTextures){if(!preview.upload(renderer))return false;onlineResultTextures=true;texturesPending=true;}
    const int width=renderer.width,height=renderer.height;
    const float fit=std::min(width/640.f,height/480.f),left=(width-640*fit)*.5f,top=(height-480*fit)*.5f;
    const std::uint32_t* overlay=nullptr;
    // These captions belong to the remake's online flow. Keep their language
    // independent of the original arcade artwork selected for the panels.
    const int language=customMenuLanguage();
    const bool localized=language&&frontend.localizedMenuFonts[language-1].ready();
    const auto& captionFont=localized?frontend.localizedMenuFonts[language-1]:onlineResultFont;
    const auto tr=[&](std::string_view text){return std::string(localized?customMenuText(text):text);};
    auto caption=[&](std::span<std::uint32_t> canvas,const std::string& text,float x,float y,float size){
        const float measured=captionFont.width(text,size),available=616.f-x;
        if(measured>available)size*=available/measured;
        captionFont.paint(canvas,width,height,text,left+x*fit,top+y*fit,size*fit,0xffffffff,0xff000000,fit);
    };
    if(onlineResult.page==1){
        hud.resize(width,height);overlay=hud.paintResult(onlineResultPoints,{},false,false,true);
        auto canvas=std::span<std::uint32_t>(const_cast<std::uint32_t*>(overlay),std::size_t(width)*height);
        caption(canvas,tr("BATTLE LEVEL")+" "+std::to_string(onlineResultLevel)+"   "+(onlineResultBattlePoints>=0?"+":"")+std::to_string(onlineResultBattlePoints)+" "+tr("PTS"),24,463,12);
        if(onlineResult.recordFailed)caption(canvas,tr("BATTLE RECORD COULD NOT BE SAVED"),24,4,12);
        if(onlineResultPoints.draw)caption(canvas,tr("DRAW"),432,38,28);
    }else{
        onlineResultPixels.assign(std::size_t(width)*height,0u);unityUiClear(onlineResultPixels.data(),width,height);
        onlineContinueArtwork.paintChoice(onlineResultPixels,width,height,original::OriginalLegendChoiceKind::Continue,
            unsigned(onlineResult.selected),onlineResult.countdown,onlineResult.confirming&&onlineResult.decision==0,onlineResult.confirmFrames);
        const char* status=onlineResult.peerLeft?"THE OTHER DRIVER LEFT":onlineResult.decision==1?"WAITING FOR THE OTHER DRIVER...":onlineResult.peerReady?"OPPONENT READY":"";
        if(*status)caption(onlineResultPixels,tr(status),24,432,18);
        if(onlineResult.decision==1)caption(onlineResultPixels,tr("ESC / B  LEAVE"),24,457,13);
        overlay=onlineResultPixels.data();
    }
    renderer.screenFadeArgb=onlineResult.page==1?onlineResult.pointsFrame.fadeAlpha<<24:0;
    renderer.sceneViewport={};renderer.fitOriginalViewport=true;
    renderer.overrideClearColor=true;renderer.clearColor={0,0,0,1};renderer.cameraUp={0,1,0};
    renderer.projectionAspect=preview.aspect;renderer.verticalFieldOfView=preview.verticalFieldOfView;
    renderer.nearClip=preview.nearClip;renderer.farClip=preview.farClip;
    renderer.vehicleLights=false;renderer.opponentLights=false;renderer.courseLampPositions.clear();
    renderer.courseFog=nullptr;renderer.courseLighting=nullptr;renderer.playerLighting=nullptr;renderer.rivalLighting=nullptr;
    return renderer.draw(preview.mesh(),preview.eye,preview.target,false,false,overlay,false,&preview.lighting);
}
