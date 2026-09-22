#include "frontend.h"
#include "original_car_color_catalog.h"
#include "original_record_rules.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace idas3;
void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
void saveBitmap(const std::filesystem::path& path,const std::vector<std::uint32_t>& pixels,int width,int height) {
    std::ofstream out(path,std::ios::binary);
    auto u16=[&](unsigned value){for(int i=0;i<2;++i)out.put(char(value>>(8*i)));};
    auto u32=[&](unsigned value){for(int i=0;i<4;++i)out.put(char(value>>(8*i)));};
    u16(0x4d42);u32(54+unsigned(pixels.size()*4));u32(0);u32(54);
    u32(40);u32(width);u32(unsigned(-height));u16(1);u16(32);u32(0);
    u32(unsigned(pixels.size()*4));u32(2835);u32(2835);u32(0);u32(0);
    for(auto pixel:pixels)u32(pixel);
    require(bool(out),"Unable to save original menu preview");
}
int main(int argc,char**argv) {
    try {
        Frontend menu;
        require(!menu.back(),"Back on title should report exit");
        std::set<int> allCars,allChunks;
        for(int make=0;make<7;++make) {
            const auto cars=Frontend::carsForMake(make);
            require(!cars.empty(),"Every make must expose cars");
            for(int id:cars) {require(allCars.insert(id).second,"Car appears in multiple makes");allChunks.insert(Frontend::carNameChunk(id));}
            menu.stage=FrontendStage::Car;menu.make=make;menu.car=cars.front();
            menu.advance(8./60.);
            for(std::size_t n=0;n<cars.size();++n)menu.change(1);
            require(menu.car==cars.front(),"Car traversal should close after one complete cycle");
            menu.change(-1);require(menu.car==cars.back(),"Previous car should wrap within selected make");
        }
        require(allCars.size()==35 && allChunks.size()==35,"Original car and menu permutation must each cover 35 unique IDs");
        require(Frontend::carNameChunk(0)==31 && Frontend::carNameChunk(20)==6,"Original AE86 and Evo IV identities must use recovered menu mapping");
        menu=Frontend{};
        for(int stage=0;stage<int(FrontendStage::Time);++stage) {
            // The save-file screen is exercised inside the Title step, because
            // leaving it is the host's decision rather than the menu's.
            if(stage==int(FrontendStage::SaveSelect))continue;
            require(int(menu.stage)==stage,"Unexpected stage traversal");
            if(menu.stage==FrontendStage::Make||menu.stage==FrontendStage::Car||menu.stage==FrontendStage::Transmission||menu.stage==FrontendStage::Mode){
                const int entry=menu.stage==FrontendStage::Transmission||menu.stage==FrontendStage::Mode?16:8;
                menu.advance(double(entry-1)/60.);require(!menu.inputReady(),"Selection accepted input before its source entry fade ended");
                menu.confirm();require(!menu.confirmationInProgress(),"Selection consumed an entry-fade confirmation");
                menu.advance(1./60.);require(menu.inputReady(),"Selection did not accept input at its source entry boundary");
            }
            if(menu.stage==FrontendStage::Mode){menu.change(1);menu.advance(1./60.);}
            require(!menu.confirm(),"Start returned before final selection");
            if(stage==int(FrontendStage::Title)){
                menu.advance(3./60.);require(menu.stage==FrontendStage::Title,"Attract owner finished before its original Start wait");
                // The attract owner now hands off to the save-file screen, and a
                // file has to be chosen before the driver picks a car.
                menu.advance(1./60.);require(menu.stage==FrontendStage::SaveSelect,"Attract owner did not hand off to the save files");
                require(!menu.takeSaveFileChosen(),"A file reported itself chosen before any confirmation");
                menu.change(1);require(menu.saveSelected==1,"The file list did not move");
                menu.change(-1);require(menu.saveSelected==0,"The file list did not move back");
                menu.confirm();require(menu.takeSaveFileChosen(),"Confirming a file raised no choice");
                require(!menu.takeSaveFileChosen(),"The file choice was reported twice");
                require(menu.back(),"Backing out of the file screen was refused");
                require(menu.stage==FrontendStage::Title,"Backing out of the file screen left the wrong stage");
                menu.stage=FrontendStage::Make;
            }
            if(stage!=int(FrontendStage::Title)) {
                const int duration=stage==int(FrontendStage::Make)?134:stage==int(FrontendStage::Car)?164:stage==int(FrontendStage::Transmission)?142:stage==int(FrontendStage::Mode)?138:(stage>=int(FrontendStage::Course)?31:36);
                require(int(menu.stage)==stage,"Confirmation should retain the original strip during its shrink");
                menu.advance(double(duration-1)/60.);
                require(int(menu.stage)==stage,"Confirmation ended before original duration");
                const int chosen=menu.car;menu.change(1);require(menu.car==chosen,"Car changed during confirmation");
                menu.advance(1./60.);
                require(int(menu.stage)==stage+1,"Confirmation failed to advance after original duration");
            }
        }
        require(menu.stage==FrontendStage::Time && !menu.confirm(),"Final confirmation should retain the original blink");
        menu.advance(30./60.);require(!menu.takeStartRequest(),"Race start preceded the original confirmation wait");
        menu.advance(16./60.);require(!menu.takeStartRequest(),"Race start skipped the source TA exit phase");
        menu.advance(1./60.);require(menu.takeStartRequest() && !menu.takeStartRequest(),"Final confirmation must request exactly one start");
        for(int stage=int(FrontendStage::Time);stage>0;--stage){
            if(menu.stage==FrontendStage::Car)menu.advance(8./60.);
            require(menu.back(),"Back failed inside menu flow");
            if(menu.stage==FrontendStage::Car){menu.advance(16./60.);require(menu.stage==FrontendStage::Car,"Car cancellation skipped its source fade");menu.advance(1./60.);}
        }
        require(menu.stage==FrontendStage::Title && !menu.back(),"Back traversal did not return to title");
        menu.stage=FrontendStage::Mode;menu.advance(16./60.);
        require(menu.gameMode==original::OriginalGameMode::LegendOfTheStreets,"Mode Init must select Legend");
        require(!menu.unsupportedModeSelected() && !menu.confirm(),"Legend confirmation must enter the original course/rival branch");
        menu.advance(138./60.);require(menu.stage==FrontendStage::Course,"Legend did not enter course selection");
        menu.course=3;menu.confirm();menu.advance(31./60.);require(menu.stage==FrontendStage::Rival,"Legend must select a rival rather than a TA route");
        const auto enemy=menu.battleProfile.u(24);menu.change(1);require(menu.battleProfile.u(24)!=enemy,"Rival selection did not update the original profile");
        require(menu.battleProfile.u(20)==original::originalRival(menu.battleProfile.u(24)).car,"Rival model must come from the original roster");
        menu.confirm();menu.advance(134./60.);require(!menu.takeStartRequest(),"Legend launched before confirmation completed");
        menu.advance(1./60.);require(menu.takeStartRequest()&&!menu.takeStartRequest(),"Legend must request exactly one battle start");
        menu.stage=FrontendStage::Mode;menu.advance(16./60.);menu.change(-1);menu.advance(1./60.);require(menu.gameMode==original::OriginalGameMode::BuntaChallenge && menu.unsupportedModeSelected(),"Original three-mode navigation must wrap");
        menu.confirm();menu.advance(3);require(menu.stage==FrontendStage::Mode,"Bunta must not silently launch TimeAttack");
        require(menu.buntaEligibility()==original::OriginalBuntaEligibility::InsufficientPoints,"Fresh Bunta profile must preserve the original points rule");
        menu.battleProfile.setu(72,4000);require(menu.buntaEligibility()==original::OriginalBuntaEligibility::CardRequired,"Bunta must require the original driver-card bits after points qualify");
        menu.battleProfile.setu(1180,menu.battleProfile.u(1180)|1u);
        require(!menu.unsupportedModeSelected(),"Qualified Bunta mode remained blocked");
        menu.confirm();menu.advance(138./60.);require(menu.stage==FrontendStage::Course,"Bunta must enter its course menu");
        {
            Frontend timeout;timeout.stage=FrontendStage::Mode;timeout.advance(16./60.);
            timeout.change(-1);timeout.advance(1278./60.);
            require(timeout.gameMode==original::OriginalGameMode::BuntaChallenge&&!timeout.confirmationInProgress(),"Mode timeout fired early");
            timeout.advance(1./60.);
            require(timeout.gameMode==original::OriginalGameMode::TimeAttack&&timeout.battleProfile.u(0)==1&&timeout.battleProfile.byte(1191)==2,"Ineligible Bunta timeout did not commit the source TA fallback");
            timeout.advance(137./60.);require(timeout.stage==FrontendStage::Course,"Mode timeout failed to complete confirmation");
        }
        for(unsigned level:{0u,6u,11u})for(unsigned slot=0;slot<8;++slot){
            Frontend bunta;bunta.gameMode=original::OriginalGameMode::BuntaChallenge;bunta.stage=FrontendStage::Course;bunta.car=34;
            bunta.battleProfile.setu(72,4000);bunta.battleProfile.setu(1180,bunta.battleProfile.u(1180)|1u);
            for(unsigned i=0;i<8;++i)bunta.battleProfile.setu(1080+i*4,level);
            const auto choices=bunta.courseChoices();
            require(choices.size()==8 && choices[4]==5 && choices[5]==4,"Bunta course slots do not follow original order");
            require(choices[3]==(level>10?8:3),"Bunta Akina slot did not follow original Snow progression");
            bunta.course=choices[slot];
            for(int i=0;i<8;++i)bunta.change(1);
            require(bunta.course==choices[slot],"Bunta traversal exposed an extra course slot");
            auto expected=bunta.battleProfile;expected.setu(0,2);expected.setu(16,34);original::selectOriginalBuntaCourse(expected,slot);
            bunta.confirm();bunta.advance(121./60.);require(!bunta.takeStartRequest(),"Bunta launched before course confirmation");
            bunta.advance(1./60.);
            require(bunta.takeStartRequest() && !bunta.takeStartRequest() && bunta.stage==FrontendStage::Course,"Bunta must start directly from course selection, exactly once");
            require(bunta.battleProfile.words==expected.words,"Bunta frontend changed original selection profile fields");
            const auto selected=original::originalBattleSelection(expected);
            require(bunta.reverse==bool(selected.direction) && bunta.night==bool(selected.night) && bunta.wet==bool(selected.weather),"Bunta host course conditions disagree with the original profile");
        }
        // A new TA setup must not inherit the last race or Snow's forced flags.
        for(int course=0;course<9;++course)for(unsigned stale=0;stale<8;++stale){
            Frontend defaults;defaults.gameMode=original::OriginalGameMode::TimeAttack;
            defaults.stage=FrontendStage::Course;defaults.course=course;
            defaults.reverse=(stale&1)!=0;defaults.wet=(stale&2)!=0;defaults.night=(stale&4)!=0;
            defaults.confirm();defaults.advance(31./60.);
            require(defaults.stage==FrontendStage::Route&&!defaults.reverse,"New TA course did not default to its left route");
            require(defaults.wet==(course==8)&&defaults.night==(course==4||course==8),"New TA course inherited stale weather/time");
            // User choices survive subsequent pages and backing up within setup.
            defaults.change(1);defaults.confirm();defaults.advance(31./60.);
            require(defaults.reverse,"Explicit right route was reset");
            if(course==8)continue;
            require(defaults.stage==FrontendStage::Weather&&!defaults.wet,"Weather did not start on Dry");
            defaults.change(1);defaults.confirm();defaults.advance(31./60.);
            require(defaults.reverse&&defaults.wet,"Explicit route/weather was reset");
            if(course==4)continue;
            require(defaults.stage==FrontendStage::Time&&!defaults.night,"Time did not start on Day");
            defaults.change(1);defaults.back();defaults.advance(0);
            require(defaults.stage==FrontendStage::Weather&&defaults.wet,"Back lost explicit wet selection");
            defaults.confirm();defaults.advance(31./60.);
            require(defaults.stage==FrontendStage::Time&&defaults.night,"Back/forward lost explicit night selection");
            defaults.confirm();defaults.advance(47./60.);
            require(defaults.takeStartRequest()&&defaults.reverse&&defaults.wet&&defaults.night,"Manual setup did not reach race intact");
            defaults.stage=FrontendStage::Course;defaults.confirm();defaults.advance(31./60.);
            require(!defaults.reverse&&!defaults.wet&&!defaults.night,"New visit inherited the prior race's conditions");
        }
        {
            Frontend snow;snow.stage=FrontendStage::Course;snow.course=5;
            snow.advance(0);snow.change(1);require(snow.course==8&&snow.wet&&snow.night,"Snow browsing reproduction missing");
            snow.change(-1);require(snow.course==5,"Could not browse back from Snow");
            snow.confirm();snow.advance(31./60.);
            require(!snow.reverse&&!snow.wet&&!snow.night,"Snow browsing contaminated another course's defaults");
        }
        for(int forcedCourse:{4,8,Frontend::ennaCourse}) {
            Frontend restricted;restricted.course=forcedCourse;restricted.night=false;
            restricted.stage=forcedCourse==8?FrontendStage::Route:FrontendStage::Weather;
            require(!restricted.confirm() && restricted.confirmationInProgress(),"Restricted TA course skipped its confirmation");
            restricted.advance(31./60.);
            require(!restricted.takeStartRequest() && restricted.confirmationInProgress(),"Restricted TA course skipped its source exit phase");
            restricted.advance(16./60.);
            require(restricted.takeStartRequest() && restricted.night && !restricted.confirmationInProgress(),"Original forced-night course did not finish setup at its source boundary");
            require(restricted.stage!=FrontendStage::Time,"Source-restricted course exposed the unavailable time choice");
            if(forcedCourse==8)require(restricted.wet,"Original Snow start requires its independent wet flag");
        }
        menu.stage=FrontendStage::Make;menu.make=6;menu.car=0;menu.advance(8./60.);menu.change(1);
        const auto nissanCars=Frontend::carsForMake(3);
        require(menu.make==3 && std::find(nissanCars.begin(),nissanCars.end(),menu.car)!=nissanCars.end(),"Make navigation must follow source display order Toyota then Nissan");
        for(const auto pair:{std::array<int,3>{6,0,0},std::array<int,3>{0,2,16}}){
            Frontend maker;maker.stage=FrontendStage::Make;maker.make=pair[0];maker.battleProfile.setu(40,unsigned(pair[1]));
            maker.car=Frontend::carsForMake(maker.make).back();maker.battleProfile.setu(72,12345);
            // Force the raw-versus-display source branch for Honda too.
            if(pair[0]==0)maker.battleProfile.setu(40,0);
            maker.advance(8./60.);maker.confirm();maker.advance(134./60.);
            require(maker.car==pair[2]&&maker.battleProfile.u(16)==unsigned(pair[2]),"Maker reset used thumbnail order instead of original local-car roster");
            require(maker.battleProfile.u(72)==12345,"Maker selection altered earned points");
        }
        unsigned paletteChoices=0;
        for(int make=0;make<7;++make)for(int car:Frontend::carsForMake(make)){
            Frontend colors;colors.stage=FrontendStage::Car;colors.make=make;colors.car=car;
            const unsigned count=original::originalCarColorCounts.at(std::size_t(car));
            colors.battleProfile.setu(16,unsigned(car));colors.battleProfile.setu(64,count-1);
            colors.battleProfile.setu(72,12345);
            colors.changeColor(1);colors.advance(7./60.);
            require(colors.selectedColor()==count-1,"Color changed during the Car entry fade");
            colors.advance(1./60.);
            for(unsigned next=0;next<count;++next){
                colors.changeColor(1);colors.advance(1./60.);
                require(colors.selectedColor()==next,"Factory color traversal disagrees with the original per-car count");
                require(colors.battleProfile.u(64)==count-1,"Preview color committed before confirmation");
                ++paletteChoices;
            }
            colors.changeColor(1);colors.changeColor(-1);colors.advance(1./60.);
            require(colors.selectedColor()==0,"Simultaneous gear buttons must use the original positive-button priority");
            colors.changeColor(-1);colors.confirm();colors.advance(1./60.);
            require(colors.battleProfile.u(64)==count-1,"Same-frame gear and confirmation failed to commit the final factory color");
            require(colors.battleProfile.u(72)==12345,"Factory paint selection changed earned points");
        }
        require(paletteChoices==181,"Frontend palette coverage must include all181 original factory colors");
        {
            Frontend remembered;remembered.stage=FrontendStage::Car;remembered.make=6;remembered.car=0;
            remembered.advance(8./60.);remembered.changeColor(-1);remembered.advance(1./60.);
            require(remembered.selectedColor()==2,"AE86 reverse color wrap failed");
            remembered.change(1);remembered.battleProfile.setu(64,2);remembered.driverProfileLoaded();remembered.advance(1./60.);
            require(remembered.car==1&&remembered.selectedColor()==0,"New local car must use its source remembered preview color");
            remembered.changeColor(1);remembered.advance(1./60.);remembered.change(-1);remembered.advance(1./60.);
            require(remembered.car==0&&remembered.selectedColor()==2,"Returning to a car lost its remembered preview color");
        }
        {
            Frontend records;
            records.timeAttackBest=[](unsigned condition,unsigned weather,unsigned car){
                return condition==6&&weather==0?TimeAttackBest{1000000,car==0?1100000u:0u}:TimeAttackBest{};
            };
            records.battleProfile.setu(176+4*original::originalRecordPartition(6,false).personalIndex(),1200000);
            require(records.courseRecordTimes()==std::array<std::uint32_t,3>{1000000,1100000,1200000},"Menu did not resolve course/model/personal records");
            records.car=1;require(records.courseRecordTimes()[1]==0,"Model record leaked between cars");
            records.reverse=true;require(records.courseRecordTimes()==std::array<std::uint32_t,3>{0,0,0},"Direction records leaked");
            records.reverse=false;records.wet=true;require(records.courseRecordTimes()[2]==0,"Dry personal time leaked into wet course");
            records.course=8;records.wet=false;
            records.battleProfile.setu(176+4*original::originalRecordPartition(16,true).personalIndex(),1300000);
            require(records.courseRecordTimes()[2]==1300000,"Snow menu did not read its forced wet partition");
        }
        if(argc>1) {
            {
                Frontend records;records.initialize(argv[1]);records.stage=FrontendStage::Course;
                TimeAttackBest best{1000000,1100000};records.timeAttackBest=[&](unsigned,unsigned,unsigned){return best;};
                const auto before=records.paint(1280,720);best.course=900000;
                const auto after=records.paint(1280,720);require(before!=after,"New course record did not invalidate menu cache");
                const auto partition=original::originalRecordPartition(6,false);
                records.battleProfile.setu(176+4*partition.personalIndex(),1000123);
                require(records.paint(1280,720)!=after,"Personal record update did not repaint");
                records.gameMode=original::OriginalGameMode::LegendOfTheStreets;const auto legend=records.paint(640,480);
                best.course=800000;require(records.paint(640,480)==legend,"TA panel leaked into Legend");
                records.gameMode=original::OriginalGameMode::TimeAttack;
                const auto output=argc>2?std::filesystem::path(argv[2]):std::filesystem::path{};
                if(!output.empty()){
                    std::filesystem::create_directories(output);
                    for(auto stage:{FrontendStage::Course,FrontendStage::Route,FrontendStage::Weather,FrontendStage::Time}){
                        records.stage=stage;saveBitmap(output/("ta-records-"+std::to_string(int(stage))+".bmp"),records.paint(640,480),640,480);
                    }
                }
            }

            {
                Frontend bunta;bunta.initialize(argv[1]);bunta.stage=FrontendStage::Course;bunta.course=0;
                bunta.gameMode=original::OriginalGameMode::LegendOfTheStreets;
                const auto sharedCourse=bunta.paint(640,480);
                bunta.gameMode=original::OriginalGameMode::BuntaChallenge;
                const auto noStars=bunta.paint(640,480);
                require(noStars!=sharedCourse,"Bunta course menu still reuses the Legend screen without its original artwork");
                unsigned badgePixels=0;
                for(int y=0;y<480;++y)for(int x=0;x<640;++x){
                    const bool badge=x<352&&y>=300&&y<364;
                    if(badge)badgePixels+=noStars[std::size_t(y)*640+x]!=sharedCourse[std::size_t(y)*640+x];
                    // The upper carousel legitimately differs: original
                    // Bunta has eight slots with Happo/Iroha swapped.
                    else if(y>=180)require(noStars[std::size_t(y)*640+x]==sharedCourse[std::size_t(y)*640+x],"Bunta badge changed shared course artwork outside its source bounds");
                }
                require(badgePixels>5000,"Original Bunta portrait and level panel did not render");
                bunta.advance(34./60.);bunta.paint(640,480);
                // A level win keeps the same course. Its saved level word
                // must invalidate both native and scaled composition caches.
                bunta.battleProfile.setu(1080,1);const auto oneStar=bunta.paint(640,480);
                require(oneStar!=noStars,"Saved Bunta progress changed without refreshing level/stars");
                const auto* retained=bunta.paint(640,480).data();
                bunta.advance(1.);require(bunta.paint(640,480)==oneStar&&bunta.paint(640,480).data()==retained,"Settled Bunta stars unnecessarily repaint or change");
                const auto output=argc>2?std::filesystem::path(argv[2]):std::filesystem::path{};
                if(!output.empty())std::filesystem::create_directories(output);
                for(unsigned level:{0u,1u,4u,5u,6u,10u,11u,15u,16u}){
                    bunta.battleProfile.setu(1080,level);const auto original=bunta.paint(640,480);
                    const auto& doubled=bunta.paint(1280,960);
                    for(int y=0;y<960;++y)for(int x=0;x<1280;++x)
                        require(doubled[std::size_t(y)*1280+x]==original[std::size_t(y/2)*640+x/2],"Bunta resolution scaling changed its original pixel composition");
                    if(!output.empty())saveBitmap(output/("bunta-level-"+std::to_string(level)+".bmp"),original,640,480);
                }
                bunta.course=1;bunta.battleProfile.setu(1084,15);
                const auto popping=bunta.paint(640,480);bunta.advance(34./60.);
                require(popping!=bunta.paint(640,480),"Changing Bunta course failed to restart the original staggered stars");
                bunta.course=3;bunta.battleProfile.setu(1092,4);bunta.paint(640,480);bunta.advance(34./60.);
                if(!output.empty())saveBitmap(output/"bunta-reference-akina-level-4.bmp",bunta.paint(1280,720),1280,720);
                bunta.course=3;bunta.battleProfile.setu(1092,11);bunta.paint(640,480);
                require(bunta.course==8,"Bunta level11 course artwork did not substitute Akina Snow");
                bunta.advance(34./60.);
                if(!output.empty())saveBitmap(output/"bunta-snow-level-11.bmp",bunta.paint(1280,720),1280,720);
                bunta.course=0;bunta.gameMode=original::OriginalGameMode::LegendOfTheStreets;
                require(bunta.paint(640,480)==sharedCourse,"Bunta level/star presentation leaked into Legend");
            }
            {
                Frontend warning;warning.initialize(argv[1]);warning.stage=FrontendStage::Mode;
                warning.battleProfile.setu(1180,0);warning.advance(16./60.);
                warning.change(-1);warning.advance(6./60.);
                const auto clean=warning.paint(1280,720);
                warning.confirm();warning.advance(1./60.);const auto points=warning.paint(1280,720);
                require(points!=clean&&!warning.confirmationInProgress(),"Rejected Bunta confirmation failed to show original points panel");
                warning.advance(120./60.);require(points==warning.paint(1280,720),"Points panel disappeared before its source lifetime");
                warning.advance(1./60.);require(clean==warning.paint(1280,720),"Expired warning remained in the widescreen cache");
                warning.battleProfile.setu(72,4000);warning.confirm();warning.advance(1./60.);
                const auto card=warning.paint(1280,720);require(card!=clean&&card!=points,"Card rejection reused the points artwork");
                warning.advance(121./60.);require(clean==warning.paint(1280,720),"Card warning did not expire at its source boundary");
                if(argc>2){const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);
                    saveBitmap(out/"mode-points-warning.bmp",points,1280,720);
                    saveBitmap(out/"mode-card-warning.bmp",card,1280,720);
                    saveBitmap(out/"mode-warning-cleared.bmp",clean,1280,720);}
            }
            {
                Frontend palette;palette.initialize(argv[1]);palette.stage=FrontendStage::Car;palette.make=6;palette.car=0;
                palette.advance(8./60.);const auto whiteSelected=palette.paint(640,480);
                palette.changeColor(1);palette.advance(1./60.);const auto& redSelected=palette.paint(640,480);
                require(palette.selectedColor()==1&&redSelected!=whiteSelected,"Changing factory color did not invalidate the palette artwork cache");
                // Original AE86 red is RGB A00000. Its source64x64 quad
                // starts at x34/y200; the colored diamond is inside it.
                // Its white selection marker is behind the painted swatch;
                // submission order alone previously hid this entire region.
                unsigned redPixels=0;
                for(int y=218;y<226;++y)for(int x=50;x<66;++x){
                    const auto rgb=redSelected[std::size_t(y)*640+x];
                    redPixels+=((rgb>>16)&255)>60&&((rgb>>8)&255)<20&&(rgb&255)<20;
                }
                require(redPixels>=64,"Selected original red swatch was obscured by its white marker");
            }
            menu=Frontend{};menu.initialize(argv[1]);
            constexpr int width=1280,height=960;
            const auto output=argc>2?std::filesystem::path(argv[2]):std::filesystem::path{};
            if(!output.empty())std::filesystem::create_directories(output);
            {
                Frontend attract;attract.initialize(argv[1]);
                for(unsigned i=0;i<2000&&attract.attractChild()!=6;++i)attract.advance(1./60.);
                require(attract.attractChild()==6,"Attract sequence never reached its first title");
                std::vector<std::uint32_t> best;std::size_t largest=0,smallest=640*480;
                for(unsigned i=0;i<60;++i){
                    std::vector<std::uint32_t> overlay(640*480,0xff000000u);attract.paintAttractPrompts(overlay,640,480);
                    auto lit=std::size_t(std::count_if(overlay.begin(),overlay.end(),[](auto c){return (c&0xffffff)!=0;}));
                    if(lit>largest){largest=lit;best=overlay;}smallest=std::min(smallest,lit);attract.advance(1./60.);
                }
                require(smallest>0&&largest>smallest,"Original FREE PLAY / blinking PRESS START artwork missing");
                for(const auto bounds:std::array<std::array<int,2>,3>{{{220,284},{288,350},{350,478}}}){
                    unsigned lit=0;for(int y=425;y<441;++y)for(int x=bounds[0];x<bounds[1];++x)lit+=(best[y*640+x]&0xffffff)!=0;
                    require(lit>100,"A word of PRESS START BUTTON is missing or overlapped");
                }
                if(!output.empty())saveBitmap(output/"original-attract-prompts.bmp",best,640,480);
            }
            const std::array<const char*,9> names={"title","make","car","transmission","mode","course","route","weather","time"};
            for(int stage=0;stage<int(names.size());++stage) {
                menu.stage=FrontendStage(stage);
                const auto& pixels=menu.paint(width,height);
                require(pixels.size()==std::size_t(width*height),"Unexpected original menu frame size");
                require(std::count(pixels.begin(),pixels.end(),0xff000000u)<width*height*9/10,"Original menu mostly blank");
                const auto* data=pixels.data();
                require(menu.paint(width,height).data()==data,"Unchanged menu should reuse cached pixels");
                if(!output.empty())saveBitmap(output/(std::string(names[stage])+".bmp"),pixels,width,height);
            }
            menu.stage=FrontendStage::Car;
            for(int make=0;make<7;++make)for(int id:Frontend::carsForMake(make)){menu.make=make;menu.car=id;menu.paint(640,480);}
            menu.stage=FrontendStage::Course;for(int course=0;course<9;++course){menu.course=course;menu.paint(640,480);}
            if(!output.empty())saveBitmap(output/"snow-course.bmp",menu.paint(width,height),width,height);
            // Source-resolution composition is independent of the window.
            // Every host pixel in an integer-scale view must come from the
            // completed original pixel, including exact blended alpha/colors.
            menu=Frontend{};menu.initialize(argv[1]);
            for(int stage=0;stage<=int(FrontendStage::Rival);++stage){
                menu.stage=FrontendStage(stage);const auto original=menu.paint(640,480);
                const auto& enlarged=menu.paint(1280,960);
                for(int y=0;y<960;++y)for(int x=0;x<1280;++x)
                    require(enlarged[std::size_t(y)*1280+x]==original[std::size_t(y/2)*640+x/2],"Host resizing changed original menu color composition");
                const auto* cached=enlarged.data();require(menu.paint(1280,960).data()==cached,"Scaled menu frame was not retained");
            }
            menu=Frontend{};menu.initialize(argv[1]);menu.liveCarPreview=true;menu.stage=FrontendStage::Car;
            menu.make=6;menu.car=Frontend::carsForMake(6).front();
            menu.advance(8./60.);
            constexpr int wideWidth=1280,wideHeight=720;
            const auto settled=menu.paint(wideWidth,wideHeight);
            menu.change(-1);
            const auto scrollStart=menu.paint(wideWidth,wideHeight);
            menu.advance(2./60.);
            const auto scrollMiddle=menu.paint(wideWidth,wideHeight);
            require(scrollStart!=scrollMiddle,"Original four-frame carousel scroll did not animate");
            menu.advance(4./60.);
            const auto scrollEnd=menu.paint(wideWidth,wideHeight);
            menu.advance(.5);
            require(scrollEnd==menu.paint(wideWidth,wideHeight),"Settled carousel should stop repainting");
            menu.confirm();menu.advance(18./60.);
            const auto shrinking=menu.paint(wideWidth,wideHeight);
            require(shrinking!=scrollEnd,"Original confirmation shrink did not change thumbnail row");
            // Motion must not change the cached manufacturer/header/nameplate
            // or central showroom backing, avoiding a full-frame animation.
            for(int y=270;y<wideHeight;++y)for(int x=0;x<wideWidth;++x)
                require(shrinking[std::size_t(y)*wideWidth+x]==scrollEnd[std::size_t(y)*wideWidth+x],"Carousel animation modified static screen artwork");
            if(!output.empty()) {
                saveBitmap(output/"car-showroom-base-wide.bmp",settled,wideWidth,wideHeight);
                saveBitmap(output/"car-scroll-wide.bmp",scrollMiddle,wideWidth,wideHeight);
                saveBitmap(output/"car-confirm-wide.bmp",shrinking,wideWidth,wideHeight);
            }
            menu.advance(18./60.);
            menu.stage=FrontendStage::Make;
            menu.advance(8./60.);
            const auto makeSettled=menu.paint(wideWidth,wideHeight);
            menu.confirm();menu.advance(18./60.);
            const auto makeShrinking=menu.paint(wideWidth,wideHeight);
            require(makeShrinking!=makeSettled,"Original seven-emblem confirmation did not animate");
            if(!output.empty()) {
                saveBitmap(output/"make-strip-wide.bmp",makeSettled,wideWidth,wideHeight);
                saveBitmap(output/"make-confirm-wide.bmp",makeShrinking,wideWidth,wideHeight);
            }
            menu.advance(18./60.);
            menu.stage=FrontendStage::Course;
            const auto courseSettled=menu.paint(wideWidth,wideHeight);
            menu.change(-1);const auto courseScroll=menu.paint(wideWidth,wideHeight);
            menu.advance(3./60.);const auto courseMiddle=menu.paint(wideWidth,wideHeight);
            require(courseScroll!=courseMiddle,"Original five-frame course scroll did not animate");
            menu.advance(4./60.);const auto courseEnd=menu.paint(wideWidth,wideHeight);
            menu.advance(.5);require(courseEnd==menu.paint(wideWidth,wideHeight),"Settled course strip must remain cached");
            menu.confirm();menu.advance(18./60.);const auto courseShrinking=menu.paint(wideWidth,wideHeight);
            require(courseShrinking!=courseEnd,"Original course confirmation did not animate");
            if(!output.empty()) {
                saveBitmap(output/"course-strip-wide.bmp",courseSettled,wideWidth,wideHeight);
                saveBitmap(output/"course-scroll-wide.bmp",courseMiddle,wideWidth,wideHeight);
                saveBitmap(output/"course-confirm-wide.bmp",courseShrinking,wideWidth,wideHeight);
            }
            menu.advance(13./60.);menu.stage=FrontendStage::Mode;menu.gameMode=original::OriginalGameMode::TimeAttack;
            menu.advance(.5);const auto modeSettled=menu.paint(wideWidth,wideHeight);
            menu.advance(2);require(modeSettled==menu.paint(wideWidth,wideHeight),"Settled mode screen must remain cached");
            menu.confirm();menu.advance(30./60.);const auto modeConfirm=menu.paint(wideWidth,wideHeight);
            require(modeConfirm!=modeSettled,"Original mode confirmation did not animate");
            if(!output.empty()){
                saveBitmap(output/"mode-wide.bmp",modeSettled,wideWidth,wideHeight);
                saveBitmap(output/"mode-confirm-wide.bmp",modeConfirm,wideWidth,wideHeight);
            }
            std::cout<<"Original asset rendering passed all nine stages,35 car identities and nine courses.\n";
            Frontend preloaded,serial;preloaded.initialize(argv[1],true);serial.initialize(argv[1]);
            for(int stage=0;stage<=int(FrontendStage::Rival);++stage){
                preloaded.stage=serial.stage=FrontendStage(stage);
                require(preloaded.paint(640,480)==serial.paint(640,480),"Concurrent artwork preload changed visible menu pixels");
            }
            preloaded.initialize(argv[1],true);preloaded.initialize(argv[1],true);
            require(preloaded.paint(640,480).size()==640*480,"Artwork preload failed during menu reinitialization");
        }
        if(argc>3){
            Frontend hakone;hakone.initialize(argv[1]);hakone.enableHakoneCourse(argv[3]);
            const auto choices=hakone.courseChoices();
            require(choices.size()==10&&std::set<int>(choices.begin(),choices.end()).size()==10,"Hakone replaced an original course");
            for(auto mode:{original::OriginalGameMode::LegendOfTheStreets,original::OriginalGameMode::BuntaChallenge}){
                hakone.gameMode=mode;const auto original=hakone.courseChoices();
                require(std::find(original.begin(),original.end(),9)==original.end(),"Hakone leaked into unsupported mode");
            }
            hakone.gameMode=original::OriginalGameMode::TimeAttack;hakone.course=9;hakone.stage=FrontendStage::Course;
            require(hakone.courseRecordTimes()==std::array<std::uint32_t,3>{},"Hakone displayed another course's records");
            hakone.advance(1);hakone.paint(640,480);hakone.change(1);hakone.advance(.2);
            require(hakone.course==0,"Hakone carousel did not navigate to Myogi");
            hakone.change(-1);hakone.advance(.2);require(hakone.course==9,"Hakone carousel did not navigate back");
            for(int scenario=0;scenario<8;++scenario){
                hakone.stage=FrontendStage::Course;hakone.course=9;
                hakone.reverse=(scenario&1)!=0;hakone.wet=(scenario&2)!=0;hakone.night=(scenario&4)!=0;
                for(auto stage:{FrontendStage::Course,FrontendStage::Route,FrontendStage::Weather,FrontendStage::Time}){
                    hakone.advance(1);require(hakone.stage==stage,"Hakone source selection transition failed");
                    if(stage==FrontendStage::Route&&bool(scenario&1)!=hakone.reverse)hakone.change(1);
                    if(stage==FrontendStage::Weather&&bool(scenario&2)!=hakone.wet)hakone.change(1);
                    if(stage==FrontendStage::Time&&bool(scenario&4)!=hakone.night)hakone.change(1);
                    if(scenario==0)saveBitmap(std::filesystem::path(argv[2])/("hakone-menu-"+std::to_string(int(stage))+".bmp"),hakone.paint(1280,720),1280,720);
                    else hakone.paint(640,480);
                    hakone.confirm();hakone.advance(1);
                }
                require(hakone.takeStartRequest(),"Hakone selection did not request a race");
                require(hakone.course==9&&hakone.reverse==bool(scenario&1)&&hakone.wet==bool(scenario&2)&&hakone.night==bool(scenario&4),"Hakone conditions changed during menu confirmation");
            }
            std::cout<<"Hakone menu passed ten-course navigation, original modes, eight conditions and race-start requests.\n";
        }
        std::cout<<"Native frontend navigation passed. Original animation/projection parity is not established.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
