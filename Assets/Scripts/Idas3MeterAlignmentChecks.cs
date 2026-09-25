using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.Rendering;
using Layer = Idas3ArcadeMeterCatalog.Layer;
using Meter = Idas3ArcadeMeterCatalog.Meter;
using Sprite = Idas3ArcadeHud.Sprite;
using Telemetry = Idas3ArcadeHud.Telemetry;

// Regression for the Future meters' stationary red ghost needles. Exercise the
// production compositor, including authored pivots and retainer masks, rather
// than merely checking the history helper's returned angles.
public static class Idas3MeterAlignmentChecks
{
    const int Width = 1280, Height = 720;
    static readonly int[] Sources = { 3, 14, 15, 34, 82 };

    [Serializable] sealed class Report
    {
        public bool passed;
        public int checks, gpuFrames, composeUpdates, initialResidentTextures, finalResidentTextures;
        public string graphicsDevice;
        public string coverage = "Future 3/14/15/34/82: production Compose needle/trail alignment, preserved pivots, responsive RPM and speed histories, clock freeze, reset, 30/60/144 Hz linear input; production GPU Build/Render before/during/settled captures. This does not establish original arcade timing parity.";
        public List<Result> results = new List<Result>();
        public List<string> errors = new List<string>();
    }
    [Serializable] sealed class Result
    {
        public int sourceId, changedPixels;
        public string kind, beforeImage, duringImage, afterImage;
        public bool passed = true;
        public float maximumMatrixDifference, rpmHistorySpreadDegrees, speedHistorySpreadDegrees;
        public List<string> errors = new List<string>();
    }

    sealed class NeedleScope : IDisposable
    {
        internal readonly Meter meter;
        internal readonly Layer[] layers;
        readonly Layer[] complete;
        internal NeedleScope(int source)
        {
            int style = source + 2;
            // Bounds must be cached from the complete meter before narrowing
            // the in-memory list. No serialized catalog assets are changed.
            Idas3MeterLayoutBounds.Get(style);
            meter = Idas3ArcadeMeterCatalog.Get(style);
            complete = meter.layers;
            var selected = new List<Layer>();
            foreach (var layer in complete)
                if (layer.name == "CenterPin" || layer.name == "LeftPin" ||
                    layer.name.StartsWith("CenterPinTrail", StringComparison.Ordinal) ||
                    layer.name.StartsWith("LeftPinTrail", StringComparison.Ordinal)) selected.Add(layer);
            layers = selected.ToArray(); meter.layers = layers;
        }
        public void Dispose() => meter.layers = complete;
    }

    sealed class Runner : IDisposable
    {
        internal readonly Report report = new Report();
        readonly string output;
        readonly GameObject host;
        readonly Camera camera;
        readonly RenderTexture target;
        readonly Texture2D readback;
        readonly CommandBuffer commands;

        internal Runner(string output)
        {
            this.output = output;
            report.graphicsDevice = SystemInfo.graphicsDeviceName;
            report.initialResidentTextures = Idas3ImportedMeter.ResidentTextureCount;
            host = new GameObject("Meter alignment GPU checks") { hideFlags = HideFlags.HideAndDontSave };
            camera = host.AddComponent<Camera>();
            camera.enabled = false; camera.cullingMask = 0; camera.allowHDR = false; camera.allowMSAA = false;
            camera.clearFlags = CameraClearFlags.SolidColor; camera.backgroundColor = Color.black;
            target = new RenderTexture(Width, Height, 24, RenderTextureFormat.ARGB32)
                { name = "Meter alignment QA", hideFlags = HideFlags.HideAndDontSave, antiAliasing = 1 };
            if (!target.Create()) throw new InvalidOperationException("Could not create meter alignment target.");
            camera.targetTexture = target;
            readback = new Texture2D(Width, Height, TextureFormat.RGB24, false) { hideFlags = HideFlags.HideAndDontSave };
            commands = new CommandBuffer { name = "Meter alignment GPU checks" };
            camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, commands);
        }

        static Idas3GameOptions.Values Options(int source) => new Idas3GameOptions.Values
            { hudMeterStyle = source + 2, hudShiftLights = true, hudPedalIndicators = true, hudNameplateStyle = 0 };
        static Telemetry Input(float progress) => new Telemetry
        {
            size = 40, version = 2, flags = 1, gear = 3, revLimit = 8500,
            rpm = 1500 + 5000 * progress, speedKmh = 30 + 150 * progress,
            throttle = .7f, brake = .25f
        };
        Result Case(int source, string kind)
        {
            var result = new Result { sourceId = source, kind = kind };
            report.results.Add(result); return result;
        }
        void Verify(bool condition, Result result, string message)
        {
            ++report.checks;
            if (condition) return;
            result.passed = false; result.errors.Add(message);
            report.errors.Add(result.sourceId + " / " + result.kind + ": " + message);
        }
        Dictionary<string, Sprite> Compose(Idas3ImportedMeter adapter, NeedleScope scope, Telemetry telemetry, float seconds, Result result)
        {
            var sprites = new List<Sprite>();
            adapter.Compose(sprites, scope.meter, Options(scope.meter.id), telemetry, seconds);
            ++report.composeUpdates;
            if (sprites.Count != scope.layers.Length)
                throw new InvalidOperationException("Needle-only fixture expected " + scope.layers.Length + " sprites, got " + sprites.Count + ".");
            var named = new Dictionary<string, Sprite>();
            for (int i = 0; i < sprites.Count; ++i) named.Add(scope.layers[i].name, sprites[i]);
            return named;
        }
        static float Difference(Matrix4x4 a, Matrix4x4 b)
        {
            float maximum = 0;
            for (int i = 0; i < 16; ++i) maximum = Mathf.Max(maximum, Mathf.Abs(a[i] - b[i]));
            return maximum;
        }
        static float Angle(Sprite sprite) => Mathf.Atan2(sprite.transform.m10, sprite.transform.m00) * Mathf.Rad2Deg;
        static Vector3 Pivot(Sprite sprite) => sprite.transform.MultiplyPoint3x4(new Vector3(sprite.rect.width * .5f, sprite.rect.height * .5f, 0));
        void Aligned(Dictionary<string, Sprite> frame, Result result, string label)
        {
            int count = 0;
            foreach (var item in frame)
            {
                string head = item.Key.StartsWith("CenterPinTrail", StringComparison.Ordinal) ? "CenterPin" :
                    item.Key.StartsWith("LeftPinTrail", StringComparison.Ordinal) ? "LeftPin" : null;
                if (head == null) continue;
                ++count;
                float difference = Difference(item.Value.transform, frame[head].transform);
                result.maximumMatrixDifference = Mathf.Max(result.maximumMatrixDifference, difference);
                Verify(difference < .002f, result, label + ": " + item.Key + " remains detached from its steady needle (matrix delta " + difference + ").");
            }
            Verify(count == 20, result, label + ": expected twenty authored needle trails.");
        }
        void Same(Dictionary<string, Sprite> expected, Dictionary<string, Sprite> actual, Result result, string label, float tolerance = .002f)
        {
            foreach (var pair in expected)
            {
                float difference = Difference(pair.Value.transform, actual[pair.Key].transform);
                Verify(difference < tolerance, result, label + ": transform changed for " + pair.Key + " (delta " + difference + ").");
            }
        }
        void History(Dictionary<string, Sprite> frame, Result result)
        {
            foreach (string head in new[] { "CenterPin", "LeftPin" })
            {
                float spread = 0; var live = frame[head];
                foreach (var item in frame)
                {
                    if (!item.Key.StartsWith(head + "Trail", StringComparison.Ordinal)) continue;
                    float lag = Mathf.DeltaAngle(Angle(item.Value), Angle(live));
                    spread = Mathf.Max(spread, Mathf.Abs(lag));
                    Verify(lag >= -.02f && lag < 45, result, item.Key + " is not a recent sample behind an increasing needle.");
                    Verify(Vector3.Distance(Pivot(item.Value), Pivot(live)) < .002f, result, item.Key + " moved away from its authored needle pivot.");
                }
                Verify(spread > 1, result, head + " does not retain a visible history during a changing input.");
                if (head == "CenterPin") result.rpmHistorySpreadDegrees = spread; else result.speedHistorySpreadDegrees = spread;
            }
        }

        void Compositor(int source)
        {
            var result = Case(source, "production-compositor");
            using (var scope = new NeedleScope(source))
            using (var adapter = new Idas3ImportedMeter())
            {
                Verify(scope.layers.Length == 22, result, "Fixture must retain both primary needles and all twenty source trails.");
                Aligned(Compose(adapter, scope, Input(0), 0, result), result, "First observation");
                Dictionary<string, Sprite> moving = null;
                for (int i = 1; i <= 60; ++i) moving = Compose(adapter, scope, Input(i / 60f), i / 60f, result);
                History(moving, result);
                for (int i = 0; i < 100; ++i) Compose(adapter, scope, Input(1), 1, result);
                Same(moving, Compose(adapter, scope, Input(1), 1, result), result, "Frozen clock");
                for (int i = 1; i <= 30; ++i) Compose(adapter, scope, Input(1), 1 + i / 60f, result);
                Aligned(Compose(adapter, scope, Input(1), 1.5f, result), result, "Settled input");
                Aligned(Compose(adapter, scope, Input(.25f), .25f, result), result, "Clock rewind");
                Aligned(Compose(adapter, scope, Input(.6f), 10, result), result, "Long gap");
                var lost = Input(.6f); lost.flags = 0;
                Compose(adapter, scope, lost, 10.01f, result);
                Aligned(Compose(adapter, scope, Input(.2f), 10.02f, result), result, "Telemetry restored");
                var newScale = Input(.8f); newScale.revLimit = 11000;
                Aligned(Compose(adapter, scope, newScale, 10.03f, result), result, "Tachometer scale changed");
                adapter.Compose(new List<Sprite>(), Idas3ArcadeMeterCatalog.Get(42), Options(40), Input(.1f), 10.04f);
                Aligned(Compose(adapter, scope, Input(.9f), 10.05f, result), result, "Meter switched back");
            }
        }

        void CompositorRates(int source)
        {
            var result = Case(source, "compositor-update-rates");
            Dictionary<string, Sprite> reference = null;
            using (var scope = new NeedleScope(source))
                foreach (int rate in new[] { 30, 60, 144 })
                    using (var adapter = new Idas3ImportedMeter())
                    {
                        Dictionary<string, Sprite> frame = null;
                        for (int i = 0; i <= rate; ++i) frame = Compose(adapter, scope, Input((float)i / rate), (float)i / rate, result);
                        if (reference == null) reference = frame;
                        else Same(reference, frame, result, rate + " Hz linear input", .01f);
                    }
        }

        void Build(Idas3ArcadeHud renderer, int source, Telemetry data, float seconds)
            => renderer.Build(Options(source), data, Width, Height, seconds, true, out _);
        Color32[] Capture(Idas3ArcadeHud renderer, int source, Telemetry data, float seconds)
        {
            commands.Clear(); Build(renderer, source, data, seconds); renderer.Render(commands, Width, Height); camera.Render();
            var previous = RenderTexture.active;
            try { RenderTexture.active = target; readback.ReadPixels(new Rect(0, 0, Width, Height), 0, 0); readback.Apply(false); }
            finally { RenderTexture.active = previous; }
            ++report.gpuFrames; return readback.GetPixels32();
        }
        Color32[] Fresh(int source, Telemetry data, float seconds)
        {
            using (var renderer = new Idas3ArcadeHud())
                try { return Capture(renderer, source, data, seconds); }
                finally { commands.Clear(); }
        }
        static int Changed(Color32[] a, Color32[] b, int threshold = 0)
        {
            int changed = 0;
            for (int i = 0; i < a.Length; ++i)
                if (Mathf.Max(Mathf.Abs(a[i].r - b[i].r), Mathf.Abs(a[i].g - b[i].g), Mathf.Abs(a[i].b - b[i].b)) > threshold) ++changed;
            return changed;
        }
        static int Visible(Color32[] pixels)
        {
            int count = 0;
            foreach (var pixel in pixels) if (Mathf.Max(pixel.r, pixel.g, pixel.b) > 8) ++count;
            return count;
        }
        string Save(int source, string name, Color32[] pixels)
        {
            string file = "meter-" + source.ToString("00") + "-" + name + ".png";
            readback.SetPixels32(pixels); readback.Apply(false);
            File.WriteAllBytes(Path.Combine(output, file), readback.EncodeToPNG()); return file;
        }
        void Gpu(int source)
        {
            var result = Case(source, "gpu-alignment-and-history");
            using (var renderer = new Idas3ArcadeHud())
            {
                try
                {
                    var before = Capture(renderer, source, Input(0), 0);
                    Verify(Visible(before) > 128, result, "Meter GPU baseline is blank.");
                    for (int i = 1; i <= 60; ++i) Build(renderer, source, Input(i / 60f), i / 60f);
                    var during = Capture(renderer, source, Input(1), 1);
                    // The control has identical current telemetry and time. A
                    // difference therefore proves retained trail history rather
                    // than a changed main needle, digits or ambient animation.
                    var control = Fresh(source, Input(1), 1);
                    result.changedPixels = Changed(during, control, 8);
                    Verify(result.changedPixels > 24, result, "Changing telemetry produces no visible needle history beyond the current needle.");
                    for (int i = 0; i < 100; ++i) Build(renderer, source, Input(1), 1);
                    Verify(Changed(during, Capture(renderer, source, Input(1), 1)) == 0, result, "Frozen GPU clock consumes history.");
                    for (int i = 1; i <= 30; ++i) Build(renderer, source, Input(1), 1 + i / 60f);
                    var settled = Capture(renderer, source, Input(1), 1.5f);
                    Verify(Changed(settled, Fresh(source, Input(1), 1.5f)) == 0, result, "GPU trails do not settle onto a steady needle.");
                    result.beforeImage = Save(source, "before", before);
                    result.duringImage = Save(source, "moving-history", during);
                    result.afterImage = Save(source, "settled", settled);
                    Save(source, "same-time-no-history-control", control);
                    var rewind = Capture(renderer, source, Input(.25f), .25f);
                    Verify(Changed(rewind, Fresh(source, Input(.25f), .25f)) == 0, result, "Rewound GPU clock retains an old needle streak.");
                }
                finally { commands.Clear(); }
            }
        }
        void GpuRates(int source)
        {
            var result = Case(source, "gpu-update-rates"); Color32[] reference = null;
            foreach (int rate in new[] { 30, 60, 144 })
                using (var renderer = new Idas3ArcadeHud())
                {
                    try
                    {
                        for (int i = 0; i <= rate; ++i) Build(renderer, source, Input((float)i / rate), (float)i / rate);
                        var frame = Capture(renderer, source, Input(1), 1);
                        if (reference == null) reference = frame;
                        else Verify(Changed(reference, frame, 2) < 8, result, rate + " Hz creates a materially different GPU trail frame.");
                        Save(source, "linear-" + rate + "hz", frame);
                    }
                    finally { commands.Clear(); }
                }
        }
        internal void Run()
        {
            if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null) throw new InvalidOperationException("Alignment checks require a GPU; omit -nographics.");
            if (!Idas3ArcadeHud.Available) throw new InvalidOperationException("Shared meter artwork is unavailable.");
            foreach (int source in Sources)
            {
                try { Compositor(source); CompositorRates(source); Gpu(source); GpuRates(source); }
                catch (Exception error) { report.errors.Add(source + ": " + error); }
            }
            report.finalResidentTextures = Idas3ImportedMeter.ResidentTextureCount;
            ++report.checks;
            if (report.finalResidentTextures != report.initialResidentTextures) report.errors.Add("Alignment QA retained imported texture leases.");
            report.passed = report.errors.Count == 0;
        }
        internal void Write() => File.WriteAllText(Path.Combine(output, "report.json"), JsonUtility.ToJson(report, true));
        public void Dispose()
        {
            camera.RemoveAllCommandBuffers(); camera.targetTexture = null; commands.Dispose(); target.Release();
            Destroy(readback); Destroy(target); Destroy(host);
        }
    }
    static void Destroy(UnityEngine.Object value)
    {
        if (!value) return;
        if (Application.isPlaying) UnityEngine.Object.Destroy(value); else UnityEngine.Object.DestroyImmediate(value);
    }
    public static string Run(string outputDirectory = null)
    {
        string output = Path.GetFullPath(outputDirectory ?? Path.Combine("Verification/meter-alignment",
            "gpu-" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N").Substring(0, 8)));
        if (Directory.Exists(output) && Directory.GetFileSystemEntries(output).Length > 0) throw new IOException("Alignment QA output must be fresh: " + output);
        Directory.CreateDirectory(output);
        using (var runner = new Runner(output))
        {
            try { runner.Run(); }
            catch (Exception error) { runner.report.passed = false; runner.report.errors.Add(error.ToString()); }
            finally { runner.Write(); }
            if (!runner.report.passed) throw new InvalidOperationException("Meter alignment checks failed; see " + Path.Combine(output, "report.json"));
            Debug.Log("Meter alignment checks passed: " + runner.report.checks + " checks, " + runner.report.gpuFrames + " GPU frames. " + output);
        }
        return output;
    }
}
