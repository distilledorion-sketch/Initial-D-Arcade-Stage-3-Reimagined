using System;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

// Unity hosts the complete .29 App. All gameplay remains in the original
// C++ frame owner; no Unity Rigidbody or second simulation is involved.
[RequireComponent(typeof(Camera))]
public sealed class Idas3Game : MonoBehaviour
{
    public int renderWidth = 1280;
    public int renderHeight = 720;
    public bool audioEnabled = true;
    public bool flipVertically = true;
    private static Idas3Game instance;
    private RenderTexture deviceAnchor;
    private Texture2D gameTexture;
    private Material display;
    private IntPtr renderEvent;
    private ulong textureGeneration;
    private bool initialized, nativeReady, focus = true, stopping;
    private int lastSubmittedFrame = -1;
    private int shutdownToken;
    private string failure;
    private string assetRoot, saveRoot;
    internal string SmokeFailure => failure;
    internal bool SmokeStopped => !initialized && shutdownToken == 0;
    internal Texture2D SmokeTexture => gameTexture;
    internal bool BootInitialized => initialized;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void Bootstrap()
    {
        // The old framebuffer host remains an explicit comparison mode only.
        if (Array.IndexOf(Environment.GetCommandLineArgs(), "-idas3-legacy-host") < 0) return;
        if (FindAnyObjectByType<Idas3Game>() != null) return;
        var game = new GameObject("Initial D — native game host");
        game.AddComponent<Camera>();
        game.AddComponent<Idas3Game>();
    }

    private void Awake()
    {
        if (Idas3RomGate.Verified && Idas3Updates.StartupFinished) InitializeGame();
    }

    private void InitializeGame()
    {
        if (!Idas3RomGate.Verified || !Idas3Updates.StartupFinished) return;
        if (instance != null && instance != this) { Destroy(gameObject); return; }
        instance = this;
        DontDestroyOnLoad(gameObject);
        var camera = GetComponent<Camera>();
        camera.clearFlags = CameraClearFlags.SolidColor;
        camera.backgroundColor = Color.black;
        camera.cullingMask = 0;
        camera.allowHDR = false;
        camera.allowMSAA = false;
        Application.runInBackground = true;
        Application.targetFrameRate = 60;
        QualitySettings.vSyncCount = 1;
        focus = Application.isFocused;
        try
        {
            if (Application.platform != RuntimePlatform.WindowsEditor && Application.platform != RuntimePlatform.WindowsPlayer)
                throw new PlatformNotSupportedException("This port currently supports Windows x64.");
            if (SystemInfo.graphicsDeviceType != GraphicsDeviceType.Direct3D11)
                throw new InvalidOperationException("Start this project with Direct3D 11. Use Open in Unity.cmd or add -force-d3d11.");
            if (GraphicsSettings.currentRenderPipeline != null)
                throw new InvalidOperationException("This native port uses Unity's Built-in Render Pipeline. Run Initial D > Configure Project.");
            if (Idas3Native.Idas3UnityVersion() != 1 || Marshal.SizeOf<Idas3Native.FrameInput>() != 88 || Marshal.SizeOf<Idas3Native.Status>() != 80)
                throw new InvalidOperationException("Native plugin ABI mismatch. Run Build Native.cmd.");
            assetRoot = Application.isEditor
                ? Path.GetFullPath(Path.Combine(Application.dataPath, "../Native"))
                : Path.Combine(Application.streamingAssetsPath, "IDAS3");
            saveRoot = Path.Combine(Application.persistentDataPath, "userdata");
            Idas3RomGateSmoke.Configure(ref saveRoot);
            Idas3Smoke.Configure(this, ref saveRoot);
            Directory.CreateDirectory(saveRoot);
            if (!Directory.Exists(Path.Combine(assetRoot, "data/original_physics")))
                throw new DirectoryNotFoundException("Full game data is missing: " + assetRoot);
            var shader = Shader.Find("Hidden/IDAS3/Display");
            if (shader == null) throw new InvalidOperationException("Missing native display shader.");
            display = new Material(shader);
            // A tiny Unity-owned texture supplies the device. The native full
            // resolution target stays on that GPU; there is no CPU readback.
            deviceAnchor = new RenderTexture(16, 16, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
            deviceAnchor.Create();
            renderEvent = Idas3Native.Idas3UnityGetRenderEventFunc();
            int token = Idas3Native.Idas3UnityQueueInitialize(assetRoot, saveRoot,
                deviceAnchor.GetNativeTexturePtr(), renderWidth, renderHeight, audioEnabled ? 1 : 0);
            Issue(token);
            initialized = true;
        }
        catch (Exception error) { Fail(error.Message); }
    }

    private static readonly KeyCode[] keys = {
        KeyCode.Backspace, KeyCode.Return, KeyCode.Escape, KeyCode.Space,
        KeyCode.LeftArrow, KeyCode.UpArrow, KeyCode.RightArrow, KeyCode.DownArrow,
        KeyCode.A, KeyCode.C, KeyCode.D, KeyCode.E, KeyCode.Q, KeyCode.R, KeyCode.S, KeyCode.W,
        KeyCode.F1, KeyCode.F2, KeyCode.F3, KeyCode.F5
    };
    private static readonly int[] virtualKeys = {
        8,13,27,32,37,38,39,40,65,67,68,69,81,82,83,87,112,113,114,116
    };

    private void Update()
    {
        if (!initialized && !stopping && failure == null)
        {
            if (Idas3RomGate.Verified && Idas3Updates.StartupFinished) InitializeGame();
            return;
        }
        if (!initialized || stopping || failure != null) return;
        var status = Idas3Native.ReadStatus();
        if (status.state == 2) { Fail(Idas3Native.Error()); return; }
        nativeReady = status.state == 1;
        if (!nativeReady) return;
        if ((status.flags & 8) == 0)
        {
            StopNative();
#if UNITY_EDITOR
            UnityEditor.EditorApplication.isPlaying = false;
#else
            Application.Quit();
#endif
            return;
        }
        if (gameTexture == null || textureGeneration != status.textureGeneration)
        {
            if (gameTexture != null) DestroyImmediate(gameTexture);
            IntPtr texture = Idas3Native.Idas3UnityGetTexture();
            if (texture == IntPtr.Zero) { gameTexture = null; nativeReady = false; return; }
            gameTexture = Texture2D.CreateExternalTexture(status.width, status.height,
                TextureFormat.BGRA32, false, true, texture);
            gameTexture.wrapMode = TextureWrapMode.Clamp;
            gameTexture.filterMode = FilterMode.Bilinear;
            textureGeneration = status.textureGeneration;
        }
    }

    private void OnRenderImage(RenderTexture source, RenderTexture destination)
    {
        if (initialized && nativeReady && !stopping && failure == null && lastSubmittedFrame != Time.frameCount)
        {
            lastSubmittedFrame = Time.frameCount;
            var frame = new Idas3Native.FrameInput {
                size = (uint)Marshal.SizeOf<Idas3Native.FrameInput>(),
                flags = focus ? 1u : 0u,
                deltaSeconds = Math.Min(Time.unscaledDeltaTime, 0.25),
                // Fixed target size preserves original projection and avoids
                // replacing external textures while Unity samples them.
                width = 0, height = 0
            };
            if (focus)
            {
                for (int i = 0; i < keys.Length; ++i) if (Input.GetKey(keys[i])) frame.SetKey(virtualKeys[i]);
                if (Input.GetKey(KeyCode.KeypadEnter) || Input.GetMouseButton(0)) frame.SetKey(13);
                if (Idas3Native.ReadGamepad(0, out var pad) == 0)
                {
                    frame.padConnected = 1; frame.padButtons = pad.gamepad.buttons;
                    frame.leftTrigger = pad.gamepad.leftTrigger; frame.rightTrigger = pad.gamepad.rightTrigger;
                    frame.thumbLX = pad.gamepad.thumbLX; frame.thumbLY = pad.gamepad.thumbLY;
                    frame.thumbRX = pad.gamepad.thumbRX; frame.thumbRY = pad.gamepad.thumbRY;
                }
            }
            if (Idas3Smoke.PrepareFrame(ref frame)) Issue(Idas3Native.Idas3UnityQueueFrame(ref frame));
        }
        if (gameTexture != null && display != null && failure == null)
        {
            display.SetFloat("_FlipY", flipVertically ? 1 : 0);
            display.SetFloat("_SourceAspect", (float)gameTexture.width / gameTexture.height);
            display.SetFloat("_OutputAspect", destination != null ? (float)destination.width / destination.height : (float)Screen.width / Math.Max(1, Screen.height));
            Graphics.Blit(gameTexture, destination, display);
        }
        else Graphics.Blit(source, destination);
    }

    private void Issue(int token)
    {
        if (token <= 0) { Fail(Idas3Native.Error()); return; }
        GL.IssuePluginEvent(renderEvent, token);
    }
    private void Fail(string error)
    {
        nativeReady = false;
        failure = string.IsNullOrEmpty(error) ? "Native game initialization failed." : error;
        Debug.LogError(failure);
    }
    private void OnGUI()
    {
        if (failure == null) return;
        GUI.Box(new Rect(20, 20, Math.Min(Screen.width - 40, 900), 160), "Initial D Unity could not start\n\n" + failure);
    }
    private void OnApplicationFocus(bool focused) { focus = focused; }
    private void OnApplicationPause(bool paused) { focus = !paused && Application.isFocused; }
    private void OnApplicationQuit() { StopNative(); }
    private void OnDestroy() { StopNative(); if (instance == this) instance = null; }

    public void StopNative()
    {
        if (stopping && shutdownToken == 0) return;
        stopping = true;
        nativeReady = false;
        if (gameTexture != null) { DestroyImmediate(gameTexture); gameTexture = null; }
        if (initialized)
        {
            try
            {
                if (shutdownToken == 0)
                {
                    shutdownToken = Idas3Native.Idas3UnityQueueShutdown();
                    if (shutdownToken <= 0) throw new InvalidOperationException(Idas3Native.Error());
                    using (var commands = new CommandBuffer())
                    {
                        commands.IssuePluginEvent(renderEvent, shutdownToken);
                        Graphics.ExecuteCommandBuffer(commands);
                    }
                    GL.Flush();
                }
                if (Idas3Native.Idas3UnityWaitForEvent(shutdownToken, 5000) != 1)
                {
                    // Keep the Unity device anchor alive until the render
                    // callback actually finishes. A later stop retries.
                    Debug.LogError("Native shutdown is still pending on Unity's render thread; GPU resources retained until it completes.");
                    return;
                }
                shutdownToken = 0;
            }
            catch (Exception error) { Debug.LogError("Native shutdown: " + error.Message); return; }
            initialized = false;
        }
        if (deviceAnchor != null) { deviceAnchor.Release(); DestroyImmediate(deviceAnchor); deviceAnchor = null; }
        if (display != null) { DestroyImmediate(display); display = null; }
    }
}
