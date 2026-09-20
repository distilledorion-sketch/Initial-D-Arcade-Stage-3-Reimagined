#include "original_ranking_playback.h"
#include <algorithm>
namespace idas3::original {
void OriginalRankingPlayback::reset(unsigned course){
    page=initialOriginalRankingPage(course);resources={};events={};
    boardFrame=0;ticks=0;++resourceRevision;plateDigits={};drawLayerMask=nextDrawLayerMask_=3;
}
void OriginalRankingPlayback::step(const OriginalRankingRecords& records,
    OriginalRankingPageInput input,std::uint32_t ready){
    if(completed())return;
    const unsigned course=originalRankingCourse(page),direction=page.conditionIndex&1;
    const auto record=records.record(course,direction,page.wet,0);
    OriginalRankingCarSource source;
    source.car=record.car();source.packedAppearance=records.firstPlaceAppearance(course,direction,page.wet);
    std::copy_n(record.bytes.begin()+4,5,source.name.begin());
    events=stepOriginalRankingResources(resources,page,source,ready);
    if(events.createCar||events.configureCar)++resourceRevision;
    if(events.createCar)nextDrawLayerMask_=3; //036060->026360 constructor+224
    if(events.drawCar){drawLayerMask=nextDrawLayerMask_;nextDrawLayerMask_=2;} //02F9C6/02FA0C
    if(events.configureCar)plateDigits=events.plateDigits;
    // Main's common child draw increments its frame before the page tail.
    ++boardFrame;
    if(stepOriginalRankingPage(page,input).clearLeaderboard)boardFrame=0;
    ++ticks;
}
}
