#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("game-root required");
    App app;app.root=argv[1];app.validationMode=true;app.settings();
    app.originalCamera=OriginalChaseCamera::load(app.root);app.bumperCamera=OriginalChaseCamera::load(app.root,OriginalDrivingView::Bumper);
    app.frontend.initialize(app.root,true);app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
    unsigned wraps=0,poses=0,frames=0;
    for(unsigned enemy:{8u,13u,25u,30u}){
        app.frontend.car=1;app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();app.frontend.battleProfile.setu(16,1);app.frontend.battleProfile.setu(0,0);
        original::selectOriginalRival(app.frontend.battleProfile,enemy);app.start();
        unsigned localWraps=0;
        for(unsigned tick=0;tick<7200;++tick){DriverInput input;input.automatic=true;input.throttle=.7f;app.simulate(input);++frames;
            const auto& a=app.previousRivalWheels;const auto& b=app.rivalWheels;
            const float arc=wrapAngle(b.steeringRadians-a.steeringRadians);
            if(std::abs(b.steeringRadians-a.steeringRadians)>pi){++wraps;++localWraps;}
            for(float alpha:{.25f,.5f,.75f}){const auto wheel=interpolateCarWheels(a,b,alpha);
                const float movement=wrapAngle(wheel.steeringRadians-a.steeringRadians);
                if(!std::isfinite(movement)||std::abs(movement-arc*alpha)>1e-5f)throw std::runtime_error("Rival wheel turns through the long arc");
                ++poses;
            }
        }
        std::cout<<"enemy "<<enemy<<": "<<localWraps<<" live steering wraps\n";
    }
    if(!wraps)throw std::runtime_error("No real rival steering wraps were exercised");
    std::cout<<"PASS "<<frames<<" live rival frames, "<<wraps<<" steering wraps and "<<poses<<" short-arc steering poses.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
