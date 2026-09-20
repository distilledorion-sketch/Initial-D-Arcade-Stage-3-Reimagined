using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

// Original sprite triangles and per-glyph host labels, drawn by Unity. No native
// framebuffer or render texture is imported by this component.
public sealed class Idas3UnityUi : MonoBehaviour
{
    [StructLayout(LayoutKind.Sequential, Pack = 8)] struct Frame {
        public uint size, width, height, drawCount, vertexCount, textureCount, unresolvedSurfaces, reserved;
        public ulong revision;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)] struct Draw {
        public uint first, count, texture, tsp, pcw, flags;
        public float opacity; public uint reserved;
        public Vector4 clip;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)] struct Vertex {
        public float x, y, u, v; public uint argb, offsetArgb;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)] struct TextureInfo { public uint size, width, height, bytes; }
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3UiGetFrame(ref Frame frame);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3UiCopyDraws([Out] Draw[] draws, int capacity);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3UiCopyVertices([Out] Vertex[] vertices, int capacity);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3UiGetTextureInfo(uint id, ref TextureInfo info);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3UiCopyTextureRGBA(uint id, [Out] byte[] rgba, int capacity);

    Camera source, backgroundCamera, foregroundCamera;
    CommandBuffer background, foreground;
    Shader shader;
    Mesh mesh, fadeMesh;
    Material fadeMaterial;
    Draw[] draws = Array.Empty<Draw>(); Vertex[] vertices = Array.Empty<Vertex>();
    readonly List<Vector3> positions = new List<Vector3>();
    readonly List<Vector2> uv = new List<Vector2>();
    readonly List<Color32> colors = new List<Color32>();
    readonly List<Vector4> offsets = new List<Vector4>();
    readonly List<Texture2D> textures = new List<Texture2D>();
    readonly Dictionary<ulong, Material> materials = new Dictionary<ulong, Material>();
    readonly List<int[]> indexBuffers = new List<int[]>();
    readonly List<Draw> reusableBatches = new List<Draw>();
    readonly List<MaterialPropertyBlock> drawProperties = new List<MaterialPropertyBlock>();
    MaterialPropertyBlock fadeProperties;
    Idas3SceneRenderer sceneRenderer;
    bool performanceBaseline;
    uint lastUnresolved;
    public int DrawCount { get; private set; }
    public uint UnresolvedSurfaces { get; private set; }
    public int SourceTextureCount => textures.Count;
    static Color32 Color(uint argb) => new Color32((byte)(argb >> 16), (byte)(argb >> 8), (byte)argb, (byte)(argb >> 24));
    static Vector4 Offset(uint a) => new Vector4(((a >> 16) & 255) / 255f, ((a >> 8) & 255) / 255f, (a & 255) / 255f, (a >> 24) / 255f);

    public void Initialize(Camera camera)
    {
        if (source != null) return;
        source = camera ? camera : throw new ArgumentNullException(nameof(camera));
        // Unity native objects cannot be constructed in MonoBehaviour field
        // initializers during editor deserialization/component creation.
        fadeProperties = new MaterialPropertyBlock();
        performanceBaseline = Array.IndexOf(Environment.GetCommandLineArgs(), "-idas3-scene-perf-baseline") >= 0;
        sceneRenderer = GetComponent<Idas3SceneRenderer>();
        if (Marshal.SizeOf<Frame>() != 40 || Marshal.SizeOf<Draw>() != 48 || Marshal.SizeOf<Vertex>() != 24)
            throw new InvalidOperationException("Native UI ABI size mismatch");
        shader = Shader.Find("Idas3/Original UI");
        if (!shader) throw new InvalidOperationException("Original UI shader was not included in this build");
        backgroundCamera = CreateCamera("Original UI background", -1);
        foregroundCamera = CreateCamera("Original UI foreground", 2);
        background = new CommandBuffer { name = "Original animated UI background" };
        foreground = new CommandBuffer { name = "Original animated UI foreground" };
        backgroundCamera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, background);
        foregroundCamera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, foreground);
        mesh = new Mesh { name = "Original menu and HUD elements", indexFormat = IndexFormat.UInt32 };
        mesh.MarkDynamic();
        fadeMesh = new Mesh { name = "Original owner full-screen fade" };
        fadeMesh.vertices = new[] { new Vector3(0,0,0), new Vector3(1,0,0), new Vector3(0,1,0), new Vector3(1,1,0) };
        fadeMesh.triangles = new[] { 0,1,2,2,1,3 };
        fadeMesh.colors32 = new[] { new Color32(255,255,255,255), new Color32(255,255,255,255), new Color32(255,255,255,255), new Color32(255,255,255,255) };
        fadeMaterial = new Material(shader) { name = "Original owner fade", hideFlags = HideFlags.DontSave };
        fadeMaterial.SetInt("_SrcBlend", (int)BlendMode.SrcAlpha); fadeMaterial.SetInt("_DstBlend", (int)BlendMode.OneMinusSrcAlpha);
        fadeMaterial.SetInteger("_Tsp", 1 << 20); fadeMaterial.SetInteger("_Pcw", 0); fadeMaterial.SetInteger("_Original", 1);
    }
    Camera CreateCamera(string label, float depth)
    {
        var go = new GameObject(label) { hideFlags = HideFlags.DontSave };
        go.transform.SetParent(transform, false);
        var camera = go.AddComponent<Camera>();
        camera.clearFlags = CameraClearFlags.Nothing; camera.cullingMask = 0; camera.depth = depth;
        camera.orthographic = true; camera.allowHDR = false; camera.allowMSAA = false;
        return camera;
    }
    static BlendMode Blend(uint factor, bool sourceFactor)
    {
        switch (factor) {
            case 0: return BlendMode.Zero; case 1: return BlendMode.One;
            case 2: return sourceFactor ? BlendMode.DstColor : BlendMode.SrcColor;
            case 3: return sourceFactor ? BlendMode.OneMinusDstColor : BlendMode.OneMinusSrcColor;
            case 4: return BlendMode.SrcAlpha; case 5: return BlendMode.OneMinusSrcAlpha;
            case 6: return BlendMode.DstAlpha; default: return BlendMode.OneMinusDstAlpha;
        }
    }
    Material GetMaterial(Draw draw)
    {
        ulong key = draw.tsp | ((ulong)(draw.pcw & 12) << 32) | ((ulong)(draw.flags & 5) << 40);
        if (materials.TryGetValue(key, out var material)) return material;
        material = new Material(shader) { name = "Original UI material " + key.ToString("X"), hideFlags = HideFlags.DontSave };
        bool original = (draw.flags & 1) != 0;
        uint src = original ? draw.tsp >> 29 : 4, dst = original ? (draw.tsp >> 26) & 7 : 5;
        if ((draw.flags & 4) != 0) dst = 1;
        material.SetInteger("_Tsp", unchecked((int)draw.tsp)); material.SetInteger("_Pcw", (int)draw.pcw);
        material.SetInteger("_Original", original ? 1 : 0); material.SetInteger("_SourceIsOne", src == 1 ? 1 : 0);
        material.SetInt("_SrcBlend", (int)Blend(src, true)); material.SetInt("_DstBlend", (int)Blend(dst, false));
        materials.Add(key, material); return material;
    }
    void LoadTextures(uint count)
    {
        while ((uint)textures.Count < count) {
            var info = new TextureInfo { size = 16 };
            uint id = (uint)textures.Count;
            if (Idas3UiGetTextureInfo(id, ref info) == 0 || info.width == 0 || info.height == 0 || info.width > 2048 || info.height > 2048 || info.bytes != info.width * info.height * 4)
                throw new InvalidOperationException("Invalid original UI source texture " + id);
            var rgba = new byte[info.bytes];
            if (Idas3UiCopyTextureRGBA(id, rgba, rgba.Length) != rgba.Length) throw new InvalidOperationException("UI texture transfer failed");
            var texture = new Texture2D((int)info.width, (int)info.height, TextureFormat.RGBA32, false, true) {
                name = "Original UI sprite " + id, filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Clamp, hideFlags = HideFlags.DontSave
            };
            texture.LoadRawTextureData(rgba); texture.Apply(false, true); textures.Add(texture);
        }
    }
    static bool SameBatch(Draw a, Draw b) => a.first + a.count == b.first && a.texture == b.texture && a.tsp == b.tsp && a.pcw == b.pcw && a.flags == b.flags && a.opacity == b.opacity && a.clip == b.clip;
    public void ApplyFrame()
    {
        if (source == null) return;
        var frame = new Frame { size = 40 };
        if (Idas3UiGetFrame(ref frame) == 0) throw new InvalidOperationException("Native UI frame unavailable");
        if (frame.drawCount > 100000 || frame.vertexCount > 1000000 || frame.width == 0 || frame.height == 0) throw new InvalidOperationException("Invalid native UI frame bounds");
        if (draws.Length < frame.drawCount) draws = new Draw[frame.drawCount];
        if (vertices.Length < frame.vertexCount) vertices = new Vertex[frame.vertexCount];
        if (Idas3UiCopyDraws(draws, draws.Length) != (int)frame.drawCount || Idas3UiCopyVertices(vertices, vertices.Length) != (int)frame.vertexCount) throw new InvalidOperationException("UI frame transfer failed");
        LoadTextures(frame.textureCount);
        positions.Clear(); uv.Clear(); colors.Clear(); offsets.Clear();
        for (int i = 0; i < frame.vertexCount; ++i) {
            var v = vertices[i]; positions.Add(new Vector3(v.x, v.y, 0)); uv.Add(new Vector2(v.u, v.v)); colors.Add(Color(v.argb)); offsets.Add(Offset(v.offsetArgb));
        }
        mesh.Clear(); mesh.SetVertices(positions); mesh.SetUVs(0, uv); mesh.SetColors(colors); mesh.SetUVs(1, offsets);
        var batches = performanceBaseline ? new List<Draw>() : reusableBatches;
        batches.Clear();
        for (int i = 0; i < frame.drawCount; ++i) {
            var d = draws[i];
            if (d.first + d.count > frame.vertexCount || d.texture >= frame.textureCount) throw new InvalidOperationException("UI draw outside source frame");
            if (batches.Count != 0 && SameBatch(batches[batches.Count - 1], d)) { var merged = batches[batches.Count - 1]; merged.count += d.count; batches[batches.Count - 1] = merged; }
            else batches.Add(d);
        }
        mesh.subMeshCount = batches.Count;
        background.Clear(); foreground.Clear();
        var size = new Vector4(frame.width, frame.height, 0, 0);
        for (int i = 0; i < batches.Count; ++i) {
            var d = batches[i];
            while (indexBuffers.Count <= i) indexBuffers.Add(Array.Empty<int>());
            var indices = indexBuffers[i]; if (indices.Length != d.count) indexBuffers[i] = indices = new int[d.count];
            for (int k = 0; k < indices.Length; ++k) indices[k] = (int)d.first + k;
            mesh.SetIndices(indices, MeshTopology.Triangles, i, false);
            MaterialPropertyBlock properties;
            if (performanceBaseline) properties = new MaterialPropertyBlock();
            else {
                while (drawProperties.Count <= i) drawProperties.Add(new MaterialPropertyBlock());
                properties = drawProperties[i];
            }
            properties.SetTexture("_MainTex", textures[(int)d.texture]); properties.SetVector("_Canvas", size); properties.SetVector("_Clip", d.clip);
            properties.SetFloat("_Opacity", d.opacity); properties.SetColor("_Tint", UnityEngine.Color.white);
            var command = (d.flags & 2) != 0 ? background : foreground;
            command.DrawMesh(mesh, Matrix4x4.identity, GetMaterial(d), i, 0, properties);
        }
        mesh.bounds = new Bounds(Vector3.zero, Vector3.one * 100000);
        var renderer = performanceBaseline ? GetComponent<Idas3SceneRenderer>() : sceneRenderer;
        uint fade = renderer ? renderer.CurrentFrame.screenFadeArgb : 0;
        if ((fade >> 24) != 0) {
            var properties = performanceBaseline ? new MaterialPropertyBlock() : fadeProperties; properties.SetVector("_Canvas", new Vector4(1,1,0,0)); properties.SetVector("_Clip", new Vector4(0,0,1,1));
            properties.SetFloat("_Opacity", 1); properties.SetColor("_Tint", Color(fade));
            foreground.DrawMesh(fadeMesh, Matrix4x4.identity, fadeMaterial, 0, 0, properties);
        }
        backgroundCamera.targetTexture = foregroundCamera.targetTexture = source.targetTexture;
        backgroundCamera.rect = foregroundCamera.rect = renderer ? renderer.ViewportRect : new Rect(0,0,1,1);
        DrawCount = batches.Count; UnresolvedSurfaces = frame.unresolvedSurfaces;
        if (UnresolvedSurfaces != 0 && lastUnresolved != UnresolvedSurfaces) Debug.LogError("Original UI capture missed " + UnresolvedSurfaces + " surfaces; no framebuffer fallback is used.");
        lastUnresolved = UnresolvedSurfaces;
    }
    // Explicit captures render the same overlay cameras as the normal player.
    public void RenderOverlayForCapture()
    {
        if(source==null)return;
        backgroundCamera.targetTexture=foregroundCamera.targetTexture=source.targetTexture;
        try {backgroundCamera.Render();foregroundCamera.Render();}
        finally {backgroundCamera.targetTexture=foregroundCamera.targetTexture=null;}
    }
    public void Shutdown()
    {
        if (backgroundCamera && background != null) backgroundCamera.RemoveCommandBuffer(CameraEvent.BeforeForwardOpaque, background);
        if (foregroundCamera && foreground != null) foregroundCamera.RemoveCommandBuffer(CameraEvent.BeforeForwardOpaque, foreground);
        background?.Release(); foreground?.Release(); background = foreground = null;
        if (backgroundCamera) Destroy(backgroundCamera.gameObject); if (foregroundCamera) Destroy(foregroundCamera.gameObject);
        if (mesh) Destroy(mesh); if (fadeMesh) Destroy(fadeMesh); if (fadeMaterial) Destroy(fadeMaterial);
        foreach (var material in materials.Values) Destroy(material); materials.Clear();
        foreach (var texture in textures) Destroy(texture); textures.Clear(); source = null;
    }
    void OnDestroy() => Shutdown();
}

