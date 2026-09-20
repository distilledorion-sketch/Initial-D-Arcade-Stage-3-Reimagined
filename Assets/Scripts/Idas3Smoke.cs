using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

// Diagnostic only: no component is created without -idas3-smoke <NEW directory>.
// Captures intentionally read the GPU. Ordinary gameplay never uses this path.
public sealed class Idas3Smoke : MonoBehaviour
{
    private static Idas3Smoke active;
    private Idas3Game host;
    private string root;
    private StreamWriter events;
    private bool frozen, finished;
    private int pulseKey, heldKey;
    private double started;
    private ulong lastLoggedFrame = ulong.MaxValue;
    private readonly List<CaptureResult> captures = new List<CaptureResult>();
    private readonly HashSet<int> attractChildren = new HashSet<int>();
    private int checks;

    [Serializable] private sealed class CaptureResult
    {
        public string name, rawFormat = "RGBA32", rawRowConvention = "D3D11 resource rows; expected top row first";
        public int nativeWidth, nativeHeight, screenWidth, screenHeight;
        public string renderedFrames, simulationTicks;
        public int frontendStage, attractChild, course, car, racePhase;
        public uint flags;
        public float speedMetresPerSecond;
        public double topLeftMeanRgbError, verticallyFlippedMeanRgbError;
        public double encodedGammaMeanRgbError, decodedGammaMeanRgbError;
        public int nativeRgbRange;
        public string rawFnv64;
    }
    [Serializable] private sealed class Report
    {
        public bool passed, shutdownComplete;
        public string error, unityVersion, graphicsDevice, graphicsApi, colorSpace, saveRoot;
        public int checks;
        public double elapsedSeconds;
        public int[] observedAttractChildren;
        public CaptureResult[] captures;
        public string scope = "Two moving attract frames, original F5 quick-start, actual race bumper/chase, paused repaint, menu return, clean shutdown. This does not test all menu routes or audio output.";
    }

    // Hook in Idas3Game.Awake, before creating saves or queueing native Init.
    internal static void Configure(Idas3Game game, ref string saveRoot)
    {
        var args = Environment.GetCommandLineArgs();
        for (int i = 0; i < args.Length; ++i)
        {
            if (args[i] != "-idas3-smoke") continue;
            if (i + 1 >= args.Length || args[i + 1].StartsWith("-", StringComparison.Ordinal))
                throw new ArgumentException("-idas3-smoke requires a new output directory.");
            string path = Path.GetFullPath(args[i + 1]);
            if (Directory.Exists(path) || File.Exists(path))
                throw new IOException("Smoke output must be a new directory: " + path);
            if (active != null) throw new InvalidOperationException("Only one smoke host is allowed.");
            Directory.CreateDirectory(path);
            File.WriteAllText(Path.Combine(path, "IDAS3_UNITY_SMOKE.txt"), "Isolated diagnostic output and saves. No ordinary profile was loaded.\n");
            saveRoot = Path.Combine(path, "userdata");
            game.audioEnabled = false;
            game.renderWidth = 1280; game.renderHeight = 720;
            active = game.gameObject.AddComponent<Idas3Smoke>();
            active.host = game; active.root = path; active.started = Time.realtimeSinceStartupAsDouble;
            active.events = new StreamWriter(Path.Combine(path, "status.csv"));
            active.events.AutoFlush = true;
            active.events.WriteLine("unity_frame,rendered_frames,simulation_ticks,state,stage,attract_child,race_phase,course,car,flags,speed,rpm,last_event");
            Application.runInBackground = true;
            if (!Application.isEditor) Screen.SetResolution(1280, 720, FullScreenMode.Windowed);
            return;
        }
    }

    // Hook after physical input collection, immediately before QueueFrame.
    // Fixed steps and injected held keys replace all physical input in smoke.
    internal static bool PrepareFrame(ref Idas3Native.FrameInput frame)
    {
        if (active == null) return true;
        if (active.frozen || active.finished) return false;
        frame = new Idas3Native.FrameInput {
            size = (uint)Marshal.SizeOf<Idas3Native.FrameInput>(), flags = 1, deltaSeconds = 1.0 / 60.0
        };
        if (active.heldKey != 0) frame.SetKey(active.heldKey);
        if (active.pulseKey != 0) { frame.SetKey(active.pulseKey); active.pulseKey = 0; }
        return true;
    }

    private void Start() { StartCoroutine(Guard(Run())); }
    private void Update()
    {
        if (finished) return;
        if (host.SmokeFailure != null) { Fail(host.SmokeFailure); return; }
        if (Time.realtimeSinceStartupAsDouble - started > 600) { Fail("Smoke exceeded its 10 minute deadline."); return; }
        var s = Idas3Native.ReadStatus();
        if (s.state == 2) { Fail(Idas3Native.Error()); return; }
        if (s.state == 1 && s.renderedFrames != lastLoggedFrame)
        {
            lastLoggedFrame = s.renderedFrames;
            if ((s.flags & 1) != 0 && s.frontendStage == 0) attractChildren.Add(s.attractChild);
            events.WriteLine(string.Join(",", Time.frameCount, s.renderedFrames, s.simulationTicks, s.state,
                s.frontendStage, s.attractChild, s.racePhase, s.course, s.car, s.flags,
                s.speedMetresPerSecond.ToString("R", CultureInfo.InvariantCulture),
                s.rpm.ToString("R", CultureInfo.InvariantCulture), s.lastEventId));
        }
    }
    private IEnumerator Guard(IEnumerator sequence)
    {
        var stack = new Stack<IEnumerator>(); stack.Push(sequence);
        while (stack.Count != 0 && !finished)
        {
            object next = null; Exception problem = null;
            try
            {
                if (!stack.Peek().MoveNext()) { stack.Pop(); continue; }
                next = stack.Peek().Current;
            }
            catch (Exception error) { problem = error; }
            if (problem != null) { Fail(problem.ToString()); yield break; }
            if (next is IEnumerator nested) stack.Push(nested); else yield return next;
        }
    }
    private void Check(bool condition, string text) { ++checks; if (!condition) throw new InvalidOperationException(text); }
    private IEnumerator Frames(ulong count)
    {
        ulong until = Idas3Native.ReadStatus().renderedFrames + count;
        while (Idas3Native.ReadStatus().renderedFrames < until) yield return null;
    }
    private IEnumerator Run()
    {
        while (Idas3Native.ReadStatus().state != 1 || host.SmokeTexture == null) yield return null;
        Check(!host.audioEnabled, "Smoke enabled an audio device.");
        while (Idas3Native.ReadStatus().attractChild != 7) yield return null;
        yield return Frames(700);
        Check(Idas3Native.ReadStatus().attractChild == 7, "Opening demo ended before its capture.");
        yield return Capture("attract-a");
        yield return Frames(60);
        yield return Capture("attract-b");
        Check(captures[0].rawFnv64 != captures[1].rawFnv64, "Attract presentation did not move between source frames.");

        // F5 is an existing .29 App command, not a direct scene/state mutation.
        // It starts the actual Akina Time Attack session and source countdown.
        pulseKey = 116;
        while ((Idas3Native.ReadStatus().flags & 1) != 0) yield return null;
        Check(Idas3Native.ReadStatus().course == 3, "Original quick-start did not select Akina.");
        heldKey = 87; // W / throttle
        yield return Frames(300);
        var race = Idas3Native.ReadStatus();
        Check((race.flags & 17) == 16 && race.simulationTicks >= 180 && race.racePhase == 2,
            "Race is not running under the original solver after countdown.");
        Check(race.speedMetresPerSecond > 1, "Injected throttle did not move the actual race car.");
        yield return Capture("race-bumper");
        pulseKey = 67; // C / original chase view
        yield return Frames(30);
        yield return Capture("race-chase");

        heldKey = 0; pulseKey = 27; // Pause through the ordinary input command.
        while ((Idas3Native.ReadStatus().flags & 2) == 0) yield return null;
        ulong pausedTicks = Idas3Native.ReadStatus().simulationTicks;
        yield return Frames(5);
        Check(Idas3Native.ReadStatus().simulationTicks == pausedTicks, "Pause advanced simulation.");
        yield return Capture("race-paused");
        pulseKey = 8; // Existing paused Backspace returns to course selection.
        while ((Idas3Native.ReadStatus().flags & 1) == 0) yield return null;
        yield return Frames(60);
        Check(Idas3Native.ReadStatus().frontendStage == 5, "Pause return missed the native course menu.");
        yield return Capture("course-return");
        frozen = true;
        host.StopNative();
        for (int i = 0; !host.SmokeStopped && i < 120; ++i) yield return null;
        Check(host.SmokeStopped && Idas3Native.ReadStatus().state == 0, "Native shutdown did not drain.");
        WriteReport(true, null); finished = true; events.Dispose(); events = null;
        Debug.Log("IDAS3 Unity smoke passed: " + root);
        Exit(0);
    }

    private IEnumerator Capture(string name)
    {
        frozen = true;
        // Let already-issued native events and final display blits drain. No
        // new native frame is submitted until both diagnostic captures finish.
        yield return new WaitForEndOfFrame();
        yield return new WaitForEndOfFrame();
        var state = Idas3Native.ReadStatus();
        Check(state.state == 1 && host.SmokeTexture != null, "Native frame unavailable for " + name);
        var source = host.SmokeTexture;
        var request = AsyncGPUReadback.Request(source, 0, TextureFormat.RGBA32);
        while (!request.done) yield return null;
        Check(!request.hasError, "Raw GPU texture readback failed for " + name);
        byte[] raw = request.GetData<byte>().ToArray();
        Check(raw.Length == source.width * source.height * 4, "Unexpected native RGBA byte count.");
        yield return new WaitForEndOfFrame();
        var screenshot = ScreenCapture.CaptureScreenshotAsTexture();
        Check(screenshot != null, "Unity screen capture failed.");
        File.WriteAllBytes(Path.Combine(root, name + "-unity.png"), screenshot.EncodeToPNG());
        File.WriteAllBytes(Path.Combine(root, name + "-native.rgba"), raw);
        var result = new CaptureResult {
            name = name, nativeWidth = source.width, nativeHeight = source.height,
            screenWidth = screenshot.width, screenHeight = screenshot.height,
            renderedFrames = state.renderedFrames.ToString(), simulationTicks = state.simulationTicks.ToString(),
            frontendStage = state.frontendStage, attractChild = state.attractChild, course = state.course,
            car = state.car, racePhase = state.racePhase, flags = state.flags, speedMetresPerSecond = state.speedMetresPerSecond
        };
        Compare(raw, screenshot.GetPixels32(), result);
        Destroy(screenshot);
        Check(Idas3Native.ReadStatus().renderedFrames == state.renderedFrames, "Capture freeze allowed a native frame to advance.");
        captures.Add(result);
        File.WriteAllText(Path.Combine(root, name + ".json"), JsonUtility.ToJson(result, true));
        // Keep both alternatives in evidence: this assertion catches a flipped
        // display or Unity applying an unintended transfer function to RGB.
        Check(result.nativeRgbRange > 24, "Captured native frame is effectively blank: " + name);
        Check(result.topLeftMeanRgbError <= 4.0,
            "Unity display differs from raw native pixels: " + name + "; expected MAE=" + result.topLeftMeanRgbError.ToString("F3") +
            ", flipped=" + result.verticallyFlippedMeanRgbError.ToString("F3") +
            ", gamma-encoded=" + result.encodedGammaMeanRgbError.ToString("F3") +
            ", gamma-decoded=" + result.decodedGammaMeanRgbError.ToString("F3"));
        frozen = false;
    }
    private static void Compare(byte[] raw, Color32[] screen, CaptureResult r)
    {
        int low = 255, high = 0; ulong hash = 14695981039346656037UL;
        foreach (byte b in raw) { hash ^= b; hash = unchecked(hash * 1099511628211UL); }
        for (int i = 0; i < raw.Length; i += 4) for (int c = 0; c < 3; ++c) { low = Math.Min(low, raw[i + c]); high = Math.Max(high, raw[i + c]); }
        r.nativeRgbRange = high - low; r.rawFnv64 = hash.ToString("x16");
        double fit = Math.Min((double)r.screenWidth / r.nativeWidth, (double)r.screenHeight / r.nativeHeight);
        double left = (r.screenWidth - r.nativeWidth * fit) / 2, bottom = (r.screenHeight - r.nativeHeight * fit) / 2;
        double straight = 0, flipped = 0, encoded = 0, decoded = 0; long n = 0;
        // Sample a regular grid inside the game aperture; excludes letterbox.
        // Texture2D.GetPixels32 returns screen rows bottom first. D3D11 raw
        // resource rows are compared top first, independently of display shader.
        for (int y = 2; y < r.screenHeight - 2; y += 3) for (int x = 2; x < r.screenWidth - 2; x += 3)
        {
            double sx = (x + .5 - left) / fit - .5;
            double fromBottom = (y + .5 - bottom) / fit - .5;
            if (sx < 1 || sx >= r.nativeWidth - 2 || fromBottom < 1 || fromBottom >= r.nativeHeight - 2) continue;
            Color32 pixel = screen[y * r.screenWidth + x];
            for (int c = 0; c < 3; ++c)
            {
                double native = Sample(raw, r.nativeWidth, r.nativeHeight, sx, r.nativeHeight - 1 - fromBottom, c);
                double upsideDown = Sample(raw, r.nativeWidth, r.nativeHeight, sx, fromBottom, c);
                double actual = c == 0 ? pixel.r : c == 1 ? pixel.g : pixel.b;
                straight += Math.Abs(actual - native); flipped += Math.Abs(actual - upsideDown);
                encoded += Math.Abs(actual - 255 * Mathf.LinearToGammaSpace((float)(native / 255)));
                decoded += Math.Abs(actual - 255 * Mathf.GammaToLinearSpace((float)(native / 255))); ++n;
            }
        }
        if (n == 0) throw new InvalidOperationException("No overlapping game aperture in Unity screenshot.");
        r.topLeftMeanRgbError = straight / n; r.verticallyFlippedMeanRgbError = flipped / n;
        r.encodedGammaMeanRgbError = encoded / n; r.decodedGammaMeanRgbError = decoded / n;
    }
    private static double Sample(byte[] raw, int w, int h, double x, double y, int channel)
    {
        int ix = Math.Max(0, Math.Min(w - 2, (int)Math.Floor(x))), iy = Math.Max(0, Math.Min(h - 2, (int)Math.Floor(y)));
        double fx = x - ix, fy = y - iy;
        double a = raw[(iy * w + ix) * 4 + channel], b = raw[(iy * w + ix + 1) * 4 + channel];
        double c = raw[((iy + 1) * w + ix) * 4 + channel], d = raw[((iy + 1) * w + ix + 1) * 4 + channel];
        return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy;
    }
    private void WriteReport(bool passed, string error)
    {
        var children = new List<int>(attractChildren); children.Sort();
        var report = new Report {
            passed = passed, shutdownComplete = host.SmokeStopped, error = error, checks = checks,
            unityVersion = Application.unityVersion, graphicsDevice = SystemInfo.graphicsDeviceName,
            graphicsApi = SystemInfo.graphicsDeviceType.ToString(), colorSpace = QualitySettings.activeColorSpace.ToString(),
            saveRoot = Path.Combine(root, "userdata"), elapsedSeconds = Time.realtimeSinceStartupAsDouble - started,
            observedAttractChildren = children.ToArray(), captures = captures.ToArray()
        };
        File.WriteAllText(Path.Combine(root, "report.json"), JsonUtility.ToJson(report, true));
    }
    private void Fail(string error)
    {
        if (finished) return;
        frozen = true; finished = true; StopAllCoroutines();
        try { host.StopNative(); WriteReport(false, error); } catch (Exception e) { Debug.LogError(e); }
        events?.Dispose(); events = null; Debug.LogError("IDAS3 Unity smoke failed: " + error); Exit(2);
    }
    private static void Exit(int code)
    {
#if UNITY_EDITOR
        UnityEditor.EditorApplication.isPlaying = false;
#else
        Application.Quit(code);
#endif
    }
    private void OnDestroy()
    {
        if (!finished && root != null)
        {
            try { WriteReport(false, "Smoke host destroyed before completion."); } catch (Exception error) { Debug.LogError(error); }
        }
        events?.Dispose(); events = null;
        if (active == this) active = null;
    }
}
