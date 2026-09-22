using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Unity.Collections;
using Unity.Collections.LowLevel.Unsafe;
using UnityEngine;
using UnityEngine.Rendering;

// Unity owns every graphics object. The plugin publishes CPU scene records;
// this component never imports a native GPU texture or issues a plugin event.
public sealed class Idas3SceneRenderer : MonoBehaviour
{
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    public struct SceneCamera {
        public Vector3 eye, target, up;
        public float verticalFov, aspect, nearClip, farClip;
        public Vector4 viewport;
        public uint leftHanded;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    public struct SceneFrame {
        public uint size, version;
        public ulong frameGeneration, textureGeneration;
        public uint width, height, vertexCount, rangeCount, textureCount, overlayCount, viewCount, screenFadeArgb;
        public IntPtr vertices, ranges, textures, overlays, frameConstants, lightConstants, fogConstants;
        public SceneCamera mainCamera, mirrorCamera;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    public struct SceneOverlay { public IntPtr surface; public uint width, height, flags, reserved; }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    private struct SceneRange {
        public uint first, count, texture, tsp, pcw, isp, gmp, flags, gloss, lightScope, viewMask, sourceIndex;
        public Vector4 lightDirection;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    private struct SceneVertex { public Vector3 position, normal; public Vector4 color; public Vector2 uv; public Vector4 offset; }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    private struct SceneTexture { public uint width, height; public ulong pixelCount; public IntPtr argb; }
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Idas3SceneGetFrame(ref SceneFrame frame);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr Idas3SceneGetGeometryIds(ulong generation, uint rangeCount);

    public SceneFrame CurrentFrame { get; private set; }
    public Camera MainCamera => main;
    public Camera MirrorCamera => mirror;
    public Idas3GameOptions.Values HudOptions { get; set; } = new Idas3GameOptions.Values();
    public Rect ViewportRect { get; private set; } = new Rect(0, 0, 1, 1);
    public int ActiveMeshCount { get; private set; }
    public int UploadedVertexCount { get; private set; }
    public int GeometryUploadCount { get; private set; }
    public int MaterialUpdateCount { get; private set; }
    public int DepthCandidateCount => depthCandidates.Count;
    public int DepthDrawCount { get; private set; }
    public int DepthBufferRebuildCount { get; private set; }
    public int UploadedTextureCount => textureCache.Count;
    private Camera main, mirror, backdrop, canvasClear;
    private Shader sceneShader, directSceneShader, opaqueAlphaDepthShader;
    private bool geometryStageBaseline;
    private bool rangeReuseBaseline;
    private CommandBuffer opaqueAlphaDepth, mirrorAlphaDepth;
    private readonly List<RangeObject> depthCandidates = new List<RangeObject>();
    private readonly List<RangeObject> mainDepthDraws = new List<RangeObject>(), mirrorDepthDraws = new List<RangeObject>();
    private readonly List<RangeObject> depthScratch = new List<RangeObject>();
    private readonly Vector4[] depthSidePlanes = new Vector4[4];
    private bool diagnosticDepthSubmissionBaseline;
    private ComputeBuffer framesBuffer, lightsBuffer, fogBuffer;
    private readonly float[] frameWords = new float[184];
    private readonly int[] lightWords = new int[1248], fogWords = new int[136];
    [StructLayout(LayoutKind.Sequential)] private struct UInt4 { public uint x, y, z, w; }
    private readonly Vector4[] frameVectors = new Vector4[46];
    private readonly UInt4[] lightVectors = new UInt4[312], fogVectors = new UInt4[34];
    private readonly List<RangeObject> objects = new List<RangeObject>();
    private readonly Dictionary<ulong, Texture2D> textureCache = new Dictionary<ulong, Texture2D>();
    private readonly Transform[] owners = new Transform[4];
    private Transform worldRoot;
    private readonly Light[] lampObjects = new Light[8];
    private SceneTexture[] textureRecords = Array.Empty<SceneTexture>();
    private ulong textureGeneration = ulong.MaxValue;
    private bool initialized;
    private bool diagnosticFlipDepth, diagnosticNoCull, diagnosticCullBaseline, diagnosticAlphaDepthOff, diagnosticDepthCancellationBaseline;
    private bool diagnosticIntroFoliageBaseline;
    private bool diagnosticCarCull, diagnosticActorCullBaseline;
    private bool diagnosticPerfBaseline;
    private bool diagnosticCameraPose;
    private Vector3 diagnosticEye, diagnosticTarget;
    // Opaque, then punch-through, then translucent. Hoisted so the frame
    // loop allocates nothing.
    private static readonly uint[] SourceListOrder = { 0, 4, 2 };
    // Reused by SetCamera; it allocated one of these per camera per frame.
    private readonly float[] viewWordsScratch = new float[16];
    // Source depth-compare codes. Hoisted: this ran once per range per frame.
    private static readonly CompareFunction[] DepthComparisons = {
        CompareFunction.Never, CompareFunction.Greater, CompareFunction.Equal, CompareFunction.GreaterEqual,
        CompareFunction.Less, CompareFunction.NotEqual, CompareFunction.LessEqual, CompareFunction.Always };
    private const int SceneLayer = 30;
    private const MeshUpdateFlags UploadFlags = MeshUpdateFlags.DontRecalculateBounds | MeshUpdateFlags.DontValidateIndices;
    private static readonly VertexAttributeDescriptor[] layout = {
        new VertexAttributeDescriptor(VertexAttribute.Position, VertexAttributeFormat.Float32, 3),
        new VertexAttributeDescriptor(VertexAttribute.Normal, VertexAttributeFormat.Float32, 3),
        new VertexAttributeDescriptor(VertexAttribute.Color, VertexAttributeFormat.Float32, 4),
        new VertexAttributeDescriptor(VertexAttribute.TexCoord0, VertexAttributeFormat.Float32, 2),
        new VertexAttributeDescriptor(VertexAttribute.TexCoord1, VertexAttributeFormat.Float32, 4)
    };
    private readonly Dictionary<ulong, RangeObject> geometryLookup = new Dictionary<ulong, RangeObject>();
    private readonly List<RangeObject> selectedRanges = new List<RangeObject>();
    private int selectionStamp, freeRangeCursor;
    private bool rangeCacheOff;
    private bool geometryIdsOff, validateGeometryIds;
    private IntPtr geometryIds;
    private sealed class RangeObject {
        public GameObject gameObject;
        public Mesh mesh;
        public MeshRenderer renderer;
        public Material material;
        public Material depthMaterial;
        public int count, selectedStamp;
        public ulong geometryKey;
        public ulong geometryId;
        public RangeObject nextGeometry;
        public NativeArray<SceneVertex> vertexCache;
        public bool cachedBillboard;
        public SceneRange materialRange;
        public ulong materialTextureGeneration;
        public int materialQueue;
        public bool materialConfigured, depthMaterialDirty, depthCandidate;
        public uint parentScope = uint.MaxValue;
        public bool active = true;
        public Bounds bounds;
    }

    public void Initialize(Camera camera)
    {
        rangeCacheOff = Array.IndexOf(Environment.GetCommandLineArgs(), "-idas3-range-cache-off") >= 0;
        if (initialized) throw new InvalidOperationException("Scene renderer is already initialized.");
        if (Marshal.SizeOf<SceneFrame>() != 256 || Marshal.SizeOf<SceneCamera>() != 72 || Marshal.SizeOf<SceneRange>() != 64 || Marshal.SizeOf<SceneVertex>() != 64)
            throw new InvalidOperationException("Unity scene ABI mismatch.");
        main = camera != null ? camera : throw new ArgumentNullException(nameof(camera));
        string[] arguments = Environment.GetCommandLineArgs();
        geometryIdsOff = Array.IndexOf(arguments, "-idas3-geometry-ids-off") >= 0;
        validateGeometryIds = Array.IndexOf(arguments, "-idas3-validate-geometry-ids") >= 0;
        diagnosticFlipDepth = Array.IndexOf(arguments, "-idas3-scene-depth-test-flip") >= 0;
        diagnosticNoCull = Array.IndexOf(arguments, "-idas3-scene-no-cull") >= 0;
        diagnosticCullBaseline = Array.IndexOf(arguments, "-idas3-scene-cull-baseline") >= 0;
        diagnosticAlphaDepthOff = Array.IndexOf(arguments, "-idas3-scene-alpha-depth-off") >= 0;
        diagnosticDepthCancellationBaseline = Array.IndexOf(arguments, "-idas3-scene-depth-cancellation-baseline") >= 0;
        diagnosticIntroFoliageBaseline = Array.IndexOf(arguments, "-idas3-scene-intro-foliage-baseline") >= 0;
        diagnosticCarCull = Array.IndexOf(arguments, "-idas3-scene-car-cull-check") >= 0 &&
            Array.IndexOf(arguments, "-idas3-scene-car-door-check") >= 0;
        diagnosticActorCullBaseline = Array.IndexOf(arguments, "-idas3-scene-actor-cull-baseline") >= 0;
        diagnosticPerfBaseline = Array.IndexOf(arguments, "-idas3-scene-perf-baseline") >= 0;
        diagnosticDepthSubmissionBaseline = Array.IndexOf(arguments, "-idas3-depth-submission-baseline") >= 0;
        if (diagnosticFlipDepth || diagnosticNoCull)
            Debug.Log("IDAS3 scene diagnostics: depthTestFlip=" + diagnosticFlipDepth + ", noCull=" + diagnosticNoCull);
        sceneShader = Shader.Find("IDAS3/Original Scene Material");
        if (sceneShader == null || !sceneShader.isSupported) throw new InvalidOperationException("Original Unity scene shader is unavailable.");
        directSceneShader = Resources.Load<Shader>("Idas3SceneDirect");
        if (directSceneShader == null || !directSceneShader.isSupported) throw new InvalidOperationException("Direct Unity scene shader is unavailable.");
        geometryStageBaseline = Array.IndexOf(arguments, "-idas3-geometry-stage-baseline") >= 0;
        rangeReuseBaseline = Array.IndexOf(arguments, "-idas3-range-reuse-baseline") >= 0;
        opaqueAlphaDepthShader = Shader.Find("Hidden/IDAS3/Opaque Alpha Depth");
        if (opaqueAlphaDepthShader == null || !opaqueAlphaDepthShader.isSupported) throw new InvalidOperationException("Original opaque-alpha depth shader is unavailable.");
        // This component shares the host camera GameObject. Captured vertices
        // already contain their complete world transform, so their identity
        // mesh hierarchy must not inherit that moving camera transform. Unity
        // uses Renderer.localToWorld for CPU bounds even when our shader uses
        // the authored world positions directly.
        worldRoot = new GameObject("Original world-space scene").transform;
        string[] names = { "World and effects", "Course", "Player car", "Opponent car" };
        for (int i = 0; i < owners.Length; ++i) { owners[i] = new GameObject(names[i]).transform; owners[i].SetParent(worldRoot, false); }
        backdrop = NewCamera("Scene clear", -1000); backdrop.cullingMask = 0; backdrop.clearFlags = CameraClearFlags.SolidColor;
        canvasClear = NewCamera("Original canvas clear", -2); canvasClear.cullingMask = 0; canvasClear.clearFlags = CameraClearFlags.SolidColor;
        mirror = NewCamera("Original rear-view camera", 1);
        main.depth = 0; ConfigureCommonCamera(main); ConfigureCommonCamera(mirror);
        // The source translucent sorter lets a fully opaque texel hide every
        // farther fragment, even when its polygon is in the translucent list.
        // Establish only those fully opaque depths before scene color passes.
        // Each camera executes this buffer against its own cleared depth view.
        opaqueAlphaDepth = new CommandBuffer { name = "Original opaque-alpha occlusion" };
        mirrorAlphaDepth = new CommandBuffer { name = "Original mirror opaque-alpha occlusion" };
        main.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, opaqueAlphaDepth);
        mirror.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, mirrorAlphaDepth);
        framesBuffer = new ComputeBuffer(46, 16, ComputeBufferType.Structured);
        lightsBuffer = new ComputeBuffer(312, 16, ComputeBufferType.Structured);
        fogBuffer = new ComputeBuffer(34, 16, ComputeBufferType.Structured);
        Camera.onPreRender += BeforeCamera;
        initialized = true;
    }
    private Camera NewCamera(string name, float depth)
    {
        var obj = new GameObject(name); obj.transform.SetParent(transform, false);
        var camera = obj.AddComponent<Camera>(); camera.depth = depth; return camera;
    }
    private static void ConfigureCommonCamera(Camera camera)
    {
        // Imported scenery uses 28; challenger UI exclusively owns 29.
        // Both world cameras must see the course, including the rear-view.
        camera.cullingMask = (1 << SceneLayer) | (1 << 28); camera.clearFlags = CameraClearFlags.SolidColor;
        camera.allowHDR = false; camera.allowMSAA = true; camera.useOcclusionCulling = false;
    }
    private void BeforeCamera(Camera camera)
    {
        if (camera != main && camera != mirror) return;
        Shader.SetGlobalInteger("_IdasView", camera == mirror ? 1 : 0);
        Shader.SetGlobalBuffer("_IdasFrameWords", framesBuffer);
        Shader.SetGlobalBuffer("_IdasLightWords", lightsBuffer);
        Shader.SetGlobalBuffer("_IdasFogWords", fogBuffer);
        // Form reversed depth directly from clip W. Subtracting forward clip Z
        // from W loses the tiny near-plane term in distant-world intro shots.
        // Double precision here also keeps the coefficients accurate for the
        // opening camera's .01 .. 10000 range. Each mirror has its own planes.
        var source = camera == mirror ? CurrentFrame.mirrorCamera : CurrentFrame.mainCamera;
        double near = source.nearClip, far = source.farClip;
        Shader.SetGlobalVector("_IdasDepthProjection", new Vector4(
            (float)(near * far / (far - near)), (float)(near / (far - near)), 0,
            diagnosticDepthCancellationBaseline ? 0 : 1));
    }
    public unsafe void ApplyFrame()
    {
        if (!initialized) throw new InvalidOperationException("Initialize the Unity scene renderer first.");
        UploadedVertexCount = GeometryUploadCount = MaterialUpdateCount = 0;
        var frame = new SceneFrame { size = 256 };
        if (Idas3SceneGetFrame(ref frame) != 1 || frame.version != 1) throw new InvalidOperationException("Native scene frame is unavailable.");
        if (frame.frameGeneration == 0) return;
        if (frame.vertexCount > 2000000 || frame.rangeCount > 4000 || frame.viewCount < 1 || frame.viewCount > 2)
            throw new InvalidOperationException("Invalid Unity scene bounds.");
        CurrentFrame = frame;
        geometryIds = Idas3SceneGetGeometryIds(frame.frameGeneration, frame.rangeCount);
        if (geometryIds == IntPtr.Zero && frame.rangeCount != 0)
            throw new InvalidOperationException("Native geometry identities do not match the scene frame.");
        Marshal.Copy(frame.frameConstants, frameWords, 0, frameWords.Length);
        Marshal.Copy(frame.lightConstants, lightWords, 0, lightWords.Length);
        Marshal.Copy(frame.fogConstants, fogWords, 0, fogWords.Length);
        // Source records stay available in CurrentFrame. Only Unity's display
        // projection expands a fixed authored main aperture to the full output.
        // Use these same adjusted words for shader upload and CPU culling.
        SceneCamera outputMainCamera = ExpandMainViewport(frame.mainCamera, frame.width, frame.height, frameWords);
        if (diagnosticCameraPose) ApplyDiagnosticCamera(ref outputMainCamera);
        for (int i = 0; i < frameVectors.Length; ++i) frameVectors[i] = new Vector4(frameWords[i * 4], frameWords[i * 4 + 1], frameWords[i * 4 + 2], frameWords[i * 4 + 3]);
        PackWords(lightWords, lightVectors); PackWords(fogWords, fogVectors);
        framesBuffer.SetData(frameVectors); lightsBuffer.SetData(lightVectors); fogBuffer.SetData(fogVectors);
        if (textureGeneration != frame.textureGeneration)
        {
            foreach (var texture in textureCache.Values) Destroy(texture); textureCache.Clear();
            textureRecords = new SceneTexture[frame.textureCount];
            for (int i = 0; i < textureRecords.Length; ++i) textureRecords[i] = Marshal.PtrToStructure<SceneTexture>(IntPtr.Add(frame.textures, i * 24));
            textureGeneration = frame.textureGeneration;
        }
        var source = NativeArrayUnsafeUtility.ConvertExistingDataToNativeArray<SceneVertex>(frame.vertices.ToPointer(), (int)frame.vertexCount, Allocator.None);
#if ENABLE_UNITY_COLLECTIONS_CHECKS
        var safety = AtomicSafetyHandle.Create(); NativeArrayUnsafeUtility.SetAtomicSafetyHandle(ref source, safety);
#endif
        try
        {
            ActiveMeshCount = 0;
            depthCandidates.Clear();
            SelectCachedRanges(frame);
            // Exact source list ordering, then exact submission ordering.
            // Distinct queues prevent Unity's distance sorting from changing
            // authored alpha and projected-light destination-color blending.
            int rank = 0;
            foreach (uint list in SourceListOrder)
            for (int i = 0; i < frame.rangeCount; ++i)
            {
                var range = *(SceneRange*)IntPtr.Add(frame.ranges, i * 64);
                uint nativeList = (range.flags & 1) != 0 ? (range.pcw >> 24) & 7 : 0;
                if (nativeList != list || range.count == 0 || range.viewMask == 0) continue;
                if ((ulong)range.first + range.count > frame.vertexCount || range.lightScope > 3)
                    throw new InvalidOperationException("Scene range is outside its vertex/light bounds.");
                var item = diagnosticPerfBaseline || rangeCacheOff ? GetRangeObject(ActiveMeshCount) : selectedRanges[i] ?? ClaimRangeObject();
                if(!diagnosticPerfBaseline&&!rangeCacheOff)selectedRanges[i]=item;
                ++ActiveMeshCount;
                int queue = 1000 + rank++;
                CountDepthMode(range);
                ulong geometryId = geometryIdsOff ? 0 : ((ulong*)geometryIds.ToPointer())[i];
                // Native identities guarantee immutable vertex bytes. A cached
                // active renderer with identical material/ownership needs only
                // its source-order queue maintained. Camera/light globals still
                // update and Unity still culls/renders both views normally.
                if (!rangeReuseBaseline && !diagnosticPerfBaseline && !diagnosticNoCull && !validateGeometryIds &&
                    geometryId != 0 && item.geometryId == geometryId && item.active && item.vertexCache.IsCreated &&
                    item.parentScope == range.lightScope && item.materialConfigured &&
                    item.materialTextureGeneration == textureGeneration && SameRange(item.materialRange, range)) {
                    if (item.materialQueue != queue) {
                        item.material.renderQueue = queue; item.materialQueue = queue; ++MaterialUpdateCount;
                    }
                    if (item.depthCandidate) depthCandidates.Add(item);
                    continue;
                }
                if (diagnosticPerfBaseline || !item.active) { item.gameObject.SetActive(true); item.active = true; }
                if (diagnosticPerfBaseline || item.parentScope != range.lightScope) {
                    item.gameObject.transform.SetParent(owners[range.lightScope], false);
                    item.parentScope = range.lightScope;
                }
                if (item.count != range.count)
                {
                    if (item.vertexCache.IsCreated) item.vertexCache.Dispose();
                    item.mesh.Clear(); item.mesh.SetVertexBufferParams((int)range.count, layout);
                    item.mesh.SetIndexBufferParams((int)range.count, IndexFormat.UInt32);
                    var indices = new NativeArray<uint>((int)range.count, Allocator.Temp, NativeArrayOptions.UninitializedMemory);
                    for (int j = 0; j < indices.Length; ++j) indices[j] = (uint)j;
                    item.mesh.SetIndexBufferData(indices, 0, 0, indices.Length, UploadFlags); indices.Dispose();
                    item.mesh.subMeshCount = 1;
                    item.mesh.SetSubMesh(0, new SubMeshDescriptor(0, (int)range.count, MeshTopology.Triangles), UploadFlags);
                    item.count = (int)range.count;
                }
                bool billboard = (range.flags & 4) != 0;
                var rangeVertices = (SceneVertex*)frame.vertices.ToPointer() + (int)range.first;
                long vertexBytes = (long)range.count * 64;
                // The scene ABI publishes complete world vertices each frame,
                // including static course geometry. Immutable cached ranges
                // have native identities; dynamic/untagged ranges still get
                // an exact byte comparison. Lighting/camera buffers update.
                bool immutableMatch = geometryId != 0 && geometryId == item.geometryId && item.vertexCache.IsCreated && item.cachedBillboard == billboard;
                if (validateGeometryIds && immutableMatch &&
                    UnsafeUtility.MemCmp(rangeVertices, NativeArrayUnsafeUtility.GetUnsafeReadOnlyPtr(item.vertexCache), vertexBytes) != 0)
                    throw new InvalidOperationException("An immutable native geometry identity changed its vertices.");
                bool geometryChanged = diagnosticPerfBaseline || !item.vertexCache.IsCreated || item.cachedBillboard != billboard || (!immutableMatch &&
                    UnsafeUtility.MemCmp(rangeVertices, NativeArrayUnsafeUtility.GetUnsafeReadOnlyPtr(item.vertexCache), vertexBytes) != 0);
                if (geometryChanged) {
                    item.mesh.SetVertexBufferData(source, (int)range.first, 0, (int)range.count, 0, UploadFlags);
                    UploadedVertexCount += (int)range.count; ++GeometryUploadCount;
                    var first = source[(int)range.first];
                    Vector3 low = billboard ? first.normal : first.position, high = low;
                    for (int j = 0; j < range.count; ++j)
                    {
                        var vertex = source[(int)range.first + j]; Vector3 position = billboard ? vertex.normal : vertex.position;
                        // Lit spectator cards pack their source normal in the unused
                        // planar Z slot; this must not enlarge geometric bounds.
                        Vector3 local = vertex.position;
                        if (billboard && (range.gmp & 512) == 0) local.z = 0;
                        Vector3 radius = billboard ? Vector3.one * local.magnitude : Vector3.zero;
                        low = Vector3.Min(low, position - radius); high = Vector3.Max(high, position + radius);
                    }
                    item.bounds = diagnosticNoCull
                        ? new Bounds(frame.mainCamera.eye, Vector3.one * 20000000f)
                        : new Bounds((low + high) * .5f, Vector3.Max(high - low, Vector3.one * .001f));
                    item.mesh.bounds = item.bounds;
                    if (!diagnosticPerfBaseline) {
                        if (!item.vertexCache.IsCreated) item.vertexCache = new NativeArray<SceneVertex>((int)range.count, Allocator.Persistent, NativeArrayOptions.UninitializedMemory);
                        UnsafeUtility.MemCpy(NativeArrayUnsafeUtility.GetUnsafePtr(item.vertexCache), rangeVertices, vertexBytes);
                        item.cachedBillboard = billboard;
                        item.geometryKey = GeometryKey(rangeVertices, (int)range.count, billboard);
                    }
                }
                else if (diagnosticNoCull) { item.bounds = new Bounds(frame.mainCamera.eye, Vector3.one * 20000000f); item.mesh.bounds = item.bounds; }
                item.geometryId = geometryId;
                // Include all source words, override direction, view mask,
                // submission order and texture ownership in the exact key.
                // Lighting and diagnostic camera changes are shader globals.
                if (diagnosticPerfBaseline || !item.materialConfigured ||
                    item.materialTextureGeneration != textureGeneration || !SameRange(item.materialRange, range)) {
                    ConfigureMaterial(item.material, range, queue); ++MaterialUpdateCount;
                    item.materialRange = range; item.materialQueue = queue; item.materialTextureGeneration = textureGeneration;
                    item.materialConfigured = true; item.depthMaterialDirty = true;
                }
                else if (item.materialQueue != queue) {
                    item.material.renderQueue = queue; item.materialQueue = queue;
                    ++MaterialUpdateCount;
                }
                item.depthCandidate = !diagnosticAlphaDepthOff && (range.flags & 1) != 0 && IsCourseGeometry(range) && nativeList == 2 &&
                    ((range.tsp >> 29) & 7) == 4 && ((range.tsp >> 26) & 7) == 5;
                if (item.depthCandidate)
                {
                    if (item.depthMaterial == null) { item.depthMaterial = new Material(opaqueAlphaDepthShader); item.depthMaterialDirty = true; }
                    if (diagnosticPerfBaseline || item.depthMaterialDirty) {
                        item.depthMaterial.CopyPropertiesFromMaterial(item.material); item.depthMaterialDirty = false;
                    }
                    depthCandidates.Add(item);
                }
            }
            for (int i = 0; i < objects.Count; ++i) {
                var item = objects[i];
                if (diagnosticPerfBaseline || rangeCacheOff ? i < ActiveMeshCount : item.selectedStamp == selectionStamp) continue;
                if (diagnosticPerfBaseline || item.active) { item.gameObject.SetActive(false); item.active = false; }
            }
        }
        finally
        {
#if ENABLE_UNITY_COLLECTIONS_CHECKS
            AtomicSafetyHandle.Release(safety);
#endif
        }
        Color clear = new Color(frameWords[20], frameWords[21], frameWords[22], 1);
        // The host captures at the current output dimensions. Native race/demo
        // projection uses that aspect, so the complete canvas fills the window.
        // Fixed main apertures expand their frustum without changing the
        // authored center composition; only the 5:1 mirror stays a sub-viewport.
        ViewportRect = new Rect(0, 0, 1, 1);
        backdrop.targetTexture = canvasClear.targetTexture = mirror.targetTexture = main.targetTexture;
        backdrop.backgroundColor = Color.black; backdrop.rect = new Rect(0, 0, 1, 1);
        // Clear the complete output before the source background UI camera (-1).
        canvasClear.backgroundColor = clear; canvasClear.rect = ViewportRect;
        bool behind = false;
        for (int i = 0; i < frame.overlayCount; ++i) behind |= ((*(SceneOverlay*)IntPtr.Add(frame.overlays, i * 24)).flags & 1) != 0;
        main.clearFlags = behind ? CameraClearFlags.Depth : CameraClearFlags.SolidColor;
        SetCamera(main, outputMainCamera, 0, clear, frame.width, frame.height);
        mirror.enabled = frame.viewCount == 2;
        if (mirror.enabled) SetCamera(mirror, frame.mirrorCamera, 1, clear, frame.width, frame.height);
        DepthDrawCount = DepthBufferRebuildCount = 0;
        UpdateDepthCommands(main, 1, opaqueAlphaDepth, mainDepthDraws, true);
        UpdateDepthCommands(mirror, 2, mirrorAlphaDepth, mirrorDepthDraws, mirror.enabled);
        UpdateLampObjects();
    }
    private void UpdateDepthCommands(Camera camera, uint viewBit, CommandBuffer commands, List<RangeObject> previous, bool enabled)
    {
        depthScratch.Clear();
        // Homogeneous side planes are conservative for custom reversed depth.
        // Use the color renderer's world/billboard bounds without reducing LOD
        // or distance. Keep all near/far candidates for the GPU to clip.
        Matrix4x4 vp = camera.cullingMatrix;
        Vector4 w = vp.GetRow(3), x = vp.GetRow(0), y = vp.GetRow(1);
        depthSidePlanes[0] = w + x; depthSidePlanes[1] = w - x;
        depthSidePlanes[2] = w + y; depthSidePlanes[3] = w - y;
        if (enabled) foreach (var item in depthCandidates) {
            if (!diagnosticDepthSubmissionBaseline && !diagnosticNoCull &&
                ((item.materialRange.viewMask & viewBit) == 0 || OutsideDepthView(item.bounds))) continue;
            depthScratch.Add(item);
        }
        DepthDrawCount += depthScratch.Count;
        bool changed = diagnosticDepthSubmissionBaseline || previous.Count != depthScratch.Count;
        if (!changed) for (int i = 0; i < previous.Count; ++i)
            if (previous[i] != depthScratch[i]) { changed = true; break; }
        if (!changed) return;
        // Persistent mesh/material references stay valid when their contents
        // update. Re-record only when the ordered visible draw list changes.
        commands.Clear(); previous.Clear();
        foreach (var item in depthScratch) {
            commands.DrawMesh(item.mesh, Matrix4x4.identity, item.depthMaterial, 0, 0);
            previous.Add(item);
        }
        ++DepthBufferRebuildCount;
    }
    private bool OutsideDepthView(Bounds bounds)
    {
        Vector3 c = bounds.center, e = bounds.extents + Vector3.one * .05f;
        foreach (Vector4 p in depthSidePlanes) {
            float farthest = p.x*c.x+p.y*c.y+p.z*c.z+p.w + Mathf.Abs(p.x)*e.x+Mathf.Abs(p.y)*e.y+Mathf.Abs(p.z)*e.z;
            if (farthest < 0) return true;
        }
        return false;
    }
    // Match exact vertex bytes before claiming any free renderer: an inserted
    // roadside range must not evict every unchanged range later in the list.
    // Hash collisions are resolved with a full comparison, never treated as hits.
    private static unsafe ulong GeometryKey(SceneVertex* data, int count, bool billboard) {
        ulong hash = (ulong)count ^ (billboard ? 0x9e3779b97f4a7c15UL : 0UL);
        var first=(ulong*)data;var last=(ulong*)(data+count-1);
        unchecked { for(int i=0;i<8;++i) { hash=(hash^first[i])*1099511628211UL; hash=(hash^last[i])*1099511628211UL; } }
        return hash;
    }
    private unsafe void SelectCachedRanges(SceneFrame frame) {
        if(diagnosticPerfBaseline || rangeCacheOff)return;
        ++selectionStamp;freeRangeCursor=0;
        while(selectedRanges.Count<frame.rangeCount)selectedRanges.Add(null);
        bool lookupNeeded=false;
        // Most frames keep the same ordering. Immutable identities avoid
        // touching vertex data; other candidates use cheap endpoint words
        // here and a complete byte comparison in the upload pass.
        for(int i=0;i<frame.rangeCount;++i) {
            var previous=selectedRanges[i];selectedRanges[i]=null;
            var r=*(SceneRange*)IntPtr.Add(frame.ranges,i*64);
            uint list=(r.flags&1)!=0?(r.pcw>>24)&7:0;
            if(r.count==0||r.viewMask==0||(list!=0&&list!=4&&list!=2))continue;
            if((ulong)r.first+r.count>frame.vertexCount)throw new InvalidOperationException("Invalid cached range bounds");
            var data=(SceneVertex*)frame.vertices.ToPointer()+(int)r.first;
            bool match=false;
            if(previous!=null&&previous.selectedStamp!=selectionStamp&&previous.count==r.count&&previous.vertexCache.IsCreated&&previous.cachedBillboard==((r.flags&4)!=0)) {
                ulong identity=geometryIdsOff?0:((ulong*)geometryIds.ToPointer())[i];
                if(identity!=0&&identity==previous.geometryId)match=true;
                else {
                var cached=(SceneVertex*)NativeArrayUnsafeUtility.GetUnsafeReadOnlyPtr(previous.vertexCache);
                var a=(ulong*)data;var b=(ulong*)cached;var lastA=(ulong*)(data+r.count-1);var lastB=(ulong*)(cached+r.count-1);
                match=a[0]==b[0]&&a[1]==b[1]&&lastA[0]==lastB[0]&&lastA[1]==lastB[1];
                }
            }
            if(match){previous.selectedStamp=selectionStamp;selectedRanges[i]=previous;}
            else if(IsCourseGeometry(r))lookupNeeded=true;
        }
        if(!lookupNeeded)return; // Moving actors alone do not need a world lookup.
        geometryLookup.Clear();
        foreach(var item in objects) {
            if(!item.vertexCache.IsCreated||item.selectedStamp==selectionStamp)continue;
            geometryLookup.TryGetValue(item.geometryKey,out item.nextGeometry);
            geometryLookup[item.geometryKey]=item;
        }
        for(int i=0;i<frame.rangeCount;++i) {
            if(selectedRanges[i]!=null)continue;
            var r=*(SceneRange*)IntPtr.Add(frame.ranges,i*64);
            uint list=(r.flags&1)!=0?(r.pcw>>24)&7:0;
            if(r.count==0||r.viewMask==0||(list!=0&&list!=4&&list!=2))continue;
            var data=(SceneVertex*)frame.vertices.ToPointer()+(int)r.first;
            bool billboard=(r.flags&4)!=0;
            geometryLookup.TryGetValue(GeometryKey(data,(int)r.count,billboard),out var candidate);
            while(candidate!=null) {
                // A moved list position does not change immutable vertex data.
                // Dynamic/untagged ranges still require a full comparison.
                if(candidate.selectedStamp!=selectionStamp && candidate.count==r.count && candidate.cachedBillboard==billboard &&
                   ((!geometryIdsOff && ((ulong*)geometryIds.ToPointer())[i]!=0 && ((ulong*)geometryIds.ToPointer())[i]==candidate.geometryId) ||
                    UnsafeUtility.MemCmp(data,NativeArrayUnsafeUtility.GetUnsafeReadOnlyPtr(candidate.vertexCache),(long)r.count*64)==0)) {
                    candidate.selectedStamp=selectionStamp;selectedRanges[i]=candidate;break;
                }
                candidate=candidate.nextGeometry;
            }
        }
    }
    private RangeObject ClaimRangeObject() {
        while(freeRangeCursor<objects.Count&&objects[freeRangeCursor].selectedStamp==selectionStamp)++freeRangeCursor;
        var item=GetRangeObject(freeRangeCursor++);item.selectedStamp=selectionStamp;return item;
    }
    private RangeObject GetRangeObject(int index)
    {
        if (index < objects.Count) return objects[index];
        var obj = new GameObject("Original range"); obj.layer = SceneLayer; obj.transform.SetParent(worldRoot, false);
        var mesh = new Mesh { name = "Original authored geometry" }; mesh.MarkDynamic();
        obj.AddComponent<MeshFilter>().sharedMesh = mesh;
        var renderer = obj.AddComponent<MeshRenderer>(); renderer.shadowCastingMode = ShadowCastingMode.Off;
        renderer.receiveShadows = false; renderer.lightProbeUsage = LightProbeUsage.Off; renderer.reflectionProbeUsage = ReflectionProbeUsage.Off;
        var material = new Material(sceneShader); renderer.sharedMaterial = material;
        var result = new RangeObject { gameObject = obj, mesh = mesh, renderer = renderer, material = material };
        objects.Add(result); return result;
    }
    private static void PackWords(int[] source, UInt4[] target)
    {
        for (int i = 0; i < target.Length; ++i) target[i] = new UInt4 {
            x = unchecked((uint)source[i * 4]), y = unchecked((uint)source[i * 4 + 1]),
            z = unchecked((uint)source[i * 4 + 2]), w = unchecked((uint)source[i * 4 + 3])
        };
    }
    private static unsafe bool SameRange(SceneRange a, SceneRange b) {
        // Source offsets/order do not alter shader state; queue is handled separately.
        a.first=b.first=0;a.sourceIndex=b.sourceIndex=0;
        return UnsafeUtility.MemCmp(&a, &b, 64) == 0;
    }
    private void ConfigureMaterial(Material m, SceneRange r, int queue)
    {
        bool original = (r.flags & 1) != 0;
        bool actorCull = !diagnosticActorCullBaseline && ((r.flags & 32) != 0 || diagnosticCarCull);
        int cullMode = original && (IsCourseGeometry(r) || actorCull) && (r.flags & 4) == 0 && !diagnosticCullBaseline
            ? (int)((r.isp >> 27) & 3) : 0;
        // The geometry stage only rejects authored backfaces or selects the
        // final vertex's flat colors. When neither operation applies its output
        // is exactly the vertex shader's output, including both camera views.
        Shader wanted = !geometryStageBaseline && cullMode < 2 && (!original || (r.pcw & 2) != 0)
            ? directSceneShader : sceneShader;
        if (m.shader != wanted) m.shader = wanted;
        m.renderQueue = queue;
        // SetInt aliases SetFloat in Unity. Source material words require the
        // true integer setter or their low bits are lost above2^24.
        m.SetInteger("pcw", unchecked((int)r.pcw)); m.SetInteger("tsp", unchecked((int)r.tsp)); m.SetInteger("gmp", unchecked((int)r.gmp));
        m.SetInteger("original", (int)(r.flags & 1)); m.SetInteger("emissive", (int)((r.flags >> 1) & 1)); m.SetInteger("billboard", (int)((r.flags >> 2) & 1));
        m.SetInteger("courseLightRange", r.lightScope != 0 ? 1 : 0); m.SetInteger("_LightScope", (int)r.lightScope); m.SetInteger("viewMask", (int)r.viewMask);
        uint g = r.gloss & 255; m.SetFloat("glossCoefficient", (1 + (g & 31) / 32f) * Mathf.Pow(2, (int)(g >> 5) - 1));
        var direction = r.lightDirection; direction.w = (r.flags & 8) != 0 ? 1 : 0; m.SetVector("_LightOverride", direction);
        // Tagged car owners preserve source winding. Menu reflection owners
        // supply their explicit inverted parity in the native ISP word.
        // Test winding in world space in the existing geometry pass, avoiding
        // both the mirror's handedness and Unity render-target Y inversion.
        m.SetInteger("courseCullMode", cullMode);
        uint source = original ? r.tsp >> 29 : 1, destination = original ? (r.tsp >> 26) & 7 : 0;
        m.SetInt("_SrcBlend", (int)Factor(source, true, false)); m.SetInt("_DstBlend", (int)Factor(destination, false, false));
        m.SetInt("_SrcBlendAlpha", (int)Factor(source, true, true)); m.SetInt("_DstBlendAlpha", (int)Factor(destination, false, true));
        uint list = original ? (r.pcw >> 24) & 7 : 0;
        // The source translucent list is auto-sorted per pixel by the original
        // hardware, which compares greater-or-equal and does not write depth.
        // Punch-through is not auto-sorted on the hardware: only the translucent
        // list compares greater-or-equal. Forcing it on punch-through lets
        // coplanar fragments overwrite by submission order, which is what made
        // signs flicker under motion and let the road show through foliage.
        uint compare = !original || list == 2 ? 6 : r.isp >> 29;
        var depthTest = DepthComparisons[compare];
        // Opt-in actual-player diagnostic: isolate ShaderLab comparison
        // semantics from the existing reversed clip-space depth conversion.
        if (diagnosticFlipDepth) {
            if (depthTest == CompareFunction.Less) depthTest = CompareFunction.Greater;
            else if (depthTest == CompareFunction.Greater) depthTest = CompareFunction.Less;
            else if (depthTest == CompareFunction.LessEqual) depthTest = CompareFunction.GreaterEqual;
            else if (depthTest == CompareFunction.GreaterEqual) depthTest = CompareFunction.LessEqual;
        }
        m.SetInt("_ZTest", (int)depthTest); m.SetInt("_ZWrite", original && list == 2 ? 0 : (!original || list == 4 || ((r.isp >> 26) & 1) == 0 ? 1 : 0));
        uint sampler = original ? ((r.tsp >> 16) & 1) | (((r.tsp >> 15) & 1) << 1) | (((r.tsp >> 18) & 1) << 2) | (((r.tsp >> 17) & 1) << 3) | ((((r.tsp >> 13) & 3) != 0 ? 1u : 0u) << 4) : 16;
        m.mainTexture = Texture(r.texture, sampler);
    }
    // Diagnostic only: when a dictionary is installed, count the depth modes
    // the source materials ask for in one frame.
    internal static System.Collections.Generic.Dictionary<string,int> DepthCensus;
    private static void CountDepthMode(SceneRange r)
    {
        // Count source submissions even when cached materials need no setters.
        if (DepthCensus == null || (r.flags & 1) == 0) return;
        uint list = (r.pcw >> 24) & 7, compare = list == 2 ? 6 : r.isp >> 29;
        uint zw = (uint)(list == 2 ? 0 : list == 4 || ((r.isp >> 26) & 1) == 0 ? 1 : 0);
        uint sb = (r.tsp >> 29) & 7, db = (r.tsp >> 26) & 7;
        string k = "list" + list + " compare" + compare + " zwrite" + zw + " src" + sb + " dst" + db
            + " emissive" + ((r.flags >> 1) & 1);
        DepthCensus.TryGetValue(k, out int n); DepthCensus[k] = n + 1;
    }
    private static BlendMode Factor(uint code, bool source, bool alpha)
    {
        switch (code) {
            case 0: return BlendMode.Zero; case 1: return BlendMode.One;
            case 2: return source ? (alpha ? BlendMode.DstAlpha : BlendMode.DstColor) : (alpha ? BlendMode.SrcAlpha : BlendMode.SrcColor);
            case 3: return source ? (alpha ? BlendMode.OneMinusDstAlpha : BlendMode.OneMinusDstColor) : (alpha ? BlendMode.OneMinusSrcAlpha : BlendMode.OneMinusSrcColor);
            case 4: return BlendMode.SrcAlpha; case 5: return BlendMode.OneMinusSrcAlpha; case 6: return BlendMode.DstAlpha; default: return BlendMode.OneMinusDstAlpha;
        }
    }
    private unsafe Texture Texture(uint id, uint sampler)
    {
        if (id >= textureRecords.Length) return Texture2D.whiteTexture;
        ulong key = ((ulong)id << 32) | sampler;
        if (textureCache.TryGetValue(key, out var ready)) return ready;
        var image = textureRecords[id];
        if (image.width == 0 || image.height == 0 || image.width > 2048 || image.height > 2048 || image.argb == IntPtr.Zero ||
            image.pixelCount != (ulong)image.width * image.height || image.pixelCount > 4194304)
            throw new InvalidOperationException("Invalid Unity source texture.");
        var texture = new Texture2D((int)image.width, (int)image.height, TextureFormat.BGRA32, true, true) { name = "Original texture " + id + " sampler " + sampler };
        // The original bank carries mip0 only; native D3D GenerateMips builds
        // the rest. LoadRawTextureData expects the ENTIRE allocated mip chain.
        var pixels = NativeArrayUnsafeUtility.ConvertExistingDataToNativeArray<uint>(image.argb.ToPointer(), checked((int)image.pixelCount), Allocator.None);
#if ENABLE_UNITY_COLLECTIONS_CHECKS
        var safety = AtomicSafetyHandle.Create(); NativeArrayUnsafeUtility.SetAtomicSafetyHandle(ref pixels, safety);
#endif
        try { texture.SetPixelData(pixels, 0); }
        catch { Destroy(texture); throw; }
        finally
        {
#if ENABLE_UNITY_COLLECTIONS_CHECKS
            AtomicSafetyHandle.Release(safety);
#endif
        }
        texture.wrapModeU = (sampler & 1) != 0 ? TextureWrapMode.Clamp : (sampler & 4) != 0 ? TextureWrapMode.Mirror : TextureWrapMode.Repeat;
        texture.wrapModeV = (sampler & 2) != 0 ? TextureWrapMode.Clamp : (sampler & 8) != 0 ? TextureWrapMode.Mirror : TextureWrapMode.Repeat;
        // Point-sampled source materials crawl above the cabinet resolution,
        // which the per-pixel alternation map shows as a fine speckle over the
        // rock face, the guardrails and the road ahead.
        bool filtered = (sampler & 16) != 0 || Screen.height > 480;
        texture.filterMode = filtered ? FilterMode.Trilinear : FilterMode.Point;
        texture.anisoLevel = filtered ? 16 : 1;
        texture.Apply(true, true); textureCache.Add(key, texture); return texture;
    }
    private static Matrix4x4 NativeMatrix(float[] words, int offset)
    {
        var matrix = new Matrix4x4(); for (int row = 0; row < 4; ++row) for (int col = 0; col < 4; ++col) matrix[row, col] = words[offset + col * 4 + row]; return matrix;
    }
    // Geometry ownership is independent of the original light-array scope.
    // The opening has authored course meshes but uses its own fallback lights.
    private bool IsCourseGeometry(SceneRange r) => r.lightScope == 1 ||
        ((r.flags & 16) != 0 && !diagnosticIntroFoliageBaseline);
    // Opt-in visual probe: source geometry and its captured lighting stay fixed.
    // Only the main clip/culling camera changes, so opposing sides of the same
    // authored foliage can be compared without moving the physics actor.
    public void SetDiagnosticCameraPose(Vector3 eye, Vector3 target)
    {
        var args = Environment.GetCommandLineArgs();
        if (Array.IndexOf(args, "-idas3-scene-cull-check") < 0 &&
            Array.IndexOf(args, "-idas3-scene-intro-check") < 0 &&
            Array.IndexOf(args, "-idas3-scene-car-door-check") < 0 &&
            Array.IndexOf(args, "-idas3-scene-intro-foliage-check") < 0)
            throw new InvalidOperationException("Diagnostic camera requires the isolated culling check.");
        if ((target-eye).sqrMagnitude < .001f) throw new ArgumentException("Degenerate diagnostic camera.");
        diagnosticEye=eye;diagnosticTarget=target;diagnosticCameraPose=true;
    }
    public void ClearDiagnosticCameraPose() { diagnosticCameraPose=false; }
    public void SetDiagnosticCourseLampVisibility(bool enabled,bool carOnly=false)
    {
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-hakone-smoke")<0)
            throw new InvalidOperationException("Lamp capture requires the isolated Hakone smoke check.");
        if(enabled){framesBuffer.SetData(frameVectors);return;}
        var sample=(Vector4[])frameVectors.Clone();
        for(int view=0;view<2;view++){
            if(carOnly)sample[view*23+7].w=0;
            else for(int row=14;row<22;row++)sample[view*23+row].w=0;
        }
        framesBuffer.SetData(sample);
    }
    private void ApplyDiagnosticCamera(ref SceneCamera camera)
    {
        var oldView=NativeMatrix(Array.ConvertAll(lightWords, BitConverter.Int32BitsToSingle),0);
        var projection=NativeMatrix(frameWords,0)*oldView.inverse;
        Vector3 z=(diagnosticEye-diagnosticTarget).normalized;
        Vector3 x=Vector3.Cross(Vector3.up,z).normalized,y=Vector3.Cross(z,x);
        var view=Matrix4x4.identity;
        view.SetRow(0,new Vector4(x.x,x.y,x.z,-Vector3.Dot(x,diagnosticEye)));
        view.SetRow(1,new Vector4(y.x,y.y,y.z,-Vector3.Dot(y,diagnosticEye)));
        view.SetRow(2,new Vector4(z.x,z.y,z.z,-Vector3.Dot(z,diagnosticEye)));
        var vp=projection*view;
        for(int row=0;row<4;++row)for(int col=0;col<4;++col)frameWords[col*4+row]=vp[row,col];
        camera.eye=diagnosticEye;camera.target=diagnosticTarget;camera.up=y;
        for(int i=0;i<3;++i){frameWords[16+i]=diagnosticEye[i];frameWords[32+i]=x[i];frameWords[36+i]=y[i];}
    }
    private static SceneCamera ExpandMainViewport(SceneCamera c, uint width, uint height, float[] words)
    {
        if (width == 0 || height == 0 || c.viewport.z <= 0 || c.viewport.w <= 0)
            throw new InvalidOperationException("Invalid original main camera viewport.");
        if (c.viewport.x == 0 && c.viewport.y == 0 && c.viewport.z == width && c.viewport.w == height)
            return c; // Ordinary race/demo projection remains bit-identical.
        // A panel aperture is a hole in a menu screen, not a narrow window onto
        // the world: widening it would spill the car across the whole screen.
        if ((c.leftHanded & 2) != 0) return c;
        float sx = c.viewport.z / width, sy = c.viewport.w / height;
        float tx = (2 * c.viewport.x + c.viewport.z) / width - 1;
        float ty = 1 - (2 * c.viewport.y + c.viewport.w) / height;
        // Native row-vector VP: x' = sx*x + tx*w, y' = sy*y + ty*w.
        // Converting either result to output pixels produces the SAME position
        // and object proportions as the original smaller viewport. The extra
        // window area reveals additional world instead of stretching/cropping.
        // Including tx/ty also preserves an authored off-center aperture.
        for (int row = 0; row < 4; ++row) {
            int i = row * 4;
            words[i] = sx * words[i] + tx * words[i + 3];
            words[i + 1] = sy * words[i + 1] + ty * words[i + 3];
        }
        c.verticalFov = (float)(2 * Math.Atan(Math.Tan(c.verticalFov * .5) / sy));
        c.aspect *= sy / sx;
        c.viewport = new Vector4(0, 0, width, height);
        return c;
    }
    private void SetCamera(Camera camera, SceneCamera c, int view, Color clear, uint width, uint height)
    {
        camera.enabled = true; camera.backgroundColor = clear;
        camera.transform.SetPositionAndRotation(c.eye, Quaternion.LookRotation(c.target - c.eye, c.up));
        camera.fieldOfView = c.verticalFov * Mathf.Rad2Deg; camera.aspect = c.aspect; camera.nearClipPlane = c.nearClip; camera.farClipPlane = c.farClip;
        camera.rect = new Rect(ViewportRect.x + c.viewport.x / width * ViewportRect.width,
            ViewportRect.y + (1 - (c.viewport.y + c.viewport.w) / height) * ViewportRect.height,
            c.viewport.z / width * ViewportRect.width, c.viewport.w / height * ViewportRect.height);
        if(view==1){
            var rect=camera.rect;float scale=HudOptions.HudGroupScale(4);
            var offset=HudOptions.HudOffset(4);
            camera.rect=new Rect(.5f+(rect.x-.5f)*scale+offset.x,1f+(rect.y-1f)*scale-offset.y,rect.width*scale,rect.height*scale);
        }
        Buffer.BlockCopy(lightWords, view * 4 * 156 * 4, viewWordsScratch, 0, 64);
        var viewWords = viewWordsScratch;
        var matrix = NativeMatrix(viewWords, 0); var viewProjection = NativeMatrix(frameWords, view * 92);
        camera.worldToCameraMatrix = matrix; camera.projectionMatrix = viewProjection * matrix.inverse; camera.cullingMatrix = viewProjection;
    }
    private void UpdateLampObjects()
    {
        for (int i = 0; i < lampObjects.Length; ++i)
        {
            int row = (14 + i) * 4;
            if (lampObjects[i] == null) {
                var obj = new GameObject("Course lamp " + i); obj.transform.SetParent(owners[1], false);
                var light = obj.AddComponent<Light>(); light.type = LightType.Point; light.shadows = LightShadows.None;
                light.color = new Color(1, .91f, .76f); light.intensity = 1.6f; lampObjects[i] = light;
            }
            var lamp = lampObjects[i]; lamp.enabled = frameWords[row + 3] > 0;
            lamp.transform.position = new Vector3(frameWords[row], frameWords[row + 1], frameWords[row + 2]); lamp.range = lamp.enabled ? 1 / Mathf.Sqrt(frameWords[row + 3]) : 0;
            // Original materials evaluate these captured lights themselves;
            // Unity Light objects are inspectable scene controls, not a second
            // additive pass over source ELAN/light-projection illumination.
        }
    }
    private void OnDestroy()
    {
        Camera.onPreRender -= BeforeCamera;
        if (opaqueAlphaDepth != null) {
            if (main) main.RemoveCommandBuffer(CameraEvent.BeforeForwardOpaque, opaqueAlphaDepth);
            opaqueAlphaDepth.Release(); opaqueAlphaDepth = null;
        }
        if (mirrorAlphaDepth != null) {
            if (mirror) mirror.RemoveCommandBuffer(CameraEvent.BeforeForwardOpaque, mirrorAlphaDepth);
            mirrorAlphaDepth.Release(); mirrorAlphaDepth = null;
        }
        framesBuffer?.Release(); lightsBuffer?.Release(); fogBuffer?.Release();
        foreach (var item in objects) {
            if (item.vertexCache.IsCreated) item.vertexCache.Dispose();
            if (item.material != null) Destroy(item.material); if (item.depthMaterial != null) Destroy(item.depthMaterial); if (item.mesh != null) Destroy(item.mesh);
        }
        foreach (var texture in textureCache.Values) if (texture != null) Destroy(texture);
        if (worldRoot != null) Destroy(worldRoot.gameObject);
        if (mirror != null) Destroy(mirror.gameObject); if (backdrop != null) Destroy(backdrop.gameObject);
        if (canvasClear != null) Destroy(canvasClear.gameObject);
    }
}
