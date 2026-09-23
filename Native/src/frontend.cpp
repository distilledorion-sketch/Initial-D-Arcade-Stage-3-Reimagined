#include "unity_ui_capture.h"
#include "frontend.h"
#include "car_catalog.h"
#include "original_choice_menu.h"
#include "original_record_rules.h"
#include "original_legend_menu.h"
#include "original_bunta_menu.h"
#include "original_car_color_catalog.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <chrono>
#include <stdexcept>

namespace idas3 {
namespace {
// Exact 35-u32 permutation from original image 0C2AAE80. Each entry is a
// zero-based menu car index; original car identity remains ID0..34.
constexpr std::array<int,35> menuCarIndex = {
    30,29,28,32,34,33,31,15,16,20,22,21,23,18,19,3,2,1,0,4,5,8,10,11,9,12,13,25,26,24,27,17,6,7,14
};
// Original table2A83FC through2A8418, converted to this host's make IDs.
constexpr std::array<int,7> makeDisplayOrder = {5,1,0,6,3,2,4};
constexpr std::array<unsigned,7> profileMakeIds={2,4,3,1,5,6,0};
constexpr std::array<unsigned,7> profileMakeCounts={7,9,4,5,6,3,1};
constexpr std::array<unsigned,7> profileMakeStarts={0,7,15,19,22,27,30};
constexpr std::array<unsigned,7> profileMakeFirstCars={0,7,16,19,22,27,30};
// Exact16-byte course records2A177C; original course identity0..8.
constexpr std::array<int,9> courseMapChunk = {6,9,1,3,4,5,7,8,2};
constexpr std::array<int,9> courseThumbnailChunk = {20,24,15,17,18,19,22,23,16};
constexpr std::array<int,9> courseBackgroundChunk = {25,29,20,22,23,24,27,28,21};
constexpr std::array<int,9> courseDifficultyChunk = {36,39,31,33,34,35,37,38,32};
int wrapped(int value, int count) { return ((value % count) + count) % count; }
// Total time on a save file reads as hours and minutes, not as a lap time.
// The cabinet writes every duration hh:mm'ss; a file's total play time is
// written the same way rather than in a style of its own.
std::string formatPlayTime(std::uint64_t seconds){
    const auto pad=[](std::uint64_t value){
        return (value<10?std::string("0"):std::string())+std::to_string(value);
    };
    return pad(seconds/3600)+":"+pad((seconds%3600)/60)+"'"+pad(seconds%60);
}
bool choiceStage(FrontendStage stage){return stage==FrontendStage::Transmission || stage==FrontendStage::Route || stage==FrontendStage::Weather || stage==FrontendStage::Time;}
}

int Frontend::carNameChunk(int id) {
    if (id < 0 || id >= int(menuCarIndex.size())) throw std::out_of_range("Original car ID outside menu table");
    return menuCarIndex[std::size_t(id)] + 1;
}
int Frontend::carImageChunk(int id) {
    // The original mini-car bank has eight preceding aggregate/display chunks.
    return carNameChunk(id) + 7;
}
std::vector<int> Frontend::carsForMake(int index) {
    if (index < 0 || index >= 7) throw std::out_of_range("Original make selection outside bank");
    //31D88C/133AA0/133A60: selection uses the actual manufacturer roster.
    // The separate menuCarIndex permutation selects artwork, never car order.
    static const std::array<std::vector<int>,7> rosters={
        std::vector<int>{0,1,2,3,4,5,6},{7,8,31,9,10,11,12,13,14},{16,15,17,18},
        {19,20,32,33,21},{22,23,24,34,25,26},{27,28,29},{30}};
    return rosters[profileMakeIds[std::size_t(index)]];
}
void Frontend::enableHakoneCourse(const std::filesystem::path& root,int id) {
    if(!isImportedCourse(id))throw std::runtime_error("Unsupported imported course");
    if(importedArtwork(id).size())return;
    auto artwork=NativeTextureBank::load(root/"menu.idastex");
    if(artwork.size()!=2)throw std::runtime_error("Hakone menu requires backdrop and thumbnail");
    importedArtwork(id)=std::move(artwork);previousKey.clear();previousMotionKey.clear();
}
void Frontend::drawHakoneBackdrop(int width,int height) {
    compositeImage(pixels,width,height,importedArtwork(course).at(0),0,168,640,312);
}
std::vector<int> Frontend::courseChoices()const {
    if(gameMode==original::OriginalGameMode::TimeAttack){std::vector<int> choices{0,1,2,3,5,8,4,6,7};for(int id=hakoneCourse;id<int(supportedCourseCount);++id)if(importedArtwork(id).size())choices.push_back(id);return choices;}
    if(gameMode!=original::OriginalGameMode::BuntaChallenge)return {0,1,2,3,5,8,4,6,7};
    std::vector<int> result;
    for(unsigned i=0;i<8;++i)result.push_back(int(original::originalBuntaCourse(i,std::bit_cast<std::int32_t>(battleProfile.u(1080+i*4)))));
    return result;
}
void Frontend::initialize(const std::filesystem::path& rootPath,bool preloadArtwork) {
    preloadedBanks={};
    projectRoot=rootPath;assetRoot = rootPath / "data" / "original_assets" / "menus" / "v3";
    menuFont=MenuFont::load(rootPath);
    namePresentation.reset();nameState={};nameInput={};menuCueIds.clear();driverProfileCommit=false;
    tuningData.reset();tuningPresentation.reset();tuningCourseMenu={};tuningCourseSelected=0;tuningCourseConfirmPending=false;
    nameCommittedForVisit=driverSetupCompleted=false;
    banks.clear(); previousKey.clear(); previousMotionKey.clear(); pixels.clear(); staticPixels.clear();
    displayPixels.clear();previousModePaintKey.clear();displayedCanvas=nullptr;
    canvasRevision=displayedRevision=0;displayedWidth=displayedHeight=0;
    rivalCacheKey.clear();for(auto& frame:rivalPixels)frame.clear();
    carouselMake=-1;carouselCar=-1;carouselSlot=0;carouselScroll=displayedScroll=0;
    carConfirmationFrame=-1;selectionExitFrame=-1;frameRemainder=0;carFrame=0;
    startRequested=false;selectionMusic={};selectionMusicCommands.clear();selectionMusicStageInitialized=false;
    stageInitialized=false;makerConfirmPending=false;makerTransition={};makerConfirmationPhase=0;
    carouselCourse=-1;courseSlot=courseScroll=displayedCourseScroll=0;
    buntaBadgeCourse=-1;buntaBadgeFrame=0;
    modeState={};original::selectOriginalGameMode(modeState,gameMode);modeMenu.load(rootPath);
    const auto extensionPath=rootPath/"data/authored_extensions/title_widescreen_v1.idastex";
    hasTitleExtension=std::filesystem::exists(extensionPath);
    if(hasTitleExtension)titleExtension=NativeTextureBank::load(extensionPath);
    // Fail at initialization for the first visible screen if the private asset
    // import is missing, without silently substituting generated artwork.
    bank("adv_newtitle");
    const auto credits=rootPath/"data/original_assets/menus/credit";
    creditTextures=NativeTextureBank::load(credits/"textures/textures.idastex");
    creditSprites=NativeSpriteBank::load(credits/"original_rip.tbl",credits/"original_rip.bin");
    if(creditTextures.size()!=22||creditSprites.sprites.size()!=25)throw std::runtime_error("Original attract credit bank mismatch");
    gasstand.load(rootPath);gasstandLoaded=true;
    attractCards.load(rootPath);
    attractDemo=original::OriginalDemoData::load(rootPath);
    demoOverlays.load(rootPath);
    rankingRecords=original::OriginalRankingRecords::load(rootPath/"data/original_assets/attract/ranking/factory_records.idasrank");
    attractAudioEvents.clear();
    if(preloadArtwork){
        // Load native menu banks while the title is visible. The worker owns
        // its result completely; the UI adopts it only after completion.
        // Early navigation can still load one needed bank synchronously.
        preloadedBanks=std::async(std::launch::async,[directory=assetRoot]{
            std::map<std::string,Bank> result;
            for(const char* name:{"v3sS00common","v3sS04maker","v3sS00emblem",
                    "v3sS05cars","v3sS00minicar","v3sS06mission","v3sK00common",
                    "v3sK01course","v3sB01course","v3sT02route","v3sT03weather","v3sT04time",
                    "v3sK00rivalface","v3sK02rival"}){
                const auto folder=directory/name;
                result.emplace(name,Bank{NativeModel::load(folder/(std::string(name)+".idasmesh")),
                    NativeTextureBank::load(folder/"textures/textures.idastex")});
            }
            return result;
        });
    }
}
void Frontend::collectPreloadedArtwork(){
    if(preloadedBanks.valid()&&preloadedBanks.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        auto ready=preloadedBanks.get();banks.merge(ready);
    }
}
Frontend::Bank& Frontend::bank(const std::string& name) {
    collectPreloadedArtwork();
    auto found=banks.find(name);
    if(found!=banks.end()) return found->second;
    const auto folder=assetRoot/name;
    Bank loaded{NativeModel::load(folder/(name+".idasmesh")),NativeTextureBank::load(folder/"textures"/"textures.idastex")};
    return banks.emplace(name,std::move(loaded)).first->second;
}
void Frontend::change(int delta) {
    synchronizeStage();
    if(!delta || !inputReady()) return;
    // A native key repeat traverses the original choice set. Animation and
    // original cabinet wheel repeat timing are not asserted by this function.
    switch(stage) {
    case FrontendStage::SaveSelect:
        if(saveDeleteRequested>=0)break;
        if(saveDeleteOpen)saveDeleteSelected=wrapped(saveDeleteSelected+delta,2);
        else if(saveActionsOpen)saveActionSelected=wrapped(saveActionSelected+delta,3);
        else saveSelected=int(wrapped(saveSelected+delta,int(saveFiles.size())));
        ++canvasRevision;previousKey.clear();
        break;
    case FrontendStage::Make: {
        const auto index=std::find(makeDisplayOrder.begin(),makeDisplayOrder.end(),make)-makeDisplayOrder.begin();
        make=makeDisplayOrder[std::size_t(wrapped(int(index)+delta,7))]; const auto choices=carsForMake(make);
        if(std::find(choices.begin(),choices.end(),car)==choices.end()) car=choices.front();
        break;
    }
    case FrontendStage::Car: {
        synchronizeCarousel();
        const auto choices=carsForMake(make); const auto current=std::find(choices.begin(),choices.end(),car);
        const int index=current==choices.end()?0:int(current-choices.begin());
        const int count=int(choices.size()),next=wrapped(index+delta,count);
        // Original 1B5880: the selected slot moves within the four-car window;
        // at either edge the row itself scrolls for four original frames.
        if(count>4) {
            if(wrapped(index-next,count)==1) {
                if(carouselSlot>0){--carouselSlot;carouselScroll=0;}
                else carouselScroll=-4;
            } else if(wrapped(next-index,count)==1) {
                if(carouselSlot<=2){++carouselSlot;carouselScroll=0;}
                else carouselScroll=4;
            }
        } else {
            if(next<index && carouselSlot>0)--carouselSlot;
            if(next>index && carouselSlot<=2)++carouselSlot;
        }
        car=choices[std::size_t(next)];carouselCar=car;displayedScroll=carouselScroll;
        break;
    }
    case FrontendStage::Course: {
        synchronizeCourseCarousel();
        const auto choices=courseChoices();const int count=int(choices.size());
        const int index=int(std::find(choices.begin(),choices.end(),course)-choices.begin());
        const int next=wrapped(index+delta,count);
        // Original194A00 selects one of five visible slots and scrolls at
        // either end over five original frames.
        if(wrapped(index-next,count)==1) {
            if(courseSlot>0){--courseSlot;courseScroll=0;}else courseScroll=-5;
        } else if(wrapped(next-index,count)==1) {
            if(courseSlot<=3){++courseSlot;courseScroll=0;}else courseScroll=5;
        }
        course=choices[std::size_t(next)];carouselCourse=course;displayedCourseScroll=courseScroll;
        if(course==8){wet=true;night=true;}break;
    }
    case FrontendStage::Transmission: if(delta%2) automatic=!automatic; break;
    case FrontendStage::Name:nameInput.steering=delta>0?1.f:-1.f;break;
    case FrontendStage::TuningCourse:tuningCourseSelected=unsigned(wrapped(int(tuningCourseSelected)+delta,int(tuningCourseMenu.count504)));break;
    case FrontendStage::Mode:
        gameMode=original::OriginalGameMode(wrapped(int(gameMode)+delta,3));
        original::selectOriginalGameMode(modeState,gameMode);break;
    case FrontendStage::Route: if(delta%2) reverse=!reverse; break;
    case FrontendStage::Weather: if(delta%2) wet=!wet; break;
    case FrontendStage::Time: if(delta%2) night=!night; break;
    case FrontendStage::Rival:
        rivalChoice=wrapped(rivalChoice+delta,int(original::originalLegendChoices(battleProfile,unsigned(course)).count));selectRival();break;
    case FrontendStage::Title: break;
    }
}
bool Frontend::back() {
    synchronizeStage();
    if(carConfirmationFrame>=0)return true;
    if(stage==FrontendStage::Title) return false;
    if(stage==FrontendStage::Name){if(inputReady())nameInput.backPressed=true;return true;}
    // Main12AD80 has confirm/timeout input, no cancel branch.
    if(stage==FrontendStage::TuningCourse)return true;
    if(stage==FrontendStage::SaveSelect){
        if(saveDeleteRequested>=0)return true;
        if(saveDeleteOpen){saveDeleteOpen=false;saveDeleteSelected=0;saveDeleteFailed=false;previousKey.clear();return true;}
        if(saveActionsOpen){saveActionsOpen=false;previousKey.clear();return true;}
        stage=FrontendStage::Title;savedDriverSelected=false;synchronizeStage();return true;
    }
    if((stage==FrontendStage::Make&&changingSavedCar)||(stage==FrontendStage::Mode&&savedDriverSelected)){
        changingSavedCar=false;stage=FrontendStage::SaveSelect;saveActionsOpen=saveActionsEnabled;saveActionSelected=0;
        synchronizeStage();return true;
    }
    if(stage==FrontendStage::Mode){stage=FrontendStage::Transmission;return true;}
    if(stage==FrontendStage::Car){
        if(inputReady()&&(changingSavedCar||!(carTransition.profileFlags1180&8u))){
            if(changingSavedCar)carTransition.profileFlags1180&=~8u;
            carCancelPending=true;carConfirmationFrame=0;
        }
        return true;
    }
    if(stage==FrontendStage::Rival){stage=FrontendStage::Course;return true;}
    if(course==8&&stage==FrontendStage::Time){stage=FrontendStage::Route;return true;}
    stage=FrontendStage(int(stage)-1); return true;
}
void Frontend::initializeColorSelection(){
    const auto roster=carsForMake(make);const auto found=std::find(roster.begin(),roster.end(),car);
    if(found==roster.end())throw std::logic_error("Car color owner has a different manufacturer");
    std::vector<std::uint32_t> counts;for(int id:roster)counts.push_back(original::originalCarColorCounts[std::size_t(id)]);
    const auto saved=battleProfile.u(64),selected=unsigned(found-roster.begin());
    original::initializeOriginalCarColorSelection(colorSelection,counts,selected,saved<counts[selected]?saved:0);
    colorSelectionMake=make;pendingColorButtons=0;
}
unsigned Frontend::selectedColor()const{
    if(stage==FrontendStage::Car&&stageInitialized&&observedStage==stage&&colorSelectionMake==make&&
            colorSelection.currentColor684<original::originalCarColorCounts.at(std::size_t(car)))return colorSelection.currentColor684;
    const auto saved=battleProfile.u(64);return saved<original::originalCarColorCounts.at(std::size_t(car))?saved:0;
}
void Frontend::driverProfileLoaded(){
    // At entry the source seeds the saved color. While browsing, its own
    // remembered colors determine the preview independently of card progress.
    if(stage==FrontendStage::Car&&stageInitialized&&observedStage==stage){
        if(carTransition.phase456==0||changingSavedCar)initializeColorSelection();
        carTransition.profileFlags1180=battleProfile.u(1180);
    }
    if(stage==FrontendStage::Transmission&&stageInitialized&&observedStage==stage&&transmissionTransition.phase476==0){
        transmissionTransition.profileTransmission68=battleProfile.u(68);automatic=battleProfile.u(68)==0;
        transmissionTransition.selected512=battleProfile.u(68);
    }
}
void Frontend::changeColor(int delta){
    if(stage==FrontendStage::SaveSelect){change(-delta);return;}
    if(stage==FrontendStage::Name){if(inputReady()&&delta)nameInput.rowJump=delta>0?-1:1;return;}
    synchronizeStage();if(stage!=FrontendStage::Car||!inputReady()||!delta)return;
    pendingColorButtons|=delta>0?0x20:0x10;
}
void Frontend::setNameSteering(float steering){
    if(!std::isfinite(steering))throw std::invalid_argument("Invalid name-entry steering");
    nameInput.steering=std::clamp(steering,-1.f,1.f);
}
bool Frontend::confirm() {
    synchronizeStage();
    if(!inputReady())return false;
    if(startRequested)return false;
    if(stage==FrontendStage::Title){titleConfirmPending=true;return false;}
    if(stage==FrontendStage::SaveSelect){
        if(saveFileChosen||saveCarChangeRequested||saveDeleteRequested>=0)return false;
        if(saveDeleteOpen){
            if(saveDeleteSelected==1&&saveFiles.at(std::size_t(saveSelected)).used)saveDeleteRequested=saveSelected;
            else {saveDeleteOpen=false;saveDeleteSelected=0;saveDeleteFailed=false;}
            previousKey.clear();return false;
        }
        if(saveActionsEnabled&&saveFiles.at(std::size_t(saveSelected)).used){
            if(!saveActionsOpen){saveActionsOpen=true;saveActionSelected=0;previousKey.clear();return false;}
            if(saveActionSelected==1){saveCarChangeRequested=true;return false;}
            if(saveActionSelected==2){saveDeleteOpen=true;saveDeleteSelected=0;saveDeleteFailed=false;previousKey.clear();return false;}
        }
        saveFileChosen=true;return false;
    }
    if(stage==FrontendStage::Name){nameInput.confirmPressed=true;return false;}
    if(stage==FrontendStage::TuningCourse){tuningCourseConfirmPending=true;carConfirmationFrame=0;return false;}
    if(stage==FrontendStage::Transmission){transmissionConfirmPending=true;carConfirmationFrame=0;return false;}
    if(stage==FrontendStage::Mode){modeConfirmPending=true;carConfirmationFrame=0;return false;}
    if(choiceStage(stage)){carConfirmationFrame=0;return false;}
    if(stage==FrontendStage::Car){synchronizeCarousel();carConfirmPending=true;carConfirmationFrame=0;return false;}
    if(stage==FrontendStage::Course || stage==FrontendStage::Rival){carConfirmationFrame=0;return false;}
    if(stage==FrontendStage::Make) {
        const auto choices=carsForMake(make);
        if(std::find(choices.begin(),choices.end(),car)==choices.end()) car=choices.front();
        makerConfirmPending=true;carConfirmationFrame=0;return false;
    }
    stage=FrontendStage(int(stage)+1); return false;
}
bool Frontend::clickSaveMenu(float x,float y){
    if(stage!=FrontendStage::SaveSelect||!std::isfinite(x)||!std::isfinite(y))return false;
    if(saveFileChosen||saveCarChangeRequested||saveDeleteRequested>=0)return true;
    if(saveDeleteOpen){
        if(y>=270&&y<306){
            if(x>=180&&x<308){saveDeleteSelected=0;confirm();}
            else if(x>=332&&x<460){saveDeleteSelected=1;confirm();}
        }
        return true;
    }
    for(int slot=0;slot<int(saveFiles.size());++slot){
        const int top=110+slot*59;
        if(x>=42&&x<294&&y>=top&&y<top+50){
            saveSelected=slot;saveActionsOpen=false;saveActionSelected=0;
            previousKey.clear();confirm();return true;
        }
    }
    if(saveActionsOpen&&y>=365&&y<395){
        if(x>=326&&x<449){saveActionSelected=0;previousKey.clear();confirm();}
        else if(x>=459&&x<582){saveActionSelected=1;previousKey.clear();confirm();}
    }
    if(saveActionsOpen&&x>=326&&x<582&&y>=399&&y<425){
        saveActionSelected=2;previousKey.clear();confirm();
    }
    return true;
}
bool Frontend::hoverSaveMenu(float x,float y){
    if(stage!=FrontendStage::SaveSelect||!std::isfinite(x)||!std::isfinite(y))return false;
    if(saveDeleteOpen&&saveDeleteRequested<0&&y>=270&&y<306){
        const int selected=x>=180&&x<308?0:x>=332&&x<460?1:-1;
        if(selected>=0&&selected!=saveDeleteSelected){saveDeleteSelected=selected;previousKey.clear();}
    }
    return true;
}
void Frontend::finishSaveDeletion(bool success){
    saveDeleteRequested=-1;saveDeleteSelected=0;saveDeleteFailed=!success;
    if(success){
        saveFiles.at(std::size_t(saveSelected))={};saveDeleteOpen=false;saveActionsOpen=false;
        saveActionSelected=0;savedDriverSelected=false;changingSavedCar=false;
    }
    previousKey.clear();
}
void Frontend::selectRival(){
    battleProfile.setu(0,0);battleProfile.setu(16,unsigned(car));battleProfile.setu(4,unsigned(course));
    original::selectOriginalRival(battleProfile,original::originalLegendRivalId(unsigned(course),unsigned(rivalChoice)));
    const auto selected=original::originalBattleSelection(battleProfile);
    reverse=selected.direction!=0;wet=selected.weather!=0;night=selected.night!=0;
}
void Frontend::selectBuntaCourse(){
    const auto choices=courseChoices();const auto selectedCourse=std::find(choices.begin(),choices.end(),course);
    if(selectedCourse==choices.end())throw std::logic_error("Selected Bunta course is outside its eight original slots");
    battleProfile.setu(0,2);battleProfile.setu(16,unsigned(car));
    original::selectOriginalBuntaCourse(battleProfile,unsigned(selectedCourse-choices.begin()));
    const auto selected=original::originalBattleSelection(battleProfile);
    course=int(selected.course);reverse=selected.direction!=0;wet=selected.weather!=0;night=selected.night!=0;
}
void Frontend::synchronizeCarousel() {
    if(carouselMake==make && carouselCar==car)return;
    carouselMake=make;carouselCar=car;carouselSlot=0;
    carouselScroll=displayedScroll=0;carFrame=0;carConfirmationFrame=-1;
}
void Frontend::appendSelectionMusic(const original::OriginalSelectionMusicCommands& commands){
    selectionMusicCommands.insert(selectionMusicCommands.end(),commands.begin(),commands.end());
}
void Frontend::endSelectionMusic(std::int32_t scene){
    appendSelectionMusic(original::changeOriginalSelectionMusicScene(selectionMusic,scene));
    selectionMusicStageInitialized=false;selectionExitFrame=-1;
}
void Frontend::synchronizeSelectionMusicStage(){
    if(selectionMusicStageInitialized&&selectionMusicObservedStage==stage)return;
    selectionMusicStageInitialized=true;selectionMusicObservedStage=stage;
    appendSelectionMusic(original::changeOriginalSelectionMusicScene(selectionMusic,stage==FrontendStage::Title?0:1));
    if(stage==FrontendStage::Title)return;
    // Source maker/car/mission owners request TYPE. Mode/rival and the
    // Time Attack/Bunta course owners request SELECT, retained through choices.
    // SaveSelect is not a source owner -- it stands in for the card reader --
    // and it leads straight into Make, so it takes TYPE too. Left on the
    // default SELECT it stopped, unloaded and reloaded the bank one frame
    // after the player picked a file.
    const auto cue=stage==FrontendStage::SaveSelect||stage==FrontendStage::Make||stage==FrontendStage::Car||stage==FrontendStage::Transmission||stage==FrontendStage::Name||stage==FrontendStage::TuningCourse
        ?original::OriginalSelectionMusicCue::Type:original::OriginalSelectionMusicCue::Select;
    appendSelectionMusic(original::requestOriginalSelectionMusic(selectionMusic,cue));
}
void Frontend::synchronizeStage(){
    synchronizeSelectionMusicStage();
    if(stageInitialized&&observedStage==stage)return;
    observedStage=stage;stageInitialized=true;carFrame=0;carConfirmationFrame=-1;selectionExitFrame=-1;
    buntaBadgeCourse=-1;buntaBadgeFrame=0;
    makerConfirmPending=false;frameRemainder=0;
    carConfirmPending=carCancelPending=false;
    transmissionConfirmPending=false;
    tuningCourseConfirmPending=false;
    modeConfirmPending=false;
    if(stage==FrontendStage::Title){attractExit={};titleConfirmPending=false;titleExitBlack=false;enterAttractChild(3);}
    else attractAudioEvents.push_back({~0u,0,false});
    if(stage==FrontendStage::Make){
        makerTransition={};
        makerTransition.selected440=std::uint32_t(std::find(makeDisplayOrder.begin(),makeDisplayOrder.end(),make)-makeDisplayOrder.begin());
        //1195E0/192780/1346A0 initialize this temporary selection-session timer
        // after native profile loading; it is not an earned driver statistic.
        makerTransition.sharedCountdown1176=2479;
        makerTransition.profileFlags1180=battleProfile.u(1180);
        original::initializeOriginalMakerTransition(makerTransition);makerConfirmationPhase=0;
    }
    if(stage==FrontendStage::Car){
        carTransition={};carTransition.sharedCountdown1176=battleProfile.u(1176)?battleProfile.u(1176):2479;
        carTransition.profileFlags1180=battleProfile.u(1180);carTransition.previousScreen76=4;
        original::initializeOriginalCarMenuTransition(carTransition);carConfirmationPhase=0;
        initializeColorSelection();
    }
    if(stage==FrontendStage::Transmission){
        transmissionTransition={};transmissionTransition.sharedCountdown1176=battleProfile.u(1176)?battleProfile.u(1176):2479;
        transmissionTransition.profileFlags1180=battleProfile.u(1180);transmissionTransition.profileCar16=unsigned(car);
        transmissionTransition.profileTransmission68=battleProfile.u(68);
        transmissionTransition.profileByte1192=battleProfile.byte(1192);transmissionTransition.profileByte152=battleProfile.byte(152);
        transmissionTransition.previousScreen76=0x0602;transmissionTransition.alternateScreen80=0x0802;
        original::initializeOriginalTransmissionMenuTransition(transmissionTransition);transmissionConfirmationPhase=0;
        automatic=transmissionTransition.selected512==0;
    }
    if(stage==FrontendStage::Mode){
        modeTransition={};modeTransition.profileMode0=battleProfile.u(0);
        modeTransition.profilePoints72=battleProfile.u(72);modeTransition.profileFlags1180=battleProfile.u(1180);
        modeTransition.profileByte1191=battleProfile.byte(1191);
        original::initializeOriginalModeMenuTransition(modeTransition);
        gameMode=original::OriginalGameMode(modeTransition.selected564);
        modeState={};original::selectOriginalGameMode(modeState,gameMode);modeConfirmationPhase=0;
        battleProfile.setu(1176,modeTransition.sharedCountdown1176);
    }
    if(stage==FrontendStage::Name){
        if(!namePresentation){nameTables=original::OriginalNameEntryTables::load(projectRoot);
            namePresentation=std::make_unique<OriginalNameEntryPresentation>(OriginalNameEntryPresentation::load(projectRoot));}
        nameState={};nameInput={};nameCommittedForVisit=false;nameState.profileFlags1180=battleProfile.u(1180);nameState.profileKind1192=battleProfile.byte(1192);
        std::array<std::uint32_t,5> ids{};for(unsigned i=0;i<5;++i)ids[i]=battleProfile.u(44+4*i);
        original::initializeOriginalNameEntry(nameState,ids,battleProfile.u(76));namePresentation->reset();
        battleProfile.setu(1176,nameState.sharedCountdown1176);
    }
    if(stage==FrontendStage::TuningCourse){
        if(!tuningData)tuningData=std::make_unique<original::OriginalTuningData>(original::OriginalTuningData::load(projectRoot));
        if(!tuningPresentation)tuningPresentation=std::make_unique<OriginalTuningCoursePresentation>(OriginalTuningCoursePresentation::load(projectRoot));
        tuningCourseMenu={};original::initializeOriginalTuningCourseMenu(tuningCourseMenu,battleProfile,*tuningData);
        tuningCourseSelected=tuningCourseMenu.selected496;
        tuningPresentation->reset(tuningCourseMenu,battleProfile);
    }
}
void Frontend::enterAttractChild(unsigned child){
    attractChildId=child;titleFrame=0;titleFade=0;previousKey.clear();rankingInput={};
    if(child>=3&&child<=5)attractCards.reset(child);
    else if(child==6||child==8){attractTitle={};attractTitle.child=child;titleFade=child==6?0xff000000:0;}
    else if(child==7){attractDemoCursor={0,attractDemo.shots().front().frames[2]};displayedDemoCursor=attractDemoCursor;attractDemoWrapped=false;}
    else if(child==11)gasstand.reset(nextAttractScript);
    else if(child==12)rankingPlayback.reset(rankingPlayback.page.persistedCourseIndex);
    else throw std::logic_error("Unsupported native attract child");
    // Entry is observable even if the first render has zero elapsed time.
    attractAudioEvents.push_back({child,0,false});
}
void Frontend::advanceAttractChild(){
    const unsigned oldFrame=attractChildId>=3&&attractChildId<=5?attractCards.frame():titleFrame;
    attractAudioEvents.push_back({attractChildId,oldFrame,false});
    bool completed=false;
    if(attractChildId>=3&&attractChildId<=5){attractCards.step();completed=attractCards.completed();}
    else if(attractChildId==6||attractChildId==8){original::stepOriginalAttractTitle(attractTitle);titleFade=attractTitle.fadeArgb;completed=attractTitle.completed;}
    else if(attractChildId==7){
        // 0E71DA enters state1 only after 157640 reports the recorded pose wrap.
        // Its completion threshold is evaluated on the next owner update.
        if(attractDemoWrapped)completed=titleFrame>5830;
        else{
            displayedDemoCursor=attractDemoCursor;
            attractDemoWrapped=attractDemo.step(attractDemoCursor)==2;
        }
        if(completed)attractAudioEvents.push_back({7,oldFrame,true});
    }else if(attractChildId==11){gasstand.step();completed=gasstand.completed();
        if(completed)nextAttractScript=original::nextOriginalGasstandScript(gasstand.state().script);
    }else if(attractChildId==12){rankingPlayback.step(rankingRecords,rankingInput);rankingInput={};completed=rankingPlayback.completed();}
    ++titleFrame;
    if(completed){
        const auto completion=original::originalAttractCompletion(attractChildId,false,false);
        const unsigned next=completion.requestedChild<0?*original::originalAttractNextChild(attractChildId,0):unsigned(completion.requestedChild);
        enterAttractChild(next);
    }
}
void Frontend::advance(double seconds) {
    if(!std::isfinite(seconds) || seconds<0)throw std::invalid_argument("Invalid menu frame duration");
    synchronizeStage();
    if(stage==FrontendStage::Car)synchronizeCarousel();
    if(stage==FrontendStage::Course)synchronizeCourseCarousel();
    if(stage==FrontendStage::Mode)original::selectOriginalGameMode(modeState,gameMode);
    frameRemainder+=seconds*60.;
    const auto frames=std::uint64_t(std::floor(frameRemainder+1.e-9));
    frameRemainder-=double(frames);
    const auto updateOwner=[&]() -> bool {
        ++carFrame;
        if(stage==FrontendStage::Course&&gameMode==original::OriginalGameMode::BuntaChallenge)
            buntaBadgeFrame=std::min(buntaBadgeFrame+1,34u);
        if(selectionExitFrame>=0){
            //138840 increments owner+440 before135620 services the manager.
            // The phase2 handoff sets+444, not this counter: fade starts on
            // the next update, and phase4 Stop follows15 updates later.
            if(++selectionExitFrame==1)
                appendSelectionMusic(original::exitOriginalSelectionMusic(selectionMusic));
            if(selectionExitFrame>15){
                appendSelectionMusic(original::stopOriginalSelectionMusic(selectionMusic));
                selectionExitFrame=-1;carConfirmationFrame=-1;startRequested=true;
                frameRemainder=0;return true;
            }
            return false;
        }
        if(stage==FrontendStage::Title){
            // Native Start supplies the accepted-credit event.02CA00 then
            // waits three parent updates before entering the profile menu.
            const auto events=original::tickOriginalAttractExit(attractExit,titleConfirmPending);
            titleConfirmPending=false;titleExitBlack=events.skipChildUpdate||events.clearBackgroundBlack;
            if(events.clearBackgroundBlack)attractAudioEvents.push_back({~0u,0,false});
            if(events.finishRequested){stage=FrontendStage::SaveSelect;synchronizeStage();return true;}
            if(!events.skipChildUpdate&&!events.clearBackgroundBlack){
                advanceAttractChild();
            }
            return false;
        }
        if(stage==FrontendStage::Make){
            const auto slot=std::uint32_t(std::find(makeDisplayOrder.begin(),makeDisplayOrder.end(),make)-makeDisplayOrder.begin());
            const auto events=original::tickOriginalMakerTransition(makerTransition,{makerConfirmPending,slot,battleProfile.u(40)});
            makerConfirmPending=false;
            if(events.selectionChanged)carFrame=1;
            if(events.selectionCommitted){
                const auto rawMaker=profileMakeIds.at(std::size_t(make));
                if(events.resetSelectedCar){car=int(profileMakeFirstCars[rawMaker]);battleProfile.setu(16,unsigned(car));battleProfile.setu(1168,0);battleProfile.setu(68,0);}
                battleProfile.setu(40,rawMaker);battleProfile.setu(1164,profileMakeCounts[rawMaker]);battleProfile.setu(1172,profileMakeStarts[rawMaker]);
            }
            if(events.confirmationPhaseWritten)makerConfirmationPhase=events.confirmationPhase;
            carConfirmationFrame=makerTransition.phase448>=2?int(makerTransition.frame444):-1;
            if(events.parentRequested){
                battleProfile.setu(1176,makerTransition.sharedCountdown1176);battleProfile.setu(1180,makerTransition.profileFlags1180);
                if(changingSavedCar)saveCarPreviewReset=true;
                stage=FrontendStage::Car;synchronizeStage();return true;
            }
            return false;
        }
        if(stage==FrontendStage::Car){
            if(colorSelectionMake!=make)initializeColorSelection();
            if(carTransition.phase456==1){
                const auto roster=carsForMake(make);const auto local=unsigned(std::find(roster.begin(),roster.end(),car)-roster.begin());
                const auto events=original::stepOriginalCarColorSelection(colorSelection,{local,pendingColorButtons,
                    carConfirmPending||std::bit_cast<std::int32_t>(carTransition.sharedCountdown1176)<=1});
                if(events.profileColorWritten)battleProfile.setu(64,colorSelection.profileColor64);
            }
            pendingColorButtons=0;
            const auto events=original::tickOriginalCarMenuTransition(carTransition,{carConfirmPending,carCancelPending});
            carConfirmPending=carCancelPending=false;
            if(events.selectionCommitted){
                const auto roster=carsForMake(make);
                if(battleProfile.u(16)!=unsigned(car))battleProfile.setu(68,0);
                battleProfile.setu(16,unsigned(car));
                battleProfile.setu(1168,unsigned(std::find(roster.begin(),roster.end(),car)-roster.begin()));
            }
            if(events.confirmationPhaseWritten)carConfirmationPhase=events.confirmationPhase;
            carConfirmationFrame=carTransition.phase456>=2?int(carTransition.frame452):-1;
            displayedScroll=carouselScroll;
            if(carouselScroll>0)--carouselScroll;else if(carouselScroll<0)++carouselScroll;
            if(events.parentRequested){
                battleProfile.setu(1176,carTransition.sharedCountdown1176);
                stage=carTransition.phase456==5?FrontendStage::Make:FrontendStage::Transmission;synchronizeStage();return true;
            }
            return false;
        }
        if(stage==FrontendStage::Transmission){
            const auto events=original::tickOriginalTransmissionMenuTransition(transmissionTransition,{transmissionConfirmPending,automatic?0u:1u});
            transmissionConfirmPending=false;
            if(events.selectionCommitted)battleProfile.setu(68,transmissionTransition.profileTransmission68);
            if(events.profileByte152Cleared)battleProfile.setByte(152,transmissionTransition.profileByte152);
            if(events.confirmationPhaseWritten)transmissionConfirmationPhase=events.confirmationPhase;
            carConfirmationFrame=transmissionTransition.phase476>=2?int(transmissionTransition.frame452):-1;
            if(events.parentRequested){
                battleProfile.setu(1176,transmissionTransition.sharedCountdown1176);
                const auto command=transmissionTransition.parentEvent64;
                if(command==8||command==0x08020004u)stage=FrontendStage::Mode;
                else if(command==0x06020004u)stage=FrontendStage::Name;
                else if(command&1u)stage=FrontendStage::TuningCourse;
                else throw std::logic_error("Unsupported original transmission parent route");
                synchronizeStage();return true;
            }
            return false;
        }
        if(stage==FrontendStage::TuningCourse){
            const auto events=original::tickOriginalTuningCourseMenu(tuningCourseMenu,battleProfile,
                {tuningCourseSelected,tuningCourseConfirmPending,tuningPresentation->selectorPhase()});
            tuningCourseConfirmPending=false;
            tuningPresentation->advance(tuningCourseMenu,battleProfile);
            menuCueIds.insert(menuCueIds.end(),events.cueIds.begin(),events.cueIds.end());
            carConfirmationFrame=tuningCourseMenu.phase464>=2?int(tuningCourseMenu.confirmationFrames460):-1;
            if(events.profileCommitted)driverProfileCommit=true;
            if(events.parentRequested){
                const auto command=tuningCourseMenu.parentEvent64;
                if(command&1u)stage=FrontendStage::Name;
                else if(command==0x08020004u)stage=FrontendStage::Mode;
                else throw std::logic_error("Unsupported original tuning-course parent route");
                synchronizeStage();return true;
            }
            return false;
        }
        if(stage==FrontendStage::Name){
            const auto events=original::tickOriginalNameEntry(nameState,nameInput,nameTables);
            nameInput.confirmPressed=nameInput.backPressed=false;nameInput.rowJump=0;
            menuCueIds.insert(menuCueIds.end(),events.cueIds.begin(),events.cueIds.end());
            battleProfile.setu(1176,nameState.sharedCountdown1176);
            if(events.nameCommitted){
                for(unsigned i=0;i<nameState.length528;++i)battleProfile.setu(44+4*i,nameState.glyphIds480[i]);
                battleProfile.setu(76,nameState.length528);battleProfile.setu(1180,nameState.profileFlags1180);driverProfileCommit=true;
                nameCommittedForVisit=true;
            }
            if(events.requestCardState10)nameState.selectedCardState96=10;
            namePresentation->advance(nameState);
            if(events.parentRequested){
                const auto command=nameState.parentEvent64;
                if(command!=8&&command!=0x08010004u&&command!=0x08020004u)
                    throw std::logic_error("Unsupported original name parent route");
                // Native card-check boundary: these two subgroup8 routes run
                // physical card completion before the source mode owner.
                if(nameCommittedForVisit){original::finishOriginalDriverSetupFlag(battleProfile);
                    driverProfileCommit=true;driverSetupCompleted=true;}
                stage=FrontendStage::Mode;synchronizeStage();return true;
            }
            return false;
        }
        if(stage==FrontendStage::Mode){
            modeTransition.profilePoints72=battleProfile.u(72);modeTransition.profileFlags1180=battleProfile.u(1180);
            const auto events=original::tickOriginalModeMenuTransition(modeTransition,{modeConfirmPending,std::uint32_t(gameMode)});
            modeConfirmPending=false;gameMode=original::OriginalGameMode(modeTransition.selected564);
            original::selectOriginalGameMode(modeState,gameMode);original::stepOriginalModeMenu(modeState);
            original::advanceOriginalModeMenuDraw(modeTransition);
            if(events.selectionCommitted){battleProfile.setu(0,modeTransition.profileMode0);battleProfile.setByte(1191,modeTransition.profileByte1191);}
            if(events.confirmationPhaseWritten)modeConfirmationPhase=events.confirmationPhase;
            battleProfile.setu(1176,modeTransition.sharedCountdown1176);
            carConfirmationFrame=modeTransition.phase468>=2?int(modeTransition.confirmationFrame452):-1;
            if(events.parentRequested){stage=FrontendStage::Course;synchronizeStage();return true;}
            return false;
        }
        displayedScroll=carouselScroll;
        if(carouselScroll>0)--carouselScroll;
        else if(carouselScroll<0)++carouselScroll;
        displayedCourseScroll=courseScroll;
        if(courseScroll>0)--courseScroll;
        else if(courseScroll<0)++courseScroll;
        //1321FC emits AA0 when the Legend rival owner accepts confirmation,
        // before its existing hold/exit phases. Input calls alone do not tick.
        if(stage==FrontendStage::Rival && gameMode==original::OriginalGameMode::LegendOfTheStreets && carConfirmationFrame==0)
            appendSelectionMusic(original::exitOriginalSelectionMusic(selectionMusic));
        // Legend: accepted-confirm tick,121 hold,8 fade,5 parent-delay ticks.
        const int confirmationFrames=stage==FrontendStage::Rival?135:
            (stage==FrontendStage::Course&&gameMode==original::OriginalGameMode::BuntaChallenge)?122:
            (stage==FrontendStage::Course || stage==FrontendStage::Route || stage==FrontendStage::Weather || stage==FrontendStage::Time)?31:36;
        if(carConfirmationFrame>=0 && ++carConfirmationFrame>=confirmationFrames) {
            if(gameMode==original::OriginalGameMode::TimeAttack &&
                    (stage==FrontendStage::Time || ((courseRequiresNight(unsigned(course)))&&stage==FrontendStage::Weather) ||
                     (course==8&&stage==FrontendStage::Route))){
                //138740 completes the choice, then enters phase3. Happo and
                // Snow take the same final exit after their forced choices.
                if(courseRequiresNight(unsigned(course)))night=true;
                if(course==8){wet=true;night=true;}
                selectionExitFrame=0;
                return false;
            }
            carConfirmationFrame=-1;
            if(stage==FrontendStage::Time || stage==FrontendStage::Rival)startRequested=true;
            else if(stage==FrontendStage::Course&&gameMode==original::OriginalGameMode::BuntaChallenge){
                selectBuntaCourse();startRequested=true;
            }
            else if(stage==FrontendStage::Course&&gameMode==original::OriginalGameMode::LegendOfTheStreets){
                rivalChoice=int(original::originalLegendChoices(battleProfile,unsigned(course)).selected);selectRival();stage=FrontendStage::Rival;carFrame=0;
            }
            else if(stage==FrontendStage::Course&&gameMode==original::OriginalGameMode::TimeAttack){
                // Start a fresh setup on the left choices. Saved race flags and
                // browsing Snow must not preselect the next course's conditions.
                // Later pages keep any choices the driver explicitly changes.
                reverse=false;wet=course==8;night=courseRequiresNight(unsigned(course));
                stage=FrontendStage::Route;
            }
            // Original1387A0/1387BC force night and finish the TA setup here.
            // Snow has independent snow and wet flags in the original physics.
            else if((courseRequiresNight(unsigned(course))) && stage==FrontendStage::Weather){night=true;startRequested=true;}
            else if(course==8 && stage==FrontendStage::Route){wet=true;night=true;startRequested=true;}
            else {stage=FrontendStage(int(stage)+1);if(stage==FrontendStage::Course)synchronizeCourseCarousel();}
            frameRemainder=0;return true;
        }
        return false;
    };
    for(std::uint64_t i=0;i<frames;++i){
        const bool stopBatch=updateOwner();
        // Source056EC2 owner update precedes056ECE->1415E0->143060.
        // A child entered on this update can start its requested bank now.
        synchronizeSelectionMusicStage();
        appendSelectionMusic(original::tickOriginalSelectionMusic(selectionMusic));
        if(stopBatch)break;
    }
}
void Frontend::drawMakeStrip(int width,int height) {
    // Original1B68E0: seven emblems; only the selected emblem is full size.
    // 12CF34 divides the confirmation frame by36 for1B68C0.
    const float phase=makerConfirmationPhase;
    float x=.32f;
    for(int id:makeDisplayOrder) {
        const bool selected=id==make;
        x+=selected?.48f:.3999999761581421f;
        const float scale=selected?1.f:(1.f-phase)*.8f;
        if(selected && (phase<.01f || (carFrame&6u)))draw("v3sS00emblem",id+7,width,height,x*100.f,128.f);
        if(scale>0)draw("v3sS00emblem",id,width,height,x*100.f,128.f,scale);
        x+=selected?.48f:.3999999761581421f;
    }
}
void Frontend::synchronizeCourseCarousel() {
    const auto choices=courseChoices();
    if(std::find(choices.begin(),choices.end(),course)==choices.end())course=choices[3];
    // Source19A940 resets the staggered progress stars only on selection
    // changes. Keep this separate from the scrolling carousel's identity.
    if(gameMode==original::OriginalGameMode::BuntaChallenge&&buntaBadgeCourse!=course){
        buntaBadgeCourse=course;buntaBadgeFrame=0;
    }
    if(carouselCourse==course)return;
    carouselCourse=course;courseSlot=0;courseScroll=displayedCourseScroll=0;
}
void Frontend::prepareRankingAttractFixture(unsigned page){
    if(page>=4)throw std::out_of_range("Ranking fixture model page");
    stage=FrontendStage::Title;synchronizeStage();enterAttractChild(12);
    for(unsigned i=0;i<=page;++i){queueRankingInput({true,false});advance(1.0/60.0);}
}
void Frontend::drawBuntaCourseProgress(int width,int height){
    //18462C..18463A supplies levels by original course identity (Snow uses
    // the Akina progress word). The eight display slots swap Happo/Iroha.
    const unsigned progressCourse=course==8?3u:unsigned(course);
    const auto level=std::bit_cast<std::int32_t>(battleProfile.u(1080+progressCourse*4));
    const auto commands=original::originalBuntaMenuDraws(level,buntaBadgeFrame);
    auto& assets=bank("v3sB01course");
    const auto colored=original::materializeOriginalBuntaColors(assets.model.chunks.at(commands.backdropChunk),commands.progressColors);
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    SpritePlacement placement;placement.scale=100.f*fit;
    placement.offsetX=(width-640.f*fit)*.5f;placement.offsetY=(height-480.f*fit)*.5f;
    placement.invertY=true;placement.authoredHeight=0;
    compositeOriginalMenuChunk(pixels,width,height,assets.textures,colored,placement);
    for(const auto& star:commands.stars)
        draw("v3sK01course",int(star.selector&0xffffu),width,height,star.x*100.f,-star.y*100.f,star.scale);
}
std::array<std::uint32_t,3> Frontend::courseRecordTimes()const {
    if(isImportedCourse(course)){
        const auto saved=timeAttackBest?timeAttackBest(unsigned(course*2)+unsigned(reverse),unsigned(wet),unsigned(car)):TimeAttackBest{};
        const auto personal=importedPersonalBest?importedPersonalBest(unsigned(course*2)+unsigned(reverse),unsigned(wet),unsigned(car)):TimeAttackEntry{};
        return {saved.course,saved.model,personal.ticks6000};
    }
    const auto condition=unsigned(course*2+int(reverse));
    const bool recordWet=wet||course==8;
    const auto saved=timeAttackBest?timeAttackBest(condition,unsigned(recordWet),unsigned(car)):TimeAttackBest{};
    const auto baseline=original::originalDefaultTimeAttackRecord6000(condition);
    const auto personal=original::originalPersonalTimeAttackRecord(battleProfile,
        original::originalRecordPartition(condition,recordWet));
    if(timeAttackBest)return {saved.course,saved.model,personal.ticks6000};
    return {saved.course?std::min(saved.course,baseline):baseline,
            saved.model?std::min(saved.model,baseline):baseline,personal.ticks6000};
}
void Frontend::drawCourseRecords(int width,int height) {
    // Original d3h_TA_BestTimeDisp (199B20/199960/199880): authored
    // backing/labels and 10px digits, 8px punctuation at 66,360/380/400.
    draw("v3sT02route",0,width,height);
    draw("v3sT02route",15,width,height);
    const auto times=courseRecordTimes();
    for(unsigned row=0;row<times.size();++row){
        const auto ms=std::uint64_t(times[row])*1000/6000;
        const std::array<unsigned,8> digits={unsigned(ms/60000%10),10,unsigned(ms/10000%6),
            unsigned(ms/1000%10),11,unsigned(ms/100%10),unsigned(ms/10%10),unsigned(ms%10)};
        float x=66;
        for(auto digit:digits){
            const int chunk=digit==10?1:digit==11?3:times[row]?int(digit)+4:2;
            draw("v3sT02route",chunk,width,height,x,360.f+20.f*row);
            x+=digit>=10?8.f:10.f;
        }
    }
}
std::uint32_t Frontend::choiceFadeArgb()const {
    if(stage<FrontendStage::Course||stage>FrontendStage::Rival)return 0;
    // Course and condition pages are one continuous selection flow. Only
    // its final confirmation leaves the screen; TA owns that departure in
    // selectionExitFrame, including the forced Happo/Snow conditions.
    if(stage>=FrontendStage::Route&&stage<=FrontendStage::Time)return 0;
    if(stage==FrontendStage::Course&&gameMode==original::OriginalGameMode::TimeAttack)return 0;
    if(carConfirmationFrame>=0){
        const int end=stage==FrontendStage::Rival?130:
            stage==FrontendStage::Course&&gameMode==original::OriginalGameMode::BuntaChallenge?122:31;
        if(carConfirmationFrame>=end-8)return std::uint32_t(std::min(carConfirmationFrame-(end-8)+1,8)*255/8)<<24;
    }
    return carFrame<8?std::uint32_t((8-carFrame)*255/8)<<24:0;
}
void Frontend::drawCourseCarousel(int width,int height) {
    // Original194C60, literals2A1594/194D78/194D80. The extra off-edge
    // tile and signed scroll are retained from the original draw loop.
    const auto choices=courseChoices();const int count=int(choices.size());
    const int selectedIndex=int(std::find(choices.begin(),choices.end(),course)-choices.begin());
    int index=wrapped(selectedIndex-courseSlot,count);
    const int first=displayedCourseScroll>0?-1:0;
    const int end=displayedCourseScroll<0?6:5;
    // TimeAttack13896C supplies binary confirmation to194AC0. The generic
    // iSelCrs caller1150F2 instead uses frame/36; it is not the TA lifecycle.
    // Bunta184642..656 instead supplies frame/36, clamped by194AC0.
    const float phase=carConfirmationFrame<0?0.f:gameMode==original::OriginalGameMode::BuntaChallenge?
        std::clamp(float(carConfirmationFrame)/36.f,0.f,1.f):1.f;
    for(int row=first;row<end;++row) {
        const float x=120.f+(float(row)+float(displayedCourseScroll)/5.f)*100.f;
        const bool selected=row==courseSlot;
        if(selected && (phase<.01f || (carFrame&6u)))draw("v3sK01course",25,width,height,x,128.f);
        const float scale=selected?1.f:(1.f-phase)*.9f;
        if(scale>0){
            const int selectedCourse=choices[std::size_t(index)];
            if(isImportedCourse(selectedCourse))compositeImage(pixels,width,height,importedArtwork(selectedCourse).at(1),x-48*scale,128-32*scale,96*scale,64*scale);
            else draw("v3sK01course",courseThumbnailChunk[std::size_t(selectedCourse)],width,height,x,128.f,scale);
        }
        index=wrapped(index+1,count);
    }
}
void Frontend::drawChoiceMenu(int width,int height,bool movingWidgets) {
    original::OriginalChoiceMenuState state;
    state.course=isImportedCourse(course)?3:course;state.frame=carFrame;
    state.confirmationPhase=carConfirmationFrame<0?0.f:1.f;
    if(stage==FrontendStage::Transmission){state.screen=original::OriginalChoiceScreen::Transmission;state.selected=automatic?0:1;state.confirmationPhase=transmissionConfirmationPhase;}
    else if(stage==FrontendStage::Route){state.screen=original::OriginalChoiceScreen::Route;state.selected=reverse?1:0;}
    else if(stage==FrontendStage::Weather){state.screen=original::OriginalChoiceScreen::Weather;state.selected=wet?1:0;}
    else {state.screen=original::OriginalChoiceScreen::Time;state.selected=night?1:0;}
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    auto commands=original::originalChoiceMenuDraws(state);
    // Source glows have a smaller depth than their lettering despite being
    // submitted later. Preserve that order in this depthless CPU compositor.
    if(movingWidgets)std::stable_sort(commands.begin(),commands.end(),[](const auto& a,const auto& b){return a.z<b.z;});
    std::vector<original::OriginalChoiceDraw> underlay;
    if(!movingWidgets && stage!=FrontendStage::Transmission){
        if(isImportedCourse(course))drawHakoneBackdrop(width,height);
        else underlay=original::originalChoiceCourseUnderlay(course);
    }
    const auto underlayCount=underlay.size();
    commands.insert(commands.begin(),underlay.begin(),underlay.end());
    for(std::size_t index=0;index<commands.size();++index) {
        const auto& command=commands[index];
        const bool widget=command.y!=0;
        if(index>=underlayCount && widget!=movingWidgets)continue;
        auto& assets=bank(original::originalChoiceBankName(command.selector>>16));
        const auto& chunk=assets.model.chunks.at(command.selector&0xffffu);
        SpritePlacement placement;placement.scale=100.f*fit*command.scale;
        placement.offsetX=(width-640.f*fit)*.5f+command.x*100.f*fit;
        placement.offsetY=(height-480.f*fit)*.5f-command.y*100.f*fit;
        placement.invertY=true;placement.authoredHeight=0;
        if(command.colors==original::OriginalChoiceColors::Source)compositeOriginalMenuChunk(pixels,width,height,assets.textures,chunk,placement);
        else {
            const auto changed=original::materializeOriginalChoiceColors(chunk,command.colors);
            compositeOriginalMenuChunk(pixels,width,height,assets.textures,changed,placement);
        }
    }
}
void Frontend::drawRivalMenu(int width,int height) {
    const float phase=carConfirmationFrame<0?0.f:1.f;
    auto commands=original::originalLegendMenuDraws(battleProfile,std::uint32_t(course),std::uint32_t(rivalChoice),phase,carFrame);
    // The original selected portrait is in front of its glow even though the
    // glow is submitted afterward. Equal depths preserve submission order.
    std::stable_sort(commands.begin(),commands.end(),[](const auto& a,const auto& b){return a.z<b.z;});
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    for(const auto& command:commands) {
        auto& assets=bank(original::originalLegendMenuBankName(command.selector>>16));
        const auto& chunk=assets.model.chunks.at(command.selector&0xffffu);
        SpritePlacement placement;
        placement.scale=100.f*fit*command.scale;
        placement.offsetX=(width-640.f*fit)*.5f+command.x*100.f*fit;
        placement.offsetY=(height-480.f*fit)*.5f-command.y*100.f*fit;
        placement.invertY=true;placement.authoredHeight=0;
        if(command.colors==original::OriginalChoiceColors::Source)
            compositeOriginalMenuChunk(pixels,width,height,assets.textures,chunk,placement);
        else {
            const auto changed=original::materializeOriginalLegendColors(chunk,command.colors);
            compositeOriginalMenuChunk(pixels,width,height,assets.textures,changed,placement);
        }
    }
}
void Frontend::drawCarCarousel(int width,int height) {
    const auto choices=carsForMake(make);
    const int current=int(std::find(choices.begin(),choices.end(),car)-choices.begin());
    const int count=int(choices.size()),visible=std::min(count,4);
    int index=wrapped(current-carouselSlot,count);
    float x=1.12f+float(displayedScroll)*.25f;
    const float phase=carConfirmationPhase;
    for(int row=0;row<visible;++row) {
        const bool selected=row==carouselSlot;
        const float halfWidth=selected?.64f:.576f;
        x+=halfWidth;
        if(selected && (phase<.01f || (carFrame&6u))) {
            draw("v3sS05cars",36,width,height,x*100.f,126.f);
            if(phase>0)draw("v3sS05cars",36,width,height,x*100.f,126.f);
        }
        const float scale=selected?1.f:(1.f-phase)*.7f;
        if(scale>0)draw("v3sS00minicar",carImageChunk(choices[std::size_t(index)]),width,height,x*100.f,128.f,scale);
        x+=halfWidth;
        index=wrapped(index+1,count);
    }
}
void Frontend::drawCarColorIndicator(int width,int height){
    const auto count=original::originalCarColorCounts.at(std::size_t(car));
    const auto& rgb=original::originalCarPaintRgb.at(std::size_t(car));
    const auto commands=original::originalCarColorIndicatorDraws(unsigned(car),selectedColor(),std::span(rgb).first(count));
    auto& assets=bank("v3sS05cars");const float fit=std::min(float(width)/640.f,float(height)/480.f);
    struct PaletteBatch{NativeModelChunk chunk;SpritePlacement placement;float depth;};
    std::vector<PaletteBatch> batches;
    for(const auto& command:commands){
        SpritePlacement placement;placement.scale=100.f*fit;placement.invertY=true;placement.authoredHeight=0;
        placement.offsetX=(width-640.f*fit)*.5f+command.x*100.f*fit;
        placement.offsetY=(height-480.f*fit)*.5f-command.y*100.f*fit;
        for(auto batch:assets.model.chunks.at(command.selector&0xffffu).batches){
            //1B8540 mode2 changes only GMP diffuse0. The marker at z0 is
            //behind the painted swatch at z.001, despite submitting later.
            if(command.replaceColors)batch.material[3]=command.argb;
            float depth=0;for(const auto& vertex:batch.vertices)depth+=vertex.position.z+command.z;
            depth/=float(batch.vertices.size());NativeModelChunk chunk;chunk.batches.push_back(std::move(batch));
            batches.push_back({std::move(chunk),placement,depth});
        }
    }
    std::stable_sort(batches.begin(),batches.end(),[](const auto& a,const auto& b){return a.depth<b.depth;});
    for(const auto& batch:batches)compositeOriginalMenuChunk(pixels,width,height,assets.textures,batch.chunk,batch.placement);
}
void Frontend::draw(const std::string& name,int chunk,int width,int height,float x,float y,float multiplier,float opacity) {
    auto& selected=bank(name);
    if(chunk<0 || chunk>=int(selected.model.chunks.size())) throw std::out_of_range("Original menu chunk outside bank");
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    SpritePlacement placement;
    placement.scale=100.f*fit*multiplier;
    placement.offsetX=(float(width)-640.f*fit)*.5f+x*fit;
    placement.offsetY=(float(height)-480.f*fit)*.5f+y*fit;
    placement.invertY=true; placement.authoredHeight=0; placement.opacity=opacity;
    placement.defaultOriginalUiColors=true;
    compositeOriginalMenuChunk(pixels,width,height,selected.textures,selected.model.chunks[std::size_t(chunk)],placement);
}
const std::vector<std::uint32_t>& Frontend::paint(int width,int height) {
    synchronizeStage();
    if(width<=0 || height<=0 || width>16384 || height>16384) throw std::invalid_argument("Invalid native menu dimensions");
    collectPreloadedArtwork();
    const auto& canvas=paintOriginalCanvas();
    if(width==640&&height==480)return canvas;
    if(displayedCanvas==&canvas&&displayedRevision==canvasRevision&&displayedWidth==width&&displayedHeight==height)return displayPixels;
    // Original menu geometry and color blending live on the640x480 arcade
    // canvas. Scaling that finished frame avoids rerasterizing the same tiny
    // authored textures across millions of host pixels during each animation.
    const bool sceneOverlay=stage==FrontendStage::Title&&(attractChildId==7||attractChildId==12);
    displayPixels.assign(std::size_t(width)*height,sceneOverlay?0:0xff000000u);unityUiClear(displayPixels.data(),width,height,sceneOverlay?0:0xff000000u);
    if(stage==FrontendStage::Title&&attractChildId==8&&hasTitleExtension&&float(width)/height>4.f/3){
        const auto& extension=titleExtension.at(0);const float cover=std::max(float(width)/extension.width,float(height)/extension.height);
        compositeImage(displayPixels,width,height,extension,(width-extension.width*cover)*.5f,(height-extension.height*cover)*.5f,extension.width*cover,extension.height*cover);
    }
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    const int drawWidth=std::max(1,int(640.f*fit)),drawHeight=std::max(1,int(480.f*fit));
    const int left=(width-drawWidth)/2,top=(height-drawHeight)/2;
    unityUiCopy(displayPixels.data(),canvas.data(),width,height,float(drawWidth)/640,float(drawHeight)/480,float(left),float(top),true);
    if(!unityUiEnabled()){
    std::vector<int> columns(std::size_t(drawWidth),0);
    for(int x=0;x<drawWidth;++x)columns[std::size_t(x)]=x*640/drawWidth;
    for(int y=0;y<drawHeight;++y){const auto* row=canvas.data()+std::size_t(y*480/drawHeight)*640;auto* destination=displayPixels.data()+std::size_t(top+y)*width+left;
        for(int x=0;x<drawWidth;++x)destination[x]=row[columns[std::size_t(x)]];}
    }
    if(showingGasstand())gasstand.extendBackdrop(displayPixels,width,height);
    if(stage==FrontendStage::Title&&attractChildId==7)demoOverlays.extendBackdrop(displayPixels,width,height,titleFrame?titleFrame-1:0);
    displayedCanvas=&canvas;displayedRevision=canvasRevision;displayedWidth=width;displayedHeight=height;
    return displayPixels;
}
void Frontend::paintAttractPrompts(std::span<std::uint32_t> target,int width,int height)const{
    if(stage!=FrontendStage::Title||!gasstandLoaded)return;
    const float fit=std::min(width/640.f,height/480.f),ox=(width-640.f*fit)*.5f,oy=(height-480.f*fit)*.5f;
    SpritePlacement placement;placement.scale=fit;placement.offsetX=ox;placement.offsetY=oy;
    // Credit RIP carries the original yellow, black-outlined word artwork,
    // UV orientation and screen positions; the gasstand alphabet is different.
    const auto draw=[&](unsigned index){const auto& sprite=creditSprites.sprites.at(index);
        compositeOriginalSprite(target,width,height,creditTextures.at(sprite.texture),sprite,placement);};
    if(attractChildId>=5){
        if((carFrame/30)%2==0){
            placement.offsetY=oy+(attractChildId==8?-37.f:0.f)*fit;
            for(unsigned index:{21u,23u,12u})draw(index);
        }
        placement.offsetY=oy;draw(17);
    }
}
const std::vector<std::uint32_t>& Frontend::paintOriginalCanvas() {
    constexpr int width=640,height=480;
    if(make<0 || make>=7 || car<0 || car>=35 || course<0 || (course>=9 && (!isImportedCourse(course) || !importedArtwork(course).size()))) throw std::out_of_range("Invalid native menu selection");
    if(stage==FrontendStage::Car)synchronizeCarousel();
    if(stage==FrontendStage::Course)synchronizeCourseCarousel();
    if(stage==FrontendStage::TuningCourse){
        const std::vector<int> key{int(stage),int(tuningCourseMenu.frame492)};
        if(key==previousKey)return pixels;previousKey=key;++canvasRevision;
        pixels.assign(width*height,0xff000000u);unityUiClear(pixels.data(),width,height,pixels.empty()?0:pixels[0]);
        tuningPresentation->paintCanvas(pixels,width,height,tuningCourseMenu,battleProfile.u(1176));return pixels;
    }
    if(stage==FrontendStage::SaveSelect){
        const std::vector<int> key{int(stage),saveSelected,int(saveFileCarLive),int(saveActionsOpen),saveActionSelected,
            int(saveDeleteOpen),saveDeleteSelected,int(saveDeleteFailed),int(saveFiles.at(std::size_t(saveSelected)).level)};
        if(key==previousKey)return pixels;previousKey=key;++canvasRevision;
        constexpr std::uint32_t ink=0xff0b0c0fu,panel=0xff16181du,raised=0xff22242au;
        constexpr std::uint32_t edge=0xff41444cu,red=0xffde2331u,white=0xfff4f4f6u,muted=0xffa4a6afu;
        pixels.assign(std::size_t(width)*height,0xff050607u);unityUiClear(pixels.data(),width,height,0xff050607u);
        // All backing geometry also reaches Unity's captured triangle list.
        const auto fill=[&](int x,int y,int w,int h,std::uint32_t argb){
            if(unityUiEnabled()){
                unityUiSolid(pixels.data(),width,height,float(x),float(y),float(w),float(h),argb);return;
            }
            for(int yy=std::max(0,y);yy<std::min(height,y+h);++yy)
                for(int xx=std::max(0,x);xx<std::min(width,x+w);++xx)
                    pixels[std::size_t(yy)*width+xx]=argb;
        };
        const auto frame=[&](int x,int y,int w,int h,std::uint32_t argb){
            fill(x,y,w,1,argb);fill(x,y+h-1,w,1,argb);fill(x,y,1,h,argb);fill(x+w-1,y,1,h,argb);
        };
        const auto text=[&](std::string_view value,float x,float y,float size,std::uint32_t color,float maxWidth=1000.f){
            const float measured=menuFont.width(value,size);
            if(measured>maxWidth)size*=maxWidth/measured;
            menuFont.paint(pixels,width,height,value,x,y,size,color);
        };
        const auto number=[](unsigned value){return (value<10?std::string("0"):std::string())+std::to_string(value);};
        const auto& chosen=saveFiles[std::size_t(std::clamp(saveSelected,0,int(saveFiles.size())-1))];
        const auto used=std::count_if(saveFiles.begin(),saveFiles.end(),[](const auto& file){return file.used;});

        fill(24,18,592,446,ink);frame(24,18,592,446,edge);fill(24,18,592,3,red);
        text("SAVE SELECT",42,31,30,white);
        text("INITIAL D / SELECT A DRIVER",43,73,11,muted);
        const auto count=std::to_string(used)+" / "+std::to_string(saveFiles.size())+" USED";
        text(count,596-menuFont.width(count,12),43,12,muted);
        fill(25,96,590,1,edge);

        constexpr int listX=42,listY=110,listW=252,rowH=50,pitch=59;
        for(unsigned i=0;i<saveFiles.size();++i){
            const auto& file=saveFiles[i];const bool selected=int(i)==saveSelected;
            const int y=listY+int(i)*pitch;
            fill(listX,y,listW,rowH,selected?raised:panel);
            frame(listX,y,listW,rowH,selected?red:edge);
            if(selected)fill(listX,y,3,rowH,red);
            text(number(i+1),listX+13,float(y+13),20,selected?white:muted);
            if(file.used&&paintSaveName&&std::any_of(file.name.begin(),file.name.end(),[](unsigned char c){return c>=128;}))
                paintSaveName(pixels,width,height,file.name,listX+50,float(y+9),20,listW-64);
            else text(file.used?file.name:"NEW DRIVER",listX+50,float(y+5),20,file.used?white:muted,listW-64);
            text(file.used?file.car:"EMPTY SLOT",listX+51,float(y+30),11,muted,listW-65);
        }

        fill(310,110,288,316,panel);fill(310,110,3,43,red);
        text(chosen.used?"LAST USED CAR":"FILE "+number(unsigned(saveSelected+1)),326,119,10,muted);
        if(chosen.used){
            text(chosen.car,326,135,21,white,256);
            text(chosen.grade,326,163,11,muted,256);
            const auto& box=saveCarViewport;
            fill(box[0]-1,box[1]-1,box[2]+2,box[3]+2,ink);
            frame(box[0]-1,box[1]-1,box[2]+2,box[3]+2,edge);
            if(!saveFileCarLive)text("CAR PREVIEW",392,235,12,muted);
            text("PLAY TIME",326,saveActionsOpen?312:317,10,muted);
            text(formatPlayTime(chosen.playedSeconds),326,saveActionsOpen?325:332,saveActionsOpen?17:20,white,155);
            fill(498,saveActionsOpen?312:320,1,saveActionsOpen?29:35,edge);
            text("LEVEL",516,saveActionsOpen?312:317,10,muted);
            text(chosen.level?std::to_string(chosen.level):"--",516,saveActionsOpen?325:332,saveActionsOpen?17:20,white,66);
            if(!saveActionsOpen)fill(326,366,256,1,edge);
            text("LAST PLAYED",326,saveActionsOpen?348:377,10,muted);
            const auto date=chosen.lastPlayed.empty()?std::string("----/--/--"):chosen.lastPlayed;
            text(date,582-menuFont.width(date,12),saveActionsOpen?346:374,12,white);
            if(saveActionsOpen){
                for(int action=0;action<2;++action){
                    const int x=326+action*133;const bool selected=saveActionSelected==action;
                    fill(x,365,123,30,selected?red:raised);frame(x,365,123,30,selected?white:edge);
                    const std::string_view label=action==0?"CONTINUE":"CHANGE CAR";
                    text(label,x+(123-menuFont.width(label,13))*.5f,373,13,white);
                }
                const bool selected=saveActionSelected==2;
                fill(326,399,256,26,selected?red:raised);frame(326,399,256,26,selected?white:edge);
                text("DELETE SAVE",454-menuFont.width("DELETE SAVE",12)*.5f,406,12,white);
            }
        }else{
            frame(433,196,42,42,edge);fill(443,216,22,2,muted);fill(453,206,2,22,muted);
            text("NEW DRIVER",380,258,22,white);
            text("EMPTY SLOT",419,291,11,muted);
        }
        fill(42,440,556,1,edge);
        float x=43;
        for(const auto& [keyName,action]:std::initializer_list<std::pair<const char*,const char*>>{
                {"ARROWS / STEERING","SELECT"},{"ACCEL.","CONFIRM"},{"BRAKE","BACK"}}){
            text(keyName,x,451,11,white);x+=menuFont.width(keyName,11)+7;
            text(action,x,451,11,muted);x+=menuFont.width(action,11)+25;
        }
        if(saveDeleteOpen){
            fill(154,162,332,166,ink);frame(154,162,332,166,edge);fill(154,162,332,3,red);
            text("ARE YOU SURE?",320-menuFont.width("ARE YOU SURE?",25)*.5f,186,25,white);
            const auto label="DELETE SAVE "+number(unsigned(saveSelected+1));
            text(label,320-menuFont.width(label,12)*.5f,224,12,muted);
            if(saveDeleteFailed)text("COULD NOT DELETE SAVE",320-menuFont.width("COULD NOT DELETE SAVE",10)*.5f,249,10,red);
            for(int choice=0;choice<2;++choice){
                const int buttonX=180+choice*152;const bool selected=saveDeleteSelected==choice;
                fill(buttonX,270,128,36,selected?red:raised);frame(buttonX,270,128,36,selected?white:edge);
                const std::string_view label=choice==0?"NO":"YES";
                text(label,buttonX+(128-menuFont.width(label,17))*.5f,280,17,white);
            }
        }
        return pixels;
    }
    if(stage==FrontendStage::Name){
        const std::vector<int> key{int(stage),int(nameState.frame572)};
        if(key==previousKey)return pixels;previousKey=key;++canvasRevision;
        pixels.assign(width*height,0xff000000u);unityUiClear(pixels.data(),width,height,pixels.empty()?0:pixels[0]);namePresentation->paintBackground(pixels,width,height);
        namePresentation->paintNameBacking(pixels,width,height);
        std::vector<std::uint32_t> glow(width*height);unityUiClear(glow.data(),width,height);
        const auto add=[&]{if(unityUiEnabled()){unityUiCopy(pixels.data(),glow.data(),width,height,1,1,0,0,true,true);return;}for(std::size_t i=0;i<pixels.size();++i){auto value=pixels[i]&0xff000000u;
            for(unsigned shift:{0u,8u,16u})value|=std::min(255u,((pixels[i]>>shift)&255)+((glow[i]>>shift)&255))<<shift;pixels[i]=value;}};
        namePresentation->paintGlow(glow,width,height,nameState);add();
        namePresentation->paint(pixels,width,height,nameState,nameState.sharedCountdown1176);
        std::fill(glow.begin(),glow.end(),0);unityUiClear(glow.data(),width,height);namePresentation->paintCursor(glow,width,height,nameState);add();
        namePresentation->paintLegacy(pixels,width,height,nameState);return pixels;
    }
    if(stage==FrontendStage::Title&&gasstandLoaded){
        const std::vector<int> key={int(stage),int(attractChildId),attractChildId==8?int((carFrame/30)%2):int(carFrame)};
        if(key==previousKey)return pixels;
        ++canvasRevision;previousKey=key;previousMotionKey.clear();
        pixels.assign(std::size_t(width)*height,attractChildId==7||attractChildId==12?0:0xff000000u);unityUiClear(pixels.data(),width,height,pixels.empty()?0:pixels[0]);
        if(attractChildId>=3&&attractChildId<=5)attractCards.paint(pixels,width,height);
        else if(showingGasstand())gasstand.paint(pixels,width,height);
        else if(attractChildId==6){draw("adv_title",0,width,height);draw("adv_title",1,width,height);}
        else if(attractChildId==7)demoOverlays.paintOverlay(pixels,width,height,titleFrame?titleFrame-1:0);
        else if(attractChildId==8){draw("adv_newtitle",0,width,height);draw("adv_newtitle",1,width,height);}
        paintAttractPrompts(pixels,width,height);
        return pixels;
    }
    if(stage==FrontendStage::Mode) {
        original::selectOriginalGameMode(modeState,gameMode);
        original::setOriginalModeConfirmation(modeState,modeConfirmationPhase);
        const std::vector<std::uint32_t> modeKey={std::uint32_t(modeState.selected),std::bit_cast<std::uint32_t>(modeState.confirmationPhase),std::min(modeState.selectedFrames,6u),modeState.confirmationPhase>0?(modeState.selectedFrames&6u):0u,modeTransition.pointsWarning520,modeTransition.cardWarning528};
        if(modeKey!=previousModePaintKey){++canvasRevision;previousModePaintKey=modeKey;}
        previousKey.clear();previousMotionKey.clear();
        return modeMenu.paint(width,height,modeState,original::originalModeWarningChunks(modeTransition));
    }
    if(stage==FrontendStage::Rival){
        std::vector<int> key={width,height,course,rivalChoice,int(battleProfile.u(1180)),int(battleProfile.u(8)),int(battleProfile.u(12))};
        for(unsigned i=0;i<31;++i)key.push_back(battleProfile.byte(116+i));
        for(unsigned i=0;i<9;++i)key.push_back(int(battleProfile.u(80+i*4)));
        if(key!=rivalCacheKey){for(auto& frame:rivalPixels)frame.clear();rivalCacheKey=key;}
        const std::size_t glow=carConfirmationFrame<0 || (carFrame&6u)?1u:0u;
        auto& frame=rivalPixels[glow];
        if(frame.empty()){
            ++canvasRevision;
            pixels.assign(std::size_t(width)*height,0xff000000u);unityUiClear(pixels.data(),width,height,pixels.empty()?0:pixels[0]);
            drawRivalMenu(width,height);frame.swap(pixels);
        }
        previousKey.clear();previousMotionKey.clear();
        return frame;
    }
    if(stage==FrontendStage::Course&&gameMode==original::OriginalGameMode::BuntaChallenge)synchronizeCourseCarousel();
    std::vector<int> key={width,height,int(stage),make,car,course,int(automatic),int(reverse),int(wet),int(night),int(liveCarPreview),int(gameMode)};
    if(stage==FrontendStage::Course&&gameMode==original::OriginalGameMode::BuntaChallenge){
        for(unsigned i=0;i<8;++i)key.push_back(std::bit_cast<std::int32_t>(battleProfile.u(1080+i*4)));
    }
    const bool courseRecords=gameMode==original::OriginalGameMode::TimeAttack &&
        (stage==FrontendStage::Course||stage==FrontendStage::Route||stage==FrontendStage::Weather||stage==FrontendStage::Time);
    if(courseRecords)for(auto time:courseRecordTimes())key.push_back(std::bit_cast<std::int32_t>(time));
    std::vector<int> motionKey;
    if(stage==FrontendStage::Car)motionKey={carouselSlot,displayedScroll,int(std::bit_cast<std::uint32_t>(carConfirmationPhase)),carConfirmationPhase<.01f?0:int(carFrame&6u),int(selectedColor())};
    if(stage==FrontendStage::Make)motionKey={int(std::bit_cast<std::uint32_t>(makerConfirmationPhase)),makerConfirmationPhase<.01f?0:int(carFrame&6u)};
    if(stage==FrontendStage::Course)motionKey={courseSlot,displayedCourseScroll,carConfirmationFrame,carConfirmationFrame<0?0:int(carFrame&6u)};
    if(stage==FrontendStage::Course&&gameMode==original::OriginalGameMode::BuntaChallenge)motionKey.push_back(int(buntaBadgeFrame));
    if(choiceStage(stage))motionKey={stage==FrontendStage::Transmission?int(std::bit_cast<std::uint32_t>(transmissionConfirmationPhase)):carConfirmationFrame<0?0:1,carConfirmationFrame<0?0:int(carFrame&6u)};
    if(key==previousKey && motionKey==previousMotionKey) return pixels;
    ++canvasRevision;
    if(key!=previousKey) {
    pixels.assign(std::size_t(width)*std::size_t(height),0xff000000u);unityUiClear(pixels.data(),width,height,pixels.empty()?0:pixels[0]);
    if(stage==FrontendStage::Title) {
        // Original central artwork overwrites every generated central pixel.
        draw("adv_newtitle",0,width,height); draw("adv_newtitle",1,width,height);
    } else {
        if(!choiceStage(stage)){draw("v3sS00common",46,width,height);draw("v3sS00common",20,width,height);}
        switch(stage) {
        case FrontendStage::Make:
            draw("v3sS04maker",16,width,height);
            draw("v3sS04maker",17,width,height);
            draw("v3sS04maker",7,width,height);
            draw("v3sS04maker",make,width,height);
            draw("v3sS04maker",9+make,width,height);
            draw("v3sS00minicar",1+make,width,height);
            break;
        case FrontendStage::Car:
            draw("v3sS00common",54,width,height);
            draw("v3sS05cars",42,width,height);
            draw("v3sS05cars",43,width,height);
            draw("v3sS00emblem",make+7,width,height,59,128,.9f);
            draw("v3sS00emblem",make,width,height,59,128,.9f);
            if(!liveCarPreview)draw("v3sS00minicar",carImageChunk(car),width,height,320,283,2.5f);
            draw("v3sS05cars",0,width,height);
            draw("v3sS05cars",carNameChunk(car),width,height);
            if(carsForMake(make).size()>4) {
                draw("v3sS00common",48,width,height);
                draw("v3sS00common",50,width,height,-3,0);
            }
            break;
        case FrontendStage::Transmission:
            drawChoiceMenu(width,height,false);
            break;
        case FrontendStage::Course:
            // Background/header and selected course picture/name/difficulty
            // are actual194B20/194C60 groups, not an enlarged tile.
            draw("v3sK00common",83,width,height);
            draw("v3sK01course",26,width,height);
            draw("v3sK01course",14,width,height);
            if(isImportedCourse(course)){drawHakoneBackdrop(width,height);break;}
            draw("v3sK00common",courseBackgroundChunk[std::size_t(course)],width,height);
            draw("v3sK00common",30,width,height);
            draw("v3sK00common",courseDifficultyChunk[std::size_t(course)],width,height,0,168.f);
            draw("v3sK01course",courseMapChunk[std::size_t(course)],width,height);
            break;
        case FrontendStage::Route:
        case FrontendStage::Weather:
        case FrontendStage::Time:
            drawChoiceMenu(width,height,false);
            break;
        case FrontendStage::Title: break;
        case FrontendStage::Rival: break;
        case FrontendStage::Mode: break; // Separate original mode composition above.
        }
    }
    if(courseRecords)drawCourseRecords(width,height);
    staticPixels=pixels;unityUiCopy(staticPixels.data(),pixels.data(),width,height);
    previousKey=key;
    }
    pixels=staticPixels;unityUiCopy(pixels.data(),staticPixels.data(),width,height);
    if(stage==FrontendStage::Car){drawCarCarousel(width,height);drawCarColorIndicator(width,height);}
    if(stage==FrontendStage::Make)drawMakeStrip(width,height);
    if(choiceStage(stage))drawChoiceMenu(width,height,true);
    if(stage==FrontendStage::Course) {
        drawCourseCarousel(width,height);
        // Original depth places these arrows in front of the scrolling row.
        // Keep that layering explicit in the depthless CPU compositor.
        for(int chunk:{80,81,78,79})draw("v3sK00common",chunk,width,height);
        if(gameMode==original::OriginalGameMode::BuntaChallenge)drawBuntaCourseProgress(width,height);
    }
    previousMotionKey=motionKey;
    return pixels;
}
}
