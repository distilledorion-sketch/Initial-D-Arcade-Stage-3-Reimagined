//Actual App owner, deterministic clock injection and private storage only;
//no assets/window/audio/network. Separate numerical sessions retain defaults.
#define IDAS3_UNITY_PLUGIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "../src/main.cpp"
#include <iostream>
int main(int argc,char**argv)try{
    if(argc!=2)throw std::invalid_argument("Private fixture directory required");
    unsigned checks=0;const auto require=[&](bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);};
    original::OriginalDrivingSession independent;
    require(independent.sharedRandomSeed()==1,"Numerical caller default seed changed");
    for(unsigned fps:{30u,60u,144u,240u}){
        auto app=std::make_unique<App>();app->validationMode=true;
        app->injectPlatformClock(42,950);
        require(!app->originalSession.ready()&&app->originalSession.sharedRandomSeed()==42,"App did not seed before first menu");
        app->originalSession.withSharedRandom([](auto& seed){original::selectOriginalRivalSpectator(seed);});
        const auto selected=app->originalSession.sharedRandomSeed();
        for(unsigned frame=0;frame<=fps*5;++frame){
            app->paused=frame%2!=0;app->loadingActive=frame%3!=0;
            app->commands(1./fps,950+std::uint64_t(frame)*1000/fps);
        }
        require(app->platformClock.current()==47&&app->platformClock.ticks()==298,"App menu/pause skipped original-rate platform maintenance");
        require(app->originalSession.sharedRandomSeed()==selected,"Platform maintenance reseeded running shared RNG");
        app->injectPlatformClock(999,999999);
        require(app->originalSession.sharedRandomSeed()==selected,"Repeated App activation rewound menu RNG");
        app->multiplayer.active=true;app->multiplayer.waiting=true;
        app->commands(1,6950);
        require(app->platformClock.current()==48,"Online waiting stopped local platform clock");
        require(app->originalSession.sharedRandomSeed()==selected,"Online clock tick mutated agreed driving state");
        require(independent.sharedRandomSeed()==1,"App clock crossed numerical-session boundary");
    }
    const auto privateRoot=fs::absolute(argv[1])/std::to_string(NativePlatformClock::hostMilliseconds());
    {
        auto app=std::make_unique<App>();app->validationMode=true;app->saveRoot=privateRoot;
        app->activatePlatformClock(0);
        require(!app->originalSession.ready()&&app->originalSession.sharedRandomSeed()==0,"Production App activation did not seed cleared storage before menus");
        app->originalSession.withSharedRandom([](auto& seed){original::selectOriginalRivalSpectator(seed);});
        const auto selected=app->originalSession.sharedRandomSeed();
        app->activatePlatformClock(900000);
        require(app->originalSession.sharedRandomSeed()==selected,"Repeated production activation reset shared RNG");
        app->multiplayer.active=true;app->multiplayer.waiting=true;
        app->commands(5,5000);app->saveSettings();
        require(app->platformClock.current()==4,"Production App did not publish clock while online");
        require(app->originalSession.sharedRandomSeed()==selected,"Production platform tick reseeded agreed online state");
        require(!fs::exists(privateRoot/"settings.txt"),"Online saveSettings fixture did not take early return");
        //No explicit flush: App destruction must drain despite online save exit.
    }
    {
        auto app=std::make_unique<App>();app->validationMode=true;app->saveRoot=privateRoot;
        app->activatePlatformClock(250000);
        require(app->originalSession.sharedRandomSeed()==4,"Fresh App did not seed from committed prior powered-on time");
        app->paused=true;app->commands(3,253000);
        require(app->platformClock.current()==6,"Fresh App counted offline time or skipped paused platform updates");
        require(app->originalSession.sharedRandomSeed()==4,"Fresh App maintenance reseeded canonical RNG");
    }
    {NativePlatformClock committed;require(committed.activate(privateRoot,900000)==6,"App destruction lost latest paused publication");}
    require(independent.sharedRandomSeed()==1,"Production App activation mutated standalone session");
    std::cout<<"PASS "<<checks<<" actual App platform clock checks; production activation/private persistence, activation before menus, retained stream, online shutdown drain, pause/loading/online maintenance, original rate and isolated seeds. No window/audio/network opened. Private artifacts: "<<privateRoot.string()<<'\n';
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
