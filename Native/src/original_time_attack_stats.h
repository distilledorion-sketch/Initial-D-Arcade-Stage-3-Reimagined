#pragma once
#include "original_time_attack_analysis.h"
#include "original_time_attack_telemetry.h"
#include <vector>

namespace idas3::original {
// HLecture's original lecture bank, in authored640x480 pixel coordinates.
// Y already includes18DBC0's -.24 model translation (+24screen pixels).
struct OriginalTimeAttackStatDraw { unsigned chunk=0;float x=0,y=0; };
std::vector<OriginalTimeAttackStatDraw> originalTimeAttackStatDraws(
    const OriginalTimeAttackTelemetrySnapshot&,const OriginalTimeAttackAnalysisInput&);
}
