#include "original_music_band_limit.h"
#include <algorithm>
#include <cmath>

namespace idas3 {
namespace {
// Odd so the delay is a whole sample and the block does not shift under the
// filter; long enough that the lowest cutoff still has a usable transition.
constexpr int taps = 63;

std::vector<double> lowPass(double cutoff) {
    std::vector<double> h(taps);
    const int middle = taps / 2;
    double sum = 0;
    for (int i = 0; i < taps; ++i) {
        const int n = i - middle;
        const double x = 2.0 * cutoff * n;
        const double sinc = n == 0 ? 2.0 * cutoff : std::sin(3.14159265358979323846 * x) /
                                                    (3.14159265358979323846 * n);
        // Blackman, for a stopband deep enough that what it lets through is
        // quieter than the aliasing it exists to remove.
        const double w = 0.42 - 0.5 * std::cos(2 * 3.14159265358979323846 * i / (taps - 1)) +
                         0.08 * std::cos(4 * 3.14159265358979323846 * i / (taps - 1));
        h[std::size_t(i)] = sinc * w;
        sum += h[std::size_t(i)];
    }
    for (auto& value : h) value /= sum;
    return h;
}
}

unsigned originalMusicBandLimitLevel(unsigned increment, unsigned levels) {
    if (!levels || increment <= 1024) return 0;
    unsigned level = 0, rate = increment;
    // ceil(log2(rate/1024)): the level whose cutoff is at or below Nyquist/rate.
    while (rate > 1024 && level + 1 < levels) { rate = (rate + 1) / 2; ++level; }
    return level;
}

std::vector<std::int16_t> bandLimitOriginalMusicSample(
    std::span<const std::int16_t> pcm, unsigned loopStart, unsigned loopEnd, bool looping,
    unsigned level) {
    std::vector<std::int16_t> out(pcm.size());
    if (pcm.empty()) return out;
    if (!level) { std::copy(pcm.begin(), pcm.end(), out.begin()); return out; }
    const auto h = lowPass(0.5 / double(1u << level));
    const int middle = taps / 2;
    const auto length = int(pcm.size());
    const bool wraps = looping && loopEnd > loopStart && loopEnd <= unsigned(length);
    const int span = wraps ? int(loopEnd) - int(loopStart) : 0;
    // The block is played as its attack followed by its loop repeated, so that
    // is the signal the filter sees: silence before the start, and past the
    // loop end the loop itself rather than whatever follows in the bank.
    const auto at = [&](int index) -> double {
        if (index < 0) return 0;
        if (wraps && index >= int(loopEnd)) index = int(loopStart) + (index - int(loopEnd)) % span;
        if (index >= length) return 0;
        return pcm[std::size_t(index)];
    };
    for (int i = 0; i < length; ++i) {
        double sum = 0;
        for (int j = 0; j < taps; ++j) sum += h[std::size_t(j)] * at(i + j - middle);
        out[std::size_t(i)] = std::int16_t(std::clamp<long long>(std::llround(sum), -32768, 32767));
    }
    return out;
}
}
