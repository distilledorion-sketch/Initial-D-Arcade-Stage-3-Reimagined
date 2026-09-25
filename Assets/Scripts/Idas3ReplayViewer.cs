using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.InputSystem;

[DefaultExecutionOrder(-110)]
public sealed class Idas3ReplayViewer : MonoBehaviour
{
    public static bool Requested => Array.IndexOf(Environment.GetCommandLineArgs(), "-idas3-replay-viewer") >= 0;
    public static Idas3ReplayViewer Instance { get; private set; }
    internal Idas3Native.Status Status { get; private set; }
    public Camera View { get; private set; }
    internal bool BootInitialized => startupInitialized;
    internal Idas3ReplayData.Details PresenceMetadata => browsing ? null : replay?.Metadata;
    internal bool PresenceEnabled => audioOptions.discordPresence;
    Idas3SceneRenderer scene;
    Idas3UnityUi ui;
    Idas3ReplayData replay;
    Idas3UnityAudio audioOutput;
    Idas3GameOptions.Values audioOptions=new Idas3GameOptions.Values();
    double audioSeconds=double.NaN;
    bool audioWasPlaying;
    bool initialized, playing, startupInitialized;
    double seconds, lastUpdateAt;
    uint hudTimingRevision;
    internal bool TryGetHudTiming(out double position,out uint revision){
        position=seconds;revision=hudTimingRevision;return initialized&&replay!=null;
    }
    void Seek(double position){
        seconds=Math.Max(0,Math.Min(replay.Duration,position));
        unchecked{++hudTimingRevision;}
    }
    float rate = 1, orbit;
    int cameraMode;
    string message, filename;
    Task<string> picker;
    bool resumeAfterPicker;
    bool controlsVisible = true;
    readonly Idas3MenuPointer menuPointer=new Idas3MenuPointer();
    bool PointerOwnsControls()
    {
        var key=Keyboard.current;var pad=Gamepad.current;
        bool held=key?.anyKey.isPressed==true||pad?.buttonSouth.isPressed==true||pad?.buttonEast.isPressed==true||
            pad?.buttonNorth.isPressed==true||pad?.buttonWest.isPressed==true||pad?.startButton.isPressed==true||
            pad?.selectButton.isPressed==true||(pad?.dpad.ReadValue().sqrMagnitude??0)>0||
            Mathf.Abs(pad?.rightStick.x.ReadValue()??0)>.2f;
        return menuPointer.BlockNavigation(Idas3MenuPointer.Active,held);
    }
    readonly byte[] detailBytes = new byte[132];
    readonly byte[] opponentFrameBytes=new byte[160];
    bool opponentPov,browsing;
    string libraryFolder;
    string[] libraryFiles=Array.Empty<string>();
    int librarySelection;
    Idas3ReplayData Viewed=>opponentPov?replay.Opponent:replay;
    bool CanSwitchPov=>replay?.Metadata.mode==1&&replay.Opponent!=null&&replay.Metadata.opponentTelemetry==1;
    static readonly string[] Cameras = { "Chase", "Bumper", "Overhead", "Orbit" };
    string proofDirectory;
    int proofFrame;
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3SceneInitialize([MarshalAs(UnmanagedType.LPUTF8Str)] string assets, [MarshalAs(UnmanagedType.LPUTF8Str)] string saves, int width, int height);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3SceneShutdown();
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3ReplayStart(int condition, int weather, int night, int car, int manual);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3ReplayHud(int elapsed6000, int finish6000, int[] splits, int count, int[] glyphs, int glyphCount);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3ReplayAppearance(uint[] values, int count);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3ReplayDetailFrame(byte[] values, int count);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3ReplayOpponentStart(int car,int enemy,uint[] values,int count);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3ReplayOpponentFrame(byte[] values,int count);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3SceneSetPreRaceNames([MarshalAs(UnmanagedType.LPUTF8Str)]string player,[MarshalAs(UnmanagedType.LPUTF8Str)]string opponent);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3ReplayPose(double tick, float x, float y, float z, float yaw, float speed, int gear, float pitch, int cameraMode, float orbit, int width, int height);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)] static extern int Idas3ReplayAudio(double deltaSeconds,int playing,int reset,float master,float engine,float effects);

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Bootstrap()
    {
        if (!Requested || Instance != null) return;
        new GameObject("Replay viewer").AddComponent<Idas3ReplayViewer>();
    }
    void Awake()
    {
        Instance = this;
        if (Idas3RomGate.Verified && Idas3Updates.StartupFinished) InitializeViewer();
    }
    void InitializeViewer()
    {
        if (!Idas3RomGate.Verified || !Idas3Updates.StartupFinished || startupInitialized) return;
        startupInitialized = true;
        foreach (var camera in FindObjectsByType<Camera>(FindObjectsSortMode.None)) camera.enabled = false;
        View = gameObject.AddComponent<Camera>(); View.clearFlags = CameraClearFlags.SolidColor; View.backgroundColor = Color.black;
        scene = gameObject.AddComponent<Idas3SceneRenderer>(); scene.Initialize(View);
        ui = gameObject.AddComponent<Idas3UnityUi>(); ui.Initialize(View);
        Application.runInBackground = true; Application.targetFrameRate = 60; QualitySettings.vSyncCount = 1;
        try{
            string optionsFile=Path.Combine(Application.persistentDataPath,"userdata-unity-scene","game-options.json");
            if(File.Exists(optionsFile)){JsonUtility.FromJsonOverwrite(File.ReadAllText(optionsFile),audioOptions);audioOptions=Idas3GameOptions.Normalize(audioOptions);}
        }catch(Exception e){Debug.LogWarning("Replay audio uses default volume: "+e.Message);}
        var args = Environment.GetCommandLineArgs();
        int index = Array.IndexOf(args, "-idas3-replay-viewer");
        int library=Array.IndexOf(args,"-idas3-replay-library");
        libraryFolder=library>=0&&library+1<args.Length?Path.GetFullPath(args[library+1]):Idas3ReplayLibrary.DefaultFolder;
        int proof = Array.IndexOf(args, "-idas3-replay-proof");
        if (proof >= 0 && proof + 1 < args.Length) proofDirectory = args[proof + 1];
        if (index + 1 < args.Length && !args[index + 1].StartsWith("-")) Open(args[index + 1]);
        else Browse();
        int audioProof=Array.IndexOf(args,"-idas3-replay-audio-proof");
        if(audioProof>=0&&audioProof+1<args.Length)StartCoroutine(AudioProof(args[audioProof+1]));
    }
    void Open(string path)
    {
        if (!Idas3RomGate.Verified || !startupInitialized) return;
        SilenceAudio();
        playing = false;
        try
        {
            var loaded = Idas3ReplayData.Load(path); // Validate before replacing the current replay.
            if (!initialized)
            {
                string assets = Application.isEditor ? Path.GetFullPath(Path.Combine(Application.dataPath, "../Native")) : Path.Combine(Application.streamingAssetsPath, "IDAS3");
                if (!Application.isEditor && Array.IndexOf(Environment.GetCommandLineArgs(), "-idas3-enna-test") >= 0)
                    assets = File.ReadAllText(Path.Combine(Application.streamingAssetsPath, "d3-assets.txt")).Trim();
                string storage = Path.Combine(Application.temporaryCachePath, "replay-viewer-session");
                if (Idas3SceneInitialize(assets, storage, Screen.width, Screen.height) != 1) throw new InvalidOperationException(Idas3Native.Error());
                initialized = true;
                if(Idas3Native.Idas3SceneSetMapZoom(audioOptions.minimapZoom)!=1)throw new InvalidOperationException(Idas3Native.Error());
                if(Idas3Native.Idas3SceneSetMapSize(audioOptions.minimapSize)!=1)throw new InvalidOperationException(Idas3Native.Error());
                audioOutput=gameObject.AddComponent<Idas3UnityAudio>();audioOutput.Initialize();
                foreach (var pack in Idas3CourseCatalog.Packs)
                {
                    string root = Path.Combine(Application.streamingAssetsPath, pack);
                    if (File.Exists(Path.Combine(root, "menu.idastex")) && Idas3Native.Idas3SceneRegisterImportedCourse(root) != 1) throw new InvalidOperationException(Idas3Native.Error());
                }
                new GameObject("Replay imported course").AddComponent<Idas8HakoneCourse>();
                if(FindAnyObjectByType<IdasSpecialStageEnnaCourse>()==null)
                    new GameObject("Replay Enna Skyline").AddComponent<IdasSpecialStageEnnaCourse>();
            }
            replay = loaded;opponentPov=false;ConfigurePerspective();seconds = 0; filename = Path.GetFileName(path); message = null; playing = proofDirectory == null;browsing=false;
            Present();
            lastUpdateAt = Time.realtimeSinceStartupAsDouble;
        }
        catch (Exception e) { message = "Cannot open replay: " + e.Message; Debug.LogError(message); }
    }
    void ConfigurePerspective()
    {
        SilenceAudio();audioSeconds=double.NaN;
        unchecked{++hudTimingRevision;}
        var chosen=Viewed;var m=chosen.Metadata;
        if(Idas3ReplayStart(m.condition,m.weather,m.night,m.car,m.manual)!=1)throw new InvalidOperationException(Idas3Native.Error());
        if(chosen.Detailed&&Idas3ReplayAppearance(chosen.Appearance,chosen.Appearance.Length)!=1)throw new InvalidOperationException(Idas3Native.Error());
        if(replay.Opponent!=null){
            var other=opponentPov?replay:replay.Opponent;
            int enemy=opponentPov?-1:replay.Metadata.opponentEnemy;
            if(Idas3ReplayOpponentStart(other.Metadata.car,enemy,other.Appearance,other.Appearance.Length)!=1)throw new InvalidOperationException(Idas3Native.Error());
            if(replay.Metadata.mode==1){
                string local=replay.Metadata.playerName??"PLAYER",remote=replay.Metadata.opponentName??"OPPONENT";
                if(Idas3SceneSetPreRaceNames(opponentPov?remote:local,opponentPov?local:remote)!=1)throw new InvalidOperationException(Idas3Native.Error());
            }
        }
    }
    void SwitchPov()
    {
        if(!CanSwitchPov)return;
        opponentPov=!opponentPov;
        try { ConfigurePerspective();Present();lastUpdateAt=Time.realtimeSinceStartupAsDouble; }
        catch(Exception e){playing=false;message="Could not switch driver: "+e.Message;}
    }
    static void FillFrame(Idas3ReplayData.Pose p,byte[] bytes)
    {
        BitConverter.GetBytes(p.tick).CopyTo(bytes,0);BitConverter.GetBytes(p.position.x).CopyTo(bytes,4);BitConverter.GetBytes(p.position.y).CopyTo(bytes,8);BitConverter.GetBytes(p.position.z).CopyTo(bytes,12);
        BitConverter.GetBytes(p.yaw).CopyTo(bytes,16);BitConverter.GetBytes(p.speed).CopyTo(bytes,20);BitConverter.GetBytes(p.gear).CopyTo(bytes,24);Buffer.BlockCopy(p.state,0,bytes,28,132);
    }
    void Browse()
    {
        SilenceAudio();
        playing=false;browsing=true;controlsVisible=true;
        try { libraryFiles=Idas3ReplayLibrary.Files(libraryFolder);librarySelection=Mathf.Clamp(librarySelection,0,Math.Max(0,libraryFiles.Length-1));message=null; }
        catch(Exception e){libraryFiles=Array.Empty<string>();message="Could not read replay library: "+e.Message;}
    }
    void BrowseInput()
    {
        if(picker!=null||PointerOwnsControls())return;
        var key=Keyboard.current;var pad=Gamepad.current;
        if(key?.escapeKey.wasPressedThisFrame==true||pad?.buttonEast.wasPressedThisFrame==true){if(replay!=null)browsing=false;return;}
        if(libraryFiles.Length==0)return;
        if(key?.upArrowKey.wasPressedThisFrame==true||pad?.dpad.up.wasPressedThisFrame==true)librarySelection=(librarySelection+libraryFiles.Length-1)%libraryFiles.Length;
        if(key?.downArrowKey.wasPressedThisFrame==true||pad?.dpad.down.wasPressedThisFrame==true)librarySelection=(librarySelection+1)%libraryFiles.Length;
        if(key?.enterKey.wasPressedThisFrame==true||pad?.buttonSouth.wasPressedThisFrame==true||pad?.startButton.wasPressedThisFrame==true)Open(libraryFiles[librarySelection]);
    }
    void LibraryView(float width,float height)
    {
        GUI.enabled=picker==null;float w=Mathf.Min(900,width-24),h=Mathf.Min(560,height-24);
        GUILayout.BeginArea(new Rect((width-w)/2,(height-h)/2,w,h),GUI.skin.box);
        GUILayout.BeginHorizontal();GUILayout.Label("REPLAYS — LOCAL LIBRARY");
        if(GUILayout.Button("Open file…",GUILayout.Width(110)))RequestFile();
        if(GUILayout.Button("Refresh",GUILayout.Width(90)))Browse();
        if(GUILayout.Button(replay==null?"Close viewer":"Back",GUILayout.Width(110))){if(replay==null)Application.Quit();else browsing=false;}
        GUILayout.EndHorizontal();GUILayout.Space(12);
        GUILayout.Label("Personal recordings stay on this computer. Only submitted Time Attack replays are uploaded.");
        if(libraryFiles.Length==0)GUILayout.Label("No saved replays yet. Enable recording in Options > Replays, then finish a race.");
        int first=librarySelection/8*8;
        for(int i=first;i<Math.Min(first+8,libraryFiles.Length);i++){
            GUI.color=i==librarySelection?Color.yellow:Color.white;
            if(GUILayout.Button(Path.GetFileNameWithoutExtension(libraryFiles[i]),GUILayout.Height(40))){librarySelection=i;Open(libraryFiles[i]);}
        }
        GUI.color=Color.white;GUILayout.Space(10);GUILayout.BeginHorizontal();
        if(GUILayout.Button("Previous",GUILayout.Width(100))&&first>0)librarySelection=Math.Max(0,first-8);
        GUILayout.Label(libraryFiles.Length+" recordings · ↑ ↓ / D-pad to select · Enter / A to play · Esc / B to return");
        if(GUILayout.Button("Next",GUILayout.Width(100))&&first+8<libraryFiles.Length)librarySelection=first+8;
        GUILayout.EndHorizontal();if(message!=null)GUILayout.Label(message);GUILayout.EndArea();GUI.enabled=true;
    }
    void RequestFile()
    {
        SilenceAudio();
        resumeAfterPicker=playing;playing=false;
        var result=new TaskCompletionSource<string>();picker=result.Task;
        var thread=new Thread(()=>{try{result.SetResult(ChooseFile(libraryFolder));}catch(Exception e){result.SetException(e);}});
        thread.IsBackground=true;thread.SetApartmentState(ApartmentState.STA);thread.Start();
    }
    void Present()
    {
        var p = Viewed.Sample(seconds);
        float pitch = 0;
        var m = Viewed.Metadata;
        int elapsed = Math.Min(m.ticks6000, (int)Math.Round(seconds * 6000));
        if (replay.Detailed)
        {
            Buffer.BlockCopy(p.state, 0, detailBytes, 0, detailBytes.Length);
            if (Idas3ReplayDetailFrame(detailBytes, detailBytes.Length) != 1) throw new InvalidOperationException(Idas3Native.Error());
        }
        else if (Idas3ReplayHud(elapsed, m.ticks6000, m.splits, m.splits?.Length ?? 0, m.nameGlyphs, m.nameGlyphs?.Length ?? 0) != 1) throw new InvalidOperationException(Idas3Native.Error());
        if(replay.Opponent!=null){
            var other=(opponentPov?replay:replay.Opponent).Sample(seconds);FillFrame(other,opponentFrameBytes);
            // Opponent streams store the original local signed battle gap.
            float advantage=Idas3ReplayData.Scalar(replay.Opponent.Sample(seconds).state[31]);
            BitConverter.GetBytes(opponentPov?-advantage:advantage).CopyTo(opponentFrameBytes,152);
            if(Idas3ReplayOpponentFrame(opponentFrameBytes,160)!=1)throw new InvalidOperationException(Idas3Native.Error());
        }
        if (Idas3ReplayPose(p.tick, p.position.x, p.position.y, p.position.z, p.yaw, p.speed, p.gear, pitch, cameraMode, orbit, Screen.width, Screen.height) != 1) throw new InvalidOperationException(Idas3Native.Error());
        Status = Idas3Native.ReadStatus(); scene.HudOptions=audioOptions; scene.ApplyFrame(); ui.ApplyFrame();
    }
    void Update()
    {
        if (!startupInitialized)
        {
            if (Idas3RomGate.Verified && Idas3Updates.StartupFinished) InitializeViewer();
            return;
        }
        if (picker != null && picker.IsCompleted)
        {
            try { string path = picker.GetAwaiter().GetResult(); if (path != null) Open(path); else playing = resumeAfterPicker; }
            catch (Exception e) { message = "Cannot open file picker: " + e.Message; }
            picker = null; lastUpdateAt = Time.realtimeSinceStartupAsDouble;
        }
        if(browsing){BrowseInput();lastUpdateAt=Time.realtimeSinceStartupAsDouble;return;}
        if (replay == null) return;
        try
        {
            double now = Time.realtimeSinceStartupAsDouble;
            float delta = (float)Math.Max(0, Math.Min(.25, now - lastUpdateAt)); lastUpdateAt = now;
            var keyboard = Keyboard.current; var pad = Gamepad.current;
            if(!PointerOwnsControls()){
            if(keyboard?.tabKey.wasPressedThisFrame==true||pad?.buttonWest.wasPressedThisFrame==true)SwitchPov();
            if(keyboard?.escapeKey.wasPressedThisFrame==true||pad?.buttonEast.wasPressedThisFrame==true){Browse();return;}
            if (keyboard?.hKey.wasPressedThisFrame == true || pad?.selectButton.wasPressedThisFrame == true) controlsVisible = !controlsVisible;
            if (picker == null && (keyboard?.spaceKey.wasPressedThisFrame == true || pad?.buttonSouth.wasPressedThisFrame == true || pad?.startButton.wasPressedThisFrame == true)) TogglePlay();
            if (keyboard?.cKey.wasPressedThisFrame == true || pad?.buttonNorth.wasPressedThisFrame == true) cameraMode = (cameraMode + 1) % 4;
            if (keyboard?.leftArrowKey.wasPressedThisFrame == true || pad?.dpad.left.wasPressedThisFrame == true) Seek(seconds - 5);
            if (keyboard?.rightArrowKey.wasPressedThisFrame == true || pad?.dpad.right.wasPressedThisFrame == true) Seek(seconds + 5);
            if (keyboard?.homeKey.wasPressedThisFrame == true) Seek(0);
            if (keyboard?.endKey.wasPressedThisFrame == true) { Seek(replay.Duration); playing = false; }
            orbit += (pad?.rightStick.x.ReadValue() ?? 0) * delta;
            if (keyboard?.qKey.isPressed == true) orbit -= delta;
            if (keyboard?.eKey.isPressed == true) orbit += delta;
            }
            if (playing) { seconds = Math.Min(replay.Duration, seconds + delta * rate); if (seconds >= replay.Duration) playing = false; }
            Present();
            UpdateReplayAudio(delta);
            if (proofDirectory != null) Proof();
        }
        catch (Exception e) { playing = false; SilenceAudio(); message = e.Message; Debug.LogError(e); enabled = false; }
    }
    void SilenceAudio(){
        if(initialized&&replay!=null)Idas3ReplayAudio(0,0,0,audioOptions.masterVolume,audioOptions.engineVolume,audioOptions.effectsVolume);
        audioWasPlaying=false;
    }
    void UpdateReplayAudio(double delta){
        bool audible=playing&&!browsing&&picker==null&&Viewed.Detailed;
        bool reset=!audioWasPlaying||double.IsNaN(audioSeconds)||Math.Abs(seconds-audioSeconds-delta*rate)>.1;
        if(Idas3Native.Idas3SceneSetTireVolume(audioOptions.tireVolume)!=1)throw new InvalidOperationException("Could not apply replay tire volume.");
        if(Idas3ReplayAudio(delta,audible?1:0,audible&&reset?1:0,audioOptions.masterVolume,audioOptions.engineVolume,audioOptions.effectsVolume)!=1)
            throw new InvalidOperationException(Idas3Native.Error());
        audioWasPlaying=audible;audioSeconds=seconds;
    }
    void TogglePlay() { if (!playing && seconds >= replay.Duration) Seek(0); playing = !playing;if(!playing)SilenceAudio(); }
    static string Clock(double value) => string.Format("{0}:{1:00.000}", (int)value / 60, value % 60);
    void OnGUI()
    {
        if (!startupInitialized || !Idas3RomGate.Verified) return;
        // Update already routes keyboard/gamepad actions. Do not let IMGUI
        // also submit whichever button last received keyboard focus.
        if(Event.current.type==EventType.KeyDown||Event.current.type==EventType.KeyUp)Event.current.Use();
        float scale = Mathf.Max(.6f, Mathf.Min(Screen.width / 1280f, Screen.height / 720f));
        GUI.matrix = Matrix4x4.Scale(new Vector3(scale, scale, 1));
        float width = Screen.width / scale, height = Screen.height / scale;
        if(browsing){LibraryView(width,height);return;}
        if (replay != null && !controlsVisible) return;
        float panelWidth = Mathf.Min(630, width - 24), panelX = (width - panelWidth) / 2;
        GUILayout.BeginArea(new Rect(panelX, 12, panelWidth, replay == null ? 135 : 80), GUI.skin.box);
        GUILayout.BeginHorizontal(); GUILayout.Label("REPLAY VIEWER", GUILayout.Width(125));
        GUI.enabled = picker == null;
        if (GUILayout.Button("Open replay…", GUILayout.Width(145)))
        {
            RequestFile();
        }
        if (GUILayout.Button("Library", GUILayout.Width(60))) Browse();
        GUILayout.FlexibleSpace(); if (GUILayout.Button("Close viewer", GUILayout.Width(120))) Application.Quit(); GUILayout.EndHorizontal();
        GUI.enabled = true;
        if (replay != null) { var m = replay.Metadata; GUILayout.Label(Idas3ReplayData.Courses[m.condition / 2] + "  ·  " + (m.condition % 2 == 0 ? "Forward" : "Reverse") + "  ·  " + (m.night == 1 ? "Night" : "Day") + " / " + (m.weather == 1 ? "Wet" : "Dry") + "  ·  H / Select: hide controls"); }
        if (message != null) GUILayout.Label(message);
        GUILayout.EndArea();
        if (replay == null) return;
        GUI.enabled = picker == null;
        GUILayout.BeginArea(new Rect(panelX, height - 125, panelWidth, 113), GUI.skin.box);
        GUILayout.BeginHorizontal();
        if (GUILayout.Button(playing ? "Pause" : "Play", GUILayout.Width(75))) TogglePlay();
        if (GUILayout.Button("|<", GUILayout.Width(40))) Seek(0);
        if (GUILayout.Button("−5s", GUILayout.Width(50))) Seek(seconds - 5);
        if (GUILayout.Button("+5s", GUILayout.Width(50))) Seek(seconds + 5);
        GUILayout.Label(Clock(seconds) + " / " + Clock(replay.Duration), GUILayout.Width(175));
        GUILayout.FlexibleSpace(); if (GUILayout.Button("Camera: " + Cameras[cameraMode], GUILayout.Width(150))) cameraMode = (cameraMode + 1) % 4;
        GUILayout.EndHorizontal();
        float slider=GUILayout.HorizontalSlider((float)seconds,0,(float)replay.Duration);
        // A repaint returns the same float even while the playback clock keeps
        // double precision. Only user movement is a seek, not float rounding.
        if(slider!=(float)seconds)Seek(slider);
        GUILayout.BeginHorizontal();
        foreach (float speed in new[] { .25f, .5f, 1f, 2f, 4f }) { GUI.color = rate == speed ? Color.yellow : Color.white; if (GUILayout.Button(speed + "×", GUILayout.Width(45))) rate = speed; } GUI.color = Color.white;
        GUILayout.Label(CanSwitchPov?"Space/A: pause   C/Y: camera":"Space/A: pause   C/Y: camera   Q/E: orbit");
        if(CanSwitchPov&&GUILayout.Button(opponentPov?"POV: OPPONENT":"POV: YOU",GUILayout.Width(130)))SwitchPov();
        GUILayout.EndHorizontal();
        GUILayout.Label(CanSwitchPov?"Tab / X: switch driver   ·   Esc / B: library   ·   Personal recording stays local":replay.Detailed ? "60 Hz capture · recorded RPM, speed, body, wheels, clocks and car appearance" : "LEGACY: RPM, body rotation, wheel animation and tuning were not recorded");
        GUILayout.EndArea();
        GUI.enabled = true;
    }
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)] sealed class OpenFileName
    {
        public int size; public IntPtr owner, instance; public string filter = "Initial D replay\0*.idreplay\0\0";
        public IntPtr customFilter; public int maxCustomFilter, filterIndex = 1; public IntPtr file; public int maxFile = 32768;
        public IntPtr fileTitle; public int maxFileTitle; public string initialDir; public string title = "Open a downloaded replay"; public int flags = 0x00080000 | 0x00001000 | 0x00000800 | 0x00000008;
        public short fileOffset, extension; public string defaultExtension = "idreplay"; public IntPtr customData, hook; public string template; public IntPtr reserved; public int reserved2, flagsEx;
    }
    [DllImport("comdlg32.dll", CharSet = CharSet.Unicode)] static extern bool GetOpenFileNameW([In, Out] OpenFileName data);
    [DllImport("comdlg32.dll")] static extern uint CommDlgExtendedError();
    static string ChooseFile(string initialDirectory)
    {
        var dialog = new OpenFileName{initialDir=Directory.Exists(initialDirectory)?initialDirectory:null};
        dialog.size = Marshal.SizeOf(typeof(OpenFileName));
        dialog.file = Marshal.StringToHGlobalUni(new string('\0', dialog.maxFile));
        try
        {
            if (GetOpenFileNameW(dialog)) return Marshal.PtrToStringUni(dialog.file);
            uint error = CommDlgExtendedError();
            if (error != 0) Debug.LogError("Replay file picker failed: " + error);
            return null;
        }
        finally { Marshal.FreeHGlobal(dialog.file); }
    }
    void Proof()
    {
        proofFrame++;
        bool courseSweep=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-replay-course-sweep")>=0;
        bool railSweep=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-replay-rail-sweep")>=0;
        if (proofFrame % 40 == 1) {
            int sample=proofFrame/40;
            Seek(replay.Duration * (courseSweep ? Math.Min(1,sample/4*.2) : Math.Min(1,sample*.5)));
            cameraMode = courseSweep ? sample%4 : proofFrame >= 120 ? 3 : 0;
            if(railSweep){Seek(.75+sample*.03);cameraMode=1;}
            if(!courseSweep&&CanSwitchPov&&proofFrame==81)SwitchPov();
        }
        if (proofFrame % 40 != 30) return;
        Directory.CreateDirectory(proofDirectory);
        var target = new RenderTexture(1280, 720, 24); var previous = View.targetTexture; var active = RenderTexture.active;
        View.targetTexture = target; View.Render(); ui.RenderOverlayForCapture(); RenderTexture.active = target;
        var texture = new Texture2D(1280, 720, TextureFormat.RGB24, false); texture.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); texture.Apply();
        File.WriteAllBytes(Path.Combine(proofDirectory, "replay-" + proofFrame + ".png"), texture.EncodeToPNG());
        ScreenCapture.CaptureScreenshot(Path.Combine(proofDirectory, "viewer-" + proofFrame + ".png"));
        View.targetTexture = previous; RenderTexture.active = active; Destroy(texture); target.Release(); Destroy(target);
        File.AppendAllText(Path.Combine(proofDirectory, "playback.txt"), "seconds=" + seconds + " camera=" + cameraMode + " opponentPov="+opponentPov+" meshes=" + scene.ActiveMeshCount + " position=" + Viewed.Sample(seconds).position + "\n");
        if (proofFrame >= (railSweep?470:courseSweep?950:150)) Application.Quit();
    }
    // Opt-in verification in a separate viewer; never opens player save files.
    System.Collections.IEnumerator AudioProof(string folder){
        var checks=new System.Collections.Generic.List<string>();bool passed=true;
        void Check(bool ok,string label){passed&=ok;checks.Add((ok?"PASS ":"FAIL ")+label);}
        yield return new WaitForSecondsRealtime(2);
        Check(initialized&&audioOutput!=null&&audioOutput.IsOutputRunning,"Unity audio output running");
        if(!initialized||audioOutput==null){Directory.CreateDirectory(folder);File.WriteAllText(Path.Combine(folder,"audio-proof.txt"),string.Join("\n",checks));Application.Quit(1);yield break;}
        var callbacks=audioOutput.ReadCallbackStatistics();var running=Idas3UnityAudio.ReadStatistics();
        Check(callbacks.returnedFrames>0&&running.producedFrames>0&&running.consumedFrames>0,"Unity DSP consumed replay PCM");
        playing=false;SilenceAudio();yield return new WaitForSecondsRealtime(.2f);
        var paused=Idas3UnityAudio.ReadStatistics();yield return new WaitForSecondsRealtime(.3f);
        Check(Idas3UnityAudio.ReadStatistics().consumedFrames==paused.consumedFrames,"Pause stops audio consumption");
        Seek(replay.Duration*.6);playing=true;yield return new WaitForSecondsRealtime(1);
        Check(Idas3UnityAudio.ReadStatistics().consumedFrames>paused.consumedFrames,"Seek and resume restart audio");
        if(CanSwitchPov){SwitchPov();var before=Idas3UnityAudio.ReadStatistics();yield return new WaitForSecondsRealtime(1);Check(opponentPov&&Idas3UnityAudio.ReadStatistics().consumedFrames>before.consumedFrames,"Opponent POV audio plays");}
        Browse();yield return new WaitForSecondsRealtime(.2f);var library=Idas3UnityAudio.ReadStatistics();yield return new WaitForSecondsRealtime(.3f);
        Check(Idas3UnityAudio.ReadStatistics().consumedFrames==library.consumedFrames,"Library stops audio");
        Directory.CreateDirectory(folder);File.WriteAllText(Path.Combine(folder,"audio-proof.txt"),string.Join("\n",checks));
        File.WriteAllText(Path.Combine(folder,"audio-callbacks.json"),JsonUtility.ToJson(callbacks,true));
        File.WriteAllText(Path.Combine(folder,"audio-output.json"),JsonUtility.ToJson(running,true));
        Application.Quit(passed?0:1);
    }
    void OnDisable(){SilenceAudio();}
    void OnDestroy() { if(audioOutput!=null)audioOutput.StopOutput();if (initialized) Idas3SceneShutdown();initialized=false; if (Instance == this) Instance = null; }
}
