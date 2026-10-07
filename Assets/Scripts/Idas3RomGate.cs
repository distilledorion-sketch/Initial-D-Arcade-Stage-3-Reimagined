using System;
using System.Collections;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.Networking;

[DefaultExecutionOrder(-10000)]
public sealed class Idas3RomGate : MonoBehaviour
{
    public static bool Verified { get; private set; }
    internal static Idas3RomGate Instance { get; private set; }
    internal bool Checking => validation != null || preparingBundledRom;
    internal string Message => message;
    internal string RomFolder => Path.Combine(gameRoot, "rom");
    private string gameRoot, message = "Checking GDS-0033…";
    private Task<Idas3RomValidation.Result> validation;
    private CancellationTokenSource cancellation;
    private volatile float progress;
    private bool bundledRomPrepared, preparingBundledRom;
    private Camera background;
    private int selected;
    private GUIStyle titleStyle, textStyle, pathStyle, buttonStyle;
    private RenderTexture diagnosticTarget;
    internal bool DiagnosticCaptureReady { get; private set; }

    internal void RequestDiagnosticCapture(RenderTexture target)
    {
        if (Verified || target == null || Array.IndexOf(Environment.GetCommandLineArgs(), "-idas3-rom-smoke") < 0)
            throw new InvalidOperationException("ROM capture requires the blocked startup diagnostic.");
        diagnosticTarget = target; DiagnosticCaptureReady = false;
    }

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
    private static void ResetState() { Verified = false; Instance = null; }

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    private static void Bootstrap()
    {
        // Deliberately applies to editor play, diagnostic flags, replay viewing,
        // legacy host and -idas3-skip-update-once as well as ordinary startup.
        Verified = false;
        var go = new GameObject("GDS-0033 startup check");
        DontDestroyOnLoad(go);
        Instance = go.AddComponent<Idas3RomGate>();
        Instance.gameRoot = Idas3PlatformPaths.GameRoot;
        Instance.background = go.AddComponent<Camera>();
        Instance.background.clearFlags = CameraClearFlags.SolidColor;
        Instance.background.backgroundColor = Color.black;
        Instance.background.cullingMask = 0;
        Instance.background.depth = 10000;
        Instance.background.allowHDR = false;
        Instance.background.allowMSAA = false;
        Application.runInBackground = true;
        Instance.CheckAgain();
    }

    internal void CheckAgain()
    {
        if (Verified || Checking) return;
        if (Idas3PlatformPaths.IsAndroid && !bundledRomPrepared)
        {
            if (!preparingBundledRom) StartCoroutine(PrepareBundledRom());
            return;
        }
        try
        {
            Directory.CreateDirectory(RomFolder);
            string readme = Path.Combine(RomFolder, "README.txt");
            if (!File.Exists(readme))
            {
                try { File.WriteAllText(readme, Idas3RomValidation.ReadmeText); }
                catch (Exception error) when (error is IOException || error is UnauthorizedAccessException)
                { Debug.LogWarning("Could not write ROM instructions: " + error.Message); }
            }
            cancellation?.Dispose();
            cancellation = new CancellationTokenSource();
            var token = cancellation.Token;
            progress = 0;
            message = "Checking GDS-0033…";
            validation = Task.Run(() => Idas3RomValidation.Validate(gameRoot, value => progress = value, token), token);
        }
        catch (Exception error)
        {
            message = "Could not access the rom folder. Check the game folder permissions.";
            Debug.LogWarning("GDS-0033 startup check: " + error.Message);
        }
    }

    private IEnumerator PrepareBundledRom()
    {
        preparingBundledRom = true;
        progress = 0;
        message = "Preparing bundled GDS-0033…";
        var preparation = CopyBundledRom();
        try
        {
            // C# forbids yielding inside a try block with a catch clause. Drive
            // the copy iterator here so IO/request failures can still fall back
            // to the normal ROM validator and manual import screen.
            while (true)
            {
                bool more;
                object current;
                try { more = preparation.MoveNext(); current = more ? preparation.Current : null; }
                catch (Exception error)
                {
                    Debug.LogWarning("Bundled Android ROM was not copied: " + error.Message);
                    break;
                }
                if (!more) break;
                yield return current;
            }
        }
        finally
        {
            try { (preparation as IDisposable)?.Dispose(); }
            finally { preparingBundledRom = false; bundledRomPrepared = true; }
        }
        CheckAgain();
    }

    private IEnumerator CopyBundledRom()
    {
        string source = Application.streamingAssetsPath.TrimEnd('/') + "/rom/gds-0033.chd";
        string folder = RomFolder;
        string destination = Path.Combine(folder, "gds-0033.chd");
        string temporary = destination + ".bundled-" + Guid.NewGuid().ToString("N");
        try
        {
            Directory.CreateDirectory(folder);
            // Never delete or overwrite a ROM the user has already imported.
            // Idas3RomValidation remains the authority, including SHA-256.
            if (!File.Exists(destination))
            {
                using (var request = UnityWebRequest.Get(source))
                {
                    request.downloadHandler = new DownloadHandlerFile(temporary, false) { removeFileOnAbort = true };
                    request.timeout = 300;
                    var operation = request.SendWebRequest();
                    while (!operation.isDone)
                    {
                        progress = Mathf.Clamp01(request.downloadProgress);
                        yield return null;
                    }
                    if (request.result == UnityWebRequest.Result.Success && File.Exists(temporary) &&
                        new FileInfo(temporary).Length == Idas3RomValidation.ChdBytes)
                    {
                        // File.Move also refuses to overwrite a concurrent import.
                        File.Move(temporary, destination);
                    }
                    else if (request.result != UnityWebRequest.Result.Success)
                        Debug.LogWarning("Bundled Android ROM could not be read: " + request.error);
                }
            }
        }
        finally
        {
            try { if (File.Exists(temporary)) File.Delete(temporary); } catch (Exception) { }
        }
    }

    private void Update()
    {
        if (Verified) return;
        Cursor.visible = true;
        Cursor.lockState = CursorLockMode.None;
        if (validation != null && validation.IsCompleted)
        {
            try
            {
                var result = validation.GetAwaiter().GetResult();
                Verified = result.Verified;
                message = result.Message;
                Debug.Log("GDS-0033 startup check: " + (Verified ? "verified " + result.Format : "blocked: " + message));
            }
            catch (Exception error) { message = "Could not verify GDS-0033. Try again."; Debug.LogWarning(error.Message); }
            validation = null;
            if (Verified) { background.enabled = false; return; }
        }
        var key = Keyboard.current; var pad = Gamepad.current;
        if (key?.escapeKey.wasPressedThisFrame == true || pad?.buttonEast.wasPressedThisFrame == true) { Quit(); return; }
        if (Checking) return;
        if (key?.leftArrowKey.wasPressedThisFrame == true || pad?.dpad.left.wasPressedThisFrame == true) selected = (selected + 2) % 3;
        if (key?.rightArrowKey.wasPressedThisFrame == true || pad?.dpad.right.wasPressedThisFrame == true) selected = (selected + 1) % 3;
        if (key?.enterKey.wasPressedThisFrame == true || key?.numpadEnterKey.wasPressedThisFrame == true || pad?.buttonSouth.wasPressedThisFrame == true) Activate(selected);
    }

    private void Activate(int action)
    {
        if (action == 0) CheckAgain();
        else if (action == 1)
        {
#if UNITY_ANDROID && !UNITY_EDITOR
            using (var picker = new AndroidJavaClass("com.idas3.unity.Idas3Activity"))
                picker.CallStatic("openRomPicker", gameObject.name);
#else
            Application.OpenURL(new Uri(RomFolder + Path.DirectorySeparatorChar).AbsoluteUri);
#endif
        }
        else Quit();
    }

    // Called by the Android activity after the user selects one CHD or the
    // complete CUE/BIN set. Selection names are normalized before copying and
    // the existing validator remains the authority for the final check.
    public void OnAndroidRomUris(string payload)
    {
#if UNITY_ANDROID && !UNITY_EDITOR
        StartCoroutine(ImportAndroidUris(payload));
#endif
    }

#if UNITY_ANDROID && !UNITY_EDITOR
    private System.Collections.IEnumerator ImportAndroidUris(string payload)
    {
        string folder = RomFolder;
        Directory.CreateDirectory(folder);
        bool copied = false;
        foreach (string row in (payload ?? "").Split(new[]{'\n'}, StringSplitOptions.RemoveEmptyEntries))
        {
            string[] fields = row.Split(new[]{'\t'}, 2);
            if (fields.Length != 2) continue;
            string name = Path.GetFileName(fields[1]).ToLowerInvariant();
            string destinationName = null;
            if (name == "gds-0033.chd" || name == "gds-0033.cue" || name == "gds-0033-track1.bin" ||
                name == "gds-0033-track2.bin" || name == "gds-0033-track3.bin") destinationName = name;
            if (destinationName == null) continue;
            using (var bridge = new AndroidJavaClass("com.idas3.unity.Idas3Activity"))
                copied |= bridge.CallStatic<bool>("copyUriToFile", fields[0], Path.Combine(folder, destinationName));
        }
        if (!copied) { message = "No supported GDS-0033 file was selected."; yield break; }
        CheckAgain();
        yield break;
    }
#endif

    private static void Quit()
    {
#if UNITY_EDITOR
        UnityEditor.EditorApplication.isPlaying = false;
#else
        Application.Quit();
#endif
    }

    private static void Fill(Rect rect, Color color)
    { GUI.color = color; GUI.DrawTexture(rect, Texture2D.whiteTexture); GUI.color = Color.white; }

    private void OnGUI()
    {
        if (Verified) return;
        if (Event.current.type == EventType.KeyDown || Event.current.type == EventType.KeyUp) Event.current.Use();
        var oldMatrix = GUI.matrix; var oldColor = GUI.color;
        GUI.depth = -10000;
        bool diagnostic = diagnosticTarget != null && Event.current.type == EventType.Repaint;
        var previousTarget = RenderTexture.active;
        if (diagnostic) { RenderTexture.active = diagnosticTarget; GL.PushMatrix(); GL.LoadPixelMatrix(0, Screen.width, Screen.height, 0); }
        try { DrawWindow(); }
        finally
        {
            GUI.matrix = oldMatrix; GUI.color = oldColor;
            if (diagnostic) { GL.PopMatrix(); RenderTexture.active = previousTarget; diagnosticTarget = null; DiagnosticCaptureReady = true; }
        }
    }

    private void DrawWindow()
    {
        float scale = Mathf.Min(Screen.width / 1280f, Screen.height / 720f);
        GUI.matrix = Matrix4x4.TRS(new Vector3((Screen.width - 1280 * scale) / 2, (Screen.height - 720 * scale) / 2, 0), Quaternion.identity, Vector3.one * scale);
        if (titleStyle == null)
        {
            titleStyle = new GUIStyle(GUI.skin.label) { fontSize = 36, fontStyle = FontStyle.Bold, normal = { textColor = Color.white } };
            textStyle = new GUIStyle(GUI.skin.label) { fontSize = 22, wordWrap = true, normal = { textColor = Color.white } };
            pathStyle = new GUIStyle(textStyle) { fontSize = 17, normal = { textColor = new Color(.66f, .67f, .71f) } };
            buttonStyle = new GUIStyle(GUI.skin.label) { fontSize = 19, fontStyle = FontStyle.Bold, alignment = TextAnchor.MiddleCenter, normal = { textColor = Color.white } };
        }
        Fill(new Rect(0, 0, 1280, 720), Color.black);
        Fill(new Rect(160, 170, 960, 380), new Color(.04f, .045f, .05f));
        Fill(new Rect(160, 170, 960, 5), new Color(.89f, .08f, .16f));
        GUI.Label(new Rect(195, 194, 880, 54), Checking ? "CHECKING GDS-0033" : "ROM REQUIRED", titleStyle);
        GUI.Label(new Rect(198, 273, 875, 72), message, textStyle);
        GUI.Label(new Rect(198, 351, 875, 62), RomFolder, pathStyle);
        if (Checking)
        {
            Fill(new Rect(198, 439, 875, 8), new Color(.18f, .19f, .21f));
            Fill(new Rect(198, 439, 875 * Mathf.Clamp01(progress), 8), new Color(.89f, .08f, .16f));
            GUI.Label(new Rect(198, 470, 875, 30), "ESC / B  QUIT", pathStyle);
        }
        else
        {
            string[] labels = { "CHECK AGAIN", Idas3PlatformPaths.IsAndroid ? "IMPORT ROM FILES" : "OPEN ROM FOLDER", "QUIT" };
            for (int i = 0; i < labels.Length; ++i)
            {
                var rect = new Rect(198 + i * 295, 445, 280, 56);
                if (Event.current.type == EventType.MouseMove && rect.Contains(Event.current.mousePosition)) selected = i;
                Fill(rect, selected == i ? new Color(.89f, .08f, .16f) : new Color(.11f, .12f, .14f));
                if (GUI.Button(rect, labels[i], buttonStyle)) { selected = i; Activate(i); }
            }
        }
    }

    private void OnDestroy()
    {
        cancellation?.Cancel(); cancellation?.Dispose();
        if (Instance == this) { Verified = false; Instance = null; }
    }
}
