// Headless driver for the portable scene core. It uses the same exports the Unity host uses
// (Idas3SceneInitialize / Step / GetFrame) and can replay a small input script, so menus can be
// navigated and frames captured without any renderer.
//
// Usage: headless_scene <assetRoot containing data/> <saveRoot> [script | @scriptfile]
//
// Script: whitespace-separated tokens, executed in order, at a fixed 60 Hz step.
//   w<N>            step N frames with no input
//   k<VK>[:<N>]     hold virtual key VK (decimal) for N frames (default 4), e.g. k13 = Enter
//   p<HEX>[:<N>]    hold gamepad button mask (XInput bits, hex) for N frames
//   t<L>:<R>[:<N>]  hold triggers (left, right, 0..255) for N frames
//   x<X>[:<N>]      hold left stick X (-32768..32767) for N frames
//   s<name>         write <name>.ppm for every overlay surface (UI) of the current frame
//   g<name>         write <name>.frame, a raw dump of the scene frame (see dumpFrame)
//   u<name>         write <name>.ui, a raw dump of the UI draw lists (see dumpUi)
//   A<path>         start recording audio (drains the PCM ring after every step, like a host would)
//   a               stop recording and write the 44.1 kHz stereo float WAV to the path given to A
//   i               print a one-line frame summary
// With no script it steps 120 frames and prints summaries.
#include "unity_bridge.h"
#include "unity_scene_capture.h"
#include "unity_ui_capture.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
extern "C" IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneGetFrame(Idas3SceneFrame* destination);
extern "C" int Idas3UnityReadAudio(float* stereo, int frameCount);
extern "C" void Idas3UnitySetAudioRunning(int running);
extern "C" int Idas3UnityGetAudioStatistics(struct Idas3UnityAudioStatistics* destination);

static int g_frames = 0;

// Audio capture: after each step read exactly what that step produced (the ring primes at 1024 frames).
struct Idas3UnityAudioStatistics { uint32_t size = 64, version = 1; uint64_t produced = 0, consumed = 0, dropped = 0, underrun = 0, priming = 0, discarded = 0, queued = 0; };
static bool g_recording = false;
static std::string g_audioPath;
static std::vector<float> g_pcm;
static uint64_t g_producedBefore = 0;
static uint64_t g_underruns = 0, g_primingFrames = 0;

static void fail(const char* what) {
    char err[1024] = {};
    Idas3UnityCopyError(err, sizeof err);
    std::fprintf(stderr, "%s (frame %d): %s\n", what, g_frames, err);
    std::exit(1);
}

static Idas3SceneFrame getFrame() {
    Idas3SceneFrame f{};
    f.size = sizeof f;
    if (!Idas3SceneGetFrame(&f)) fail("GetFrame failed");
    return f;
}

static void summary(const char* label) {
    const auto f = getFrame();
    Idas3UnityStatus st{};
    st.size = sizeof st;
    Idas3UnityGetStatus(&st);
    std::printf("[%s f%d] gen=%llu texgen=%llu %ux%u views=%u verts=%u ranges=%u tex=%u overlays=%u | stage=%d race=%d course=%d car=%d flags=%u ticks=%llu\n",
        label, g_frames, (unsigned long long)f.frameGeneration, (unsigned long long)f.textureGeneration, f.width, f.height,
        f.viewCount, f.vertexCount, f.rangeCount, f.textureCount, f.overlayCount, st.frontendStage, st.racePhase, st.course, st.car,
        st.flags, (unsigned long long)st.simulationTicks);
}

static void stepInput(const Idas3UnityInput& in) {
    if (!Idas3SceneStep(&in)) fail("Idas3SceneStep failed");
    ++g_frames;
    if (g_recording) {
        Idas3UnityAudioStatistics st;
        Idas3UnityGetAudioStatistics(&st);
        const uint64_t produced = st.produced - g_producedBefore;
        g_producedBefore = st.produced;
        if (produced > 0 && produced <= 65536) {
            const size_t at = g_pcm.size();
            g_pcm.resize(at + produced * 2);
            Idas3UnityReadAudio(g_pcm.data() + at, int(produced));
        }
        g_underruns = st.underrun; g_primingFrames = st.priming;
    }
}

static void writeWav(const std::string& path, const std::vector<float>& pcm) {
    FILE* o = std::fopen(path.c_str(), "wb");
    if (!o) return;
    const uint32_t dataBytes = uint32_t(pcm.size() * 4), rate = 44100, byteRate = rate * 8;
    const uint16_t fmt = 3, channels = 2, align = 8, bits = 32;
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, o); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, o); };
    std::fwrite("RIFF", 1, 4, o); u32(36 + dataBytes); std::fwrite("WAVEfmt ", 1, 8, o); u32(16);
    u16(fmt); u16(channels); u32(rate); u32(byteRate); u16(align); u16(bits);
    std::fwrite("data", 1, 4, o); u32(dataBytes); std::fwrite(pcm.data(), 4, pcm.size(), o);
    std::fclose(o);
}

static void writePpm(const std::string& path, const uint32_t* argb, uint32_t w, uint32_t h) {
    FILE* o = std::fopen(path.c_str(), "wb");
    if (!o) return;
    std::fprintf(o, "P6\n%u %u\n255\n", w, h);
    for (uint32_t i = 0; i < w * h; ++i) {
        const uint8_t px[3] = {uint8_t(argb[i] >> 16), uint8_t(argb[i] >> 8), uint8_t(argb[i])};
        std::fwrite(px, 1, 3, o);
    }
    std::fclose(o);
}

// Raw frame dump so a renderer can be developed and tested offline. All sections are length-prefixed
// little-endian blobs in a fixed order: header, vertices, ranges, textures (header + argb), overlays
// (header + argb), frameConstants, lightConstants, fogConstants.
static void dumpFrame(const std::string& path) {
    const auto f = getFrame();
    FILE* o = std::fopen(path.c_str(), "wb");
    if (!o) return;
    auto put = [&](const void* p, size_t n) { std::fwrite(p, 1, n, o); };
    const uint32_t magic = 0x31464449;  // "IDF1"
    put(&magic, 4);
    put(&f, sizeof f);  // pointers inside are meaningless on reload; counts and cameras are not
    put(f.vertices, f.vertexCount * sizeof(Idas3SceneVertex));
    put(f.ranges, f.rangeCount * sizeof(Idas3SceneRange));
    for (uint32_t i = 0; i < f.textureCount; ++i) {
        put(&f.textures[i].width, 4);
        put(&f.textures[i].height, 4);
        put(f.textures[i].argb, size_t(f.textures[i].width) * f.textures[i].height * 4);
    }
    for (uint32_t i = 0; i < f.overlayCount; ++i) {
        put(&f.overlays[i].width, 4);
        put(&f.overlays[i].height, 4);
        put(&f.overlays[i].flags, 4);
        put(f.overlays[i].surface, size_t(f.overlays[i].width) * f.overlays[i].height * 4);
    }
    put(f.frameConstants, 2 * 92 * 4);
    put(f.lightConstants, 2 * 4 * 156 * 4);
    put(f.fogConstants, 136 * 4);
    std::fclose(o);
}

// UI draw-list dump: "IDU1", UnityUiFrame (40 B), draws (48 B each), vertices (24 B each), then for
// each texture {width,height,bytes} + RGBA8 pixels. This is exactly what Unity's Idas3UnityUi reads.
static void dumpUi(const std::string& path) {
    idas3::UnityUiFrame f{};
    f.size = sizeof f;
    if (!Idas3UiGetFrame(&f)) fail("Idas3UiGetFrame failed");
    std::vector<idas3::UnityUiDraw> draws(f.drawCount);
    std::vector<idas3::UnityUiVertex> verts(f.vertexCount);
    if (f.drawCount && Idas3UiCopyDraws(draws.data(), int(draws.size())) != int(draws.size())) fail("CopyDraws failed");
    if (f.vertexCount && Idas3UiCopyVertices(verts.data(), int(verts.size())) != int(verts.size())) fail("CopyVertices failed");
    FILE* o = std::fopen(path.c_str(), "wb");
    if (!o) return;
    const uint32_t magic = 0x31554449;  // "IDU1"
    std::fwrite(&magic, 4, 1, o);
    std::fwrite(&f, sizeof f, 1, o);
    std::fwrite(draws.data(), sizeof(idas3::UnityUiDraw), draws.size(), o);
    std::fwrite(verts.data(), sizeof(idas3::UnityUiVertex), verts.size(), o);
    for (uint32_t i = 0; i < f.textureCount; ++i) {
        idas3::UnityUiTextureInfo info{};
        info.size = sizeof info;
        if (!Idas3UiGetTextureInfo(i, &info)) fail("GetTextureInfo failed");
        std::vector<uint8_t> rgba(info.bytes);
        if (Idas3UiCopyTextureRGBA(i, rgba.data(), int(rgba.size())) != int(rgba.size())) fail("CopyTextureRGBA failed");
        std::fwrite(&info.width, 4, 1, o);
        std::fwrite(&info.height, 4, 1, o);
        std::fwrite(&info.bytes, 4, 1, o);
        std::fwrite(rgba.data(), 1, rgba.size(), o);
    }
    std::fclose(o);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s assetRoot saveRoot [script]\n", argv[0]);
        return 2;
    }
    std::string script = "w1 i w60 i w120 i";
    if (argc > 3) script = argv[3];
    if (!script.empty() && script[0] == '@') {  // @file: read the script from a file, '#' starts a comment
        FILE* f = std::fopen(script.c_str() + 1, "rb");
        if (!f) { std::fprintf(stderr, "cannot open script %s\n", script.c_str() + 1); return 2; }
        std::string text; char buf[4096]; size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
        std::fclose(f);
        script.clear();
        bool comment = false;
        for (char c : text) {
            if (c == '#') comment = true;
            if (c == '\n') { comment = false; script += ' '; continue; }
            if (!comment) script += (c == '\t' || c == '\r') ? ' ' : c;
        }
    }
    if (!Idas3SceneInitialize(argv[1], argv[2], 640, 480)) fail("Idas3SceneInitialize failed");
    summary("init");

    Idas3UnityInput base{};
    base.size = sizeof base;
    base.flags = 1;  // focused
    base.deltaSeconds = 1.0 / 60.0;

    size_t pos = 0;
    while (pos < script.size()) {
        while (pos < script.size() && script[pos] == ' ') ++pos;
        size_t end = script.find(' ', pos);
        if (end == std::string::npos) end = script.size();
        const std::string tok = script.substr(pos, end - pos);
        pos = end;
        if (tok.empty()) continue;
        const char cmd = tok[0];
        const char* arg = tok.c_str() + 1;
        auto num = [&](int index, long fallback, int radix = 10) {  // index-th ':' separated field
            const char* p = arg;
            for (int i = 0; i < index; ++i) {
                p = std::strchr(p, ':');
                if (!p) return fallback;
                ++p;
            }
            return std::strtol(p, nullptr, radix);
        };
        Idas3UnityInput in = base;
        long hold = 0;
        switch (cmd) {
        case 'w':
            hold = num(0, 1);
            break;
        case 'k': {
            const long vk = num(0, 0);
            if (vk >= 0 && vk < 256) in.keys[vk / 32] |= 1u << (vk % 32);
            hold = num(1, 4);
            break;
        }
        case 'p':
            in.padConnected = 1;
            in.padButtons = (uint32_t)num(0, 0, 16);
            hold = num(1, 4);
            break;
        case 't':
            in.padConnected = 1;
            in.leftTrigger = (uint32_t)num(0, 0);
            in.rightTrigger = (uint32_t)num(1, 0);
            hold = num(2, 4);
            break;
        case 'x':
            in.padConnected = 1;
            in.thumbLX = (int32_t)num(0, 0);
            hold = num(1, 4);
            break;
        case 's': {
            const auto f = getFrame();
            for (uint32_t i = 0; i < f.overlayCount; ++i)
                writePpm(std::string(arg) + "_o" + std::to_string(i) + ".ppm", f.overlays[i].surface, f.overlays[i].width, f.overlays[i].height);
            continue;
        }
        case 'g':
            dumpFrame(std::string(arg) + ".frame");
            continue;
        case 'u':
            dumpUi(std::string(arg) + ".ui");
            continue;
        case 'A': {
            g_audioPath = arg; g_pcm.clear(); g_recording = true;
            Idas3UnitySetAudioRunning(1);
            Idas3UnityAudioStatistics st; Idas3UnityGetAudioStatistics(&st); g_producedBefore = st.produced;
            continue;
        }
        case 'a': {
            g_recording = false;
            writeWav(g_audioPath, g_pcm);
            double sum = 0; size_t nonzero = 0;
            for (float v : g_pcm) { sum += double(v) * v; nonzero += v != 0.f; }
            std::printf("[audio] %zu frames (%.2f s) rms=%.5f nonzero=%.1f%% underrun=%llu priming=%llu\n", g_pcm.size() / 2,
                g_pcm.size() / 2 / 44100.0, g_pcm.empty() ? 0.0 : std::sqrt(sum / g_pcm.size()), g_pcm.empty() ? 0.0 : 100.0 * nonzero / g_pcm.size(),
                (unsigned long long)g_underruns, (unsigned long long)g_primingFrames);
            continue;
        }
        case 'i':
            summary("info");
            continue;
        default:
            std::fprintf(stderr, "unknown script token '%s'\n", tok.c_str());
            return 2;
        }
        for (long i = 0; i < hold; ++i) stepInput(in);
        // After a held key, give the game one idle frame so the release is seen as an edge.
        if (cmd == 'k' || cmd == 'p') stepInput(base);
    }
    summary("end");
    Idas3SceneShutdown();
    std::puts("OK");
    return 0;
}
