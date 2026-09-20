#pragma once
#include "original_battle_profile.h"
#include <cstdint>
#include <vector>

namespace idas3::original {
// A_VISIT child IDs, from08E400. EjectCard/Ending remain explicit parent
// destinations: a native adapter must not silently treat them as CourseSelect.
enum class OriginalLegendReturnDestination : std::uint32_t {
    Select=0,CourseLoad=2,RivalLost=6,RivalWon=7,Ending=14,EjectCard=16
};
OriginalLegendReturnDestination originalLegendPostResultChild(std::uint32_t resultStatus80);
OriginalLegendReturnDestination originalLegendReturnDestination(bool continue92,std::uint32_t action96);
std::uint32_t originalLegendNextRival(const OriginalBattleProfile&,std::uint32_t enemy);
// The after-race track a lost battle asks for, from the owner's own table.
unsigned originalLegendLossMusic(std::uint32_t enemy);

enum class OriginalLegendReturnCommand {
    DestroyCommon,   //0F07C0: releases the dialog/continue objects InitializeCommon built.
    InitializeCommon,//0EF620: graphics reset, sound set2, dialog/continue construction,
                     // phase0, rival from profile24, selection/animation counters0, timer879.
    ConfigureDialog, //0F8780: a=enemy,b=kind; weather/player from currentprofile.
    CloseDialog,     //0FAAA0.
    AdvanceDialog,   //0FAA40: a=relative page increment.
    SkipDialog,      //0FAD20; source debug button, distinct from normalconfirm.
    MusicRequest,MusicFade, //141EC0(a),1431E0; readiness/handle service external.
    Cue,             //141F80(a,1).
    ContinueAccepted,//0F04E0: credit141F40(1,1),card077BC0; profilewrites internal.
    DrawContinue,DrawNextRival,DrawCredit, // choice painters; next-rival painters tick the timer.
    SoundSet,        //1416A0(a): explicit set1 for the course-clear movie, set2 after it.
    StreamVolume,    //141E40(a).
    StreamStart,     //141D00(a): stream16 carries the course-clear movie audio.
    StreamPlay,      //141D40.
    StreamStop,      //141D80.
    StreamFade,      //141DC0(a).
    StartCourseClear,//graphics reset, fade quad0C4F60 and movie0C1960(a=course) built; fade0.
    StepCourseClear, //0C1DA0/0C1DC0: advance and draw once per update, a=movie frame.
    CourseClearFade, //0C5200(a=alpha) on the fade quad.
    FinishCourseClear,//movie and fade quad destroyed.
    Finish           //0F0760 profile update, then ownervirtual+40 notification.
};
struct OriginalLegendReturnEvent {
    OriginalLegendReturnCommand command{};
    std::uint32_t a{},b{};
};
struct OriginalLegendReturnSetup {
    std::uint32_t resultStatus80{}; //0 win,1 loss,2 timeout; alreadyrecorded.
    bool continue92{};             //retained A_VISIT parentbyte, not inferred.
    std::uint32_t action96{};      //retained A_VISIT parentbranch.
};
struct OriginalLegendReturnInput {
    // Sample AFTER the common dialog's own Main update (0F0560).
    bool dialogReady{},dialogClosed{},debugSkip{},confirm{};
    std::uint32_t selectedIndex{}; //actual09C580 result, source binaryselector.
    bool continuationEnabled{};   //04FCC0()->word12 bit5.
    bool freePlay{},creditReady{}; //156B00,202B60(0,1).
    std::uint32_t coinEvent{};     //2EFF1C:1/2 reset insert-credit timer.
    bool nextDialogPageExists{};  //0FA5C0(dialog,currentPage+1)==1.
};
struct OriginalLegendReturnState {
    bool initialized{},finished{},won{},continue92{},courseClear528{};
    std::uint32_t resultStatus80{},action96{},phase108{},enemy112{};
    std::uint32_t selected248{},creditFrame492{},confirmFrame504{};
    std::uint32_t choiceFrame520{},courseClearFrame532{},fadeFrame104{};
};
struct OriginalLegendReturnFrame {
    std::uint32_t sourcePhase{},selectedIndex{},confirmFrame{},choiceFrame{};
    std::vector<OriginalLegendReturnEvent> events;
    bool finished{};
    OriginalLegendReturnDestination destination{OriginalLegendReturnDestination::EjectCard};
};
// Once per post-result owner entry. No race counters, points or rank are
// re-awarded here. Won Init alone refreshes course-progress selection words.
OriginalLegendReturnFrame initializeOriginalLegendReturn(OriginalLegendReturnState&,
    OriginalBattleProfile&,const OriginalLegendReturnSetup&);
// Exactly one source owner update. Rendering/repainting must consume the last
// returned frame and never call this function. Completed state is inert.
OriginalLegendReturnFrame advanceOriginalLegendReturn(OriginalLegendReturnState&,
    OriginalBattleProfile&,const OriginalLegendReturnInput&);
}
