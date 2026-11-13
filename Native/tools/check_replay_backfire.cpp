#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>

int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and NEW evidence directory required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    unsigned checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    auto& f=app->frontend;f.car=19;f.course=3;f.night=true;f.wet=false;f.gameMode=original::OriginalGameMode::TimeAttack;
    f.battleProfile=original::makeOriginalFreshBattleProfile();f.battleProfile.setu(16,19);
    f.battleProfile.setByte(162,1);f.battleProfile.setByte(166,1);
    app->courseIndex=3;app->night=true;app->wet=false;app->replayRecordingFlags=1;app->demoPreparationStarted=true;app->start();
    app->vsActive=false;app->loadingActive=false;app->race.phase=RacePhase::Running;
    app->audio.selectOriginalEngine(root,f.battleProfile);
    auto gains=app->audio.outputGains();gains.master=gains.effects=0;app->audio.setOutputGains(gains);
    unsigned seed=123,flashes=0;std::vector<unsigned> expected;
    original::OriginalEngineControlInput input;input.rpm=7000;input.gear=3;
    for(unsigned tick=0;tick<180;++tick){
        input.throttle=tick%90<60?1.f:0.f;app->audio.stepOriginalEngine(input,seed);app->audio.finishSoundFrame(seed);
        const auto phase=unsigned(app->audio.backfireFrame()+1);flashes+=phase!=0;expected.push_back(phase);
        app->race.ticks=tick+1;app->race.elapsed6000=(tick+1)*100;app->captureReplayFrame();
    }
    check(flashes>0,"Original engine controller did not accept a flash");
    const auto raw=app->recording.sharedBytes(18000);
    check(raw.size()>=96,"Ordinary capture incomplete");
    const auto word=[&](unsigned at){unsigned value;std::memcpy(&value,raw.data()+at,4);return value;};
    check(word(24)==4,"Ordinary replay does not preserve accepted backfire states");
    check(word(16)==100&&word(20)==168&&word(8)==180,"Backfire replay layout");
    for(unsigned tick=0;tick<180;++tick)check(word(100+tick*168+164)==expected[tick],"Recorded flash differs from accepted live cue");
    std::ofstream file(out/"accepted.idr2",std::ios::binary);file.write(reinterpret_cast<const char*>(raw.data()),raw.size());
    std::cout<<"PASS checks="<<checks<<" recorded_frames="<<expected.size()<<" flash_frames="<<flashes<<"; original engine cues, muted capture.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
