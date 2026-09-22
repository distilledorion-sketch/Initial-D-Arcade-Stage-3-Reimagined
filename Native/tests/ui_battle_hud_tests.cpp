#include "unity_ui_capture.h"
#include "ui.h"
#include "frontend.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

// This isolated HUD check never enters the frontend. Keep its dependency at
// the existing public boundary instead of compiling the active menu owner.
namespace idas3 {
const std::vector<std::uint32_t>& Frontend::paint(int,int){throw std::logic_error("Unexpected frontend in battle HUD test");}
}
namespace {
void save(const std::filesystem::path& path,std::vector<std::uint32_t> pixels,int w,int h){
    for(auto& p:pixels)if(!(p>>24))p=0xff26384a;
    BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+std::uint32_t(pixels.size()*4);
    BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=w;info.biHeight=-h;info.biPlanes=1;info.biBitCount=32;
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof(file));out.write(reinterpret_cast<const char*>(&info),sizeof(info));out.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()*4));
}
}
int main(int argc,char** argv)try{
    if(argc!=3&&argc!=4)throw std::invalid_argument("nativeRoot outputDirectory");
    using namespace idas3;
    const std::filesystem::path output=argv[2];std::filesystem::create_directories(output);
    constexpr int w=1280,h=720;constexpr std::size_t n=w*h;
    Course course;course.name="Akina";course.length=1000;
    VehicleState car;car.speed=27.5f;car.rpm=6700;car.gear=3;
    RaceClock race;race.phase=RacePhase::Running;race.ticks=300;race.originalTiming=true;race.elapsed6000=30000;race.remaining6000=480000;
    UiState s;s.menu=false;s.battle=true;s.course=&course;s.car=&car;s.race=&race;s.battleEnemy=13;s.battleRivalPositionFraction=.2f;s.battleAdvantage=-25.3f;
    Hud hud;hud.loadOriginal(argv[1]);hud.resize(w,h);
    std::vector<std::uint32_t> first;
    for(int frame=41;frame<103;++frame){
        s.battleHudFrame=frame;const auto pixels=hud.paint(s);std::vector<std::uint32_t> expected(pixels,pixels+n);
        if(frame==41){first=expected;save(output/"battle-hud-portrait.bmp",expected,w,h);}
        for(int repeated=0;repeated<3;++repeated){const auto next=hud.paint(s);if(!std::equal(expected.begin(),expected.end(),next))throw std::runtime_error("Repeated simulation frame changed HUD pixels");}
    }
    s.battleHudFrame=41;const auto reset=hud.paint(s);if(!std::equal(first.begin(),first.end(),reset))throw std::runtime_error("New battle did not reset original blink state");
    s.battleEnemy=30;s.battleAdvantage=128.7f;const auto bunta=hud.paint(s);save(output/"battle-hud-other-portrait.bmp",std::vector<std::uint32_t>(bunta,bunta+n),w,h);
    // Actual online UiState has no Legend/Bunta owner. Verify the port's
    // existing source-profile3 commands reach the live compositor anyway.
    const auto nameBank=OriginalBattleNames::load(argv[1]);
    const auto battleBank=OriginalBattleHudAssets::load(argv[1]);
    unsigned onlineChecks=0;
    const auto require=[&](bool value,const char* message){++onlineChecks;if(!value)throw std::runtime_error(message);};
    const auto local=nameBank.onlineName("virus",true,32),remote=nameBank.onlineName("DUSK",false,29);
    require(local.text=="VIRUS"&&local.carCode=="CP9A Evo.V"&&remote.carCode=="GC8V","Original online font/code identity mismatch");
    require(local.glyphs.size()==5&&local.glyphs[0].x==510.f&&local.glyphs[0].y==130.f&&local.glyphs[0].width==12.f,"Original profile3 DRIVER placement changed");
    require(remote.glyphs.size()==4&&remote.glyphs[0].x==510.f&&remote.glyphs[0].y==83.f&&remote.glyphs[0].width==12.f,"Original profile3 OPPONENT placement changed");
    const auto longName=nameBank.onlineName(std::string(32,'W'),true,8);
    require(longName.text.size()==32&&longName.glyphs.size()==32,"Long peer name lost characters");
    for(const auto& glyph:longName.glyphs)require(glyph.x>=510.f&&glyph.x+glyph.width<=572.001f&&glyph.height==glyph.width,"Peer name overlaps original car code or stretches glyphs");
    require(nameBank.onlineName("a\xf0\x9f\x8f\x8e",false,0).text=="A?","Unsupported Unicode did not use source question glyph");
    require(nameBank.onlineName("\xc0\xaf",false,0).text=="??","Malformed UTF8 was not bounded");
    for(unsigned carId=0;carId<35;++carId)require(!nameBank.onlineName("TEST",true,carId).carCode.empty(),"Missing original car-code label");
    UiState online=s;online.battle=false;online.multiplayer=true;online.rearView=true;online.onlineBattleHud.active=true;
    auto& peer=online.onlineBattleHud;peer.playerName="SMOKE HOST";peer.rivalName="SMOKE JOIN";peer.playerCar=0;peer.rivalCar=8;peer.rivalPositionFraction=.25f;
    Course mapCourse;mapCourse.name="Akina";mapCourse.length=1000;mapCourse.points={{0,0,0},{0,0,1000}};
    mapCourse.left={{-5,0,0},{-5,0,1000}};mapCourse.right={{5,0,0},{5,0,1000}};mapCourse.cumulative={0,1000};
    online.course=&mapCourse;VehicleState rival=car;rival.position={0,0,25};online.rival=&rival;
    RaceClock onlineClock=race;onlineClock.sector=2;onlineClock.sectionCapacity=4;onlineClock.sectionTimes6000={6000,18000,0,0};onlineClock.elapsed6000=30000;online.race=&onlineClock;
    if(argc>3&&std::string(argv[3])=="--group-only"){
        Idas3UiEnable(1);
        for(const auto viewport:std::array<std::array<int,2>,3>{{{640,480},{1280,720},{2560,1080}}})for(int mode=0;mode<3;++mode){
            const int ww=viewport[0],hh=viewport[1];hud.resize(ww,hh);Idas3UiBeginFrame(ww,hh);
            auto state=online;state.hudIntroFrame=240;state.onlineBattleHud.frame=240;state.battleHudFrame=240;
            state.onlineBattleHud.active=mode==2;state.battle=mode==1;state.battleEnemy=13;state.battleProfileMode=0;
            OriginalResultsState records;records.livePanel=true;records.edgeAnchored=true;state.results=mode==0?&records:nullptr;
            const auto pixels=hud.paint(state);unityUiSubmit(pixels,ww,hh,false,false,false);
            UnityUiFrame frame{sizeof(UnityUiFrame)};require(Idas3UiGetFrame(&frame)==1,"No captured HUD frame");
            std::vector<UnityUiDraw> draws(frame.drawCount);require(Idas3UiCopyDraws(draws.data(),int(draws.size()))==int(draws.size()),"No HUD draws");
            bool groups[8]{};
            for(const auto& draw:draws){require((draw.flags&8)!=0,"Race HUD draw lost its HUD marker");const unsigned group=(draw.flags>>8)&15;require(group<8,"Invalid HUD group");groups[group]=true;}
            require(groups[1]&&groups[2]&&groups[5],"Timer, speedometer or map is missing its group");
            require(groups[mode==0?3:mode==1?6:7],"Record/Legend/online group is missing");
            require(!groups[mode==2?6:7],"Legend and online panels share a group");
            require(unityUiHudGroup()==0,"HUD group escaped its rendering scope");
        }
        std::cout<<"PASS independent HUD groups: Time Attack, Legend and online at three aspect ratios\n";return 0;
    }
    for(const auto viewport:std::array<std::array<int,2>,3>{{{640,480},{1280,720},{800,1000}}}){
        const auto ww=viewport[0],hh=viewport[1];hud.resize(ww,hh);const float fit=std::min(ww/640.f,hh/480.f);
        for(float gap:{25.f,-25.f,0.f,.05f,-.05f}){
            // Reset40->41 before every source gap case to start its blink at1.
            peer.frame=40;peer.advantage=gap;hud.paint(online);
            require(hud.lastBattlePresentation().playerGlyphs==0&&hud.lastBattlePresentation().rivalGlyphs==0,"Source40-frame name gate changed");
            peer.frame=41;const auto live=hud.paint(online);std::vector<std::uint32_t> actual(live,live+std::size_t(ww)*hh);
            const auto proof=hud.lastBattlePresentation();
            require(proof.online&&proof.profileMode==3&&proof.portraitCommands==0&&proof.mirrorEnabled,"Online HUD lost source mode3/mirror or invented portrait");
            require(proof.playerName=="SMOKE HOST"&&proof.rivalName=="SMOKE JOIN"&&proof.playerCarCode=="AE86 TRUENO"&&proof.rivalCarCode=="BNR34","Online HUD associated wrong name/car with driver/opponent");
            require(proof.playerGlyphs>0&&proof.rivalGlyphs>0&&proof.game2dCommands>=10&&proof.signedAdvantage==gap,"Online source submissions missing");
            require(proof.localMapMarkers==1&&proof.opponentMapMarkers==1,"Original map did not submit both player indicators");
            require(proof.sectionCount==2&&proof.sectionCapacity==4&&proof.renderedSectionDurations==std::array<std::uint32_t,4>{6000,12000,12000,0xffffffff},"Original section cumulative-to-duration presentation changed");
            OriginalBattleHudState expected;expected.flags104=0x001fffff;expected.profileMode0C31C99C=3;expected.frame204=41;
            expected.validity96=.25f;expected.signedAdvantage100=gap;OriginalBattleHudAnimation expectedAnimation;
            const auto commands=drawOriginalBattleHud(expected,expectedAnimation);std::vector<std::uint32_t> expectedPixels(std::size_t(ww)*hh);
            battleBank.paintGame2d(expectedPixels,ww,hh,commands,true);
            nameBank.paintOnline(expectedPixels,ww,hh,peer.playerName,peer.rivalName,peer.playerCar,peer.rivalCar);
            for(int y=0;y<int(160*fit);++y)for(int x=int(ww-150*fit);x<ww;++x)
                if(actual[std::size_t(y)*ww+x]!=expectedPixels[std::size_t(y)*ww+x])throw std::runtime_error("Live online upper-right pixels differ from source profile3");
            require(std::equal(actual.begin(),actual.end(),hud.paint(online)),"Repeated online simulation frame changed HUD pixels");
            if(gap==25.f)save(output/("online-hud-"+std::to_string(ww)+"x"+std::to_string(hh)+".bmp"),actual,ww,hh);
        }
    }
    hud.resize(w,h);peer.frame=42;std::swap(peer.playerName,peer.rivalName);std::swap(peer.playerCar,peer.rivalCar);hud.paint(online);
    require(hud.lastBattlePresentation().playerName=="SMOKE JOIN"&&hud.lastBattlePresentation().playerCarCode=="BNR34"&&hud.lastBattlePresentation().rivalName=="SMOKE HOST","Guest HUD must use local DRIVER independent of pre-race grid slot");
    online.finishBanner=OriginalHudState::FinishBanner::win;hud.paint(online);
    require(!hud.lastBattlePresentation().online&&!hud.lastBattlePresentation().game2dCommands&&!hud.lastBattlePresentation().playerGlyphs,"Finish announcement retained racing HUD snapshot");
    hud.paint(s);require(hud.lastBattlePresentation().portraitCommands==1&&!hud.lastBattlePresentation().online,"Returning from online changed offline portrait branch");
    std::cout<<"PASS "<<onlineChecks<<" multiplayer HUD layout/name/code/gap/map/timing checks;15 source-profile3 upper-right pixel comparisons across3 aspect ratios.\n";
    // Exercise the live UiState -> saved profile -> original dial boundary.
    // A correct selector tested in isolation did not prevent the old caller
    // from dropping the saved upgrade bytes and rotating an 8k face fully.
    Frontend frontend;s.frontend=&frontend;s.battle=false;frontend.car=0;
    const auto originalHud=OriginalRaceHud::load(argv[1]);
    unsigned tachCases=0;
    for(unsigned package:{0u,2u,3u})for(unsigned upgrade:{0u,4u,5u,75u})for(float rpm:{800.f,7500.f,11500.f,20000.f,std::numeric_limits<float>::quiet_NaN()}){
        frontend.battleProfile.setByte(152,std::uint8_t(package));frontend.battleProfile.setByte(164,std::uint8_t(upgrade));car.rpm=rpm;
        const auto live=hud.paint(s);
        OriginalHudState expected;expected.speedKmh=car.speedKmh();expected.gear=car.gear;expected.automatic=s.automatic;expected.edgeAnchored=true;
        expected.tachType=upgrade>4&&package<=2?3u:0u;expected.rpm=originalTachDisplayRpm(rpm,expected.tachType);
        std::vector<std::uint32_t> pixels(n);originalHud.paint(pixels,w,h,expected);
        for(int y=420;y<h;++y)for(int x=960;x<w;++x)if(live[y*w+x]!=pixels[y*w+x])throw std::runtime_error("Live tach pixels do not match saved-profile dial and bounded RPM");
        if(package==0&&upgrade==75&&rpm==11500.f)save(output/"tach-ae86-racing-engine-11500.bmp",std::vector<std::uint32_t>(live,live+n),w,h);
        ++tachCases;
    }
    std::cout<<"PASS "<<tachCases<<" live HUD saved-profile/RPM pixel comparisons, including upgraded AE86 and invalid telemetry.\n";
    // Exercise the real live compositor, including both sides, TA names and
    // the portrait. Repainting a source age must not tick the entrance.
    OriginalResultsState records;records.livePanel=true;records.edgeAnchored=true;
    records.bestTimes6000={1082940,1082940,1057416};records.modelBestAvailable=true;
    RaceClock introRace=race;introRace.phase=RacePhase::Countdown;introRace.elapsed6000=0;introRace.remaining6000=450000;introRace.sector=0;introRace.sectionTimes6000={};
    for(const auto viewport:std::array<std::array<int,2>,3>{{{640,480},{1280,720},{800,1000}}}){
        const int ww=viewport[0],hh=viewport[1];hud.resize(ww,hh);
        for(unsigned mode=0;mode<3;++mode){
            UiState intro=s;intro.frontend=nullptr;intro.race=&introRace;intro.paused=false;intro.results=&records;
            intro.battle=mode==1;intro.battleEnemy=13;intro.battleProfileMode=0;intro.battleAdvantage=0;
            intro.onlineBattleHud=peer;intro.onlineBattleHud.active=mode==2;
            std::vector<std::uint32_t> previous,initial;
            for(unsigned age=0;age<=60;++age){
                intro.hudIntroFrame=age;intro.battleHudFrame=int(age);intro.onlineBattleHud.frame=int(age);
                const auto p=hud.paint(intro);std::vector<std::uint32_t> current(p,p+std::size_t(ww)*hh);
                require(std::equal(current.begin(),current.end(),hud.paint(intro)),"Entrance advances during repeated paint");
                if(age==0)initial=current;
                if(age==24)require(current!=initial,"Entrance panels did not move");
                if(mode==2)require((hud.lastBattlePresentation().playerGlyphs>0)==(age>40),"Entrance name gate disagrees with source frame40");
                if(ww==640||age==0||age==12||age==60)
                    save(output/("entrance-"+std::to_string(mode)+"-"+std::to_string(ww)+"-"+std::to_string(hh)+"-"+std::to_string(age)+".bmp"),current,ww,hh);
                previous=std::move(current);
            }
            intro.hudIntroFrame=0;intro.battleHudFrame=0;intro.onlineBattleHud.frame=0;
            require(std::equal(initial.begin(),initial.end(),hud.paint(intro)),"Race restart did not reset the entrance");
        }
    }
    std::cout<<"PASS live TIME/RECORD/portrait/online entrances at 3 aspect ratios, every source age0..60, repeat and restart.\n";
    std::cout<<"PASS 62 negative-advantage frames, 186 repeat-frame pixel comparisons, reset and original portrait composition.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
