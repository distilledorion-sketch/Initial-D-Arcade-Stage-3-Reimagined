using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.Rendering;

// Production GPU checks for reconstructed procedural materials. Original art,
// colors and parameters are exercised; exact cooked-shader parity is not claimed.
public static class Idas3MeterRemainingEffectsChecks
{
    const int Width = 1280, Height = 720;
    static readonly float[] Silence = new float[32];
    [Serializable] sealed class Report
    {
        public bool passed;
        public int checks, gpuFrames, initialTextures, finalTextures;
        public string graphicsDevice;
        public string coverage = "Production HUD Build/Render with isolated authored layers. Aura uses a colored-background transparency check. Audio rendering receives deterministic 32-band spectra through the per-renderer override; the PCM analyzer is tested separately. No original shader/operator equivalence is established.";
        public List<Result> results = new List<Result>();
        public List<string> errors = new List<string>();
    }
    [Serializable] sealed class Result
    {
        public int sourceId, largestChangedPixels;
        public string kind;
        public bool passed = true;
        public List<Sample> samples = new List<Sample>();
        public List<string> errors = new List<string>();
    }
    [Serializable] sealed class Sample
    {
        public string name, image;
        public float seconds;
        public int visiblePixels, softPixels;
        public double red, green, blue;
    }
    sealed class LayerScope : IDisposable
    {
        readonly Idas3ArcadeMeterCatalog.Meter meter;
        readonly Idas3ArcadeMeterCatalog.Layer[] original;
        internal readonly Idas3ArcadeMeterCatalog.Layer[] selected;
        internal LayerScope(int source, Predicate<Idas3ArcadeMeterCatalog.Layer> predicate)
        {
            int style = source + 2;
            meter = Idas3ArcadeMeterCatalog.Get(style);
            if (meter == null) throw new InvalidOperationException("Missing meter " + source);
            // Bounds must come from the complete meter, not this test fixture.
            Idas3MeterLayoutBounds.Get(style);
            original = meter.layers;
            var layers = new List<Idas3ArcadeMeterCatalog.Layer>();
            foreach (var layer in original) if (layer != null && predicate(layer)) layers.Add(layer);
            if (layers.Count == 0) throw new InvalidOperationException("No matching source layers for meter " + source);
            selected = layers.ToArray(); meter.layers = selected;
        }
        public void Dispose() { meter.layers = original; }
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
            report.initialTextures = Idas3ImportedMeter.ResidentTextureCount;
            host = new GameObject("Remaining meter effects QA") { hideFlags = HideFlags.HideAndDontSave };
            camera = host.AddComponent<Camera>(); camera.enabled = false; camera.cullingMask = 0;
            camera.allowHDR = camera.allowMSAA = false;
            camera.clearFlags = CameraClearFlags.SolidColor; camera.backgroundColor = Color.black;
            target = new RenderTexture(Width, Height, 24, RenderTextureFormat.ARGB32) { hideFlags = HideFlags.HideAndDontSave, antiAliasing = 1 };
            if (!target.Create()) throw new InvalidOperationException("Could not create remaining-effects QA target.");
            camera.targetTexture = target;
            readback = new Texture2D(Width, Height, TextureFormat.RGB24, false) { hideFlags = HideFlags.HideAndDontSave };
            commands = new CommandBuffer { name = "Remaining meter effects QA" };
            camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, commands);
        }
        static Idas3GameOptions.Values Options(int source) => new Idas3GameOptions.Values
            { hudMeterStyle = source + 2, hudPedalIndicators = true, hudShiftLights = true, hudNameplateStyle = 0 };
        static Idas3ArcadeHud.Telemetry Data() => new Idas3ArcadeHud.Telemetry
            { size = 40, version = 2, flags = 1, gear = 3, speedKmh = 123, rpm = 4500, revLimit = 8500, throttle = 0, brake = 0, driftOpacity = 0 };
        static bool Contains(string value, string search) => value != null && value.IndexOf(search, StringComparison.OrdinalIgnoreCase) >= 0;
        void Verify(bool condition, Result result, string message)
        {
            ++report.checks; if (condition) return;
            result.passed = false; result.errors.Add(message); report.errors.Add(result.sourceId + " / " + result.kind + ": " + message);
        }
        void Case(int source, string name, Action<Result> action)
        {
            var result = new Result { sourceId = source, kind = name }; report.results.Add(result);
            try { action(result); }
            catch (Exception error) { Verify(false, result, error.ToString()); }
            finally { commands.Clear(); camera.backgroundColor = Color.black; }
        }
        void Build(Idas3ArcadeHud renderer, Idas3GameOptions.Values options, Idas3ArcadeHud.Telemetry data, float seconds, float[] bands = null)
        {
            renderer.AudioBandsOverride = bands ?? Silence;
            renderer.Build(options, data, Width, Height, seconds, true, out _);
        }
        Color32[] Read()
        {
            var prior = RenderTexture.active;
            try { RenderTexture.active = target; readback.ReadPixels(new Rect(0, 0, Width, Height), 0, 0); readback.Apply(false); }
            finally { RenderTexture.active = prior; }
            ++report.gpuFrames; return readback.GetPixels32();
        }
        Color32[] Capture(Idas3ArcadeHud renderer, Idas3GameOptions.Values options, Idas3ArcadeHud.Telemetry data, float seconds, float[] bands = null)
        {
            commands.Clear(); Build(renderer, options, data, seconds, bands); renderer.Render(commands, Width, Height); camera.Render(); return Read();
        }
        static Sample Stats(Color32[] pixels)
        {
            var result = new Sample();
            foreach (var p in pixels)
            {
                int max = Math.Max(p.r, Math.Max(p.g, p.b));
                if (max > 8) ++result.visiblePixels;
                if (max > 8 && max < 220) ++result.softPixels;
                result.red += p.r; result.green += p.g; result.blue += p.b;
            }
            return result;
        }
        static int Difference(Color32[] a, Color32[] b, int threshold = 8)
        {
            int changed = 0;
            for (int i = 0; i < a.Length; ++i)
                if (Math.Max(Math.Abs(a[i].r - b[i].r), Math.Max(Math.Abs(a[i].g - b[i].g), Math.Abs(a[i].b - b[i].b))) > threshold) ++changed;
            return changed;
        }
        void Save(Result owner, string name, float seconds, Color32[] pixels)
        {
            var sample = Stats(pixels); sample.name = name; sample.seconds = seconds;
            sample.image = owner.sourceId.ToString("00") + "-" + owner.kind + "-" + name + ".png";
            readback.SetPixels32(pixels); readback.Apply(false); File.WriteAllBytes(Path.Combine(output, sample.image), readback.EncodeToPNG());
            owner.samples.Add(sample);
        }
        Rect LayerRect(Idas3GameOptions.Values options, Idas3ArcadeMeterCatalog.Layer layer)
        {
            var content = Idas3MeterLayoutBounds.Get(options.hudMeterStyle);
            var bounds = Idas3ArcadeHud.MeterBounds(Width, Height, options); float scale = bounds.width / content.width;
            var matrix = Idas3ImportedMeter.Matrix(layer.transform, layer.x, layer.y);
            var a = matrix.MultiplyPoint3x4(Vector3.zero); var b = matrix.MultiplyPoint3x4(new Vector3(layer.width, layer.height, 0));
            return Rect.MinMaxRect(bounds.x + (a.x - content.x) * scale, bounds.y + (a.y - content.y) * scale,
                bounds.x + (b.x - content.x) * scale, bounds.y + (b.y - content.y) * scale);
        }
        void TransparentCorners(Result result, Color32[] painted, Color32[] empty, Rect bounds)
        {
            foreach (var fraction in new[] { new Vector2(.04f, .04f), new Vector2(.96f, .04f), new Vector2(.04f, .96f), new Vector2(.96f, .96f) })
            {
                int x = Mathf.Clamp(Mathf.RoundToInt(bounds.x + bounds.width * fraction.x), 1, Width - 2);
                int y = Mathf.Clamp(Mathf.RoundToInt(bounds.y + bounds.height * fraction.y), 1, Height - 2);
                int differences = 0;
                for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
                {
                    int index = (Height - 1 - y - dy) * Width + x + dx;
                    var a = painted[index]; var b = empty[index];
                    if (Math.Max(Math.Abs(a.r - b.r), Math.Max(Math.Abs(a.g - b.g), Math.Abs(a.b - b.b))) > 2) ++differences;
                }
                Verify(differences == 0, result, "An empty source-mask corner overwrites the colored background.");
            }
        }
        void Aura(Result result)
        {
            var options = Options(77); var data = Data();
            using (var scope = new LayerScope(77, l => Contains(l.materialParent, "M_Aura2.")))
            using (var renderer = new Idas3ArcadeHud())
            {
                var first = Capture(renderer, options, data, .125f); var stats = Stats(first);
                Verify(stats.visiblePixels > 128, result, "Recovered aura is not visible.");
                Verify(stats.red > stats.green * 1.5 && stats.red > stats.blue * 3, result, "Aura lost its authored orange/red palette.");
                var animated = Capture(renderer, options, data, .725f);
                result.largestChangedPixels = Difference(first, animated);
                Verify(result.largestChangedPixels > 64, result, "Recovered noise textures do not animate inside the aura.");
                for (int i = 0; i < 60; ++i) Build(renderer, options, data, .725f);
                Verify(Difference(animated, Capture(renderer, options, data, .725f), 0) == 0, result, "Aura changes while its input clock is frozen.");
                Save(result, "before", .125f, first); Save(result, "animated", .725f, animated);
                camera.backgroundColor = new Color(.1f, .2f, .3f, 1);
                commands.Clear(); camera.Render(); var empty = Read();
                var background = Capture(renderer, options, data, .725f);
                TransparentCorners(result, background, empty, LayerRect(options, scope.selected[0]));
                Save(result, "transparent-mask-on-blue", .725f, background);
            }
        }
        void Ball(Result result, string name, string gate)
        {
            var options = Options(66); var data = Data();
            using (var scope = new LayerScope(66, l => l.name == name))
            using (var renderer = new Idas3ArcadeHud())
            {
                var inactive = Capture(renderer, options, data, .125f);
                if (gate != "static") Verify(Stats(inactive).visiblePixels == 0, result, "Procedural glow appears without its telemetry activation.");
                if (gate == "rev") data.rpm = data.revLimit;
                if (gate == "drift") { data.driftOpacity = 1; data.flags |= 8; }
                if (gate == "pedal") data.throttle = data.brake = 1;
                var active = Capture(renderer, options, data, .125f); var stats = Stats(active);
                Verify(stats.visiblePixels > 16 && stats.softPixels > 12, result, "Procedural ball is absent or has no soft radial falloff.");
                if (gate == "rev")
                {
                    Verify(stats.red > stats.green * 2 && stats.red > stats.blue * 2, result, "Rev glow lost its authored red tint.");
                    options.hudShiftLights = false;
                    Verify(Stats(Capture(renderer, options, data, .125f)).visiblePixels == 0, result, "Disabled shift warnings leave procedural rev glow visible.");
                    options.hudShiftLights = true;
                }
                if (gate == "drift")
                {
                    data.driftOpacity = .35f; var fading = Capture(renderer, options, data, .125f); var faded = Stats(fading);
                    Verify(faded.red + faded.green + faded.blue < (stats.red + stats.green + stats.blue) * .8, result, "Drift glow does not follow actual fade opacity.");
                    Save(result, "fading", .125f, fading); data.driftOpacity = 1;
                }
                if (gate == "pedal")
                {
                    var changed = Capture(renderer, options, data, .325f);
                    result.largestChangedPixels = Difference(active, changed);
                    Verify(result.largestChangedPixels > 12, result, "Blink ball ignores its authored blink rate.");
                    options.hudPedalIndicators = false;
                    Verify(Difference(changed, Capture(renderer, options, data, .325f), 0) == 0, result, "Hiding pedal indicators removes procedural coil decoration.");
                    Save(result, "blinking", .325f, changed);
                }
                Save(result, "inactive", .125f, inactive); Save(result, "active", .125f, active);
            }
        }
        void Audio(Result result)
        {
            int source = result.sourceId; var options = Options(source); var data = Data();
            var low = new float[32]; low[3] = .9f; low[4] = .5f;
            var high = new float[32]; high[24] = .9f; high[25] = .5f;
            using (var scope = new LayerScope(source, l => Contains(l.materialParent, "M_AudioCapture.") || Contains(l.name, "VisualizerBase")))
            using (var renderer = new Idas3ArcadeHud())
            {
                var silent = Capture(renderer, options, data, .125f, Silence);
                Verify(Stats(silent).visiblePixels == 0, result, "Silence creates fabricated audio bars or glow.");
                var bass = Capture(renderer, options, data, .125f, low);
                Verify(Stats(bass).visiblePixels > 24, result, "Real nonzero spectrum input produces no visualizer.");
                var treble = Capture(renderer, options, data, .125f, high);
                result.largestChangedPixels = Difference(bass, treble);
                Verify(result.largestChangedPixels > 12, result, "Different frequency bands with the same peak produce identical bars.");
                for (int i = 0; i < 60; ++i) Build(renderer, options, data, .125f, low);
                Verify(Difference(bass, Capture(renderer, options, data, .125f, low), 0) == 0, result, "Frozen visualizer inputs advance their output.");
                using (var second = new Idas3ArcadeHud())
                {
                    Verify(Stats(Capture(second, options, data, .125f, Silence)).visiblePixels == 0, result, "Audio input leaks into another renderer.");
                    Verify(Difference(bass, Capture(renderer, options, data, .125f, low), 0) == 0, result, "Another renderer changes the first renderer's spectrum.");
                }
                Verify(Stats(Capture(renderer, options, data, 2f, Silence)).visiblePixels == 0, result, "Visualizer keeps animating after injected audio becomes silent.");
                Save(result, "silence", .125f, silent); Save(result, "low-bands", .125f, bass); Save(result, "high-bands", .125f, treble);
                Color32[] reference = null;
                foreach (int rate in new[] { 30, 60, 144 })
                {
                    using (var rateRenderer = new Idas3ArcadeHud())
                    {
                        for (int tick = 0; tick <= rate; ++tick) Build(rateRenderer, options, data, (float)tick / rate, low);
                        var frame = Capture(rateRenderer, options, data, 1.125f, low);
                        if (reference == null) reference = frame;
                        else Verify(Difference(reference, frame, 0) == 0, result, rate + " Hz changes audio material output at the same time and spectrum.");
                    }
                }
            }
        }
        void Led(Result result)
        {
            var options = Options(75); var data = Data();
            using (var scope = new LayerScope(75, l => Contains(l.materialParent, "LedMotion")))
            using (var renderer = new Idas3ArcadeHud())
            {
                Color32[] previous = null, strongest = null; float strongestTime = 0; int visible = 0;
                foreach (float seconds in new[] { .125f, 1f, 3f, 6f, 9f, 13f, 17f, 23f, 31f, 41f, 51f, 61f })
                {
                    var frame = Capture(renderer, options, data, seconds); visible = Math.Max(visible, Stats(frame).visiblePixels);
                    int difference = previous == null ? 0 : Difference(previous, frame);
                    if (difference > result.largestChangedPixels) { result.largestChangedPixels = difference; strongest = frame; strongestTime = seconds; }
                    if (previous == null) Save(result, "initial", seconds, frame);
                    previous = frame;
                }
                Verify(visible > 64 && result.largestChangedPixels > 24, result, "Authored LED phase/atlas sequence is absent or static.");
                if (strongest != null) Save(result, "moving-letters", strongestTime, strongest);
                for (int i = 0; i < 100; ++i) Build(renderer, options, data, 61f);
                Verify(Difference(previous, Capture(renderer, options, data, 61f), 0) == 0, result, "LED cells advance while their clock is frozen.");
            }
        }
        internal void Run()
        {
            if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null) throw new InvalidOperationException("A GPU is required; omit -nographics.");
            Case(77, "aura", Aura);
            Case(66, "nixie-ball", r => Ball(r, "NixeAdd", "static"));
            Case(66, "rev-ball", r => Ball(r, "RevLampAddColor", "rev"));
            Case(66, "drift-ball", r => Ball(r, "DriftLampGreen1", "drift"));
            Case(66, "accelerator-coil-ball", r => Ball(r, "coil_add_1", "pedal"));
            Case(66, "brake-coil-ball", r => Ball(r, "coil_add", "pedal"));
            foreach (int source in new[] { 68, 69, 70, 74 }) Case(source, "audio-spectrum", Audio);
            Case(75, "led-sequence", Led);
            report.finalTextures = Idas3ImportedMeter.ResidentTextureCount; ++report.checks;
            if (report.finalTextures != report.initialTextures) report.errors.Add("Remaining-effects renderer retained imported texture leases.");
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
        string output = Path.GetFullPath(outputDirectory ?? Path.Combine("Verification/meter-remaining-effects",
            "gpu-" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N").Substring(0, 8)));
        if (Directory.Exists(output) && Directory.GetFileSystemEntries(output).Length > 0) throw new IOException("Remaining-effects output must be fresh: " + output);
        Directory.CreateDirectory(output);
        using (var runner = new Runner(output))
        {
            try { runner.Run(); }
            catch (Exception error) { runner.report.passed = false; runner.report.errors.Add(error.ToString()); }
            finally { runner.Write(); }
            if (!runner.report.passed) throw new InvalidOperationException("Remaining meter-effects checks failed; see " + Path.Combine(output, "report.json"));
            Debug.Log("Remaining meter-effects checks passed: " + runner.report.checks + " checks, " + runner.report.gpuFrames + " GPU frames. " + output);
        }
        return output;
    }
}
