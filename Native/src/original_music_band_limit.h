#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace idas3 {
// Pre-filtered copies of one sample block, for voices that read it faster than
// it was authored.
//
// The chip reads a sample with linear interpolation and no decimation filter,
// so a note transposed up folds everything above Nyquist/rate back into the
// audible band. The selection cue's lead reaches 7.55x, where that fold-back
// measures about fifteen decibels under the signal. These levels remove it by
// band-limiting the block in advance: level k is low-passed to Nyquist/2^k, and
// a voice reading at rate r takes level ceil(log2 r).
//
// Each level keeps the block's own length and loop points, so nothing about
// position, looping or interpolation changes -- only the numbers that are read.
// The loop region is filtered as the circle it actually is, so the join stays
// where the author put it.
//
// This is deliberately not what the hardware does. The hardware-exact read is
// the default and is what the reference tests compare; these are opt-in.
std::vector<std::int16_t> bandLimitOriginalMusicSample(
    std::span<const std::int16_t> pcm, unsigned loopStart, unsigned loopEnd, bool looping,
    unsigned level);
// How many levels cover a playback rate, unity being level zero.
unsigned originalMusicBandLimitLevel(unsigned increment, unsigned levels);
}
