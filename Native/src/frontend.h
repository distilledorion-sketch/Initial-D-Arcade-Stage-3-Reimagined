#pragma once
#include "native_assets.h"
#include "imported_course_catalog.h"
#include "time_attack_records.h"
#include <functional>
#include "original_mode_menu.h"
#include "original_mode_menu_transition.h"
#include "original_battle_profile.h"
#include "original_bunta_setup.h"
#include "original_frontend_transition.h"
#include "original_attract.h"
#include "original_gasstand_attract.h"
#include "menu_font.h"
#include "original_attract_cards.h"
#include "original_demo_data.h"
#include "original_demo_overlays.h"
#include "original_ranking_playback.h"
#include "original_car_menu_transition.h"
#include "original_transmission_menu_transition.h"
#include "original_car_color_selection.h"
#include "original_selection_music.h"
#include "original_name_entry_presentation.h"
#include "original_tuning_course_menu.h"
#include "original_tuning_course_presentation.h"
#include "original_driver_entry_route.h"
#include <array>
#include <filesystem>
#include <future>
#include <map>
#include <string>
#include <vector>

namespace idas3 {
enum class FrontendStage { Title, SaveSelect, Make, Car, Transmission, Mode, Course, Route, Weather, Time, Rival, Name, TuningCourse };

// Original artwork and authored mesh chunks with native selection/navigation.
// Car-strip transforms and confirmation shrink use original 1B5880/1B5B40
// and 12EA64. Native input/navigation owns the remaining stage lifecycle.
class Frontend {
public:
    struct AttractAudioEvent {unsigned child=0,sourceFrame=0;bool finished=false;};
    FrontendStage stage = FrontendStage::Title;
    int make = 6, car = 0, course = 3;
    bool automatic = true, reverse = false, wet = false, night = false;
    bool liveCarPreview = false;
    original::OriginalGameMode gameMode=original::OriginalGameMode::TimeAttack;
    original::OriginalBattleProfile battleProfile=original::makeOriginalFreshBattleProfile();
    // One driver per slot, with a remembered active car. The host fills these
    // in from the slot store; the menu only displays and chooses.
    struct SaveFileSummary {
        bool used=false;
        std::string name,car,grade,lastPlayed;
        std::uint64_t playedSeconds=0;
        unsigned level=0; // Exact battle level used by aura; zero means unavailable.
    };
    std::array<SaveFileSummary,5> saveFiles{};
    // Set while the host is drawing the highlighted file's own car into the
    // panel's box; the box is then left clear for it.
    bool saveFileCarLive=false;
    // Shared by the menu backing and live showroom camera, in canvas pixels.
    static constexpr std::array<int,4> saveCarViewport{326,181,256,126};
    // Non-ASCII driver names use the recovered alphabet rather than the UI face.
    std::function<void(std::span<std::uint32_t>,int,int,const std::string&,float,float,float,float)> paintSaveName;
    MenuFont menuFont;
    // In-memory host lookup; menu painting never reads save files.
    std::function<TimeAttackBest(unsigned,unsigned,unsigned)> timeAttackBest;
    std::function<TimeAttackEntry(unsigned,unsigned,unsigned)> importedPersonalBest;
    std::array<std::uint32_t,3> courseRecordTimes()const;
    int saveSelected=0;
    bool saveActionsEnabled=true,saveActionsOpen=false;
    int saveActionSelected=0;
    bool saveDeleteOpen=false,saveDeleteFailed=false;
    int saveDeleteSelected=0; // No is always the initial choice; Yes requires navigation.
    int saveDeleteRequested=-1;
    bool changingSavedCar=false,savedDriverSelected=false;
    bool clickSaveMenu(float x,float y);
    bool hoverSaveMenu(float x,float y);
    void finishSaveDeletion(bool success);
    int takeSaveDeleteRequested(){const int result=saveDeleteRequested;saveDeleteRequested=-1;return result;}
    // Set once when a file is confirmed; the host decides whether that means
    // a fresh setup or straight to the mode menu.
    bool takeSaveFileChosen(){const bool result=saveFileChosen;saveFileChosen=false;return result;}
    bool takeSaveCarChangeRequested(){const bool result=saveCarChangeRequested;saveCarChangeRequested=false;return result;}
    bool takeSaveCarPreviewReset(){const bool result=saveCarPreviewReset;saveCarPreviewReset=false;return result;}
    int rivalChoice=0;
    original::OriginalBuntaEligibility buntaEligibility()const {return original::originalBuntaEligibility(battleProfile);}
    bool unsupportedModeSelected()const {return stage==FrontendStage::Mode && gameMode==original::OriginalGameMode::BuntaChallenge && buntaEligibility()!=original::OriginalBuntaEligibility::Eligible;}
    static constexpr int hakoneCourse=9,sadamineCourse=10,ennaCourse=11;
    static bool isImportedCourse(int value){return isImportedCourseId(value);}
    void enableHakoneCourse(const std::filesystem::path& root,int id=hakoneCourse);
    std::vector<int> courseChoices()const;
    void initialize(const std::filesystem::path& rootPath,bool preloadArtwork=false);
    void advance(double seconds);
    void change(int delta);
    void changeColor(int delta);
    unsigned selectedColor()const;
    // Loading another native car profile keeps source per-screen selection
    // state separate from persistent progress and saved appearance.
    void driverProfileLoaded();
    void setNameSteering(float steering);
    const original::OriginalNameEntryState& nameEntryState()const{return nameState;}
    const OriginalNameEntryPresentation& nameEntryPresentation()const{return *namePresentation;}
    const original::OriginalTuningCourseMenu& tuningCourseState()const{return tuningCourseMenu;}
    const OriginalTuningCoursePresentation& tuningCoursePresentation()const{return *tuningPresentation;}
    std::vector<unsigned> takeMenuCueIds(){auto result=std::move(menuCueIds);menuCueIds.clear();return result;}
    bool takeDriverProfileCommit(){const auto result=driverProfileCommit;driverProfileCommit=false;return result;}
    // Distinct from name commit: emitted once only after the source name child
    // exits. The host saves the final profile before marking native setup done.
    bool takeDriverSetupCompleted(){const bool result=driverSetupCompleted;driverSetupCompleted=false;return result;}
    bool back();
    bool confirm();
    bool takeStartRequest(){const bool result=startRequested;startRequested=false;return result;}
    bool saveFileChosen=false;
    bool saveCarChangeRequested=false;
    bool saveCarPreviewReset=false;
    bool showingGasstand()const{return stage==FrontendStage::Title&&attractChildId==11;}
    unsigned attractChild()const{return attractChildId;}
    // Source gear/view edges, consumed once at the next original60Hz tick.
    void queueRankingInput(original::OriginalRankingPageInput input){
        if(stage==FrontendStage::Title&&attractChildId==12){
            rankingInput.detailPressed|=input.detailPressed;
            rankingInput.nextConditionPressed|=input.nextConditionPressed;
        }
    }
    // Only the private visual diagnostic calls this; ordinary users reach
    // child12 through the original attract sequence and gear/view inputs.
    void prepareRankingAttractFixture(unsigned modelPage);
    unsigned attractFrame()const{return titleFrame;}
    const original::OriginalDemoData& demoData()const{return attractDemo;}
    original::OriginalDemoCursor demoCursor()const{return displayedDemoCursor;}
    const original::OriginalRankingPlayback& rankingState()const{return rankingPlayback;}
    const original::OriginalRankingRecords& rankingData()const{return rankingRecords;}
    std::vector<AttractAudioEvent> takeAttractAudioEvents(){auto result=std::move(attractAudioEvents);attractAudioEvents.clear();return result;}
    std::vector<original::OriginalSelectionMusicCommand> takeSelectionMusicCommands(){auto result=std::move(selectionMusicCommands);selectionMusicCommands.clear();return result;}
    const original::OriginalSelectionMusicState& selectionMusicState()const{return selectionMusic;}
    void endSelectionMusic(std::int32_t scene=4);
    void paintAttractPrompts(std::span<std::uint32_t>,int width,int height)const;
    unsigned attractScript()const{return gasstand.state().script;}
    bool confirmationInProgress()const{return carConfirmationFrame>=0 || (stage==FrontendStage::Name&&nameState.phase588>=2) || (stage==FrontendStage::TuningCourse&&tuningCourseMenu.phase464>=2) || (stage==FrontendStage::Title&&(titleConfirmPending||attractExit.pendingFrames348));}
    bool inputReady()const{return !confirmationInProgress() &&
        (stage!=FrontendStage::Make || (observedStage==stage&&stageInitialized&&makerTransition.phase448==1)) &&
        (stage!=FrontendStage::Car || (observedStage==stage&&stageInitialized&&carTransition.phase456==1)) &&
        (stage!=FrontendStage::Transmission || (observedStage==stage&&stageInitialized&&transmissionTransition.phase476==1)) &&
        (stage!=FrontendStage::Name || (observedStage==stage&&stageInitialized&&nameState.phase588==1)) &&
        (stage!=FrontendStage::TuningCourse || (observedStage==stage&&stageInitialized&&tuningCourseMenu.phase464==1)) &&
        (stage!=FrontendStage::Mode || (observedStage==stage&&stageInitialized&&modeTransition.phase468==1));}
    std::uint32_t screenFadeArgb()const{return selectionExitFrame>=0?(std::uint32_t(std::min(selectionExitFrame,15)*255/15)<<24):stage==FrontendStage::Title?(titleExitBlack?0xff000000u:attractChildId==11?original::originalGasstandFadeArgb(gasstand.state()):attractChildId>=3&&attractChildId<=5?attractCards.fadeArgb():titleFade):
        stage==FrontendStage::Make&&makerTransition.overlayEnabled460?original::originalMakerFadeArgb(makerTransition):
        stage==FrontendStage::Car&&carTransition.overlayEnabled472?original::originalCarMenuFadeArgb(carTransition):
        stage==FrontendStage::Transmission&&transmissionTransition.overlayEnabled464?original::originalTransmissionMenuFadeArgb(transmissionTransition):
        stage==FrontendStage::Name&&nameState.fadeEnabled584?original::originalNameEntryFadeArgb(nameState):
        stage==FrontendStage::TuningCourse&&tuningCourseMenu.fadeEnabled480?original::originalTuningCourseFadeArgb(tuningCourseMenu):
        stage==FrontendStage::Mode&&modeTransition.overlayEnabled456?original::originalModeMenuFadeArgb(modeTransition):choiceFadeArgb();}
    const std::vector<std::uint32_t>& paint(int width, int height);
    static int carNameChunk(int originalCarId);
    static int carImageChunk(int originalCarId);
    static std::vector<int> carsForMake(int makeIndex);
private:
    std::uint32_t choiceFadeArgb()const;
    std::array<NativeTextureBank,importedCourseDefinitions.size()> importedArtworks;
    NativeTextureBank& importedArtwork(int id){return importedArtworks.at(unsigned(id-hakoneCourse));}
    const NativeTextureBank& importedArtwork(int id)const{return importedArtworks.at(unsigned(id-hakoneCourse));}
    void drawHakoneBackdrop(int width,int height);
    struct Bank { NativeModel model; NativeTextureBank textures; };
    std::filesystem::path projectRoot,assetRoot;
    std::map<std::string, Bank> banks;
    std::future<std::map<std::string,Bank>> preloadedBanks;
    std::vector<std::uint32_t> pixels;
    std::vector<std::uint32_t> staticPixels;
    std::vector<std::uint32_t> displayPixels;
    std::vector<std::uint32_t> previousModePaintKey;
    const std::vector<std::uint32_t>* displayedCanvas=nullptr;
    std::uint64_t canvasRevision=0,displayedRevision=0;
    int displayedWidth=0,displayedHeight=0;
    // 196CE0 changes only its selected glow during confirmation. Keep both
    // complete compositions, including their original depth ordering.
    std::array<std::vector<std::uint32_t>,2> rivalPixels;
    std::vector<int> rivalCacheKey;
    std::vector<int> previousKey;
    std::vector<int> previousMotionKey;
    int carouselMake=-1, carouselCar=-1, carouselSlot=0, carouselScroll=0;
    int displayedScroll=0, carConfirmationFrame=-1;
    // Time Attack owner+440 during phase3: AA0 at1, Stop after exceeding15.
    int selectionExitFrame=-1;
    int carouselCourse=-1, courseSlot=0, courseScroll=0, displayedCourseScroll=0;
    int buntaBadgeCourse=-1;
    std::uint32_t buntaBadgeFrame=0;
    std::uint32_t carFrame=0;
    double frameRemainder=0;
    NativeTextureBank titleExtension,creditTextures;
    NativeSpriteBank creditSprites;
    bool hasTitleExtension=false;
    bool startRequested=false;
    original::OriginalModeMenu modeMenu;
    original::OriginalModeMenuState modeState;
    original::OriginalModeMenuTransition modeTransition;
    bool modeConfirmPending=false;
    float modeConfirmationPhase=0;
    FrontendStage observedStage=FrontendStage::Title;
    bool stageInitialized=false,makerConfirmPending=false;
    original::OriginalMakerTransition makerTransition;
    float makerConfirmationPhase=0;
    original::OriginalAttractExitState attractExit;
    original::OriginalGasstandAttract gasstand;
    original::OriginalAttractCards attractCards;
    original::OriginalAttractTitleState attractTitle;
    original::OriginalDemoData attractDemo;
    original::OriginalDemoOverlays demoOverlays;
    original::OriginalDemoCursor attractDemoCursor,displayedDemoCursor;
    bool attractDemoWrapped=false;
    original::OriginalRankingPlayback rankingPlayback;
    original::OriginalRankingPageInput rankingInput{};
    original::OriginalRankingRecords rankingRecords;
    std::vector<AttractAudioEvent> attractAudioEvents;
    original::OriginalSelectionMusicState selectionMusic;
    std::vector<original::OriginalSelectionMusicCommand> selectionMusicCommands;
    FrontendStage selectionMusicObservedStage=FrontendStage::Title;
    bool selectionMusicStageInitialized=false;
    bool gasstandLoaded=false;
    unsigned attractChildId=3;
    unsigned titleFrame=0,nextAttractScript=0;
    std::uint32_t titleFade=0;
    bool titleConfirmPending=false,titleExitBlack=false;
    original::OriginalCarMenuTransition carTransition;
    bool carConfirmPending=false,carCancelPending=false;
    float carConfirmationPhase=0;
    original::OriginalTransmissionMenuTransition transmissionTransition;
    bool transmissionConfirmPending=false;
    float transmissionConfirmationPhase=0;
    original::OriginalCarColorSelection colorSelection;
    original::OriginalNameEntryTables nameTables;
    original::OriginalNameEntryState nameState;
    original::OriginalNameEntryInput nameInput;
    std::unique_ptr<OriginalNameEntryPresentation> namePresentation;
    std::unique_ptr<original::OriginalTuningData> tuningData;
    std::unique_ptr<OriginalTuningCoursePresentation> tuningPresentation;
    original::OriginalTuningCourseMenu tuningCourseMenu;
    unsigned tuningCourseSelected=0;
    bool tuningCourseConfirmPending=false,nameCommittedForVisit=false,driverSetupCompleted=false;
    std::vector<unsigned> menuCueIds;
    bool driverProfileCommit=false;
    std::uint8_t pendingColorButtons=0;
    int colorSelectionMake=-1;
    void initializeColorSelection();
    void synchronizeStage();
    void synchronizeSelectionMusicStage();
    void appendSelectionMusic(const original::OriginalSelectionMusicCommands&);
    void enterAttractChild(unsigned child);
    void advanceAttractChild();
    Bank& bank(const std::string& name);
    void collectPreloadedArtwork();
    void synchronizeCarousel();
    void drawCarCarousel(int width,int height);
    void drawCarColorIndicator(int width,int height);
    void drawMakeStrip(int width,int height);
    void synchronizeCourseCarousel();
    void drawCourseCarousel(int width,int height);
    void drawCourseRecords(int width,int height);
    void drawBuntaCourseProgress(int width,int height);
    void drawChoiceMenu(int width,int height,bool movingWidgets);
    void selectRival();
    void selectBuntaCourse();
    void drawRivalMenu(int width,int height);
    const std::vector<std::uint32_t>& paintOriginalCanvas();
    void draw(const std::string& name, int chunk, int width, int height,
              float x=0, float y=0, float multiplier=1, float opacity=1);
};
}
