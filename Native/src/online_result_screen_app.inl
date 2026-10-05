// Included inside App. All artwork below is rendered by the existing native
// result/Continue painters and captured by Unity's normal scene UI pipeline.
OnlineResultScreen onlineResult;
FixedClock onlineResultClock;
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
    onlineResultTextures=false;onlineResultBattlePoints=0;onlineResultLevel=1;
}
void beginOnlineResult(){
    onlineResultProfile=battleProfile;
    if(multiplayer.rewardSelection>=0){
        original::OriginalBattleProfile saved;
        if(onlineCarProfile(multiplayer.rewardSelection,saved))onlineResultProfile=saved;
    }
    onlineResultProfile.setu(16,multiplayer.config.localCar);
    onlineResult.begin(multiplayer.pointsEarned,onlineResultProfile.u(72));onlineResultClock.reset();
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
void advanceOnlineResult(double dt){
    if(onlineResult.page<0)return;
    if(onlineResult.blocked){onlineResultClock.reset();return;}
    onlineResultClock.advance(dt,[&]{
        const auto page=onlineResult.page;
        const auto cue=onlineResult.advance(audio.raceMusicFinished());
        if(cue)audio.playOriginalMenuCue(cue);
        if(page!=onlineResult.page){
            if(!tuningTables)tuningTables=original::OriginalTuningData::load(root);
            onlineResultPreview=std::make_unique<original::OriginalTuningPreviewPresentation>();
            onlineResultPreview->load(root,onlineResultProfile,false,onlineResult.page==2);
            onlineResultTextures=false;texturesPending=true;
            if(!onlineResultFont.ready())onlineResultFont=MenuFont::load(root);
            if(onlineResult.page==1)audio.beginResultMusic(true);
            else if(!onlineContinueArtwork.loaded())onlineContinueArtwork.load(root);
        }
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
    auto caption=[&](std::span<std::uint32_t> canvas,const std::string& text,float x,float y,float size){
        onlineResultFont.paint(canvas,width,height,text,left+x*fit,top+y*fit,size*fit,0xffffffff,0xff000000,fit);
    };
    if(onlineResult.page==1){
        hud.resize(width,height);overlay=hud.paintResult(onlineResultPoints,{},false,false,true);
        auto canvas=std::span<std::uint32_t>(const_cast<std::uint32_t*>(overlay),std::size_t(width)*height);
        caption(canvas,"BATTLE LEVEL "+std::to_string(onlineResultLevel)+"   "+(onlineResultBattlePoints>=0?"+":"")+std::to_string(onlineResultBattlePoints)+" PTS",24,463,12);
        if(onlineResult.recordFailed)caption(canvas,"BATTLE RECORD COULD NOT BE SAVED",24,4,12);
        if(onlineResultPoints.draw)caption(canvas,"DRAW",432,38,28);
    }else{
        onlineResultPixels.assign(std::size_t(width)*height,0u);unityUiClear(onlineResultPixels.data(),width,height);
        onlineContinueArtwork.paintChoice(onlineResultPixels,width,height,original::OriginalLegendChoiceKind::Continue,
            unsigned(onlineResult.selected),onlineResult.countdown);
        const char* status=onlineResult.peerLeft?"THE OTHER DRIVER LEFT":onlineResult.decision==1?"WAITING FOR THE OTHER DRIVER...":onlineResult.peerReady?"OPPONENT READY":"";
        if(*status)caption(onlineResultPixels,status,24,432,18);
        if(onlineResult.decision==1)caption(onlineResultPixels,"ESC / B  LEAVE",24,457,13);
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
