using System;
using System.Collections;
using System.IO;
using UnityEngine;

// Observes the real startup gate; cannot select a ROM path or approve a ROM.
internal sealed class Idas3RomGateSmoke : MonoBehaviour
{
    private string root, expectation;
    private int checks;
    private bool finished;
    private double began;
    [Serializable] private sealed class Report
    {
        public bool passed, verified, nativeInitialized, shutdownComplete;
        public int checks;
        public string error, expectation, message, applicationVersion;
    }

    private static string Output()
    {
        var args = Environment.GetCommandLineArgs();
        int at = Array.IndexOf(args, "-idas3-rom-smoke");
        if (at < 0) return null;
        if (at + 1 >= args.Length) throw new ArgumentException("ROM smoke requires a fresh output folder.");
        return Path.GetFullPath(args[at + 1]);
    }

    internal static bool Configure(ref string saves)
    {
        string output = Output();
        if (output == null) return false;
        saves = Path.Combine(output, "userdata");
        return true;
    }

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void Bootstrap()
    {
        string output = Output();
        if (output == null) return;
        if (Directory.Exists(output) || File.Exists(output)) throw new IOException("Use a fresh ROM smoke output folder.");
        Directory.CreateDirectory(output);
        var probe = new GameObject("ROM startup observation").AddComponent<Idas3RomGateSmoke>();
        probe.root = output;
        var args = Environment.GetCommandLineArgs();
        int at = Array.IndexOf(args, "-idas3-rom-expect");
        probe.expectation = at >= 0 && at + 1 < args.Length ? args[at + 1] : "blocked";
        probe.began = Time.realtimeSinceStartupAsDouble;
        Screen.SetResolution(1280, 720, FullScreenMode.Windowed);
        AudioListener.volume = 0;
        probe.StartCoroutine(probe.Observe());
    }

    private bool HostStarted()
    {
        var scene = FindAnyObjectByType<Idas3SceneGame>();
        var legacy = FindAnyObjectByType<Idas3Game>();
        var viewer = FindAnyObjectByType<Idas3ReplayViewer>();
        return scene != null && scene.Ready || legacy != null && legacy.BootInitialized || viewer != null && viewer.BootInitialized;
    }

    private void Check(bool condition, string error)
    { ++checks; if (!condition) throw new InvalidOperationException(error); }

    private IEnumerator Observe()
    {
        while (Idas3RomGate.Instance == null || Idas3RomGate.Instance.Checking) yield return null;
        yield return new WaitForSecondsRealtime(.5f);
        try
        {
            if (expectation == "ready") Check(Idas3RomGate.Verified, "Valid ROM did not unlock startup.");
            else
            {
                Check(!Idas3RomGate.Verified, "Missing/invalid ROM unlocked startup.");
                Check(!HostStarted(), "A host initialized before ROM verification.");
                Check(Idas3Native.ReadStatus().state == 0, "Native game state changed while ROM was blocked.");
                Check(!Directory.Exists(Path.Combine(root, "userdata")), "Save setup ran while ROM was blocked.");
                Check(Idas3Updates.Instance == null || !Idas3Updates.Instance.WindowVisible, "Updater appeared ahead of the ROM gate.");
                Check(File.Exists(Path.Combine(Idas3RomGate.Instance.RomFolder, "README.txt")), "ROM instructions were not created.");
            }
        }
        catch (Exception error) { Finish(false, error.ToString()); yield break; }
        if (expectation != "ready")
        {
            // ScreenCapture can omit IMGUI on this Unity/D3D11 path. Capture
            // the actual OnGUI Repaint, as the pause-menu diagnostic does.
            var target = new RenderTexture(Screen.width, Screen.height, 24, RenderTextureFormat.ARGB32) { antiAliasing = 1 };
            target.Create(); Idas3RomGate.Instance.RequestDiagnosticCapture(target);
            while (!Idas3RomGate.Instance.DiagnosticCaptureReady) yield return null;
            yield return new WaitForEndOfFrame();
            var previous = RenderTexture.active; RenderTexture.active = target;
            var image = new Texture2D(target.width, target.height, TextureFormat.RGB24, false);
            image.ReadPixels(new Rect(0, 0, target.width, target.height), 0, 0); image.Apply(); RenderTexture.active = previous;
            File.WriteAllBytes(Path.Combine(root, "startup.png"), image.EncodeToPNG());
            int visible = 0; foreach (var pixel in image.GetPixels32()) if (Math.Max(pixel.r, Math.Max(pixel.g, pixel.b)) > 40) ++visible;
            Destroy(image); target.Release(); Destroy(target);
            try { Check(visible > 10000, "The actual ROM gate repaint was blank."); }
            catch (Exception error) { Finish(false, error.ToString()); yield break; }
        }
        if (expectation == "retry")
        {
            File.WriteAllText(Path.Combine(root, "waiting-for-rom.txt"), "Place the actual ROM in the game's fixed rom folder.\n");
            while (!File.Exists(Path.Combine(Idas3RomGate.Instance.RomFolder, "gds-0033.chd"))) yield return null;
            Idas3RomGate.Instance.CheckAgain();
            while (Idas3RomGate.Instance.Checking) yield return null;
        }
        if (expectation == "ready" || expectation == "retry")
        {
            while (!HostStarted()) yield return null;
            try
            {
                Check(Idas3RomGate.Verified, "Retry did not verify the original ROM.");
                Check(HostStarted(), "Verified ROM did not start a host.");
            }
            catch (Exception error) { Finish(false, error.ToString()); yield break; }
            yield return new WaitForEndOfFrame();
            var capture = ScreenCapture.CaptureScreenshotAsTexture();
            File.WriteAllBytes(Path.Combine(root, "verified.png"), capture.EncodeToPNG()); Destroy(capture);
        }
        Finish(true, null);
    }

    private void Update()
    {
        if (!finished && Time.realtimeSinceStartupAsDouble - began > 75) Finish(false, "ROM startup observation timed out.");
    }

    private void Finish(bool passed, string error)
    {
        if (finished) return;
        finished = true;
        bool started = HostStarted();
        var scene = FindAnyObjectByType<Idas3SceneGame>(); scene?.StopNative();
        var legacy = FindAnyObjectByType<Idas3Game>(); legacy?.StopNative();
        var viewer = FindAnyObjectByType<Idas3ReplayViewer>(); if (viewer != null) DestroyImmediate(viewer.gameObject);
        var report = new Report { passed = passed, error = error, expectation = expectation, checks = checks,
            verified = Idas3RomGate.Verified, nativeInitialized = started, shutdownComplete = !HostStarted(),
            applicationVersion = Application.version, message = Idas3RomGate.Instance?.Message };
        File.WriteAllText(Path.Combine(root, "report.json"), JsonUtility.ToJson(report, true));
        Application.Quit(passed ? 0 : 1);
    }
}
