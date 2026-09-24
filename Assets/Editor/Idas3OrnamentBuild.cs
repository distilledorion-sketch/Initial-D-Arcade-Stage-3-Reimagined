using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

// Run with -batchmode -force-d3d11 -quit; do not use -nographics.
// Screenshots use the production geometry renderer, not catalog thumbnails.
public static class Idas3OrnamentBuild
{
    const BindingFlags Instance = BindingFlags.Instance | BindingFlags.NonPublic;
    const BindingFlags Static = BindingFlags.Static | BindingFlags.NonPublic;
    static readonly Type RendererType = typeof(Idas3OrnamentRenderer);
    static readonly MethodInfo RenderMethod = RendererType.GetMethod("RenderPose", Instance);
    static readonly PropertyInfo OutputProperty = RendererType.GetProperty("Output", Instance);
    static readonly PropertyInfo PartCountProperty = RendererType.GetProperty("PartCount", Instance);
    static readonly PropertyInfo SelectedProperty = RendererType.GetProperty("SelectedId", Instance);
    static readonly PropertyInfo ResidentProperty = RendererType.GetProperty("ResidentTextureCount", Static);
    static Texture Render(Idas3OrnamentRenderer renderer, int id, Quaternion pose) => (Texture)RenderMethod.Invoke(renderer, new object[] { id, pose });
    static int Resident => (int)ResidentProperty.GetValue(null);

    [Serializable] sealed class Report
    {
        public bool passed, sharedCacheRetained, sharedCacheReleased, offReleased, allDisposed;
        public int checks, models, straps, gpuCases, textures, vertices, triangles, initialResidentTextures, finalResidentTextures, peakResidentTextures;
        public string graphicsDevice, colorSpace, motionChecks;
        public string coverage = "280 actual source models with all four assigned chain families. Five production GPU poses per model: front, maximum left/right pendulum angles, 45 degree yaw and menu preview angle. Pixel checks distinguish the ornament body from its strap, require transparent margins, verify movement and detect viewport clipping. Source shader equivalence and original native assembly transforms are not established by these checks.";
        public List<ModelResult> results = new List<ModelResult>();
        public List<string> errors = new List<string>();
        public List<string> contactSheets = new List<string>();
    }
    [Serializable] sealed class ModelResult
    {
        public int id, strapId, vertices, triangles, meshParts, peakResidentTextures;
        public string name, sourceModel, frontImage, swingImage, error;
        public bool recoveredSocket;
        public List<PoseResult> poses = new List<PoseResult>();
    }
    [Serializable] sealed class PoseResult
    {
        public string name;
        public Vector3 degrees;
        public int visiblePixels, bodyPixels, strapPixels, frameEdgePixels, coloredPixels, changedPixels;
        public Rect visibleBounds;
    }

    sealed class Runner : IDisposable
    {
        internal readonly Report report = new Report();
        readonly string output;
        readonly HashSet<string> verifiedTextures = new HashSet<string>();
        readonly HashSet<Texture2D> priorTextures = new HashSet<Texture2D>();
        Texture2D readback;
        Color32[] sheet;
        int sheetNumber, tiles;
        const int Columns = 4, Rows = 10, TileWidth = 256, TileHeight = 168;
        const int SheetWidth = Columns * TileWidth, SheetHeight = Rows * TileHeight;

        internal Runner(string output)
        {
            this.output = output;
            report.graphicsDevice = SystemInfo.graphicsDeviceName;
            report.colorSpace = QualitySettings.activeColorSpace.ToString();
            report.initialResidentTextures = Resident;
            foreach (var texture in Resources.FindObjectsOfTypeAll<Texture2D>()) priorTextures.Add(texture);
        }

        void Require(bool condition, string message)
        {
            ++report.checks;
            if (!condition) throw new InvalidOperationException(message);
        }
        static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);
        static bool Finite(Vector3 value) => Finite(value.x) && Finite(value.y) && Finite(value.z);
        void Failure(ModelResult model, Exception error)
        {
            if (error is TargetInvocationException && error.InnerException != null) error = error.InnerException;
            model.error = error.ToString();
            report.errors.Add(model.id + " " + model.name + ": " + error.Message);
            Debug.LogError("Ornament GPU QA: " + report.errors[report.errors.Count - 1]);
        }

        void VerifyModel(Idas3OrnamentCatalog.Entry entry, Idas3OrnamentCatalog.MeshParts geometry)
        {
            Require(entry != null && geometry != null && geometry.Parts.Length > 0, "Missing recovered mesh resource.");
            Require(Finite(entry.boundsMin) && Finite(entry.boundsMax) && entry.boundsMax.y > entry.boundsMin.y, "Invalid recovered model bounds: " + entry.id);
            int vertices = 0, triangles = 0;
            foreach (var part in geometry.Parts)
            {
                Require(part.mesh != null && part.material != null, "Missing source mesh/material: " + entry.id);
                var positions = part.mesh.vertices;
                var normals = part.mesh.normals;
                var uv = part.mesh.uv;
                var indices = part.mesh.triangles;
                Require(positions.Length > 0 && positions.Length == normals.Length && uv.Length == positions.Length && indices.Length > 0 && indices.Length % 3 == 0,
                    "Source attributes do not match geometry: " + entry.id);
                bool finite = true, validIndices = true;
                for (int i = 0; i < positions.Length; ++i)
                    finite &= Finite(positions[i]) && Finite(normals[i]) && Finite(uv[i].x) && Finite(uv[i].y);
                foreach (int index in indices) validIndices &= index >= 0 && index < positions.Length;
                Require(finite && validIndices, "Nonfinite geometry or invalid triangle index: " + entry.id);
                vertices += positions.Length;
                triangles += indices.Length / 3;
                var material = part.material;
                Require(!string.IsNullOrEmpty(material.texture) && !material.texture.ToLowerInvariant().Contains("icon"), "Ornament has no genuine material texture: " + entry.id);
                Require(Finite(material.alphaCutoff) && material.alphaCutoff >= 0 && material.alphaCutoff <= 1 && material.tint.a > 0, "Invalid alpha material: " + entry.id);
                if (verifiedTextures.Add(material.texture))
                {
                    var texture = Resources.Load<Texture2D>(material.texture);
                    Require(texture != null && texture.width > 0 && texture.height > 0, "Missing material texture: " + material.texture);
                    ++report.textures;
                    if (!priorTextures.Contains(texture)) Resources.UnloadAsset(texture);
                }
            }
            Require(vertices == entry.vertices && triangles == entry.triangles, "Runtime mesh does not match source counts: " + entry.id);
            report.vertices += vertices;
            report.triangles += triangles;
        }

        Color32[] Read(Texture texture)
        {
            Require(texture is RenderTexture, "Production ornament renderer did not return a render texture.");
            if (!readback || readback.width != texture.width || readback.height != texture.height)
            {
                if (readback) UnityEngine.Object.DestroyImmediate(readback);
                readback = new Texture2D(texture.width, texture.height, TextureFormat.RGBA32, false) { hideFlags = HideFlags.HideAndDontSave };
            }
            var previous = RenderTexture.active;
            try
            {
                RenderTexture.active = (RenderTexture)texture;
                readback.ReadPixels(new Rect(0, 0, texture.width, texture.height), 0, 0, false);
                readback.Apply(false, false);
                return readback.GetPixels32();
            }
            finally { RenderTexture.active = previous; }
        }

        PoseResult Inspect(string name, Vector3 degrees, Color32[] pixels, Color32[] front)
        {
            var result = new PoseResult { name = name, degrees = degrees };
            int width = readback.width, height = readback.height, minX = width, minY = height, maxX = -1, maxY = -1;
            // The renderer camera covers world Y [-2.45, .25], and its chain
            // ends at -.9. Split at that attachment, with a three-pixel guard,
            // rather than at the image midpoint: shallow objects such as the
            // drone can be fully visible without extending into the lower half.
            const float cameraBottom = -2.45f, cameraSpan = 2.7f, chainEndY = -.9f;
            float attachmentPixel = height * (chainEndY - cameraBottom) / cameraSpan;
            float bodyEndPixel = attachmentPixel - 3, strapStartPixel = attachmentPixel + 3;
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                {
                    int index = y * width + x;
                    var pixel = pixels[index];
                    if (pixel.a > 8)
                    {
                        ++result.visiblePixels;
                        if (y < bodyEndPixel) ++result.bodyPixels;
                        if (y > strapStartPixel) ++result.strapPixels;
                        if (x <= 1 || y <= 1 || x >= width - 2 || y >= height - 2) ++result.frameEdgePixels;
                        if (pixel.r > 8 || pixel.g > 8 || pixel.b > 8) ++result.coloredPixels;
                        minX = Math.Min(x, minX); minY = Math.Min(y, minY); maxX = Math.Max(x, maxX); maxY = Math.Max(y, maxY);
                    }
                    if (front != null && (Math.Abs(pixel.r - front[index].r) > 4 || Math.Abs(pixel.g - front[index].g) > 4
                        || Math.Abs(pixel.b - front[index].b) > 4 || Math.Abs(pixel.a - front[index].a) > 4)) ++result.changedPixels;
                }
            result.visibleBounds = maxX >= minX ? new Rect(minX, minY, maxX - minX + 1, maxY - minY + 1) : Rect.zero;
            return result;
        }

        internal void Run()
        {
            Require(SystemInfo.graphicsDeviceType != GraphicsDeviceType.Null, "Ornament GPU checks need a graphics device; omit -nographics.");
            Require(RenderMethod != null && OutputProperty != null && PartCountProperty != null && SelectedProperty != null && ResidentProperty != null,
                "Production renderer QA API has changed.");
            Require(Idas3OrnamentCatalog.Count == 281 && Idas3OrnamentCatalog.IdAt(0) == 0, "Expected Off and all 280 source ornaments.");
            var strapIds = new HashSet<int>();
            // Inspect source assets before leasing textures to a live renderer.
            for (int i = 1; i < Idas3OrnamentCatalog.Count; ++i)
            {
                int id = Idas3OrnamentCatalog.IdAt(i);
                var entry = Idas3OrnamentCatalog.Get(id);
                Require(entry != null && Idas3OrnamentCatalog.IndexOf(id) == i && Idas3OrnamentCatalog.IsValid(id), "Stable source ID lookup failed.");
                using (var geometry = Idas3OrnamentCatalog.LoadModel(id)) VerifyModel(entry, geometry);
                strapIds.Add(entry.strapId);
            }
            foreach (int id in strapIds)
            {
                using (var geometry = Idas3OrnamentCatalog.LoadStrap(id)) VerifyModel(Idas3OrnamentCatalog.GetStrap(id), geometry);
                ++report.straps;
            }
            Require(strapIds.Count == 4, "Not all recovered chain families are represented.");
            var names = new[] { "front", "maximum-left", "maximum-right", "side-angle", "preview" };
            var poses = new[] { Vector3.zero,
                new Vector3(-Idas3OrnamentMotion.MaximumPitchDegrees, 0, -Idas3OrnamentMotion.MaximumRollDegrees),
                new Vector3(Idas3OrnamentMotion.MaximumPitchDegrees, 0, Idas3OrnamentMotion.MaximumRollDegrees),
                new Vector3(0, 45, 0), new Vector3(9, 12, 17) };
            using (var renderer = new Idas3OrnamentRenderer())
                for (int i = 1; i < Idas3OrnamentCatalog.Count; ++i)
                {
                    int id = Idas3OrnamentCatalog.IdAt(i);
                    var entry = Idas3OrnamentCatalog.Get(id);
                    var result = new ModelResult { id = id, name = entry.name, strapId = entry.strapId,
                        sourceModel = entry.sourceModel, vertices = entry.vertices, triangles = entry.triangles, recoveredSocket = entry.recoveredSocket };
                    report.results.Add(result);
                    Color32[] front = null, swing = null;
                    try
                    {
                        for (int pose = 0; pose < poses.Length; ++pose)
                        {
                            var texture = Render(renderer, id, Quaternion.Euler(poses[pose]));
                            var pixels = Read(texture);
                            var probe = Inspect(names[pose], poses[pose], pixels, front);
                            result.poses.Add(probe); ++report.gpuCases;
                            if (pose == 0)
                            {
                                front = pixels;
                                result.frontImage = "ornament-" + id.ToString("0000") + "-front.png";
                                File.WriteAllBytes(Path.Combine(output, result.frontImage), readback.EncodeToPNG());
                            }
                            if (pose == 2)
                            {
                                swing = pixels;
                                result.swingImage = "ornament-" + id.ToString("0000") + "-swing.png";
                                File.WriteAllBytes(Path.Combine(output, result.swingImage), readback.EncodeToPNG());
                            }
                            Require(probe.visiblePixels >= 100 && probe.coloredPixels >= 30, "No visible textured geometry: " + names[pose]);
                            Require(probe.visiblePixels < pixels.Length / 2, "Transparent background is missing: " + names[pose]);
                            Require(probe.frameEdgePixels == 0, "Ornament is clipped by the viewport: " + names[pose]);
                            Require(probe.visibleBounds.width > 4 && probe.visibleBounds.height > texture.height / 4, "Ornament geometry is collapsed: " + names[pose]);
                            if (pose == 0)
                            {
                                Require(probe.bodyPixels >= 50, "The strap renders but the ornament body is missing.");
                                Require(probe.strapPixels >= 20, "The ornament body renders but its chain is missing.");
                            }
                            else Require(probe.changedPixels >= 50, "The rendered geometry did not respond to pose: " + names[pose]);
                        }
                        result.meshParts = (int)PartCountProperty.GetValue(renderer);
                        result.peakResidentTextures = Resident;
                        report.peakResidentTextures = Math.Max(report.peakResidentTextures, Resident);
                        Require((int)SelectedProperty.GetValue(renderer) == id && result.meshParts >= 2, "The selected ornament and chain were not assembled.");
                        ++report.models;
                    }
                    catch (Exception error) { Failure(result, error); }
                    AddTile(result, front, swing);
                }
            FlushSheet();
            CacheChecks();
            report.finalResidentTextures = Resident;
            Require(Resident == report.initialResidentTextures, "Texture resources remain after all renderers were disposed.");
            report.allDisposed = true;
            report.passed = report.errors.Count == 0 && report.models == 280 && report.gpuCases == 1400;
        }

        void CacheChecks()
        {
            int baseline = Resident;
            using (var first = new Idas3OrnamentRenderer())
            using (var second = new Idas3OrnamentRenderer())
            {
                var pixels = Read(Render(first, 528, Quaternion.identity));
                int one = Resident;
                Require(one > baseline, "The first model did not lease textures.");
                Read(Render(second, 528, Quaternion.identity));
                Require(Resident == one, "Two users of one ornament duplicated pooled textures.");
                first.Dispose();
                Require(Resident == one, "Disposing the first model released shared textures still in use.");
                var probe = Inspect("shared-cache", Vector3.one, Read(Render(second, 528, Quaternion.Euler(8, 8, 8))), pixels);
                Require(probe.visiblePixels > 100 && probe.bodyPixels > 50 && probe.changedPixels > 50, "Remaining renderer lost visible material after shared disposal.");
                report.sharedCacheRetained = true;
                Require(Render(second, 0, Quaternion.identity) == null && (int)SelectedProperty.GetValue(second) == 0
                    && (int)PartCountProperty.GetValue(second) == 0 && Resident == baseline, "Off did not release selected geometry and texture leases.");
                report.offReleased = true;
                second.Dispose();
                Require((Texture)OutputProperty.GetValue(second) == null, "Disposed renderer retained its GPU output.");
            }
            Require(Resident == baseline, "Shared texture leases leaked after both users closed.");
            report.sharedCacheReleased = true;
        }

        void AddTile(ModelResult result, Color32[] front, Color32[] swing)
        {
            if (sheet == null)
            {
                sheet = new Color32[SheetWidth * SheetHeight];
                for (int i = 0; i < sheet.Length; ++i) sheet[i] = new Color32(15, 17, 20, 255);
            }
            int x = tiles % Columns * TileWidth, y = tiles / Columns * TileHeight;
            DrawText("#" + result.id.ToString("0000") + "  " + result.name.ToUpperInvariant(), x + 5, y + 5, TileWidth / 6 - 1);
            DrawText("FRONT             SWING", x + 5, y + 18, 40);
            Thumbnail(front, x, y + 32); Thumbnail(swing, x + 128, y + 32);
            if (!string.IsNullOrEmpty(result.error)) DrawText("CHECK FAILED", x + 5, y + 151, 40);
            if (++tiles == Columns * Rows) FlushSheet();
        }

        void Thumbnail(Color32[] source, int left, int top)
        {
            if (source == null || !readback) return;
            for (int y = 0; y < 128; ++y)
                for (int x = 0; x < 128; ++x)
                {
                    int sourceX = x * readback.width / 128, sourceY = (127 - y) * readback.height / 128;
                    var pixel = source[sourceY * readback.width + sourceX];
                    int background = ((x / 16 + y / 16) & 1) == 0 ? 29 : 35;
                    // Production RT pixels are premultiplied, just like the overlay.
                    float remainder = (255 - pixel.a) / 255f;
                    SetPixel(left + x, top + y, new Color32((byte)Math.Min(255, pixel.r + background * remainder),
                        (byte)Math.Min(255, pixel.g + background * remainder), (byte)Math.Min(255, pixel.b + background * remainder), 255));
                }
        }
        void SetPixel(int x, int y, Color32 value)
        {
            if (x >= 0 && x < SheetWidth && y >= 0 && y < SheetHeight) sheet[(SheetHeight - 1 - y) * SheetWidth + x] = value;
        }
        void DrawText(string text, int x, int y, int maximum)
        {
            for (int letter = 0; letter < Math.Min(text.Length, maximum); ++letter)
            {
                if (!Font.TryGetValue(text[letter], out string glyph)) glyph = Font['?'];
                for (int row = 0; row < 7; ++row)
                    for (int column = 0; column < 5; ++column)
                        if (glyph[row * 5 + column] == '1') SetPixel(x + letter * 6 + column, y + row, new Color32(226, 231, 240, 255));
            }
        }
        void FlushSheet()
        {
            if (sheet == null || tiles == 0) return;
            var image = new Texture2D(SheetWidth, SheetHeight, TextureFormat.RGB24, false) { hideFlags = HideFlags.HideAndDontSave };
            try
            {
                image.SetPixels32(sheet); image.Apply(false, false);
                string name = "contact-sheet-" + (++sheetNumber).ToString("00") + ".png";
                File.WriteAllBytes(Path.Combine(output, name), image.EncodeToPNG()); report.contactSheets.Add(name);
            }
            finally { UnityEngine.Object.DestroyImmediate(image); }
            sheet = null; tiles = 0;
        }

        internal void SaveReport()
        {
            FlushSheet();
            File.WriteAllText(Path.Combine(output, "report.json"), JsonUtility.ToJson(report, true));
            var html = new StringBuilder("<!doctype html><meta charset='utf-8'><title>Hanging ornaments GPU audit</title><style>body{background:#111;color:#eee;font:16px Arial;margin:24px}section{display:inline-block;width:340px;vertical-align:top;margin:8px;padding:12px;background:#1c1e22}img{max-width:100%}section img{width:160px;background:repeating-conic-gradient(#252830 0% 25%,#1a1c22 0% 50%) 50%/24px 24px}small{display:block;color:#abb2bf}a{color:#88caff}</style><h1>Actual hanging ornaments</h1><p>Production 3D renders. Left: front. Right: maximum swing. Original native assembly and shader equivalence are not established.</p>");
            html.Append("<p>").Append(report.models).Append(" / 280 models passed; ").Append(report.gpuCases).Append(" GPU cases. <a href='report.json'>Detailed report</a></p>");
            foreach (string contact in report.contactSheets) html.Append("<a href='").Append(contact).Append("'>").Append(contact).Append("</a> ");
            html.Append("<hr>");
            foreach (var result in report.results)
            {
                html.Append("<section><strong>#").Append(result.id).Append(" ").Append(Escape(result.name)).Append("</strong><small>Chain ").Append(result.strapId).Append(" · ").Append(result.triangles).Append(" triangles</small>");
                if (!string.IsNullOrEmpty(result.frontImage)) html.Append("<a href='").Append(result.frontImage).Append("'><img src='").Append(result.frontImage).Append("'></a>");
                if (!string.IsNullOrEmpty(result.swingImage)) html.Append("<a href='").Append(result.swingImage).Append("'><img src='").Append(result.swingImage).Append("'></a>");
                if (!string.IsNullOrEmpty(result.error)) html.Append("<p>").Append(Escape(result.error)).Append("</p>");
                html.Append("</section>");
            }
            File.WriteAllText(Path.Combine(output, "index.html"), html.ToString());
        }
        static string Escape(string value) => (value ?? "").Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;").Replace("\"", "&quot;");
        public void Dispose() { if (readback) UnityEngine.Object.DestroyImmediate(readback); readback = null; }
    }

    // Tiny report-only bitmap font keeps contact sheets self-contained in batch
    // mode without opening an editor window or relying on system fonts.
    static readonly Dictionary<char, string> Font = new Dictionary<char, string> {
        [' ']="00000000000000000000000000000000000", ['?']="01110100010000100110001000000000100", ['#']="01010010101111101010111110101001010",
        ['A']="01110100011000111111100011000110001", ['B']="11110100011000111110100011000111110", ['C']="01111100001000010000100001000001111",
        ['D']="11110100011000110001100011000111110", ['E']="11111100001000011110100001000011111", ['F']="11111100001000011110100001000010000",
        ['G']="01111100001000010111100011000101111", ['H']="10001100011000111111100011000110001", ['I']="11111001000010000100001000010011111",
        ['J']="00111000100001000010100101001001100", ['K']="10001100101010011000101001001010001", ['L']="10000100001000010000100001000011111",
        ['M']="10001110111010110101100011000110001", ['N']="10001110011010110011100011000110001", ['O']="01110100011000110001100011000101110",
        ['P']="11110100011000111110100001000010000", ['Q']="01110100011000110001101011001001101", ['R']="11110100011000111110101001001010001",
        ['S']="01111100001000001110000010000111110", ['T']="11111001000010000100001000010000100", ['U']="10001100011000110001100011000101110",
        ['V']="10001100011000110001100010101000100", ['W']="10001100011000110101101011010101010", ['X']="10001100010101000100010101000110001",
        ['Y']="10001100010101000100001000010000100", ['Z']="11111000010001000100010001000011111", ['0']="01110100011001110101110011000101110",
        ['1']="00100011000010000100001000010001110", ['2']="01110100010000100010001000100011111", ['3']="11110000010000101110000010000111110",
        ['4']="00010001100101010010111110001000010", ['5']="11111100001000011110000010000111110", ['6']="01110100001000011110100011000101110",
        ['7']="11111000010001000100010000100001000", ['8']="01110100011000101110100011000101110", ['9']="01110100011000101111000010000101110",
        ['-']="00000000000000011111000000000000000", ['.']="00000000000000000000000000011000110", ['/']="00001000010001000100010001000010000",
        ['(']="00010001000100001000010000010000010", [')']="01000001000001000010000100010001000"
    };

    public static void VerifyOnly()
    {
        string output = Path.GetFullPath("Verification/ornaments-20260924/gpu-" + DateTime.UtcNow.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N").Substring(0, 6));
        Directory.CreateDirectory(output);
        Idas3HudCustomizationChecks.Run();
        string motion = Idas3OrnamentMotionChecks.RunChecks() + " " + Idas3OrnamentMotionChecks.RunViewChecks();
        File.WriteAllText(Path.Combine(output, "motion-checks.txt"), motion + "\n");
        Debug.Log(motion);
        foreach (string name in new[] { "ArcadeOrnament", "ArcadeOrnamentOverlay" })
        {
            var shader = Resources.Load<Shader>(name);
            if (!shader) throw new InvalidOperationException("Missing ornament shader: " + name);
            if (!shader.isSupported || ShaderUtil.ShaderHasError(shader))
            {
                var details = new StringBuilder();
                foreach (var message in ShaderUtil.GetShaderMessages(shader)) details.AppendLine(message.severity + ": " + message.message + " (" + message.file + ":" + message.line + ")");
                throw new InvalidOperationException("Invalid ornament shader " + name + ": " + details);
            }
        }
        using (var runner = new Runner(output))
        {
            runner.report.motionChecks = motion;
            try { runner.Run(); }
            finally { runner.SaveReport(); }
            if (!runner.report.passed) throw new InvalidOperationException("Ornament GPU audit failed. Review " + output + "/report.json");
            Debug.Log("Ornament GPU QA PASS: " + runner.report.models + " models, " + runner.report.gpuCases + " rendered poses, " + runner.report.checks + " checks. Output: " + output);
        }
    }

    public static void VerifyAndBuild() { VerifyOnly(); Idas3Build.RebuildWindowsPlayer(); }
}
