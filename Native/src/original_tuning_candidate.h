#pragma once
#include "original_tuning.h"
#include "original_car_appearance_config.h"
#include <limits>

namespace idas3::original {
//12B440 resets installed preview parts, preserving factory paint and bit6.
// Rebuild029040 is an explicit geometry/material boundary for the consumer.
std::vector<OriginalTuningMutation::CarCall> originalTuningStockAppearanceCalls(bool rebuild);
struct OriginalTuningCandidateAppearance {
    std::vector<OriginalTuningMutation::CarCall> carCalls;
    std::uint32_t carState224=2;
};
//12B520: all authored package steps are previewed, without committing any
// profile parts. The optional prefix count exposes source owner+512 for tests.
OriginalTuningCandidateAppearance originalTuningCandidateAppearance(
    const OriginalTuningData&,unsigned car,unsigned package,
    unsigned stepCount=std::numeric_limits<unsigned>::max());
// Apply setter calls to an existing showroom configuration. The caller handles
//029040 once, then rebuilds its private geometry/materials and carState224.
void applyOriginalTuningCandidateAppearance(OriginalCarAppearanceConfig&,
                                          const OriginalTuningCandidateAppearance&);
}
