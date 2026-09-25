using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.Rendering;
using Layer = Idas3ArcadeMeterCatalog.Layer;
using Meter = Idas3ArcadeMeterCatalog.Meter;
using Telemetry = Idas3ArcadeHud.Telemetry;
using Sprite = Idas3ArcadeHud.Sprite;

// Meter66 source tint/opacity regression. White clipping is measured only in
// the gear and coil widgets, against identical telemetry without their glows.
public static class Idas3SteampunkLightingChecks
{
    const int Width = 1280, Height = 720, Style = 68;
    static readonly string[] Glows = { "NixeAdd", "coil_add", "coil_add_1" };
    [Serializable] sealed class Report
    {
        public bool passed;
        public int checks, gpuFrames, initialResidentTextures, finalResidentTextures;
        public string graphicsDevice;
        public string coverage = "Production66 Compose source brush colors/alpha; GPU day/night, gears3/5, pedals0/1, drift/rev0/1; isolated colored glows; region-specific added-white clipping against no-glow and legacy-white-brush controls. Fixture changes are scoped catalog data only. No original material graph parity is asserted.";
        public List<Result> results = new List<Result>();
        public List<string> errors = new List<string>();
    }
    [Serializable] sealed class Result
    {
        public string kind, image, controlImage, legacyImage;
        public bool passed = true;
        public int regionPixels, visibleEffectPixels, correctedAddedWhite, legacyAddedWhite, maxAddedBlue;
        public long addedRed, addedGreen, addedBlue;
        public List<string> errors = new List<string>();
    }
    sealed class Scope : IDisposable
    {
        readonly Meter meter;
        readonly Layer[] original;
        readonly Dictionary<Layer, float[]> colors = new Dictionary<Layer, float[]>();
        internal Scope(Meter meter, Func<Layer, bool> keep = null, bool legacyWhite = false)
        {
            this.meter = meter; original = meter.layers;
            if (keep != null) { var layers = new List<Layer>(); foreach (var layer in original) if (keep(layer)) layers.Add(layer); meter.layers = layers.ToArray(); }
            if (legacyWhite) foreach (var layer in original) if (IsGlow(layer.name)) { colors[layer] = layer.brushColor; layer.brushColor = new[] { 1f, 1f, 1f, 1f }; }
        }
        public void Dispose() { meter.layers = original; foreach (var pair in colors) pair.Key.brushColor = pair.Value; }
    }
    static bool IsGlow(string name) => name == "NixeAdd" || name == "coil_add" || name == "coil_add_1";
    static Layer Find(Meter meter, string name)
    {
        foreach (var layer in meter.layers) if (layer.name == name) return layer;
        throw new InvalidOperationException("Missing Steampunk widget: " + name);
    }
    sealed class Runner : IDisposable
    {
        internal readonly Report report = new Report();
        readonly string output;
        readonly Meter meter;
        readonly Idas3GameOptions.Values options;
        readonly GameObject host;
        readonly Camera camera;
        readonly RenderTexture target;
        readonly Texture2D readback;
        readonly CommandBuffer commands;
        internal Runner(string output)
        {
            this.output = output; report.graphicsDevice = SystemInfo.graphicsDeviceName;
            report.initialResidentTextures = Idas3ImportedMeter.ResidentTextureCount;
            meter = Idas3ArcadeMeterCatalog.Get(Style);
            options = new Idas3GameOptions.Values { hudMeterStyle = Style, hudNameplateStyle = 0, hudShiftLights = true, hudPedalIndicators = true };
            options.SetHudSizePercent(2, 150); Idas3MeterLayoutBounds.Get(Style);
            host = new GameObject("Steampunk lighting GPU checks") { hideFlags = HideFlags.HideAndDontSave };
            camera = host.AddComponent<Camera>(); camera.enabled = false; camera.cullingMask = 0;
            camera.allowHDR = false; camera.allowMSAA = false; camera.backgroundColor = Color.black; camera.clearFlags = CameraClearFlags.SolidColor;
            target = new RenderTexture(Width, Height, 24, RenderTextureFormat.ARGB32)
                { name = "Steampunk lighting QA", antiAliasing = 1, hideFlags = HideFlags.HideAndDontSave };
            if (!target.Create()) throw new InvalidOperationException("Could not create Steampunk GPU target.");
            camera.targetTexture = target;
            readback = new Texture2D(Width, Height, TextureFormat.RGB24, false) { hideFlags = HideFlags.HideAndDontSave };
            commands = new CommandBuffer { name = "Steampunk lighting GPU checks" };
            camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, commands);
        }
        Result Case(string kind) { var result = new Result { kind = kind }; report.results.Add(result); return result; }
        void Verify(bool condition, Result result, string message)
        {
            ++report.checks; if (condition) return;
            result.passed = false; result.errors.Add(message); report.errors.Add(result.kind + ": " + message);
        }
        static Telemetry Data(bool night = false, int gear = 5, float pedals = 1, bool warnings = false) => new Telemetry
        {
            size = 40, version = 2, flags = 1u | (night ? 4u : 0u) | (warnings ? 8u : 0u), gear = gear,
            revLimit = 8500, rpm = warnings ? 8500 : 7000, speedKmh = 160, throttle = pedals, brake = pedals,
            driftOpacity = warnings ? 1 : 0
        };
        List<Sprite> Single(string name, Telemetry data)
        {
            using (var scope = new Scope(meter, layer => layer.name == name))
            using (var adapter = new Idas3ImportedMeter())
            {
                var sprites = new List<Sprite>(); adapter.Compose(sprites, meter, options, data, .125f); return sprites;
            }
        }
        static bool Near(float a, float b) => Mathf.Abs(a - b) < .0001f;
        void SourceChecks()
        {
            var result = Case("source-brush-and-opacity");
            foreach (string name in Glows)
            {
                var brush = Find(meter, name).brushColor;
                float green = name == "coil_add" ? .179943f : .26252f, alpha = name == "coil_add" ? .8f : 1;
                Verify(brush != null && brush.Length == 4 && Near(brush[0], 1) && Near(brush[1], green) && Near(brush[2], 0) && Near(brush[3], alpha), result, name + " lost recovered orange brush tint.");
                var sprites = Single(name, Data());
                Verify(sprites.Count == 1, result, name + " does not produce one procedural glow.");
                if (sprites.Count == 1) Verify(Near(sprites[0].color.r, 1) && Near(sprites[0].color.g, green) && Near(sprites[0].color.b, 0) && Near(sprites[0].color.a, alpha), result,
                    name + " brush tint is overwritten by widget animation or activation.");
                if (name != "NixeAdd") Verify(Single(name, Data(pedals: 0)).Count == 0, result, name + " remains lit with its pedal released.");
            }
            var drift = Single("DriftLampGreen1", Data(warnings: true));
            Verify(drift.Count == 1, result, "Native drift no longer lights the lamp.");
            if (drift.Count == 1) Verify(Near(drift[0].color.r, 0) && Near(drift[0].color.g, .666667f) && Near(drift[0].color.b, .0875f) && Near(drift[0].color.a, .8f), result,
                "Drift activation discards the green brush or authored .8 alpha.");
            var fading = Data(warnings: true); fading.driftOpacity = .35f;
            drift = Single("DriftLampGreen1", fading);
            Verify(drift.Count == 1 && Near(drift[0].color.a, .28f), result, "Native drift fade is not multiplied by source lamp alpha.");
            Verify(Single("DriftLampGreen1", Data()).Count == 0, result, "Drift glow remains active without drift.");
            var rev = Single("RevLamp1", Data(warnings: true));
            Verify(rev.Count == 1 && Near(rev[0].color.a, .52f * .438095f), result, "Rev activation discards recovered brush/source opacity.");
            Verify(Single("RevLampAdd", Data(warnings: true)).Count == 0, result, "Clamped-zero authored rev layer reappears at full opacity.");
            Verify(Single("RevLamp1", Data()).Count == 0, result, "Rev glow remains active below its threshold.");
        }
        Color32[] Capture(Telemetry data, float seconds, out Rect bounds)
        {
            using (var renderer = new Idas3ArcadeHud())
            {
                try
                {
                    commands.Clear(); renderer.Build(options, data, Width, Height, seconds, true, out bounds);
                    renderer.Render(commands, Width, Height); camera.Render();
                    var previous = RenderTexture.active;
                    try { RenderTexture.active = target; readback.ReadPixels(new Rect(0, 0, Width, Height), 0, 0); readback.Apply(false); }
                    finally { RenderTexture.active = previous; }
                    ++report.gpuFrames; return readback.GetPixels32();
                }
                finally { commands.Clear(); }
            }
        }
        string Save(string label, Color32[] pixels, Rect bounds)
        {
            string file = "steampunk-" + label + ".png";
            readback.SetPixels32(pixels); readback.Apply(false); File.WriteAllBytes(Path.Combine(output, file), readback.EncodeToPNG());
            int x = Mathf.Clamp(Mathf.FloorToInt(bounds.x) - 3, 0, Width - 1), y = Mathf.Clamp(Mathf.FloorToInt(Height - bounds.yMax) - 3, 0, Height - 1);
            int w = Mathf.Min(Width - x, Mathf.CeilToInt(bounds.width) + 6), h = Mathf.Min(Height - y, Mathf.CeilToInt(bounds.height) + 6);
            var crop = new Texture2D(w, h, TextureFormat.RGB24, false) { hideFlags = HideFlags.HideAndDontSave };
            try { crop.SetPixels(readback.GetPixels(x, y, w, h)); crop.Apply(false); File.WriteAllBytes(Path.Combine(output, "steampunk-" + label + "-crop.png"), crop.EncodeToPNG()); }
            finally { Destroy(crop); }
            return file;
        }
        Rect ScreenRegion(Layer layer, Rect bounds)
        {
            var matrix = Idas3ImportedMeter.Matrix(layer.transform); var content = Idas3MeterLayoutBounds.Get(Style);
            float scale = bounds.width / content.width, xmin = float.PositiveInfinity, ymin = xmin, xmax = float.NegativeInfinity, ymax = xmax;
            foreach (var point in new[] { Vector3.zero, new Vector3(layer.width, 0), new Vector3(0, layer.height), new Vector3(layer.width, layer.height) })
            {
                Vector2 source = matrix.MultiplyPoint3x4(point);
                Vector2 screen = bounds.position + (source - content.position) * scale;
                xmin = Mathf.Min(xmin, screen.x); xmax = Mathf.Max(xmax, screen.x); ymin = Mathf.Min(ymin, screen.y); ymax = Mathf.Max(ymax, screen.y);
            }
            return Rect.MinMaxRect(xmin, Height - ymax, xmax, Height - ymin);
        }
        void Region(Result result, Rect region, Color32[] corrected, Color32[] control, Color32[] legacy)
        {
            for (int y = Mathf.Max(0, Mathf.CeilToInt(region.yMin)); y < Mathf.Min(Height, Mathf.FloorToInt(region.yMax)); ++y)
                for (int x = Mathf.Max(0, Mathf.CeilToInt(region.xMin)); x < Mathf.Min(Width, Mathf.FloorToInt(region.xMax)); ++x)
                {
                    ++result.regionPixels; int i = y * Width + x; var a = corrected[i]; var b = control[i]; var old = legacy[i];
                    int dr = Math.Max(0, a.r - b.r), dg = Math.Max(0, a.g - b.g), db = Math.Max(0, a.b - b.b);
                    result.addedRed += dr; result.addedGreen += dg; result.addedBlue += db; result.maxAddedBlue = Math.Max(result.maxAddedBlue, db);
                    if (Math.Max(dr, dg) > 8) ++result.visibleEffectPixels;
                    bool baseWhite = Math.Min(b.r, Math.Min(b.g, b.b)) >= 250;
                    if (!baseWhite && Math.Min(a.r, Math.Min(a.g, a.b)) >= 250) ++result.correctedAddedWhite;
                    if (!baseWhite && Math.Min(old.r, Math.Min(old.g, old.b)) >= 250) ++result.legacyAddedWhite;
                }
            Verify(result.regionPixels > 100 && result.visibleEffectPixels > 12, result, "Target widget region is empty or its lighting was removed.");
            Verify(result.legacyAddedWhite > 12, result, "Legacy-white fixture does not reproduce local washout.");
            Verify(result.correctedAddedWhite < result.legacyAddedWhite * .35f + 3, result, "Colored lighting still erases local contrast with white clipping.");
            Verify(result.maxAddedBlue <= 2 && result.addedRed > 0 && result.addedGreen > 0, result, "Orange additive glow raises the unlit blue channel and washes out underlying detail.");
        }
        void Contrast()
        {
            var data = Data(); var corrected = Capture(data, .125f, out Rect bounds);
            Color32[] control, legacy;
            using (var scope = new Scope(meter, layer => !IsGlow(layer.name))) control = Capture(data, .125f, out _);
            using (var scope = new Scope(meter, legacyWhite: true)) legacy = Capture(data, .125f, out _);
            string image = Save("corrected", corrected, bounds), baseline = Save("no-white-glows-control", control, bounds), before = Save("legacy-white-brush-fixture", legacy, bounds);
            foreach (string name in Glows)
            {
                var result = Case("local-contrast-" + name); result.image = image; result.controlImage = baseline; result.legacyImage = before;
                // Measure the numeral itself for the Nixie glow, rather than
                // its larger mostly-black halo rectangle.
                Region(result, ScreenRegion(Find(meter, name == "NixeAdd" ? "GearRate01" : name), bounds), corrected, control, legacy);
            }
        }
        void IsolatedColors()
        {
            foreach (string name in Glows)
            {
                var result = Case("isolated-" + name);
                using (var scope = new Scope(meter, layer => layer.name == name))
                {
                    var image = Capture(Data(), .125f, out Rect bounds); long r = 0, g = 0, b = 0; int visible = 0;
                    foreach (var pixel in image) { r += pixel.r; g += pixel.g; b += pixel.b; if (pixel.r > 8) ++visible; }
                    Verify(visible > 12 && r > g * 1.3 && g > b * 2 && b < r / 100, result, "Recovered glow is not visibly orange on the GPU.");
                    result.image = Save(result.kind, image, bounds);
                }
            }
        }
        void States()
        {
            foreach (bool night in new[] { false, true })
                foreach (int pedals in new[] { 0, 1 })
                    foreach (bool warnings in new[] { false, true })
                    {
                        int gear = (pedals == 0) != warnings ? 3 : 5;
                        var result = Case((night ? "night" : "day") + "-gear" + gear + "-pedals" + pedals + "-warnings" + (warnings ? 1 : 0));
                        var image = Capture(Data(night, gear, pedals, warnings), warnings ? .325f : .125f, out Rect bounds);
                        result.image = Save(result.kind, image, bounds); int visible = 0;
                        foreach (var pixel in image) if (Mathf.Max(pixel.r, pixel.g, pixel.b) > 8) ++visible;
                        Verify(visible > 128, result, "Complete HUD state is blank.");
                    }
        }
        internal void Run()
        {
            if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null) throw new InvalidOperationException("Steampunk lighting checks require a GPU.");
            if (!Idas3ArcadeHud.Available) throw new InvalidOperationException("Shared HUD assets are unavailable.");
            SourceChecks(); Contrast(); IsolatedColors(); States();
            report.finalResidentTextures = Idas3ImportedMeter.ResidentTextureCount; ++report.checks;
            if (report.finalResidentTextures != report.initialResidentTextures) report.errors.Add("Lighting QA retained imported texture leases.");
            report.passed = report.errors.Count == 0;
        }
        internal void Write() => File.WriteAllText(Path.Combine(output, "report.json"), JsonUtility.ToJson(report, true));
        public void Dispose() { camera.RemoveAllCommandBuffers(); camera.targetTexture = null; commands.Dispose(); target.Release(); Destroy(readback); Destroy(target); Destroy(host); }
    }
    static void Destroy(UnityEngine.Object value) { if (!value) return; if (Application.isPlaying) UnityEngine.Object.Destroy(value); else UnityEngine.Object.DestroyImmediate(value); }
    public static string Run(string outputDirectory = null)
    {
        string output = Path.GetFullPath(outputDirectory ?? Path.Combine("Verification/steampunk-lighting", "gpu-" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N").Substring(0, 8)));
        if (Directory.Exists(output) && Directory.GetFileSystemEntries(output).Length > 0) throw new IOException("Lighting QA output must be fresh: " + output);
        Directory.CreateDirectory(output);
        using (var runner = new Runner(output))
        {
            try { runner.Run(); } catch (Exception error) { runner.report.passed = false; runner.report.errors.Add(error.ToString()); } finally { runner.Write(); }
            if (!runner.report.passed) throw new InvalidOperationException("Steampunk lighting checks failed; see " + Path.Combine(output, "report.json"));
            Debug.Log("Steampunk lighting checks passed: " + runner.report.checks + " checks, " + runner.report.gpuFrames + " GPU frames. " + output);
        }
        return output;
    }
}
