using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using UnityEngine;
using UnityEngine.Rendering;
using Layer = Idas3ArcadeMeterCatalog.Layer;
using Sprite = Idas3ArcadeHud.Sprite;

// Source-anchor checks for the two DIVA/Miku families. Their unconfigured
// OverlaySlots use Left/Top, so frame artwork must retain its brush dimensions.
public static class Idas3MikuMeterChecks
{
    const int Width = 1280, Height = 720;
    static readonly FieldInfo LiveSprites = typeof(Idas3ArcadeHud).GetField("sprites", BindingFlags.Instance | BindingFlags.NonPublic);
    static readonly FieldInfo PreviewSprites = typeof(Idas3ArcadeHud).GetField("previewSprites", BindingFlags.Static | BindingFlags.NonPublic);
    static readonly FieldInfo LiveMesh = typeof(Idas3ArcadeHud).GetField("mesh", BindingFlags.Instance | BindingFlags.NonPublic);
    [Serializable] sealed class Report
    {
        public bool passed;
        public int checks, gpuFrames, initialResidentTextures, finalResidentTextures;
        public string graphicsDevice;
        public string coverage = "Source-derived frame dimensions/centers for Miku68–73, unchanged dial pivots, runtime-selected day/night needles71–73, production live/preview sprite geometry and uniform HUD resizing, GPU live-path day/night/audio/rotating-ring crops. Preview IMGUI rasterization requires separate player visual review.";
        public List<Result> results = new List<Result>();
        public List<string> errors = new List<string>();
    }
    [Serializable] sealed class Result
    {
        public int sourceId;
        public bool passed = true;
        public Vector2 frameSize, frameCenter, needleCenter;
        public string dayImage, nightImage, effectsImage, resizedImage;
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
            host = new GameObject("Miku meter GPU checks") { hideFlags = HideFlags.HideAndDontSave };
            camera = host.AddComponent<Camera>(); camera.enabled = false; camera.cullingMask = 0;
            camera.allowHDR = false; camera.allowMSAA = false; camera.clearFlags = CameraClearFlags.SolidColor;
            camera.backgroundColor = Color.black;
            target = new RenderTexture(Width, Height, 24, RenderTextureFormat.ARGB32)
                { name = "Miku alignment QA", antiAliasing = 1, hideFlags = HideFlags.HideAndDontSave };
            if (!target.Create()) throw new InvalidOperationException("Could not create Miku QA render target.");
            camera.targetTexture = target;
            readback = new Texture2D(Width, Height, TextureFormat.RGB24, false) { hideFlags = HideFlags.HideAndDontSave };
            commands = new CommandBuffer { name = "Miku meter GPU checks" };
            camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, commands);
        }
        void Verify(bool condition, Result result, string message)
        {
            ++report.checks; if (condition) return;
            result.passed = false; result.errors.Add(message); report.errors.Add(result.sourceId + ": " + message);
        }
        static bool Near(float a, float b, float tolerance = .002f) => Mathf.Abs(a - b) < tolerance;
        static bool Near(Vector2 a, Vector2 b, float tolerance = .002f) => Vector2.Distance(a, b) < tolerance;
        static Vector2 Center(Layer layer) => Idas3ImportedMeter.Matrix(layer.transform).MultiplyPoint3x4(new Vector3(layer.width * .5f, layer.height * .5f, 0));
        static Layer Find(Idas3ArcadeMeterCatalog.Meter meter, string name)
        {
            foreach (var layer in meter.layers) if (layer.name == name) return layer;
            throw new InvalidOperationException("Missing source widget " + name + " for meter " + meter.id);
        }
        static Idas3GameOptions.Values Options(int source)
        {
            var options = new Idas3GameOptions.Values { hudMeterStyle = source + 2, hudNameplateStyle = 0, hudShiftLights = true, hudPedalIndicators = true };
            options.SetHudSizePercent(2, 150); return options;
        }
        static Idas3ArcadeHud.Telemetry Data(bool night = false) => new Idas3ArcadeHud.Telemetry
            { size = 40, version = 2, flags = night ? 5u : 1u, gear = 5, rpm = 7000, revLimit = 8500, speedKmh = 160, throttle = .7f, brake = .25f };
        void SourceGeometry(Result result, Idas3GameOptions.Values options)
        {
            bool diva = result.sourceId <= 70;
            var meter = Idas3ArcadeMeterCatalog.Get(options.hudMeterStyle);
            var frame = Find(meter, diva ? "DIVAMeterFrame" : "Baseframe");
            var needle = Find(meter, "CenterPin"); var dial = Find(meter, "CenterMeter");
            result.frameSize = new Vector2(frame.width, frame.height); result.frameCenter = Center(frame); result.needleCenter = Center(needle);
            Verify(Near(result.frameSize, diva ? new Vector2(512, 340) : new Vector2(400, 400)), result,
                "An unconfigured frame slot stretches its source brush dimensions.");
            Verify(Near(result.frameCenter, diva ? new Vector2(296, 196) : new Vector2(220, 202)), result,
                "Frame center is displaced from its authored Left/Top slot.");
            Verify(Near(result.needleCenter, diva ? new Vector2(382, 197) : new Vector2(221, result.sourceId == 73 ? 199.5f : 200)), result,
                "Correcting the frame also moved the already aligned needle.");
            Verify(Vector2.Distance(Center(needle), Center(dial)) <= 3.01f, result, "Needle pivot and dial center are separated.");
            if (diva)
            {
                var glow = Find(meter, result.sourceId == 68 ? "Visualizerbase" : "VisualizerBase");
                var audio = Find(meter, "AudioVisualizer");
                Verify(Near(new Vector2(glow.width, glow.height), new Vector2(310, 310)), result,
                    "Audio glow stretches away from its authored square brush.");
                Verify(Near(Center(glow), new Vector2(171, 207)) && Near(Center(audio), new Vector2(175, 208)), result,
                    "Audio glow or spectrum moved away from the authored left-dial location.");
                var bands = new float[32]; bands[4] = .8f;
                using (var adapter = new Idas3ImportedMeter { AudioBandsOverride = bands })
                {
                    var sprites = new List<Sprite>(); adapter.Compose(sprites, meter, options, Data(), .125f);
                    int glows = 0, spectra = 0;
                    foreach (var sprite in sprites)
                    {
                        if (sprite.materialEffect == 1)
                        {
                            ++glows;
                            Vector2 center = sprite.transform.MultiplyPoint3x4(new Vector3(sprite.rect.width * .5f, sprite.rect.height * .5f, 0));
                            Verify(Near(sprite.rect.size, new Vector2(310, 310)) && Near(center, new Vector2(171, 207)) && sprite.effectParams2.w > 0, result,
                                "Production audio glow is stretched, misplaced, or not driven by input energy.");
                        }
                        if (sprite.materialEffect == 3) ++spectra;
                    }
                    Verify(glows == 1 && spectra == 1, result, "Production meter does not retain its audio glow and spectrum layers.");
                }
            }
            else
            {
                Verify(Vector2.Distance(Center(frame), Center(dial)) < 1.5f, result, "Rotating ring is no longer centered on the dial.");
                Verify(Near(frame.transform[0], 1) && Near(frame.transform[4], 1) && Near(frame.transform[1], 0) && Near(frame.transform[3], 0), result,
                    "Rotating ring has an unintended stretch or shear.");
                using (var adapter = new Idas3ImportedMeter())
                    foreach (bool night in new[] { false, true })
                    {
                        var sprites = new List<Sprite>(); adapter.Compose(sprites, meter, options, Data(night), .125f);
                        string expected = night ? "T_Meter00_PointRmp_B" : "T_Meter00_PointRmp_A";
                        int correct = 0, wrong = 0;
                        foreach (var sprite in sprites)
                        {
                            if (sprite.texture && sprite.texture.name == expected) ++correct;
                            if (sprite.texture && sprite.texture.name.StartsWith("T_Meter49_PointRmp", StringComparison.Ordinal)) ++wrong;
                        }
                        Verify(correct == 1 && wrong == 0, result, "Production compositor did not select the source runtime " + (night ? "night" : "day") + " needle.");
                    }
            }
        }
        static float MatrixDifference(Matrix4x4 a, Matrix4x4 b)
        {
            float difference = 0; for (int i = 0; i < 16; ++i) difference = Mathf.Max(difference, Mathf.Abs(a[i] - b[i])); return difference;
        }
        void PreviewAndResize(Result result, Idas3GameOptions.Values options)
        {
            Idas3ArcadeHud.ReleasePreview();
            using (var live = new Idas3ArcadeHud())
            {
                try
                {
                    const float seconds = .125f;
                    live.Build(options, Idas3ArcadeHud.Demo(seconds), Width, Height, seconds, true, out Rect largeBounds);
                    Verify(Idas3ArcadeHud.PreparePreview(options, seconds), result, "Production preview did not prepare.");
                    var preview = (List<Sprite>)PreviewSprites.GetValue(null); var race = (List<Sprite>)LiveSprites.GetValue(live);
                    Verify(preview.Count == race.Count && race.Count > 0, result, "Live and preview select different widget counts.");
                    for (int i = 0; i < Mathf.Min(preview.Count, race.Count); ++i)
                    {
                        Verify(preview[i].rect == race[i].rect && MatrixDifference(preview[i].transform, race[i].transform) < .002f, result,
                            "Live and preview disagree on widget geometry at index " + i + ".");
                        Verify(preview[i].texture && race[i].texture && preview[i].texture.name == race[i].texture.name, result,
                            "Live and preview select different artwork at index " + i + ".");
                    }
                    var largeVertices = ((Mesh)LiveMesh.GetValue(live)).vertices;
                    options.SetHudSizePercent(2, 75);
                    live.Build(options, Idas3ArcadeHud.Demo(seconds), Width, Height, seconds, true, out Rect smallBounds);
                    var smallVertices = ((Mesh)LiveMesh.GetValue(live)).vertices;
                    Verify(largeVertices.Length == smallVertices.Length, result, "Resizing changes meter mesh topology.");
                    float largest = 0;
                    for (int i = 0; i < Mathf.Min(largeVertices.Length, smallVertices.Length); ++i)
                    {
                        Vector2 a = ((Vector2)largeVertices[i] - largeBounds.position) / 1.5f;
                        Vector2 b = ((Vector2)smallVertices[i] - smallBounds.position) / .75f;
                        largest = Mathf.Max(largest, Vector2.Distance(a, b));
                    }
                    Verify(largest < .002f, result, "HUD resizing changes relative frame/needle positions (delta " + largest + ").");
                }
                finally { options.SetHudSizePercent(2, 150); Idas3ArcadeHud.ReleasePreview(); }
            }
        }
        string Capture(Idas3ArcadeHud renderer, Idas3GameOptions.Values options, Idas3ArcadeHud.Telemetry data, float seconds, Result result, string label)
        {
            commands.Clear(); renderer.Build(options, data, Width, Height, seconds, true, out Rect bounds);
            renderer.Render(commands, Width, Height); camera.Render();
            var previous = RenderTexture.active;
            try { RenderTexture.active = target; readback.ReadPixels(new Rect(0, 0, Width, Height), 0, 0); readback.Apply(false); }
            finally { RenderTexture.active = previous; }
            ++report.gpuFrames;
            var pixels = readback.GetPixels32(); int visible = 0;
            foreach (var pixel in pixels) if (Mathf.Max(pixel.r, pixel.g, pixel.b) > 8) ++visible;
            Verify(visible > 128, result, label + " rendered blank.");
            string file = "meter-" + result.sourceId + "-" + label + ".png";
            File.WriteAllBytes(Path.Combine(output, file), readback.EncodeToPNG());
            // Save standalone GPU crops for visual review. ReadPixels origin
            // is bottom-left; the production meter bounds use top-left.
            int x = Mathf.Clamp(Mathf.FloorToInt(bounds.x) - 3, 0, Width - 1);
            int y = Mathf.Clamp(Mathf.FloorToInt(Height - bounds.yMax) - 3, 0, Height - 1);
            int w = Mathf.Min(Width - x, Mathf.CeilToInt(bounds.width) + 6);
            int h = Mathf.Min(Height - y, Mathf.CeilToInt(bounds.height) + 6);
            var crop = new Texture2D(w, h, TextureFormat.RGB24, false) { hideFlags = HideFlags.HideAndDontSave };
            try
            {
                crop.SetPixels(readback.GetPixels(x, y, w, h)); crop.Apply(false);
                File.WriteAllBytes(Path.Combine(output, "meter-" + result.sourceId + "-" + label + "-crop.png"), crop.EncodeToPNG());
            }
            finally { Destroy(crop); }
            return file;
        }
        void Gpu(Result result, Idas3GameOptions.Values options)
        {
            using (var renderer = new Idas3ArcadeHud { AudioBandsOverride = new float[32] })
            {
                try
                {
                    result.dayImage = Capture(renderer, options, Data(), .125f, result, "day");
                    result.nightImage = Capture(renderer, options, Data(true), .125f, result, "night");
                    var bands = new float[32];
                    for (int i = 0; i < bands.Length; ++i) bands[i] = .2f + .65f * (i % 5) / 4;
                    renderer.AudioBandsOverride = bands;
                    var active = Data(); active.driftOpacity = .75f; active.rpm = 8400;
                    result.effectsImage = Capture(renderer, options, active, .725f, result, "active-effects");
                    options.SetHudSizePercent(2, 75);
                    result.resizedImage = Capture(renderer, options, Data(), .125f, result, "resized");
                }
                finally { commands.Clear(); options.SetHudSizePercent(2, 150); }
            }
        }
        internal void Run()
        {
            if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null) throw new InvalidOperationException("Miku GPU checks require a graphics device.");
            if (!Idas3ArcadeHud.Available) throw new InvalidOperationException("Shared HUD assets are unavailable.");
            if (LiveSprites == null || PreviewSprites == null || LiveMesh == null) throw new InvalidOperationException("Production HUD inspection fields changed.");
            for (int source = 68; source <= 73; ++source)
            {
                var result = new Result { sourceId = source }; report.results.Add(result);
                try { var options = Options(source); SourceGeometry(result, options); PreviewAndResize(result, options); Gpu(result, options); }
                catch (Exception error) { result.passed = false; result.errors.Add(error.ToString()); report.errors.Add(source + ": " + error); }
            }
            report.finalResidentTextures = Idas3ImportedMeter.ResidentTextureCount;
            ++report.checks;
            if (report.finalResidentTextures != report.initialResidentTextures) report.errors.Add("Miku QA retained imported texture leases.");
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
        string output = Path.GetFullPath(outputDirectory ?? Path.Combine("Verification/miku-meter-alignment",
            "gpu-" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N").Substring(0, 8)));
        if (Directory.Exists(output) && Directory.GetFileSystemEntries(output).Length > 0) throw new IOException("Miku QA output must be fresh: " + output);
        Directory.CreateDirectory(output);
        using (var runner = new Runner(output))
        {
            try { runner.Run(); }
            catch (Exception error) { runner.report.passed = false; runner.report.errors.Add(error.ToString()); }
            finally { runner.Write(); }
            if (!runner.report.passed) throw new InvalidOperationException("Miku meter checks failed; see " + Path.Combine(output, "report.json"));
            Debug.Log("Miku meter checks passed: " + runner.report.checks + " checks, " + runner.report.gpuFrames + " GPU frames. " + output);
        }
        return output;
    }
}
