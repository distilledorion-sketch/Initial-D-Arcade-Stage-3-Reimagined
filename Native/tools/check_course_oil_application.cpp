#include "original_course_oil.h"
namespace idas3 {
inline bool suppressOilFixture=false;
inline unsigned oilCalls=0,oilDraws=0;
inline std::optional<NativeAssemblyInsertion> checkedOilInsertion(const NativeAssembly& base,unsigned course,bool night,bool wet,unsigned opponent){
    ++oilCalls;auto result=originalCourseOilInsertion(base,course,night,wet,opponent);
    if(suppressOilFixture)return std::nullopt;
    oilDraws+=bool(result);return result;
}
}
#define originalCourseOilInsertion checkedOilInsertion
#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#undef originalCourseOilInsertion
#include <iostream>

int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native asset root and NEW evidence directory required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    unsigned checks=0;auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);app->audio.enabled=false;
    check(app->renderer.initialize(nullptr,1200,800,true),"Offscreen renderer failed");
    const Vec3 oilCenter{654,534,1408};
    for(unsigned scenario=0;scenario<15;++scenario){
        auto& profile=app->frontend.battleProfile;profile=original::makeOriginalFreshBattleProfile();
        app->frontend.car=0;app->automatic=app->frontend.automatic=true;
        app->courseIndex=7;app->reverse=scenario==3;app->night=scenario==1;app->wet=scenario==2;
        app->frontend.gameMode=scenario<4?original::OriginalGameMode::TimeAttack:original::OriginalGameMode::LegendOfTheStreets;
        if(scenario>=13){
            app->frontend.gameMode=original::OriginalGameMode::TimeAttack;
            profile.setu(0,1);profile.setu(24,24+scenario-13);
        }
        else if(scenario>=10){
            app->frontend.gameMode=original::OriginalGameMode::BuntaChallenge;
            profile.setu(0,2);profile.setu(1080+7*4,(scenario-10)*6);
            original::selectOriginalBuntaCourse(profile,7);
        }
        else if(scenario>=4){original::selectOriginalRival(profile,24+scenario-4);profile.setu(32,0);}
        app->start();app->menu=false;app->loadingActive=app->preRaceDialogueActive=app->vsActive=false;
        const auto projection=app->course.project(oilCenter);
        app->progress=projection.sample.distance;app->courseLightPathIndex=int(projection.segment);
        const bool expected=scenario<10&&scenario!=2&&scenario!=4&&scenario!=5;
        const auto digest=app->presentedSession().rollbackDigest();
        Mesh before,after;suppressOilFixture=true;app->scenery(before,app->progress);
        suppressOilFixture=false;oilCalls=oilDraws=0;app->scenery(after,app->progress);
        check(oilCalls==1&&oilDraws==unsigned(expected),"Actual App oil admission differs from race rules");
        check((after.vertices.size()>before.vertices.size())==expected,"Actual App oil geometry missing or added to excluded race");
        check(app->presentedSession().rollbackDigest()==digest,"Oil drawing changed simulation state");
        if(scenario<4){
            check(app->renderer.loadTextures(app->originalCourseTextures),"Course textures failed");
            app->renderer.courseLighting=&*app->raceLighting;app->renderer.courseFog=&app->raceFog;
            app->renderer.vehicleLights=false;app->renderer.opponentLights=false;
            app->renderer.overrideClearColor=true;app->renderer.clearColor={.25f,.35f,.45f,1};
            const auto target=projection.sample.center+Vec3{0,.5f,0};
            const auto eye=target-projection.sample.tangent*23.f+Vec3{0,9,0};
            for(auto [mesh,label]:{std::pair{&before,"before"},std::pair{&after,"after"}}){
                for(auto& range:mesh->ranges)range.courseLighting=true;
                check(app->renderer.draw(*mesh,eye,target,app->night,app->wet),"Oil scene draw failed");
                check(app->renderer.saveBitmap((out/("case-"+std::to_string(scenario)+"-"+label+".bmp")).wstring()),"Oil scene capture failed");
            }
        }
        std::cout<<"scenario="<<scenario<<" enabled="<<oilDraws<<" added vertices="<<after.vertices.size()-before.vertices.size()<<" path="<<projection.segment<<'\n';
    }
    for(unsigned version:{1u,2u})for(unsigned previousEnemy:{24u,25u}){
        auto local=original::makeOriginalFreshBattleProfile(),remote=original::makeOriginalFreshBattleProfile();
        local.setu(24,previousEnemy);
        Idas3MultiplayerConfig config{sizeof(config),version,7,0,0,0,0,1,0,1};
        app->startMultiplayer(config,&local,&remote);
        check(app->originalSession.selection().roadModeOverride==0&&app->originalSession.parameters().road.mode0C9015E0==0,
            "Initial/legacy online App omitted explicit road policy");
        if(version==2)app->enableAuthority(700000+previousEnemy,true,false);
        check(app->presentedSession().selection().roadModeOverride==0&&app->presentedSession().parameters().road.mode0C9015E0==0,
            "Active online App changed its oil policy");
        check(app->presentedSession().selection().physics.vehicleMode0C9015E0==previousEnemy,"Online App oil policy changed profile selection");
        const auto projection=app->course.project(oilCenter);app->courseLightPathIndex=int(projection.segment);
        Mesh scenery;oilCalls=oilDraws=0;app->scenery(scenery,projection.sample.distance);
        check(oilCalls==1&&oilDraws==1,"Online App lost oil after a saved excluded opponent");
        app->leaveMultiplayer();
    }
    std::ofstream(out/"result.txt")<<"PASS "<<checks<<" actual App oil checks; 4 camera comparisons, 6 Legend opponents, 3 Bunta tiers, 2 retained-opponent solo cases, legacy/version2 online; isolated saves; simulation unchanged by rendering.\n";
    std::cout<<"PASS "<<checks<<" actual App oil checks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
