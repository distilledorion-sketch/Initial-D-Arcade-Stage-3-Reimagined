using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.Rendering;

// GPU regressions for restored meter animation. Comparisons use production
// Build/Render, fixed telemetry and explicit animation times; a fresh renderer
// at the same time and gear separates shift effects from digits and ambience.
public static class Idas3MeterEffectsChecks
{
    const int Width = 1280, Height = 720;
    [Serializable] sealed class Report
    {
        public bool passed;
        public int checks, gpuFrames, animationUpdates, initialResidentTextures, finalResidentTextures;
        public string graphicsDevice, colorSpace;
        public string coverage = "Production HUD Build/Render on a GPU. Fixed-telemetry ambient motion; first-observation suppression; actual gear changes and expiry against same-time/same-gear controls; frozen clocks; matching 30/60/144 Hz final frames. Source 66 uses both pedals at 100% to activate its authored lightning, with separate below-threshold and hidden-pedal-indicator checks. These tests do not establish equivalence to the original arcade shaders.";
        public List<Result> results = new List<Result>();
        public List<string> errors = new List<string>();
    }
    [Serializable] sealed class Result
    {
        public int sourceId, changedPixels, maxChannelDifference, belowThresholdVisiblePixels, hiddenPedalVisiblePixels;
        public float seconds, eventAge;
        public string kind, name, beforeImage, duringImage, afterImage, controlImage, belowThresholdImage, hiddenPedalImage;
        public bool passed = true;
        public List<string> errors = new List<string>();
        public List<GearEventSample> gearEventSamples = new List<GearEventSample>();
    }
    [Serializable] sealed class GearEventSample
    {
        public float age;
        public int changedPixels, maxChannelDifference;
        public bool meetsVisibilityThresholds;
    }
    struct Difference
    {
        public int changed, max;
        public bool Equal => max == 0;
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
            report.colorSpace = QualitySettings.activeColorSpace.ToString();
            report.initialResidentTextures = Idas3ImportedMeter.ResidentTextureCount;
            host = new GameObject("Meter effects GPU checks") { hideFlags = HideFlags.HideAndDontSave };
            camera = host.AddComponent<Camera>();
            camera.enabled = false; camera.cullingMask = 0; camera.allowHDR = false; camera.allowMSAA = false;
            camera.clearFlags = CameraClearFlags.SolidColor; camera.backgroundColor = Color.black;
            target = new RenderTexture(Width, Height, 24, RenderTextureFormat.ARGB32)
                { name = "Meter effects QA", hideFlags = HideFlags.HideAndDontSave, antiAliasing = 1 };
            if (!target.Create()) throw new InvalidOperationException("Could not create meter effects render target.");
            camera.targetTexture = target;
            readback = new Texture2D(Width, Height, TextureFormat.RGB24, false) { hideFlags = HideFlags.HideAndDontSave };
            commands = new CommandBuffer { name = "Meter effects GPU checks" };
            camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, commands);
        }

        static Idas3GameOptions.Values Options(int source) => new Idas3GameOptions.Values
        {
            hudMeterStyle = source == 31 ? 1 : source + 2,
            hudShiftLights = true, hudPedalIndicators = true, hudNameplateStyle = 0
        };
        static Idas3ArcadeHud.Telemetry Telemetry(Idas3GameOptions.Values options, int gear = 3) => new Idas3ArcadeHud.Telemetry
        {
            size = 40, version = 2, flags = 1, gear = gear, speedKmh = 123,
            // Both recovered coil-opacity curves are zero through 75% input.
            // Keep the activated telemetry identical across animation samples.
            rpm = 4500, revLimit = 8500,
            throttle = options.hudMeterStyle == 68 ? 1 : .7f,
            brake = options.hudMeterStyle == 68 ? 1 : .25f, driftOpacity = 0
        };
        Result Case(int source, string kind)
        {
            var result = new Result { sourceId = source, kind = kind, name = Idas3ArcadeMeterCatalog.Name(Options(source).hudMeterStyle) };
            report.results.Add(result); return result;
        }
        void Verify(bool condition, Result result, string message)
        {
            ++report.checks;
            if (condition) return;
            result.passed = false; result.errors.Add(message);
            report.errors.Add(result.sourceId + " / " + result.kind + ": " + message);
        }
        void Build(Idas3ArcadeHud renderer, Idas3GameOptions.Values options, int gear, float seconds)
        {
            Build(renderer, options, Telemetry(options, gear), seconds);
        }
        void Build(Idas3ArcadeHud renderer, Idas3GameOptions.Values options, Idas3ArcadeHud.Telemetry telemetry, float seconds)
        {
            renderer.Build(options, telemetry, Width, Height, seconds, true, out _);
            ++report.animationUpdates;
        }
        Color32[] Capture(Idas3ArcadeHud renderer, Idas3GameOptions.Values options, int gear, float seconds)
        {
            return Capture(renderer, options, Telemetry(options, gear), seconds);
        }
        Color32[] Capture(Idas3ArcadeHud renderer, Idas3GameOptions.Values options, Idas3ArcadeHud.Telemetry telemetry, float seconds)
        {
            commands.Clear(); Build(renderer, options, telemetry, seconds); renderer.Render(commands, Width, Height); camera.Render();
            var previous = RenderTexture.active;
            try
            {
                RenderTexture.active = target;
                readback.ReadPixels(new Rect(0, 0, Width, Height), 0, 0); readback.Apply(false);
            }
            finally { RenderTexture.active = previous; }
            ++report.gpuFrames;
            return readback.GetPixels32();
        }
        Color32[] Fresh(Idas3GameOptions.Values options, int gear, float seconds)
        {
            using (var renderer = new Idas3ArcadeHud())
            {
                try { return Capture(renderer, options, gear, seconds); }
                finally { commands.Clear(); }
            }
        }
        static Difference Compare(Color32[] a, Color32[] b)
        {
            var result = new Difference();
            for (int i = 0; i < a.Length; ++i)
            {
                int delta = Math.Max(Math.Abs(a[i].r - b[i].r), Math.Max(Math.Abs(a[i].g - b[i].g), Math.Abs(a[i].b - b[i].b)));
                result.max = Math.Max(result.max, delta);
                if (delta > 8) ++result.changed;
            }
            return result;
        }
        static int Visible(Color32[] pixels)
        {
            int count = 0;
            foreach (var pixel in pixels) if (Math.Max(pixel.r, Math.Max(pixel.g, pixel.b)) > 8) ++count;
            return count;
        }
        string Save(int source, string name, Color32[] pixels)
        {
            string file = "meter-" + source.ToString("00") + "-" + name + ".png";
            readback.SetPixels32(pixels); readback.Apply(false);
            File.WriteAllBytes(Path.Combine(output, file), readback.EncodeToPNG()); return file;
        }

        void Ambient(int source)
        {
            var result = Case(source, "ambient"); var options = Options(source);
            using (var renderer = new Idas3ArcadeHud())
            {
                try
                {
                    var baseline = Capture(renderer, options, 3, .125f);
                    Verify(Visible(baseline) > 128, result, "Ambient baseline is blank.");
                    var strongest = baseline; var best = new Difference();
                    // Sample Penguin farther apart to cover its slower LED
                    // scroll too. Isolated checks below verify frame rotation.
                    float[] times = source == 75 ? new[] { .925f, 1.725f } : new[] { .325f, .725f, 1.025f };
                    foreach (float seconds in times)
                    {
                        var frame = Capture(renderer, options, 3, seconds); var difference = Compare(baseline, frame);
                        if (difference.changed > best.changed) { best = difference; strongest = frame; result.seconds = seconds; }
                    }
                    result.changedPixels = best.changed; result.maxChannelDifference = best.max;
                    Verify(best.changed > 24 && best.max > 16, result, "Fixed telemetry does not produce visible ambient animation.");
                    result.beforeImage = Save(source, "ambient-before", baseline);
                    result.duringImage = Save(source, "ambient-during", strongest);
                    // Repeated UI repaints must not consume animation frames.
                    var held = Capture(renderer, options, 3, 1.125f);
                    for (int i = 0; i < 100; ++i) Build(renderer, options, 3, 1.125f);
                    var heldAgain = Capture(renderer, options, 3, 1.125f);
                    Verify(Compare(held, heldAgain).Equal, result, "Ambient animation advances while its clock is frozen.");
                    result.afterImage = Save(source, "ambient-frozen", heldAgain);
                }
                finally { commands.Clear(); }
            }
        }

        void GearEvent(int source)
        {
            var options=Options(source);var meter=Idas3ArcadeMeterCatalog.Get(options.hudMeterStyle);
            Idas3MeterLayoutBounds.Get(options.hudMeterStyle);
            var original=meter.layers;
            // A completed speed burst leaves a continuous rotation phase offset.
            // Isolate transient gear art for expiry; material integration has
            // separate analytic burst/phase checks and whole-meter FPS checks.
            var layers=new List<Idas3ArcadeMeterCatalog.Layer>();
            foreach(var layer in original)
                if(layer.materialParent==null||(!layer.materialParent.Contains("MeterRotation")&&!layer.materialParent.Contains("EffRotation")))layers.Add(layer);
            try{meter.layers=layers.ToArray();GearEventArt(source);}
            finally{meter.layers=original;}
        }
        void GearEventArt(int source)
        {
            var result = Case(source, "gear-event"); var options = Options(source);
            using (var renderer = new Idas3ArcadeHud())
            {
                try
                {
                    Build(renderer, options, 3, 0);
                    var steady = Capture(renderer, options, 3, 4.125f);
                    var first = Fresh(options, 3, 4.125f);
                    Verify(Compare(steady, first).Equal, result, "First observation creates an unwanted gear event.");
                }
                finally { commands.Clear(); }
            }
            using (var renderer = new Idas3ArcadeHud())
            {
                try
                {
                    var before = Capture(renderer, options, 3, .95f);
                    Build(renderer, options, 4, 1f);
                    var strongest = before; Color32[] control = null; var best = new Difference();
                    foreach (float age in new[] { .035f, .07f, .12f, .22f, .35f })
                    {
                        var frame = Capture(renderer, options, 4, 1f + age);
                        var baseline = Fresh(options, 4, 1f + age);
                        var difference = Compare(frame, baseline);
                        bool visible = difference.changed > 12 && difference.max > 16;
                        bool bestVisible = best.changed > 12 && best.max > 16;
                        result.gearEventSamples.Add(new GearEventSample { age = age, changedPixels = difference.changed,
                            maxChannelDifference = difference.max, meetsVisibilityThresholds = visible });
                        // A growing/fading effect can cover more pixels late
                        // while losing contrast. Prefer a frame that satisfies
                        // both existing requirements, then maximize its area.
                        if ((visible && !bestVisible) || (visible == bestVisible && difference.changed > best.changed))
                        {
                            best = difference; strongest = frame; control = baseline; result.eventAge = age;
                        }
                    }
                    result.changedPixels = best.changed; result.maxChannelDifference = best.max;
                    Verify(best.changed > 12 && best.max > 16, result, "A real gear change produces no event beyond the new gear digit.");
                    var expired = Capture(renderer, options, 4, 3f);
                    Verify(Compare(expired, Fresh(options, 4, 3f)).Equal, result, "Gear effect does not expire to the same-time idle state.");
                    result.beforeImage = Save(source, "shift-before", before);
                    result.duringImage = Save(source, "shift-during", strongest);
                    result.afterImage = Save(source, "shift-after", expired);
                    if (control != null) result.controlImage = Save(source, "shift-during-no-event-control", control);
                }
                finally { commands.Clear(); }
            }
        }

        void UpdateRates(int source)
        {
            var result = Case(source, "update-rates-and-frozen-event"); var options = Options(source);
            Color32[] reference = null;
            foreach (int rate in new[] { 30, 60, 144 })
            {
                using (var renderer = new Idas3ArcadeHud())
                {
                    try
                    {
                        // Every rate observes the event at exactly 1.0 second.
                        // Final age .125 is independent of the preceding count.
                        for (int frame = 0; frame <= Mathf.FloorToInt(1.125f * rate); ++frame)
                        {
                            float seconds = (float)frame / rate;
                            Build(renderer, options, frame >= rate ? 4 : 3, seconds);
                        }
                        var image = Capture(renderer, options, 4, 1.125f);
                        if (reference == null) reference = image;
                        else Verify(Compare(reference, image).Equal, result, rate + " Hz produces a different final GPU frame.");
                        for (int repeat = 0; repeat < rate; ++repeat) Build(renderer, options, 4, 1.125f);
                        Verify(Compare(image, Capture(renderer, options, 4, 1.125f)).Equal, result,
                            "The active shift event advances during a frozen clock at " + rate + " Hz.");
                    }
                    finally { commands.Clear(); }
                }
            }
            result.duringImage = Save(source, "shift-rate-independent", reference);
        }

        void IsolatedMaterial(int source, string materialFragment, string kind, bool gearEvent)
        {
            var result = Case(source, kind); var options = Options(source);
            var meter = Idas3ArcadeMeterCatalog.Get(options.hudMeterStyle);
            if (meter == null) { Verify(false, result, "Source meter is missing."); return; }
            // Cache bounds while the complete authored meter is present. The
            // isolated fixture cannot replace normal layout envelopes.
            Idas3MeterLayoutBounds.Get(options.hudMeterStyle);
            var complete = meter.layers;
            var selected = new List<Idas3ArcadeMeterCatalog.Layer>();
            foreach (var layer in complete)
                if (layer?.materialParent != null && layer.materialParent.IndexOf(materialFragment, StringComparison.OrdinalIgnoreCase) >= 0)
                    selected.Add(layer);
            Verify(selected.Count > 0, result, "No authored layers match the isolated material fixture.");
            if (selected.Count == 0) return;
            // Synchronous QA narrows only the in-memory fixture and restores
            // it in finally. Build/Render, source layers and shader are intact;
            // unrelated blinking or gear digits cannot satisfy this test.
            meter.layers = selected.ToArray();
            try
            {
                using (var renderer = new Idas3ArcadeHud())
                {
                    var before = Capture(renderer, options, 3, gearEvent ? .95f : .125f);
                    Verify(Visible(before) > 12, result, "The isolated authored effect has no visible baseline.");
                    if (gearEvent) Build(renderer, options, 4, 1f);
                    var strongest = before; Color32[] strongestControl = null; var best = new Difference();
                    foreach (float sample in gearEvent ? new[] { .07f, .12f, .22f, .35f } : new[] { .325f, .725f, 1.125f })
                    {
                        float seconds = gearEvent ? 1f + sample : sample;
                        var frame = Capture(renderer, options, gearEvent ? 4 : 3, seconds);
                        var control = gearEvent ? Fresh(options, 4, seconds) : before;
                        var difference = Compare(frame, control);
                        if (difference.changed > best.changed)
                        {
                            best = difference; strongest = frame; strongestControl = control;
                            result.seconds = seconds; result.eventAge = gearEvent ? sample : 0;
                        }
                    }
                    result.changedPixels = best.changed; result.maxChannelDifference = best.max;
                    Verify(best.changed > 8 && best.max > 16, result, "The isolated authored material does not animate visibly.");
                    var after = Capture(renderer, options, gearEvent ? 4 : 3, 3f);
                    if (gearEvent) Verify(Compare(after, Fresh(options, 4, 3f)).Equal, result, "Isolated UV scroll does not reset after its shift event.");
                    else Verify(Compare(after, Capture(renderer, options, 3, 3f)).Equal, result, "Isolated material changes with a frozen clock.");
                    result.beforeImage = Save(source, kind + "-before", before);
                    result.duringImage = Save(source, kind + "-during", strongest);
                    result.afterImage = Save(source, kind + "-after", after);
                    if (strongestControl != null) result.controlImage = Save(source, kind + "-control", strongestControl);
                    if (source == 66 && materialFragment == "FlipBook_Loop")
                    {
                        var lowPedals = Telemetry(options); lowPedals.throttle = lowPedals.brake = .7f;
                        var belowThreshold = Capture(renderer, options, lowPedals, 3.125f);
                        result.belowThresholdVisiblePixels = Visible(belowThreshold);
                        Verify(result.belowThresholdVisiblePixels == 0, result,
                            "Authored lightning activates below its 75% pedal threshold.");
                        Verify(Visible(Capture(renderer, options, lowPedals, 3.325f)) == 0, result,
                            "Inactive lightning becomes visible as its flipbook clock advances.");
                        result.belowThresholdImage = Save(source, kind + "-below-threshold", belowThreshold);

                        var active = Capture(renderer, options, 3, 3.5f);
                        var hiddenPedals = Options(source); hiddenPedals.hudPedalIndicators = false;
                        var retained = Capture(renderer, hiddenPedals, 3, 3.5f);
                        result.hiddenPedalVisiblePixels = Visible(retained);
                        Verify(result.hiddenPedalVisiblePixels > 12 && Compare(active, retained).Equal, result,
                            "Hiding pedal indicators removes or alters the authored decorative lightning.");
                        result.hiddenPedalImage = Save(source, kind + "-pedal-indicators-hidden", retained);
                    }
                }
            }
            finally { commands.Clear(); meter.layers = complete; }
        }

        void ShiftWarningOption(int source)
        {
            var result = Case(source, "shift-warning-option-preserves-decoration");
            var enabled = Options(source); var disabled = Options(source); disabled.hudShiftLights = false;
            using (var first = new Idas3ArcadeHud())
            using (var second = new Idas3ArcadeHud())
            {
                try
                {
                    // Low RPM keeps the actual rev warning inactive. Turning
                    // that setting off must not disable unrelated decorations.
                    var ambient = Capture(first, enabled, 3, .725f);
                    Verify(Compare(ambient, Capture(second, disabled, 3, .725f)).Equal, result,
                        "Disabling shift warnings suppresses low-RPM decorative animation.");
                    Build(first, enabled, 4, 1f); Build(second, disabled, 4, 1f);
                    var shifted = Capture(first, enabled, 4, 1.12f);
                    var withoutWarning = Capture(second, disabled, 4, 1.12f);
                    Verify(Compare(shifted, withoutWarning).Equal, result,
                        "Disabling shift warnings suppresses an actual gear-change decoration.");
                    result.duringImage = Save(source, "shift-warning-disabled-decoration", withoutWarning);
                }
                finally { commands.Clear(); }
            }
        }

        internal void Run()
        {
            if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null) throw new InvalidOperationException("Meter effects checks require a GPU; omit -nographics.");
            if (!Idas3ArcadeHud.Available) throw new InvalidOperationException("Shared meter artwork is unavailable.");
            foreach (int source in new[] { 66, 71, 72, 73, 75, 77, 79, 80, 81, 87, 88, 89 }) Ambient(source);
            foreach (int source in new[] { 0, 1, 3, 31, 66, 71, 75, 76, 79, 89 }) GearEvent(source);
            foreach (int source in new[] { 66, 71, 75, 76, 79, 87 }) UpdateRates(source);
            IsolatedMaterial(66, "M_UVScroll.", "isolated-authored-uv-scroll", true);
            IsolatedMaterial(66, "FlipBook_Loop", "isolated-flipbook-loop", false);
            IsolatedMaterial(75, "MeterRotation", "isolated-rotating-frame", false);
            foreach (int source in new[] { 66, 76, 79 }) ShiftWarningOption(source);
            report.finalResidentTextures = Idas3ImportedMeter.ResidentTextureCount;
            ++report.checks;
            if (report.initialResidentTextures != report.finalResidentTextures) report.errors.Add("Meter texture leases survived effects QA disposal.");
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
        string output = Path.GetFullPath(outputDirectory ?? Path.Combine("Verification/meter-effects",
            "gpu-" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N").Substring(0, 8)));
        if (Directory.Exists(output) && Directory.GetFileSystemEntries(output).Length > 0) throw new IOException("Meter effects QA output must be fresh: " + output);
        Directory.CreateDirectory(output);
        using (var runner = new Runner(output))
        {
            try { runner.Run(); }
            catch (Exception error) { runner.report.passed = false; runner.report.errors.Add(error.ToString()); }
            finally { runner.Write(); }
            if (!runner.report.passed) throw new InvalidOperationException("Meter effects GPU checks failed; see " + Path.Combine(output, "report.json"));
            Debug.Log("Meter effects GPU checks passed: " + runner.report.checks + " checks, " + runner.report.gpuFrames + " frames. " + output);
        }
        return output;
    }
}
