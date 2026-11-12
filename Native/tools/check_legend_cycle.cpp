#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>

int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and NEW evidence directory required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");
    fs::create_directories(out);
    unsigned checks=0,cases=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";
    app->settings();app->frontend.initialize(root,true);
    // No audio device, native window or online service is opened by this test.
    const auto make=[&](unsigned car,unsigned cycles,unsigned wins,unsigned bunta){
        auto p=original::makeOriginalFreshBattleProfile();p.setu(16,car);
        original::applyOriginalAcceptedCardFlag(p);original::finishOriginalDriverSetupFlag(p);
        p.setu(72,345678);p.setu(76,5);p.setu(64,2);p.setu(68,1);
        for(unsigned i=0;i<5;++i)p.setu(44+i*4,164+i);
        for(unsigned i=152;i<192;++i)p.setByte(i,std::uint8_t(i*3));
        p.setu(476,63);p.setu(496,21);p.setu(480,24);
        p.setu(1080,12);p.setu(1108,7);p.setu(148,cycles);
        for(unsigned e=0;e<30;++e)p.setByte(116+e,std::uint8_t((wins<<4)|(e%15)));
        p.setByte(146,std::uint8_t(bunta));p.setByte(147,0xa5);
        return p;
    };
    const auto seed=[&](unsigned slot,unsigned car,const original::OriginalBattleProfile& p){
        const auto dir=app->saveSlots.profileDirectory(slot);
        check(LocalDriverProfiles(dir).save(car,p),"Seed profile");
        check(LocalDriverSetup(dir).markComplete(car),"Seed setup");
        check(app->saveSlots.adopt(slot,p),"Seed slot");
    };
    const auto other=make(7,2,2,0x20);
    seed(1,7,other);seed(0,7,other);
    for(unsigned cycle:{0u,1u,6u,7u})for(unsigned wins:{1u,2u,15u})for(unsigned bunta:{1u,0x10u,0x20u}){
        auto p=make(0,cycle,wins,bunta);seed(0,0,p);
        // Merely previewing the car, refreshing its records or looking it up
        // for an online lobby must not consume the completed story cycle.
        app->useSaveSlot(0);app->frontend.car=0;app->frontend.changingSavedCar=false;
        app->loadSelectedProfile();
        check(app->frontend.battleProfile.words==p.words,"Preview changed completed story");
        original::OriginalBattleProfile online;
        check(app->onlineCarProfile(0,online)==1&&online.words==p.words,"Online lookup changed completed story");
        check(app->profiles.load(0).profile.words==p.words,"Preview persisted a reset");
        app->openSaveFile(0);
        const auto after=app->frontend.battleProfile;
        check(app->frontend.stage==FrontendStage::SavedData,"Saved Data was bypassed");
        check(after.u(148)==std::min(cycle+1,7u),"Cycle did not advance/saturate");
        for(unsigned e=0;e<31;++e)check(after.byte(116+e)==0,"Rival result retained");
        check((after.u(1180)&0x0800c000u)==0x0800c000u,"Unlocked rivals disappeared");
        for(unsigned offset=0;offset<1228;++offset){
            if((offset>=80&&offset<147)||(offset>=148&&offset<152)||(offset>=1180&&offset<1184))continue;
            check(after.byte(offset)==p.byte(offset),"Non-Legend profile data changed");
        }
        auto selection=after;original::selectOriginalRival(selection,0);
        check(selection.u(32)==0,"New cycle did not restore dry rival weather");
        check(app->profiles.load(0).profile.words==after.words,"Reset was not persisted");
        check(app->profiles.load(7).profile.words==other.words,"Other car was reset");
        check(LocalDriverProfiles(app->saveSlots.profileDirectory(1)).load(7).profile.words==other.words,"Other slot was reset");
        // Another read can recompute menu defaults, but must not award a cycle.
        app->openSaveFile(0);check(app->frontend.battleProfile.u(148)==after.u(148),"Repeat load awarded a cycle");
        const auto stable=app->frontend.battleProfile.words;
        app->openSaveFile(0);check(app->frontend.battleProfile.words==stable,"Repeated card read not stable");
        ++cases;
    }
    for(unsigned missing=0;missing<31;++missing){
        auto p=make(0,0,2,0x20);p.setByte(116+missing,missing==30?0:7);seed(0,0,p);
        app->openSaveFile(0);const auto& after=app->frontend.battleProfile;
        check(after.u(148)==0,"Incomplete story was reset");
        for(unsigned e=0;e<31;++e)check(after.byte(116+e)==p.byte(116+e),"Incomplete rival data changed");
        ++cases;
    }
    // Changing cars uses a preview until the new car is confirmed.
    auto p=make(7,3,2,0x20);seed(0,7,p);seed(0,0,make(0,0,0,0));
    app->openSaveFile(0,true);app->frontend.car=7;app->loadSelectedProfile();
    check(app->profiles.load(7).profile.words==p.words,"Change-car preview reset progress");
    check(app->finishSavedCarSelection(),"Change-car confirmation failed");
    check(app->profiles.load(7).profile.u(148)==4&&app->profiles.load(7).profile.byte(116)==0,"Confirmed car did not start its next cycle");
    // Reload through a fresh store, as on another process launch.
    const auto persisted=LocalDriverProfiles(app->saveSlots.profileDirectory(0)).load(7);
    check(persisted.origin==LocalDriverProfiles::Origin::Saved&&persisted.profile.u(148)==4,"Cycle lost on reload");
    std::ofstream(out/"PASS.txt")<<checks<<" checks; "<<cases<<" saved-driver cases; cycle cap, all 31 incomplete boundaries, dry selection, byte-preserved car/tuning/points/rank, other cars/slots, preview/online read-only behavior, confirmed car change, persistence. No window/audio/online connection.\n";
    std::cout<<"PASS "<<checks<<" Legend cycle application checks, "<<cases<<" saved-driver cases\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
