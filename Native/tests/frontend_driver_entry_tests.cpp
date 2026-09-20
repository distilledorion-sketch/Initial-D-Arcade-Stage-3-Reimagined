#include "frontend.h"
#include <iostream>
#include <stdexcept>
using namespace idas3;using namespace idas3::original;
namespace {
unsigned checks=0;
void check(bool x,const char* s){++checks;if(!x)throw std::runtime_error(s);}
void tick(Frontend& f,unsigned count=1){for(unsigned n=0;n<count;++n)f.advance(1./60.);}
void finishTransmission(Frontend& f){
    f.stage=FrontendStage::Transmission;f.advance(0);tick(f,16);check(f.inputReady(),"transmission entry");
    f.confirm();tick(f,141);check(f.stage==FrontendStage::Transmission,"transmission early exit");tick(f);
}
void finishPackage(Frontend& f){
    check(f.stage==FrontendStage::TuningCourse,"source intermediate package stage missing");
    tick(f,8);check(f.inputReady(),"tuning entry fade");
    const auto saved=f.battleProfile.words;f.advance(0);f.back();check(f.stage==FrontendStage::TuningCourse&&saved==f.battleProfile.words,"tuning back invented profile mutation");
    if(f.car==0){
        const auto frame=f.tuningCourseState().frame492;const auto a=f.paint(640,480);
        check(a==f.paint(640,480),"paused tuning repaint changed original artwork");
        check(frame==f.tuningCourseState().frame492&&saved==f.battleProfile.words,"tuning paint advanced source state");
        unsigned visible=0;for(auto pixel:a)if(pixel&0x00ffffffu)++visible;
        check(visible>1000,"original tuning artwork missing");
    }
    f.confirm();tick(f,137);check(f.stage==FrontendStage::TuningCourse,"tuning early exit");tick(f);
    check(f.stage==FrontendStage::Name,"tuning did not advance to source Name child");
    check(f.takeDriverProfileCommit()&&!f.takeDriverProfileCommit(),"package commit event count");
}
void finishImportedName(Frontend& f,bool visual){
    check(f.stage==FrontendStage::Name,"name stage missing");tick(f,16);check(f.inputReady(),"name fade");
    f.confirm();tick(f);check(f.takeDriverProfileCommit()&&!f.takeDriverProfileCommit(),"name commit should emit once");
    check(!f.takeDriverSetupCompleted(),"setup completed before name owner exit");
    const auto profile=f.battleProfile.words;const auto frame=f.nameEntryState().frame572;
    f.advance(0);f.confirm();f.back();f.change(1);f.advance(0);
    check(f.battleProfile.words==profile&&f.nameEntryState().frame572==frame,"pause or repeated input advanced committed name");
    if(visual){const auto a=f.paint(640,480);check(a==f.paint(640,480),"paused repaint changed original name artwork");}
    tick(f,50);check(f.stage==FrontendStage::Name&&!f.takeDriverSetupCompleted(),"setup exit early");tick(f);
    check(f.stage==FrontendStage::Mode,"source name parent did not reach native card-completion boundary");
    check(f.takeDriverSetupCompleted()&&!f.takeDriverSetupCompleted(),"setup-complete event not exactly once");
    check(f.takeDriverProfileCommit()&&!f.takeDriverProfileCommit(),"final flags need a distinct profile save");
    check((f.battleProfile.u(1180)&3u)==1u,"source setup flag did not become accepted flag");
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Usage: frontend_driver_entry_tests game_root");
    Frontend f;f.initialize(argv[1]);
    for(unsigned car=0;car<35;++car){
        f.car=int(car);f.battleProfile=makeOriginalFreshBattleProfile();f.battleProfile.setu(16,car);
        f.battleProfile.setu(1180,0x81);requestOriginalDriverSetup(f.battleProfile);f.battleProfile.setByte(1192,2);
        f.battleProfile.setu(72,134567);f.battleProfile.setByte(153,5);f.battleProfile.setByte(164,6);f.battleProfile.setByte(156,1);
        f.battleProfile.setu(76,1);f.battleProfile.setu(44,17);
        for(unsigned j=1;j<5;++j)f.battleProfile.setu(44+4*j,0xdead0000+j);
        const auto before=f.battleProfile;
        finishTransmission(f);
        if(car==29)check(f.stage==FrontendStage::Name,"car29 should skip tuning course");else finishPackage(f);
        finishImportedName(f,car==0);
        check(f.battleProfile.u(72)==before.u(72),"migration lost points");
        for(unsigned j=153;j<172;++j)check(f.battleProfile.byte(j)==before.byte(j),"migration reset earned tuning or appearance");
        for(unsigned j=0;j<5;++j)check(f.battleProfile.u(44+4*j)==before.u(44+4*j),"migration changed active name or inactive original tail");
        check(f.battleProfile.u(76)==1,"migration name length changed");
        // Explicit native metadata would now be Complete. Its source adapter
        // chooses ordinary kind0 and leaves setup mask2 clear.
        f.battleProfile.setByte(1192,0);finishTransmission(f);
        check(f.stage==FrontendStage::Mode&&!f.takeDriverSetupCompleted(),"established driver repeated setup");
    }
    // A fresh blank profile takes kind0, so five ordinary confirmed glyphs
    // automatically move the actual original selector to END.
    f.car=0;f.battleProfile=makeOriginalFreshBattleProfile();requestOriginalDriverSetup(f.battleProfile);
    finishTransmission(f);finishPackage(f);tick(f,16);
    for(unsigned j=0;j<5;++j){f.confirm();tick(f);check(f.nameEntryState().length528==j+1,"fresh source glyph entry");}
    check(f.stage==FrontendStage::Name&&!f.takeDriverSetupCompleted(),"typing glyphs completed setup");
    // Already past Name entry fade: commit END and check owner exit directly.
    f.confirm();tick(f);check(f.takeDriverProfileCommit(),"fresh name END did not commit");tick(f,51);
    check(f.stage==FrontendStage::Mode&&f.takeDriverSetupCompleted(),"fresh driver did not finish complete setup flow");
    f.takeDriverProfileCommit();
    // Natural source timeouts: no fabricated name or direct phase assignment.
    f.car=0;f.battleProfile=makeOriginalFreshBattleProfile();requestOriginalDriverSetup(f.battleProfile);
    finishTransmission(f);check(f.stage==FrontendStage::TuningCourse,"timeout starts at tuning");tick(f,1824);
    check(f.stage==FrontendStage::Name,"package timeout routing");f.takeDriverProfileCommit();
    tick(f,16+4879+51);check(f.stage==FrontendStage::Mode&&f.takeDriverSetupCompleted(),"source name timeout completion");
    check(f.battleProfile.u(76)>0&&f.battleProfile.u(76)<=5,"source name timeout default length");
    check(!f.takeDriverSetupCompleted(),"timeout completion replayed");
    std::cout<<"PASS frontend driver setup: "<<checks<<" checks;35 migrated/reloaded cars, fresh input, source timeouts and inert repaint. No saved-driver writes.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

