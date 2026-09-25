using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.Rendering;

// Complement the source/Compose checks with actual Halloween pixels. Lantern
// controls retain the same telemetry and time so only an observed drift edge
// can explain the isolated visual difference.
public static class Idas3HalloweenMeterGpuChecks
{
    const int Width = 1280, Height = 720, Style = 85;
    [Serializable] sealed class Report
    {
        public bool passed;
        public int checks, gpuFrames, initialResidentTextures, finalResidentTextures;
        public string graphicsDevice;
        public string coverage = "Production Halloween83 Build/Render: day/night 10000/13000 RPM faces at 75/150% size with full decorations; isolated Cantera drift entry/exit against same-time fresh controls, freeze and expiry. Full and cropped PNGs require visual inspection; this does not claim arcade shader parity.";
        public List<Result> results = new List<Result>();
        public List<string> errors = new List<string>();
    }
    [Serializable] sealed class Result
    {
        public string kind, image, controlImage, afterImage;
        public int maximumRpm, sizePercent, visiblePixels, changedPixels;
        public bool night, passed = true;
        public List<string> errors = new List<string>();
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
            this.output = output; report.graphicsDevice = SystemInfo.graphicsDeviceName;
            report.initialResidentTextures = Idas3ImportedMeter.ResidentTextureCount;
            host = new GameObject("Halloween meter GPU checks") { hideFlags = HideFlags.HideAndDontSave };
            camera = host.AddComponent<Camera>(); camera.enabled = false; camera.cullingMask = 0;
            camera.allowHDR = false; camera.allowMSAA = false; camera.backgroundColor = Color.black; camera.clearFlags = CameraClearFlags.SolidColor;
            target = new RenderTexture(Width, Height, 24, RenderTextureFormat.ARGB32)
                { name = "Halloween meter QA", antiAliasing = 1, hideFlags = HideFlags.HideAndDontSave };
            if (!target.Create()) throw new InvalidOperationException("Could not create Halloween GPU target.");
            camera.targetTexture = target;
            readback = new Texture2D(Width, Height, TextureFormat.RGB24, false) { hideFlags = HideFlags.HideAndDontSave };
            commands = new CommandBuffer { name = "Halloween meter GPU checks" };
            camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, commands);
        }
        static Idas3GameOptions.Values Options(int size)
        {
            var options = new Idas3GameOptions.Values { hudMeterStyle = Style, hudNameplateStyle = 0, hudPedalIndicators = true, hudShiftLights = true };
            options.SetHudSizePercent(2, size); return options;
        }
        static Idas3ArcadeHud.Telemetry Data(int maximum = 10000, bool night = false, bool drifting = false) => new Idas3ArcadeHud.Telemetry
        {
            size = 40, version = 2, flags = 1u | (night ? 4u : 0u) | (drifting ? 8u : 0u), gear = 4,
            revLimit = maximum == 10000 ? 9500 : 11000, rpm = maximum * .7f, speedKmh = 145,
            throttle = .65f, brake = .2f, driftOpacity = drifting ? .8f : 0
        };
        Result Case(string kind)
        {
            var result = new Result { kind = kind }; report.results.Add(result); return result;
        }
        void Verify(bool condition, Result result, string message)
        {
            ++report.checks; if (condition) return;
            result.passed = false; result.errors.Add(message); report.errors.Add(result.kind + ": " + message);
        }
        static int Visible(Color32[] pixels)
        {
            int visible = 0; foreach (var pixel in pixels) if (Mathf.Max(pixel.r, pixel.g, pixel.b) > 8) ++visible; return visible;
        }
        static int Changed(Color32[] first, Color32[] second, int threshold = 0)
        {
            int changed = 0;
            for (int i = 0; i < first.Length; ++i)
                if (Mathf.Max(Mathf.Abs(first[i].r - second[i].r), Mathf.Abs(first[i].g - second[i].g), Mathf.Abs(first[i].b - second[i].b)) > threshold) ++changed;
            return changed;
        }
        void Build(Idas3ArcadeHud renderer, Idas3GameOptions.Values options, Idas3ArcadeHud.Telemetry data, float seconds)
            => renderer.Build(options, data, Width, Height, seconds, true, out _);
        Color32[] Capture(Idas3ArcadeHud renderer, Idas3GameOptions.Values options, Idas3ArcadeHud.Telemetry data, float seconds, out Rect bounds)
        {
            commands.Clear(); renderer.Build(options, data, Width, Height, seconds, true, out bounds); renderer.Render(commands, Width, Height); camera.Render();
            var previous = RenderTexture.active;
            try { RenderTexture.active = target; readback.ReadPixels(new Rect(0, 0, Width, Height), 0, 0); readback.Apply(false); }
            finally { RenderTexture.active = previous; }
            ++report.gpuFrames; return readback.GetPixels32();
        }
        Color32[] Fresh(Idas3GameOptions.Values options, Idas3ArcadeHud.Telemetry data, float seconds)
        {
            using (var renderer = new Idas3ArcadeHud())
                try { return Capture(renderer, options, data, seconds, out _); }
                finally { commands.Clear(); }
        }
        string Save(string label, Color32[] pixels, Rect bounds)
        {
            string file = "halloween-" + label + ".png";
            readback.SetPixels32(pixels); readback.Apply(false); File.WriteAllBytes(Path.Combine(output, file), readback.EncodeToPNG());
            int x = Mathf.Clamp(Mathf.FloorToInt(bounds.x) - 3, 0, Width - 1);
            int y = Mathf.Clamp(Mathf.FloorToInt(Height - bounds.yMax) - 3, 0, Height - 1);
            int w = Mathf.Min(Width - x, Mathf.CeilToInt(bounds.width) + 6), h = Mathf.Min(Height - y, Mathf.CeilToInt(bounds.height) + 6);
            var crop = new Texture2D(w, h, TextureFormat.RGB24, false) { hideFlags = HideFlags.HideAndDontSave };
            try { crop.SetPixels(readback.GetPixels(x, y, w, h)); crop.Apply(false); File.WriteAllBytes(Path.Combine(output, "halloween-" + label + "-crop.png"), crop.EncodeToPNG()); }
            finally { Destroy(crop); }
            return file;
        }
        void WholeMeters()
        {
            foreach (int maximum in new[] { 10000, 13000 })
                foreach (bool night in new[] { false, true })
                    foreach (int size in new[] { 75, 150 })
                    {
                        var result = Case("whole-" + maximum + "-" + (night ? "night" : "day") + "-" + size);
                        result.maximumRpm = maximum; result.night = night; result.sizePercent = size;
                        using (var renderer = new Idas3ArcadeHud())
                            try
                            {
                                var frame = Capture(renderer, Options(size), Data(maximum, night), .125f, out Rect bounds);
                                result.visiblePixels = Visible(frame); Verify(result.visiblePixels > 128, result, "Whole Halloween meter is blank.");
                                result.image = Save(result.kind, frame, bounds);
                            }
                            finally { commands.Clear(); }
                    }
            var drifting = Case("whole-drifting");
            using (var renderer = new Idas3ArcadeHud())
                try
                {
                    var options = Options(150); Build(renderer, options, Data(), 0); Build(renderer, options, Data(drifting: true), .1f);
                    var image = Capture(renderer, options, Data(drifting: true), .1f + 10000f / 24000, out Rect bounds);
                    drifting.image = Save(drifting.kind, image, bounds); Verify(Visible(image) > 128, drifting, "Whole drifting meter is blank.");
                }
                finally { commands.Clear(); }
        }
        void Lantern(bool exiting)
        {
            var result = Case(exiting ? "lantern-leave" : "lantern-enter");
            var options = Options(150); var meter = Idas3ArcadeMeterCatalog.Get(Style);
            Idas3MeterLayoutBounds.Get(Style); // Cache complete meter before isolation.
            var complete = meter.layers; var selected = new List<Idas3ArcadeMeterCatalog.Layer>();
            foreach (var layer in complete)
                if (layer.parents != null)
                    foreach (var owner in layer.parents)
                        if (owner.name == "Cantera") { selected.Add(layer); break; }
            Verify(selected.Count > 0, result, "Cantera owner has no recovered layers.");
            try
            {
                meter.layers = selected.ToArray();
                using (var renderer = new Idas3ArcadeHud())
                {
                    try
                    {
                        var before = Data(drifting: exiting); var after = Data(drifting: !exiting);
                        Build(renderer, options, before, 0); Build(renderer, options, after, .1f);
                        float peak = .1f + (exiting ? 20000f : 10000f) / 24000;
                        var image = Capture(renderer, options, after, peak, out Rect bounds);
                        var control = Fresh(options, after, peak);
                        result.visiblePixels = Visible(image); result.changedPixels = Changed(image, control, 8);
                        Verify(result.visiblePixels > 24, result, "Isolated lantern art is blank.");
                        Verify(result.changedPixels > 24, result, "Observed drift edge produces no lantern swing beyond identical current telemetry.");
                        result.image = Save(result.kind + "-peak", image, bounds); result.controlImage = Save(result.kind + "-no-edge-control", control, bounds);
                        for (int i = 0; i < 60; ++i) Build(renderer, options, after, peak);
                        Verify(Changed(image, Capture(renderer, options, after, peak, out _)) == 0, result, "Frozen presentation clock advances lantern swing.");
                        var settled = Capture(renderer, options, after, 1.5f, out bounds);
                        Verify(Changed(settled, Fresh(options, after, 1.5f)) == 0, result, "Lantern does not return to its authored resting pose after the event.");
                        result.afterImage = Save(result.kind + "-settled", settled, bounds);
                    }
                    finally { commands.Clear(); }
                }
            }
            finally { meter.layers = complete; }
        }
        internal void Run()
        {
            if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null) throw new InvalidOperationException("Halloween checks require a GPU.");
            if (!Idas3ArcadeHud.Available) throw new InvalidOperationException("Shared HUD art is unavailable.");
            WholeMeters(); Lantern(false); Lantern(true);
            report.finalResidentTextures = Idas3ImportedMeter.ResidentTextureCount; ++report.checks;
            if (report.finalResidentTextures != report.initialResidentTextures) report.errors.Add("Halloween GPU QA retained imported texture leases.");
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
        string output = Path.GetFullPath(outputDirectory ?? Path.Combine("Verification/halloween-meter",
            "gpu-" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N").Substring(0, 8)));
        if (Directory.Exists(output) && Directory.GetFileSystemEntries(output).Length > 0) throw new IOException("Halloween GPU output must be fresh: " + output);
        Directory.CreateDirectory(output);
        using (var runner = new Runner(output))
        {
            try { runner.Run(); }
            catch (Exception error) { runner.report.passed = false; runner.report.errors.Add(error.ToString()); }
            finally { runner.Write(); }
            if (!runner.report.passed) throw new InvalidOperationException("Halloween GPU checks failed; see " + Path.Combine(output, "report.json"));
            Debug.Log("Halloween GPU checks passed: " + runner.report.checks + " checks, " + runner.report.gpuFrames + " frames. " + output);
        }
        return output;
    }
}
