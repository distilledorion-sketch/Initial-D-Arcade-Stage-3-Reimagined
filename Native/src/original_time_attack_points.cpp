#include "original_time_attack_points.h"

namespace idas3::original {
OriginalTimeAttackPoints calculateOriginalTimeAttackPoints(const OriginalBattleProfile& p,
    const OriginalTimeAttackPointsInput& in){
    OriginalTimeAttackPoints out;
    // All18 original rows2A1194 have the same two values. The source range
    // fallbacks also select one of these rows; weather does not affect them.
    out.participation=1000;
    if(in.resultStatus92<=1){
        out.finish=2000;
        if(in.previousCourse6000>in.elapsed6000)out.courseBonus=1000;
        if(in.previousModel6000>in.elapsed6000)out.modelBonus=500;
        if(in.previousPersonal6000==0||in.previousPersonal6000>in.elapsed6000)out.personalBonus=500;
    }
    out.recordBonus=out.courseBonus+out.modelBonus+out.personalBonus;
    out.total=out.participation+out.finish+out.recordBonus;
    out.balanceBeforeCap=p.u(72)+out.total;
    return out;
}
}
