using System;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;
using Idas3.Multiplayer;

// Unity owns drawing, cameras, lights and audio output. The original C++
// simulation and presentation timelines remain the authoritative data model.
[DefaultExecutionOrder(-100)]
[RequireComponent(typeof(Camera))]
public sealed class Idas3SceneGame : MonoBehaviour
{
    // Actual output dimensions, refreshed on every window/target resize.
    public int renderWidth = 1280, renderHeight = 720;
    public bool audioEnabled = true;
    [SerializeField] private string stage;
    [SerializeField] private float speedKmh;
    [SerializeField] private float simulationAndSubmissionMs;
    private bool ready, stopping;
    private string failure;
    private Idas3SceneRenderer scene;
    private Idas3UnityUi ui;
    private Idas3UnityAudio sound;
    private Idas3MultiplayerSession multiplayer;
    private Idas3MultiplayerMenu multiplayerMenu;
    private Idas3ChallengerOverlay challenger;
    private Idas3GameOptions gameOptions;
    private Idas3PauseMenu pauseMenu;
    private Idas3ReplayLibrary replayLibrary;
    private Idas3ControlBindings controlBindings;
    private Idas3ControllerDevices controllerDevices = new Idas3ControllerDevices();
    private Idas3WheelFeedback wheelFeedback;
    internal Idas3ControllerDevices ControllerDevices => controllerDevices;
    internal Idas3ControlBindings ControlBindings => controlBindings;
    internal Idas3Native.FrameInput DiagnosticSubmittedInput { get; private set; }
    private Idas3RaceMusicCatalog raceMusic;
    private Idas3RaceMusicMenu raceMusicMenu;
    internal Idas3CustomRaceMusic CustomMusic {get;private set;}
    private Idas3Native.FrameInput previousMusicInput;
    private readonly Idas3MenuPointer musicPointer=new Idas3MenuPointer();
    private readonly Idas3MenuPointer pausePointer=new Idas3MenuPointer();
    private float musicHoldSeconds;
    private bool musicReleaseBlocked;
    private bool savePointerReleaseBlocked;
    private Vector3 previousSavePointer;
    private bool savePointerTracked;
    private bool saveCarLevelsLoaded, saveCarLevelsWarned;
    private int musicPickerContext, musicNavigationAxis;
    private double nextMusicNavigation;
    private float attractOptionsHoldSeconds;
    private int attractViewTapChild;
    private bool attractOptionsReleaseBlocked;
    internal Idas3RaceMusicCatalog RaceMusic => raceMusic;
    internal Idas3RaceMusicMenu RaceMusicMenu => raceMusicMenu;
    private Idas3Native.FrameInput previousMenuInput;
    private bool diagnosticMode, preservePauseOnClose, suppressPauseControls, pauseOpenReleaseBlocked;
    private bool? diagnosticFocusOverride;
    private int pauseBlockThroughFrame = -1, menuNavigationAxis;
    private double nextMenuNavigation;
    private bool appliedBackgroundMute;
    internal Idas3GameOptions GameOptions => gameOptions;
    internal Idas3PauseMenu PauseMenu => pauseMenu;
    internal Idas3MultiplayerSession MultiplayerSession => multiplayer;
    internal bool? DiagnosticFocusOverride
    {
        get => diagnosticFocusOverride;
        set
        {
            if (!diagnosticMode) throw new InvalidOperationException("Focus injection requires an isolated diagnostic session.");
            diagnosticFocusOverride = value;
        }
    }
    private bool Focused => diagnosticFocusOverride ?? Application.isFocused;
    private bool OnlineRace => multiplayer != null && multiplayer.IsRacing;
    private bool CanOpenPause => ready && (Status.flags & (1u | 32u | 512u | 1024u | 4096u)) == 0 && Status.racePhase < 3;
    private bool AttractScreen => ready && (Status.flags & (1u | 32u | 512u | 1024u | 4096u)) == 1u &&
        Status.frontendStage == 0 && !OnlineRace && !multiplayer.ChallengerPending;
    private bool AttractOptionsAllowed => AttractScreen && !pauseMenu.IsOpen && !multiplayerMenu.IsOpen && !raceMusicMenu.IsOpen;
    private static Idas3SceneGame instance;
    private Camera outputCamera;
    private int windowedWidth = 1280, windowedHeight = 720;
    internal string Failure => failure;
    internal bool Ready => ready;
    internal bool PresenceAllowed => ready&&!stopping&&!diagnosticMode;
    internal bool ReplayViewerOpen => replayLibrary!=null&&replayLibrary.ViewerOpen;
    internal Idas3Native.Status Status { get; private set; }
    internal float SubmissionMilliseconds => simulationAndSubmissionMs;
    internal float NativeStepMilliseconds { get; private set; }
    internal float RendererMilliseconds { get; private set; }
    internal float UiMilliseconds { get; private set; }
    internal float NetworkMilliseconds { get; private set; }
    private bool performanceDiagnostics;

    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Idas3SceneInitialize(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string assets,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string saves, int width, int height);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Idas3SceneStep(ref Idas3Native.FrameInput frame);
    [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Idas3SceneShutdown();

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void Bootstrap()
    {
        if (Idas3ReplayViewer.Requested) return;

        if (Array.IndexOf(Environment.GetCommandLineArgs(), "-idas3-legacy-host") >= 0) return;
        if (FindAnyObjectByType<Idas3SceneGame>() != null) return;
        new GameObject("Initial D — Unity scene").AddComponent<Idas3SceneGame>();
    }

    private void Awake(){if(Idas3Updates.StartupFinished)InitializeGame();}
    private void InitializeGame()
    {
        if (Idas3ReplayViewer.Requested) { enabled = false; return; }
        if (Array.IndexOf(Environment.GetCommandLineArgs(), "-idas3-legacy-host") >= 0) { enabled = false; return; }
        if (instance != null && instance != this) { Destroy(gameObject); return; }
        instance = this;
        Application.runInBackground = true;
        // The host owns focus gating and online games remain live. Keep device
        // discovery alive when unfocused; neutralize driving below as usual.
        UnityEngine.InputSystem.InputSystem.settings.backgroundBehavior =
            UnityEngine.InputSystem.InputSettings.BackgroundBehavior.IgnoreFocus;
        // Presentation runs at the display rate; the source simulation stays a
        // fixed 60 Hz and the renderer interpolates between its steps.
        Application.targetFrameRate = 60;
        QualitySettings.vSyncCount = 1;
        outputCamera = GetComponent<Camera>();
        try
        {
            string assets = Application.isEditor
                ? Path.GetFullPath(Path.Combine(Application.dataPath, "../Native"))
                : Path.Combine(Application.streamingAssetsPath, "IDAS3");
            string saves = Path.Combine(Application.persistentDataPath, "userdata-unity-scene");
            var hakone=FindAnyObjectByType<Idas8HakoneCourse>();
            var enna=FindAnyObjectByType<IdasSpecialStageEnnaCourse>();
            bool ennaTest=enna!=null&&enna.testBuild;
            bool importedTest=(hakone!=null&&hakone.testBuild)||ennaTest;
            string pack=Path.Combine(Application.streamingAssetsPath,"HAKONE");
            bool importedCourse=File.Exists(Path.Combine(pack,"menu.idastex"));
            if(importedTest&&!Application.isEditor)assets=File.ReadAllText(Path.Combine(Application.streamingAssetsPath,"d3-assets.txt")).Trim();
            if(importedTest)saves=Path.Combine(Application.persistentDataPath,"hakone-race-test");
            bool diagnostic = Idas3SceneSmoke.Configure(this, ref saves);
            diagnostic = Idas8HakoneTimeAttackSmoke.Configure(ref saves) || diagnostic;
            diagnostic = Idas3MultiplayerSmoke.Configure(ref saves) || diagnostic;
            diagnostic = Idas3MultiplayerPresentationSmoke.Configure(ref saves) || diagnostic;
            diagnostic = Idas3PauseSmoke.Configure(ref saves) || diagnostic;
            diagnostic = Idas3PreRaceSmoke.Configure(ref saves) || diagnostic;
            diagnostic = Idas3ModeFlowSmoke.Configure(ref saves) || diagnostic;
            diagnostic = Idas3ControlsSmoke.Configure(ref saves) || diagnostic;
            bool controllerDeviceDiagnostic = Idas3ControllerDevicesSmoke.Configure(ref saves);
            diagnostic = controllerDeviceDiagnostic || diagnostic;
            if (controllerDeviceDiagnostic) controllerDevices = Idas3ControllerDevicesSmoke.CreateProvider();
            diagnostic = Idas3OnlineControllerSmoke.Configure(ref saves) || diagnostic;
            diagnostic = Idas3RaceMusicSmoke.Configure(ref saves) || diagnostic;
            diagnostic = Idas3AttractOptionsSmoke.Configure(ref saves) || diagnostic;
            diagnosticMode = diagnostic || (importedTest && (Array.IndexOf(Environment.GetCommandLineArgs(),"-hakone-smoke")>=0||Array.IndexOf(Environment.GetCommandLineArgs(),"-hakone-menu-smoke")>=0));
            performanceDiagnostics = diagnostic && (Array.IndexOf(Environment.GetCommandLineArgs(), "-idas3-scene-perf-check") >= 0 || Array.IndexOf(Environment.GetCommandLineArgs(), "-idas3-multiplayer-profile") >= 0);
            MatchOutputResolution();
            if (!diagnostic && !Directory.Exists(saves))
            {
                string prior = Path.Combine(Application.persistentDataPath, "userdata");
                if (Directory.Exists(prior))
                {
                    string staging = saves + ".import-" + Guid.NewGuid().ToString("N");
                    string parent = Path.GetFullPath(Application.persistentDataPath);
                    if (Path.GetDirectoryName(Path.GetFullPath(saves)) != parent || Path.GetDirectoryName(Path.GetFullPath(staging)) != parent)
                        throw new IOException("Save import must stay inside the Unity save directory.");
                    Directory.CreateDirectory(staging);
                    foreach (string file in Directory.EnumerateFiles(prior, "*", SearchOption.AllDirectories))
                    {
                        string target = Path.Combine(staging, file.Substring(prior.Length + 1));
                        Directory.CreateDirectory(Path.GetDirectoryName(target));
                        File.Copy(file, target, false);
                        using (var hash = System.Security.Cryptography.SHA256.Create())
                        using (var original = File.OpenRead(file))
                        using (var copied = File.OpenRead(target))
                            if (Convert.ToBase64String(hash.ComputeHash(original)) != Convert.ToBase64String(hash.ComputeHash(copied)))
                                throw new IOException("Save import verification failed: " + file);
                    }
                    Directory.Move(staging,saves);
                }
            }
            Directory.CreateDirectory(saves);
            if (Idas3SceneInitialize(assets, saves, renderWidth, renderHeight) != 1)
                throw new InvalidOperationException(Idas3Native.Error());
            ready = true;
            var camera = GetComponent<Camera>();
            scene = gameObject.AddComponent<Idas3SceneRenderer>();
            scene.Initialize(camera);
            ui = gameObject.AddComponent<Idas3UnityUi>();
            ui.Initialize(camera);
            sound = gameObject.AddComponent<Idas3UnityAudio>();
            if (audioEnabled) sound.Initialize();
            RefreshScene();
            Debug.Log("IDAS3 Unity scene ready: native simulation, Unity meshes/materials/cameras/UI and AudioSource. No external framebuffer.");
            multiplayer = new Idas3MultiplayerSession(Status.car,saves);
            multiplayerMenu = gameObject.AddComponent<Idas3MultiplayerMenu>();
            multiplayerMenu.Initialize(multiplayer);
            if(!diagnosticMode)multiplayer.InitializeOnlinePresence();
            gameOptions = new Idas3GameOptions();
            gameOptions.Changed += ApplyNativeOptions;
            gameOptions.Initialize(saves);
            controlBindings = new Idas3ControlBindings();
            controlBindings.Initialize(saves);
            multiplayerMenu.ManagedControlInput = true;
            pauseMenu = gameObject.AddComponent<Idas3PauseMenu>();
            pauseMenu.Initialize(gameOptions);
            pauseMenu.Updates=Idas3Updates.Instance;
            if(pauseMenu.Updates==null){pauseMenu.Updates=gameObject.AddComponent<Idas3Updates>();pauseMenu.Updates.Initialize(false);}
            replayLibrary=gameObject.AddComponent<Idas3ReplayLibrary>();replayLibrary.Initialize(this,pauseMenu,saves);
            if(!diagnosticMode)gameObject.AddComponent<Idas3CommunityTimes>().Initialize(this,gameOptions,pauseMenu);
            pauseMenu.InitializeBindings(controlBindings);
            controllerDevices.Initialize(saves);
            controllerDevices.ActiveDeviceChanged += ControllerDeviceChanged;
            ControllerDeviceChanged();
            pauseMenu.InitializeControllerDevices(controllerDevices);
            wheelFeedback=new Idas3WheelFeedback(diagnosticMode);
            pauseMenu.InitializeWheelFeedback(wheelFeedback);
            pauseMenu.OpenChanged += PauseVisibilityChanged;
            raceMusic = new Idas3RaceMusicCatalog();
            raceMusic.Initialize();
            raceMusicMenu = gameObject.AddComponent<Idas3RaceMusicMenu>();
            raceMusicMenu.Initialize(raceMusic.Entries, raceMusic.State.selectedIndex);
            CustomMusic=gameObject.AddComponent<Idas3CustomRaceMusic>();
            CustomMusic.Initialize(saves,raceMusicMenu,raceMusic);
            raceMusicMenu.Selected += SelectRaceMusic;
            raceMusicMenu.OpenChanged += MusicVisibilityChanged;
            multiplayerMenu.MusicSelectionRequested += () => OpenRaceMusic(1);
            multiplayer.RaceDisconnected += ShowDisconnectedFinish;
            multiplayer.ReturnedToLobby += ShowReturnedLobby;
            challenger=gameObject.AddComponent<Idas3ChallengerOverlay>();
            challenger.Initialize(this,multiplayer);
            Idas3MultiplayerSmoke.Attach(this, multiplayer);
            Idas3PauseSmoke.Attach(this, gameOptions, pauseMenu);
            Idas3PreRaceSmoke.Attach(this);
            Idas3ModeFlowSmoke.Attach(this);
            Idas3ControlsSmoke.Attach(this);
            Idas3ControllerDevicesSmoke.Attach(this);
            Idas3OnlineControllerSmoke.Attach(this);
            Idas3RaceMusicSmoke.Attach(this);
            Idas3AttractOptionsSmoke.Attach(this);
            string sadaminePack=Path.Combine(Application.streamingAssetsPath,"SADAMINE");
            string ennaPack=Path.Combine(Application.streamingAssetsPath,"ENNA");
            if(File.Exists(Path.Combine(ennaPack,"menu.idastex"))){
                if(Idas3Native.Idas3SceneRegisterImportedCourse(ennaPack)!=1)throw new InvalidOperationException(Idas3Native.Error());
                if(enna==null)enna=new GameObject("Enna Skyline course").AddComponent<IdasSpecialStageEnnaCourse>();
                if(ennaTest){
                    int direction=Array.IndexOf(Environment.GetCommandLineArgs(),"-enna-uphill")>=0?1:0;
                    bool direct=Array.IndexOf(Environment.GetCommandLineArgs(),"-enna-race")>=0;
                    int weather=Array.IndexOf(Environment.GetCommandLineArgs(),"-enna-wet")>=0?1:0;
                    int started=direct?Idas3Native.Idas3SceneStartImportedCourseConditions(ennaPack,direction,1,weather):Idas3Native.Idas3SceneShowImportedCourseMenu(ennaPack);
                    if(started!=1)throw new InvalidOperationException(Idas3Native.Error());RefreshScene();
                }
            }
            foreach(var name in new[]{"MYOGI_SPECIAL","USUI_SPECIAL","MOMIJI"}){
                string specialPack=Path.Combine(Application.streamingAssetsPath,name);
                if(!File.Exists(Path.Combine(specialPack,"menu.idastex")))continue;
                if(Idas3Native.Idas3SceneRegisterImportedCourse(specialPack)!=1)throw new InvalidOperationException(Idas3Native.Error());
                if(enna==null)enna=new GameObject("Special Stage courses").AddComponent<IdasSpecialStageEnnaCourse>();
            }
            if(File.Exists(Path.Combine(sadaminePack,"menu.idastex"))){
                if(Idas3Native.Idas3SceneRegisterImportedCourse(sadaminePack)!=1)throw new InvalidOperationException(Idas3Native.Error());
                if(hakone==null)hakone=new GameObject("Imported courses").AddComponent<Idas8HakoneCourse>();
            }
            if(importedCourse){
                if(Idas3Native.Idas3SceneRegisterImportedCourse(pack)!=1)throw new InvalidOperationException(Idas3Native.Error());
                if(hakone==null)hakone=new GameObject("Hakone course").AddComponent<Idas8HakoneCourse>();
                if(importedTest&&!ennaTest){
                int direction=Array.IndexOf(Environment.GetCommandLineArgs(),"-hakone-uphill")>=0?1:0;
                int night=Array.IndexOf(Environment.GetCommandLineArgs(),"-hakone-night")>=0?1:0;
                int wet=Array.IndexOf(Environment.GetCommandLineArgs(),"-hakone-wet")>=0?1:0;
                bool direct=Array.IndexOf(Environment.GetCommandLineArgs(),"-hakone-race")>=0||Array.IndexOf(Environment.GetCommandLineArgs(),"-hakone-smoke")>=0;
                int started=direct?Idas3Native.Idas3SceneStartImportedCourseConditions(pack,direction,night,wet):Idas3Native.Idas3SceneShowImportedCourseMenu(pack);
                if(started!=1)throw new InvalidOperationException(Idas3Native.Error());
                RefreshScene();
                }
                if(diagnosticMode)DiagnosticFocusOverride=true;
            }
            Idas8HakoneTimeAttackSmoke.Attach(this);
        }
        catch (Exception error) { Fail(error.ToString()); }
    }

    private static readonly KeyCode[] keys = {
        KeyCode.Backspace,KeyCode.Return,KeyCode.Escape,KeyCode.Space,
        KeyCode.LeftArrow,KeyCode.UpArrow,KeyCode.RightArrow,KeyCode.DownArrow,
        KeyCode.A,KeyCode.C,KeyCode.D,KeyCode.E,KeyCode.Q,KeyCode.R,KeyCode.S,KeyCode.W,
        KeyCode.F1,KeyCode.F2,KeyCode.F3,KeyCode.F5
    };
    private static readonly int[] virtualKeys = {8,13,27,32,37,38,39,40,65,67,68,69,81,82,83,87,112,113,114,116};

    private void MatchOutputResolution()
    {
        var target = outputCamera != null ? outputCamera.targetTexture : null;
        int width = target != null ? target.width : Screen.width;
        int height = target != null ? target.height : Screen.height;
        // A minimized/initializing window can briefly report no drawable area.
        if (width > 0 && height > 0) { renderWidth = width; renderHeight = height; }
    }

    private void ToggleFullscreen()
    {
        if (Screen.fullScreen)
            Screen.SetResolution(windowedWidth, windowedHeight, FullScreenMode.Windowed);
        else
        {
            windowedWidth = Screen.width; windowedHeight = Screen.height;
            var display = Screen.currentResolution;
            Screen.SetResolution(display.width, display.height, FullScreenMode.FullScreenWindow);
        }
    }

    private void Update()
    {
        if(!ready&&!stopping&&failure==null){if(Idas3Updates.StartupFinished)InitializeGame();return;}
        if (!ready || stopping || failure != null) return;
        if(pauseMenu!=null&&pauseMenu.Updates!=null&&pauseMenu.Updates.WindowVisible)return;
        try
        {
            gameOptions.Tick(Time.realtimeSinceStartupAsDouble);
            controllerDevices.Tick(Focused && !pauseMenu.IsOpen && !multiplayerMenu.IsOpen &&
                !raceMusicMenu.IsOpen && !controlBindings.IsCapturing && !controlBindings.SuppressInput);
            pauseMenu.FrameRate = Time.unscaledDeltaTime > 0 ? 1f / Time.unscaledDeltaTime : 0;
            pauseMenu.SetContext(OnlineRace, CanOpenPause && !OnlineRace, Idas3CourseCatalog.SceneName(Status), (Status.flags & 8192u) != 0);
            pauseMenu.FullTuneAvailable=pauseMenu.IsOpen&&!multiplayer.InLobby&&!multiplayer.Busy&&!multiplayer.ChallengerPending&&Idas3Native.Idas3SceneCanFullTune()==1;
            ExecutePauseCommands();
            if (stopping) return;
            if (Focused && !pauseMenu.IsOpen && !controlBindings.SuppressInput && Input.GetKeyDown(KeyCode.F11)) ToggleFullscreen();
            bool muteBackground = gameOptions.Current.muteWhenUnfocused && !Focused;
            if (muteBackground != appliedBackgroundMute) ApplyNativeOptions(gameOptions.Current, false);
            MatchOutputResolution();
            double networkBegan=performanceDiagnostics?Time.realtimeSinceStartupAsDouble:0;
            multiplayer.BeforeFrame();
            if(performanceDiagnostics)NetworkMilliseconds=(float)((Time.realtimeSinceStartupAsDouble-networkBegan)*1000);
            challenger.Tick();
            var frame = new Idas3Native.FrameInput {
                size = (uint)Marshal.SizeOf<Idas3Native.FrameInput>(),
                // The native host receives actual focus and keeps online
                // simulation live while independently neutralizing input.
                flags = Focused ? 1u : 0u,
                deltaSeconds = Math.Min(Time.unscaledDeltaTime, .25),
                width = renderWidth, height = renderHeight
            };
            // A waiting room must not capture the original offline menus.
            bool networkRoom = multiplayer.IsRacing || multiplayer.ChallengerPending;
            bool saveMenuOwnsPointer = Idas3Native.Idas3SceneSaveMenuPointer(0, 0, Screen.width, Screen.height, 0) == 1;
            SyncSaveCarLevels(saveMenuOwnsPointer);
            var physicalPad = new Idas3ControlBindings.PadState();
            if (Focused)
            {
                for (int i = 0; i < keys.Length; ++i)
                    if (keys[i] != KeyCode.F1 && Input.GetKey(keys[i])) frame.SetKey(virtualKeys[i]);
                // Managed menus hit-test the pointer themselves. Translating
                // their click to Enter activates the controller selection first.
                Idas3MenuPointer.ApplyConfirm(ref frame,Input.GetKey(KeyCode.KeypadEnter),Input.GetMouseButton(0),
                    !networkRoom&&!pauseMenu.IsOpen&&!multiplayerMenu.BlocksGameInput&&!raceMusicMenu.BlocksGameInput&&
                    !saveMenuOwnsPointer&&!savePointerReleaseBlocked);
                controllerDevices.TryRead(out physicalPad);
            }
            Func<KeyCode,bool> physicalKey = Focused ? Input.GetKey : NoKeyHeld;
            bool diagnosticPad = Idas3MultiplayerSmoke.PreparePhysicalInput(ref physicalKey, ref physicalPad);
            diagnosticPad |= Idas3ControlsSmoke.PreparePhysicalInput(ref physicalKey, ref physicalPad);
            diagnosticPad |= Idas3ModeFlowSmoke.PreparePhysicalInput(ref physicalKey, ref physicalPad);
            diagnosticPad |= Idas3RaceMusicSmoke.PreparePhysicalInput(ref physicalKey, ref physicalPad);
            diagnosticPad |= Idas3AttractOptionsSmoke.PreparePhysicalInput(ref physicalKey, ref physicalPad);
            diagnosticPad |= Idas3PauseSmoke.PreparePhysicalInput(ref physicalPad);
            Idas3ControllerDevicesSmoke.PreparePhysicalInput(ref physicalKey);
            Idas3OnlineControllerSmoke.PreparePhysicalInput(ref physicalKey);
            if (physicalPad.connected)
            {
                frame.padConnected = 1; frame.padButtons = physicalPad.buttons;
                frame.leftTrigger = physicalPad.leftTrigger; frame.rightTrigger = physicalPad.rightTrigger;
                frame.thumbLX = physicalPad.thumbLX; frame.thumbLY = physicalPad.thumbLY;
                frame.thumbRX = physicalPad.thumbRX; frame.thumbRY = physicalPad.thumbRY;
            }
            if (!Focused) controlBindings.CancelCapture();
            // An unfocused synthetic neutral sample is not a physical release.
            // Keep capture's release latch until focused hardware is neutral.
            else controlBindings.Poll(physicalKey, physicalPad, Time.realtimeSinceStartupAsDouble,
                diagnosticPad ? null : controllerDevices.Controls);
            bool bindingInputBlocked = controlBindings.SuppressInput || controlBindings.IsCapturing;
            multiplayerMenu.ProcessControlInput(controlBindings.RawOnlineHeld, controlBindings.RawPauseHeld,
                bindingInputBlocked || !Focused || raceMusicMenu.BlocksGameInput || musicReleaseBlocked || pauseMenu.AttractOptions || challenger.Active);
            if (multiplayerMenu.IsOpen && pauseMenu.IsOpen)
            {
                // The offline race stays paused while visiting the F1 room.
                preservePauseOnClose = true;
                pauseMenu.SetOpen(false);
                preservePauseOnClose = false;
            }
            // The native menus retain their established navigation. Only the
            // driving packet is translated; original steering conditioning
            // and simulation code receive the same analog ranges as before.
            bool drivingBindings = (Status.flags & (1u | 32u | 512u | 1024u | 4096u)) == 0 && Status.racePhase < 3;
            if (drivingBindings && !pauseMenu.IsOpen && !multiplayerMenu.BlocksGameInput)
                controlBindings.ApplyDriving(ref frame);
            else controlBindings.ApplyMenu(ref frame,controllerDevices.ActiveIsGeneric,true);
            if (!Idas3SceneSmoke.PrepareFrame(ref frame)) return;
            if (!Idas3MultiplayerSmoke.PrepareFrame(ref frame)) return;
            if (!Idas3PauseSmoke.PrepareFrame(ref frame)) return;
            if (!Idas3PreRaceSmoke.PrepareFrame(ref frame)) return;
            if (!Idas3ModeFlowSmoke.PrepareFrame(ref frame)) return;
            if (!Idas3ControlsSmoke.PrepareFrame(ref frame)) return;
            if (!Idas3ControllerDevicesSmoke.PrepareFrame(ref frame)) return;
            if (!Idas3OnlineControllerSmoke.PrepareFrame(ref frame)) return;
            if (!Idas3RaceMusicSmoke.PrepareFrame(ref frame)) return;
            if (!Idas3AttractOptionsSmoke.PrepareFrame(ref frame)) return;
            // Start belongs to the original frontend until a host modal or race owns it.
            // Translating it to Escape on Title used to request application exit.
            if (controlBindings.RawPauseHeld && ((Status.flags & 1u) == 0 || pauseMenu.IsOpen || multiplayerMenu.IsOpen || raceMusicMenu.IsOpen)) frame.SetKey(27);
            multiplayerMenu.ProcessResultsInput(Held(frame, 13) || (frame.padButtons & 0x1000) != 0,
                controlBindings.PauseHeld || Held(frame, 27) || Held(frame, 8) || (frame.padButtons & 0x2000) != 0,
                !Focused || bindingInputBlocked);
            bool disconnectedFinish = multiplayer.DisconnectedFinish;
            multiplayerMenu.ProcessDisconnectedInput(Held(frame, 13) || (frame.padButtons & 0x1000) != 0,
                controlBindings.PauseHeld || Held(frame, 27) || Held(frame, 8) || (frame.padButtons & 0x2000) != 0,
                !Focused || bindingInputBlocked);
            if (disconnectedFinish && !multiplayer.DisconnectedFinish) musicReleaseBlocked = true;
            bool musicInputBlocked = UpdateRaceMusic(ref frame, bindingInputBlocked);
            bool attractInputBlocked = UpdateAttractOptions(ref frame, bindingInputBlocked || musicInputBlocked);
            bool savePointerBlocked = RouteSaveMenuPointer(frame, saveMenuOwnsPointer,
                bindingInputBlocked || musicInputBlocked || attractInputBlocked || disconnectedFinish || challenger.Active || networkRoom);
            if (diagnosticFocusOverride.HasValue) frame.flags = (frame.flags & ~1u) | (diagnosticFocusOverride.Value ? 1u : 0u);
            if (bindingInputBlocked || musicInputBlocked || attractInputBlocked || disconnectedFinish || challenger.Active || savePointerBlocked)
            {
                previousMenuInput = frame;
                NeutralizeControls(ref frame);
                menuNavigationAxis = 0;
            }
            else RoutePauseInput(ref frame);
            if ((frame.flags & 1u) == 0) NeutralizeControls(ref frame);
            if ((Status.flags & 1u) == 0) ClearKey(ref frame, 114); // No song switching during a race.
            if (networkRoom)
            {
                // Neither shortcuts nor menu confirmation may restart or
                // leave an online race outside the session's ownership.
                for (int i = 0; i < keys.Length; ++i)
                    if (!MultiplayerKey(keys[i])) ClearKey(ref frame, virtualKeys[i]);
                frame.padButtons &= 0xE30F;
            }
            Idas8HakoneRaceSmoke.PrepareFrame(ref frame);
            Idas8HakoneTimeAttackSmoke.PrepareFrame(ref frame);
            // Diagnostic input replaces gameplay controls, never output sizing.
            frame.width = renderWidth; frame.height = renderHeight;
            if (diagnosticMode) DiagnosticSubmittedInput = frame;
            double began = Time.realtimeSinceStartupAsDouble;
            if (Idas3SceneStep(ref frame) != 1) throw new InvalidOperationException(Idas3Native.Error());
            if (performanceDiagnostics) NativeStepMilliseconds = (float)((Time.realtimeSinceStartupAsDouble - began) * 1000);
            RefreshScene();
            simulationAndSubmissionMs = (float)((Time.realtimeSinceStartupAsDouble - began) * 1000);
            networkBegan=performanceDiagnostics?Time.realtimeSinceStartupAsDouble:0;
            multiplayer.AfterFrame();
            if(performanceDiagnostics)NetworkMilliseconds+=(float)((Time.realtimeSinceStartupAsDouble-networkBegan)*1000);
            Idas3MultiplayerPresentationSmoke.AfterFrame(this,multiplayer);
            UpdateWheelFeedback();
            if (!OnlineRace && !challenger.Active && CanOpenPause && (Status.flags & 2u) != 0 && !pauseMenu.IsOpen && !multiplayerMenu.IsOpen)
                pauseMenu.SetOpen(true);
            if (pauseMenu.IsOpen && !CanOpenPause && !OnlineRace && !(pauseMenu.AttractOptions && AttractScreen))
            {
                preservePauseOnClose = true;
                pauseMenu.SetOpen(false);
                preservePauseOnClose = false;
            }
            UpdateAttractPrompt();
            if ((Status.flags & 8) == 0)
            {
                StopNative();
#if UNITY_EDITOR
                UnityEditor.EditorApplication.isPlaying = false;
#else
                Application.Quit();
#endif
            }
        }
        catch (Exception error) { Fail(error.ToString()); }
    }

    private static bool MultiplayerKey(KeyCode key) => key == KeyCode.LeftArrow || key == KeyCode.RightArrow || key == KeyCode.UpArrow || key == KeyCode.DownArrow ||
        key == KeyCode.A || key == KeyCode.D || key == KeyCode.W || key == KeyCode.S || key == KeyCode.Space || key == KeyCode.Q || key == KeyCode.E || key == KeyCode.C || key == KeyCode.F2;

    private void ShowDisconnectedFinish()
    {
        controlBindings.CancelCapture();
        pauseMenu.SetOpen(false);
        raceMusicMenu.SetOpen(false);
        previousMenuInput = default;
        menuNavigationAxis = 0;
        Status = Idas3Native.ReadStatus();
        Cursor.lockState = CursorLockMode.None;
        Cursor.visible = true;
    }

    private void ShowReturnedLobby()
    {
        controlBindings.CancelCapture();
        preservePauseOnClose = true;
        pauseMenu.SetOpen(false);
        preservePauseOnClose = false;
        raceMusicMenu.SetOpen(false);
        musicReleaseBlocked = true;
        Status = Idas3Native.ReadStatus();
        previousMenuInput = default;
        menuNavigationAxis = 0;
    }

    internal void BeginChallenger()
    {
        controlBindings.CancelEdit(false);
        pauseMenu.SetOpen(false);raceMusicMenu.SetOpen(false);multiplayerMenu.SetOpen(false);
        musicReleaseBlocked=attractOptionsReleaseBlocked=false;
        if(Idas3Native.Idas3SceneChallenger(1)!=1)throw new InvalidOperationException(Idas3Native.Error());
        Status=Idas3Native.ReadStatus();
    }
    internal void CancelChallenger()
    {
        if(Idas3Native.Idas3SceneChallenger(0)!=1)throw new InvalidOperationException(Idas3Native.Error());
        musicReleaseBlocked=true;
    }
    internal void EnterChallengerLobby()
    {
        if(Idas3Native.Idas3SceneChallenger(2)!=1)throw new InvalidOperationException(Idas3Native.Error());
        ShowReturnedLobby();multiplayerMenu.SetOpen(true);
    }

    private void ApplyNativeOptions(Idas3GameOptions.Values values) => ApplyNativeOptions(values, true);
    private void ApplyNativeOptions(Idas3GameOptions.Values values, bool changeCamera)
    {
        if (!ready || stopping) return;
        if(Idas3ReplayLibrary.Idas3ReplayRecordingOptions(Idas3ReplayLibrary.RecordingFlags(values))!=1)throw new InvalidOperationException("Could not apply replay recording options.");
        var options = Idas3Native.ReadOptions();
        appliedBackgroundMute = values.muteWhenUnfocused && !Focused;
        options.masterGain = appliedBackgroundMute ? 0 : values.masterVolume;
        options.musicGain = values.musicVolume;
        options.engineGain = values.engineVolume;
        options.effectsGain = values.effectsVolume;
        if (changeCamera) options.cameraView = (uint)values.defaultCamera;
        options.managedPauseOverlay = 1;
        if (Idas3Native.Idas3SceneApplyOptions(ref options) != 1)
            throw new InvalidOperationException("Could not apply audio/camera options. " + Idas3Native.Error());
        if (Idas3Native.Idas3SceneSetTireVolume(values.tireVolume) != 1)
            throw new InvalidOperationException("Could not apply tire-squeal volume. " + Idas3Native.Error());
        if (Idas3Native.Idas3SceneSetControllerResponse(values.controllerResponse) != 1)
            throw new InvalidOperationException("Could not apply controller response. " + Idas3Native.Error());
        if (Idas3Native.Idas3SceneSetSteeringDeadzone(values.SteeringDeadzone) != 1)
            throw new InvalidOperationException("Could not apply steering deadzone. " + Idas3Native.Error());
        if (Idas3Native.Idas3SceneSetSteeringSmoothing(values.steeringSmoothing) != 1)
            throw new InvalidOperationException("Could not apply steering smoothing. " + Idas3Native.Error());
        if (Idas3Native.Idas3SceneSetPerformance(values.rainDetail) != 1)
            throw new InvalidOperationException("Could not apply performance options. " + Idas3Native.Error());
        if (Idas3Native.Idas3SceneSetAiDifficulty(values.aiDifficulty) != 1)
            throw new InvalidOperationException("Could not apply AI difficulty. " + Idas3Native.Error());
        if (Idas3Native.Idas3SceneSetMapZoom(values.minimapZoom) != 1)
            throw new InvalidOperationException("Could not apply minimap zoom. " + Idas3Native.Error());
        if (Idas3Native.Idas3SceneSetMapSize(values.minimapSize) != 1)
            throw new InvalidOperationException("Could not apply minimap size. " + Idas3Native.Error());
        if(scene!=null)scene.HudOptions=values.Clone();
        wheelFeedback?.Stop();
    }

    private void UpdateWheelFeedback()
    {
        if(wheelFeedback==null)return;
        bool allowed=ready&&!stopping&&Application.isFocused&&!pauseMenu.BlocksGameInput&&
            !multiplayerMenu.BlocksGameInput&&!raceMusicMenu.BlocksGameInput&&!controlBindings.SuppressInput;
        var state=new Idas3Native.WheelState{size=40};
        if(allowed&&gameOptions.Current.wheelForceFeedback&&Idas3Native.Idas3SceneGetWheelState(ref state)!=1)allowed=false;
        wheelFeedback.Update(gameOptions.Current,controllerDevices.WheelIdentity,state,Status.car,allowed,Time.realtimeSinceStartupAsDouble);
    }
    private void OnApplicationFocus(bool focused){if(!focused)wheelFeedback?.Stop();}
    private void OnApplicationPause(bool paused){if(paused)wheelFeedback?.Stop();}
    private void OnDisable(){wheelFeedback?.Stop();}
    private static bool NoKeyHeld(KeyCode key) => false;

    private void ControllerDeviceChanged()
    {
        wheelFeedback?.Stop();
        if (controlBindings == null) return;
        controlBindings.SelectControllerProfile(controllerDevices.ActiveProfileKey,
            controllerDevices.ActiveName, controllerDevices.ActiveIsGeneric);
        controlBindings.ControllerDeviceChanged();
    }

    private bool MusicLobbyAllowed => multiplayerMenu.IsOpen && multiplayer.InLobby && !multiplayer.IsRacing && !multiplayer.Busy && !multiplayer.HasCourseDraw;
    private bool MusicOpponentAllowed => raceMusic.State.opponentEligible != 0 && !multiplayerMenu.IsOpen && !pauseMenu.IsOpen;
    private string ViewChangeHint()
    {
        string keyboard = controlBindings.BindingName(Idas3ControlBindings.ActionId.Camera, Idas3ControlBindings.Slot.Primary, false);
        for (int slot = 1; slot < 3 && keyboard == "Unbound"; ++slot)
            keyboard = controlBindings.BindingName(Idas3ControlBindings.ActionId.Camera, (Idas3ControlBindings.Slot)slot, false);
        string controller = controlBindings.BindingName(Idas3ControlBindings.ActionId.Camera, Idas3ControlBindings.Slot.Controller, false);
        string controls = keyboard == "Unbound" ? "" : keyboard;
        if (controller != "Unbound") controls += (controls.Length > 0 ? " / " : "") + controller;
        return controls.Length == 0 ? "VIEW CHANGE" : "VIEW CHANGE (" + controls + ")";
    }
    private void OpenRaceMusic(int context)
    {
        if (!ready || stopping) return;
        raceMusic.Refresh();
        if (!Focused || controlBindings.SuppressInput || raceMusicMenu.IsOpen ||
            (context == 1 ? !MusicLobbyAllowed : !MusicOpponentAllowed)) return;
        musicPickerContext = context;
        if (context == 1 && multiplayer.LocalReady) multiplayer.SetReady(false);
        raceMusicMenu.SetSelected(CustomMusic.SelectedId);
        raceMusicMenu.SetOpen(true);
        raceMusicMenu.SetNotice(context == 1 ? "Your choice plays on your game only." : "Choose the music for your next race.");
    }
    private void SelectRaceMusic(int id)
    {
        if (!ready || stopping || !raceMusicMenu.IsOpen) return;
        if(CustomMusic.Busy)return;
        raceMusic.Refresh();
        if (musicPickerContext == 1 ? !MusicLobbyAllowed : !MusicOpponentAllowed)
        { raceMusicMenu.SetOpen(false); return; }
        if(id==Idas3CustomRaceMusic.AddId){CustomMusic.AddMusic();return;}
        if(id>=Idas3CustomRaceMusic.FirstId){if(!CustomMusic.Select(id,musicPickerContext)){raceMusicMenu.SetNotice(CustomMusic.LastError);return;}}
        else {if (!raceMusic.Select(id, musicPickerContext)) { raceMusicMenu.SetNotice(raceMusic.LastError); return; }CustomMusic.ClearSelection();}
        raceMusicMenu.SetSelected(CustomMusic.SelectedId);
        raceMusicMenu.SetOpen(false);
    }
    private void MusicVisibilityChanged(bool open)
    {
        multiplayerMenu.InputCovered = open;
        musicReleaseBlocked = true;
        musicHoldSeconds = 0;
        musicNavigationAxis = 0;
        previousMusicInput = default;
    }
    private bool MusicControlsHeld(Idas3Native.FrameInput raw) =>
        controlBindings.ViewChangeHeld || controlBindings.PauseHeld || controlBindings.OnlineHeld ||
        Held(raw, 13) || Held(raw, 8) || Held(raw, 27) ||
        (raw.padButtons & 0x3010u) != 0 || Input.GetMouseButton(0);
    private bool UpdateRaceMusic(ref Idas3Native.FrameInput frame, bool bindingBlocked)
    {
        raceMusicMenu.WheelNavigation=controllerDevices.ActiveIsGeneric;
        raceMusic.Refresh();
        bool opponent = MusicOpponentAllowed, lobby = MusicLobbyAllowed;
        multiplayerMenu.RaceHudActive = (Status.flags & 1u) == 0;
        if (raceMusicMenu.IsOpen && !(musicPickerContext == 1 ? lobby : opponent)) raceMusicMenu.SetOpen(false);
        string hint = opponent || lobby ? ViewChangeHint() : "VIEW CHANGE";
        multiplayerMenu.MusicSelectionAllowed = lobby;
        multiplayerMenu.SelectedMusicTitle = CustomMusic.SelectedTitle;
        multiplayerMenu.MusicControlHint = "HOLD " + hint + " TO SELECT MUSIC";
        var raw = frame;
        bool block = raceMusicMenu.BlocksGameInput || musicReleaseBlocked;
        if (!Focused || bindingBlocked)
        {
            musicHoldSeconds = 0; previousMusicInput = raw;
            raceMusicMenu.SetContext(opponent, 0, hint);
            if (block) NeutralizeControls(ref frame);
            return block;
        }
        if (musicReleaseBlocked)
        {
            if (!MusicControlsHeld(raw)) musicReleaseBlocked = false;
            previousMusicInput = raw; NeutralizeControls(ref frame);
            raceMusicMenu.SetContext(opponent, 0, hint); return true;
        }
        if (raceMusicMenu.IsOpen)
        {
            if(musicPointer.BlockNavigation(Idas3MenuPointer.Active,MenuNavigationHeld(raw))){
                previousMusicInput=raw;NeutralizeControls(ref frame);
                raceMusicMenu.SetContext(opponent,0,hint);return true;
            }
            bool pressed(int key) => Held(raw, key) && !Held(previousMusicInput, key);
            uint buttons = raw.padButtons & ~previousMusicInput.padButtons;
            if (pressed(27) || pressed(8) || (buttons & 0x2000) != 0) raceMusicMenu.Back();
            else if (pressed(13) || (buttons & 0x1000) != 0) raceMusicMenu.Activate();
            else
            {
                int vertical = (Held(raw, 40) || (raw.padButtons & 2) != 0 || raw.thumbLY < -16000 ? 1 : 0)
                    - (Held(raw, 38) || (raw.padButtons & 1) != 0 || raw.thumbLY > 16000 ? 1 : 0);
                int horizontal = (Held(raw, 39) || (raw.padButtons & 8) != 0 || raw.thumbLX > 16000 ? 1 : 0)
                    - (Held(raw, 37) || (raw.padButtons & 4) != 0 || raw.thumbLX < -16000 ? 1 : 0);
                int axis = vertical != 0 ? vertical : horizontal * 2;
                double now = Time.realtimeSinceStartupAsDouble;
                if (axis != 0 && (axis != musicNavigationAxis || now >= nextMusicNavigation))
                {
                    raceMusicMenu.NavigateDevice(horizontal,vertical,controllerDevices.ActiveIsGeneric);
                    nextMusicNavigation = now + (axis != musicNavigationAxis ? .30 : .10);
                }
                musicNavigationAxis = axis;
            }
            previousMusicInput = raw; NeutralizeControls(ref frame);
            raceMusicMenu.SetContext(opponent, 0, hint); return true;
        }
        if ((opponent || lobby) && controlBindings.ViewChangeHeld)
        {
            musicHoldSeconds += Math.Min(Time.unscaledDeltaTime, .1f);
            if (musicHoldSeconds >= .65f) OpenRaceMusic(lobby ? 1 : 0);
            block = true;
        }
        else musicHoldSeconds = 0;
        raceMusicMenu.SetContext(opponent, Mathf.Clamp01(musicHoldSeconds / .65f), hint);
        previousMusicInput = raw;
        if (block) NeutralizeControls(ref frame);
        return block;
    }

    private void UpdateAttractPrompt()
    {
        bool visible = AttractOptionsAllowed;
        pauseMenu.SetAttractPrompt(visible,
            Mathf.Clamp01(attractOptionsHoldSeconds / .65f), visible ? ViewChangeHint() : "VIEW CHANGE");
    }
    private bool UpdateAttractOptions(ref Idas3Native.FrameInput frame, bool inputBlocked)
    {
        if (!Focused || inputBlocked)
        {
            attractOptionsHoldSeconds = 0;
            return attractOptionsReleaseBlocked;
        }
        if (attractOptionsReleaseBlocked)
        {
            // Closing with a rebound A/B or a held key must not confirm/exit
            // the original title underneath, or open settings a second time.
            if (!MusicControlsHeld(frame)) attractOptionsReleaseBlocked = false;
            attractOptionsHoldSeconds = 0;
            return true;
        }
        if (!AttractOptionsAllowed)
        {
            attractOptionsHoldSeconds = 0;
            return false;
        }
        if (controlBindings.ViewChangeHeld)
        {
            if (attractOptionsHoldSeconds == 0) attractViewTapChild = Status.attractChild;
            attractOptionsHoldSeconds += Math.Min(Time.unscaledDeltaTime, .1f);
            if (attractOptionsHoldSeconds >= .65f) pauseMenu.OpenAttractOptions();
            return true;
        }
        // Original ranking pages use View Change taps. Defer that edge until
        // release so a long hold never changes a page or confirms Start first.
        bool startHeld = Held(frame, 13) || (frame.padButtons & 0x1000) != 0;
        if (attractOptionsHoldSeconds > 0 && !startHeld &&
            attractViewTapChild == 12 && Status.attractChild == attractViewTapChild)
            frame.SetKey(67);
        attractOptionsHoldSeconds = 0;
        return false;
    }

    private void PauseVisibilityChanged(bool open)
    {
        pauseBlockThroughFrame = Time.frameCount + 1;
        menuNavigationAxis = 0;
        pauseOpenReleaseBlocked = open;
        if (!open) suppressPauseControls = true;
        if (pauseMenu.AttractOptions)
        {
            attractOptionsHoldSeconds = 0;
            attractOptionsReleaseBlocked = true;
            return; // Attract presentation keeps running; this is not a paused race.
        }
        if (!ready || stopping || OnlineRace || (!open && preservePauseOnClose)) return;
        if (Idas3Native.Idas3SceneSetPaused(open ? 1 : 0) != 1)
            Debug.LogWarning("Pause state was not changed: " + Idas3Native.Error());
        Status = Idas3Native.ReadStatus();
    }

    private void ExecutePauseCommands()
    {
        while (pauseMenu.TryConsumeCommand(out var command))
        {
            int result = 1;
            switch (command)
            {
                case Idas3PauseMenu.Command.Resume: pauseMenu.SetOpen(false); break;
                case Idas3PauseMenu.Command.Replays: replayLibrary.OpenViewer(); break;
                case Idas3PauseMenu.Command.FullTune:
                    if(multiplayer.InLobby||multiplayer.Busy||multiplayer.ChallengerPending)break;
                    result=Idas3Native.Idas3SceneFullTune();
                    if(result==1)pauseMenu.SetOpen(false);
                    break;
                case Idas3PauseMenu.Command.Restart:
                    if (OnlineRace || multiplayer.ChallengerPending) break;
                    result = Idas3Native.Idas3SceneRestart();
                    if (result == 1) pauseMenu.SetOpen(false);
                    break;
                case Idas3PauseMenu.Command.Retire:
                    result = Idas3Native.Idas3SceneRetire();
                    if (result == 1) pauseMenu.SetOpen(false);
                    break;
                case Idas3PauseMenu.Command.ReturnToCourse:
                    result = Idas3Native.Idas3SceneReturnToCourse();
                    if (result == 1) pauseMenu.SetOpen(false);
                    break;
                case Idas3PauseMenu.Command.LeaveOnline:
                    multiplayer.LeaveRoom(); pauseMenu.SetOpen(false); break;
                case Idas3PauseMenu.Command.Quit:
                    StopNative();
#if UNITY_EDITOR
                    UnityEditor.EditorApplication.isPlaying = false;
#else
                    Application.Quit();
#endif
                    return;
            }
            if (result != 1) Debug.LogWarning("Pause action is unavailable: " + Idas3Native.Error());
        }
    }

    private static bool Held(Idas3Native.FrameInput frame, int key)
    {
        uint word;
        switch (key >> 5)
        {
            case 0: word = frame.key0; break; case 1: word = frame.key1; break;
            case 2: word = frame.key2; break; case 3: word = frame.key3; break;
            case 4: word = frame.key4; break; case 5: word = frame.key5; break;
            case 6: word = frame.key6; break; default: word = frame.key7; break;
        }
        return (word & (1u << (key & 31))) != 0;
    }
    private static void ClearKey(ref Idas3Native.FrameInput frame, int key)
    {
        uint mask = ~(1u << (key & 31));
        switch (key >> 5)
        {
            case 0: frame.key0 &= mask; break; case 1: frame.key1 &= mask; break;
            case 2: frame.key2 &= mask; break; case 3: frame.key3 &= mask; break;
            case 4: frame.key4 &= mask; break; case 5: frame.key5 &= mask; break;
            case 6: frame.key6 &= mask; break; default: frame.key7 &= mask; break;
        }
    }
    private static void NeutralizeControls(ref Idas3Native.FrameInput frame)
    {
        frame.flags |= 2u; // Reset native steering smoothing while gameplay controls are blocked.
        frame.key0 = frame.key1 = frame.key2 = frame.key3 = frame.key4 = frame.key5 = frame.key6 = frame.key7 = 0;
        frame.padButtons = frame.leftTrigger = frame.rightTrigger = frame.padConnected = 0;
        frame.thumbLX = frame.thumbLY = frame.thumbRX = frame.thumbRY = 0;
    }
    private static bool MenuNavigationHeld(Idas3Native.FrameInput frame) =>
        Held(frame,13)||Held(frame,8)||Held(frame,27)||Held(frame,37)||Held(frame,38)||Held(frame,39)||Held(frame,40)||
        (frame.padButtons&0x301Fu)!=0||Math.Abs(frame.thumbLX)>16000||Math.Abs(frame.thumbLY)>16000;
    private void SyncSaveCarLevels(bool ownsPointer)
    {
        if(!ownsPointer){saveCarLevelsLoaded=false;return;}
        if(saveCarLevelsLoaded||multiplayer==null)return;
        var levels=new uint[35];
        for(int car=0;car<levels.Length;++car)
        {
            try{levels[car]=multiplayer.ReadCarBattleRecord(car).level;}
            catch(Exception error)when(error is IOException||error is InvalidDataException||error is UnauthorizedAccessException||error is ArgumentException)
            {
                // Unknown history stays unknown. Reading the save menu must
                // never reset or overwrite a damaged online record.
                levels[car]=0;
                if(!saveCarLevelsWarned)
                {
                    saveCarLevelsWarned=true;
                    Debug.LogWarning("Saved car level could not be read for "+Idas3MultiplayerSession.CarNames[car]+": "+error.Message);
                }
            }
        }
        if(Idas3Native.Idas3SceneSetSaveCarLevels(levels,levels.Length)!=1&&!saveCarLevelsWarned)
        {
            saveCarLevelsWarned=true;
            Debug.LogWarning("Save menu car levels could not be refreshed: "+Idas3Native.Error());
        }
        // Refresh on the next entry, including after an online race changes
        // the same records used by its car's aura.
        saveCarLevelsLoaded=true;
    }
    private bool RouteSaveMenuPointer(Idas3Native.FrameInput frame, bool ownsPointer, bool inputBlocked)
    {
        bool allowed = Focused && !inputBlocked && !pauseMenu.BlocksGameInput && !multiplayerMenu.BlocksGameInput;
        bool mouseHeld = !diagnosticMode && Input.GetMouseButton(0);
        if(allowed&&ownsPointer&&!diagnosticMode)
        {
            var point=Input.mousePosition;
            // A stationary cursor must not override the default No or a
            // controller choice when the confirmation first appears.
            if(savePointerTracked&&point!=previousSavePointer)
                Idas3Native.Idas3SceneSaveMenuPointer(point.x,Screen.height-point.y,Screen.width,Screen.height,2);
            previousSavePointer=point;savePointerTracked=true;
        }
        else savePointerTracked=false;
        if (allowed && ownsPointer && mouseHeld)
        {
            savePointerReleaseBlocked = true;
            if (Input.GetMouseButtonDown(0))
            {
                var point = Input.mousePosition;
                Idas3Native.Idas3SceneSaveMenuPointer(point.x, Screen.height - point.y, Screen.width, Screen.height, 1);
            }
        }
        if (!savePointerReleaseBlocked) return false;
        // A click may leave SaveSelect. Keep blocking its held mouse/controller
        // input until release so it cannot also confirm the following screen.
        bool navigationHeld = MenuNavigationHeld(frame) || Held(frame, 65) || Held(frame, 68) ||
            Held(frame, 69) || Held(frame, 81) || (frame.padButtons & 0xC000u) != 0;
        if (allowed && !mouseHeld && !navigationHeld) savePointerReleaseBlocked = false;
        return true;
    }
    private void RoutePauseInput(ref Idas3Native.FrameInput frame)
    {
        var raw = frame;
        pauseMenu.SetWheelNavigation(controllerDevices.ActiveIsGeneric);
        bool pressed(int key) => Held(raw, key) && !Held(previousMenuInput, key);
        uint pressedButtons = raw.padButtons & ~previousMenuInput.padButtons;
        // PauseHeld already translates the bound controller control to Escape.
        // Raw Start is stripped from driving packets, then reappears while the
        // menu is open; treating it as a second edge immediately closes pause.
        bool toggle = pressed(27);
        bool closeHeld = Held(raw, 27) || Held(raw, 13) || (raw.padButtons & 0x3010) != 0 ||
            (pauseMenu.AttractOptions && controlBindings.ViewChangeHeld) || (!diagnosticMode && Input.GetMouseButton(0));
        if (!closeHeld) suppressPauseControls = false;
        if (multiplayerMenu.BlocksGameInput)
        {
            int vertical=(Held(raw,40)||(raw.padButtons&2)!=0||raw.thumbLY < -16000?1:0)
                -(Held(raw,38)||(raw.padButtons&1)!=0||raw.thumbLY > 16000?1:0);
            int horizontal=(Held(raw,39)||(raw.padButtons&8)!=0||raw.thumbLX > 16000?1:0)
                -(Held(raw,37)||(raw.padButtons&4)!=0||raw.thumbLX < -16000?1:0);
            multiplayerMenu.ProcessMenuNavigation(horizontal,vertical,Held(raw,13)||(raw.padButtons&0x1000)!=0,
                Held(raw,8)||(raw.padButtons&0x2000)!=0,(raw.flags&1)==0||controlBindings.SuppressInput,
                Time.realtimeSinceStartupAsDouble,controllerDevices.ActiveIsGeneric);
            NeutralizeControls(ref frame);
        }
        else if ((raw.flags & 1) != 0)
        {
            if (pauseMenu.IsOpen)
            {
                // A rebound pause control may also be the device's menu A/B.
                // Wait for release before interpreting it as a menu command.
                if(pausePointer.BlockNavigation(Idas3MenuPointer.Active,MenuNavigationHeld(raw))){menuNavigationAxis=0;}
                else if (pauseOpenReleaseBlocked)
                {
                    if (!closeHeld) pauseOpenReleaseBlocked = false;
                }
                else if (toggle) pauseMenu.Back();
                else if (pressed(8) || (pressedButtons & 0x2000) != 0) pauseMenu.Back();
                else if (pressed(13) || (pressedButtons & 0x1000) != 0) pauseMenu.Activate();
                else
                {
                    int vertical = (Held(raw, 40) || (raw.padButtons & 2) != 0 || raw.thumbLY < -16000 ? 1 : 0)
                        - (Held(raw, 38) || (raw.padButtons & 1) != 0 || raw.thumbLY > 16000 ? 1 : 0);
                    int horizontal = (Held(raw, 39) || (raw.padButtons & 8) != 0 || raw.thumbLX > 16000 ? 1 : 0)
                        - (Held(raw, 37) || (raw.padButtons & 4) != 0 || raw.thumbLX < -16000 ? 1 : 0);
                    int axis = vertical != 0 ? vertical : horizontal * 2;
                    double now = Time.realtimeSinceStartupAsDouble;
                    if (axis != 0 && (axis != menuNavigationAxis || now >= nextMenuNavigation))
                    {
                        if (vertical != 0) pauseMenu.Navigate(vertical); else pauseMenu.NavigateHorizontal(horizontal);
                        nextMenuNavigation = now + (axis != menuNavigationAxis ? .32 : .10);
                    }
                    menuNavigationAxis = axis;
                }
                NeutralizeControls(ref frame);
            }
            else if (toggle && CanOpenPause && !suppressPauseControls)
            {
                pauseMenu.SetOpen(true);
                NeutralizeControls(ref frame);
            }
        }
        // A held menu/confirm button can also be bound to acceleration. Keep
        // its menu latch, but do not silence unrelated driving controls.
        bool returningToFrontend = (Status.flags & 1u) != 0;
        if (suppressPauseControls && !returningToFrontend)
        {
            // Do not forward the closing press to native pause/confirmation.
            ClearKey(ref frame, 27); ClearKey(ref frame, 13); ClearKey(ref frame, 8);
            frame.padButtons &= ~0x3010u;
        }
        if (pauseMenu.BlocksGameInput || (suppressPauseControls && returningToFrontend) || Time.frameCount <= pauseBlockThroughFrame)
            NeutralizeControls(ref frame);
        previousMenuInput = raw;
    }

    private void RefreshScene()
    {
        Status = Idas3Native.ReadStatus();
        if (Status.state != 1 || Status.reserved != 1)
            throw new InvalidOperationException("Unity scene simulation did not initialize correctly. " + Idas3Native.Error());
        double began = performanceDiagnostics ? Time.realtimeSinceStartupAsDouble : 0;
        scene.ApplyFrame();
        if (performanceDiagnostics) RendererMilliseconds = (float)((Time.realtimeSinceStartupAsDouble - began) * 1000);
        began = performanceDiagnostics ? Time.realtimeSinceStartupAsDouble : 0;
        ui.ApplyFrame();
        if (performanceDiagnostics) UiMilliseconds = (float)((Time.realtimeSinceStartupAsDouble - began) * 1000);
        stage = (Status.flags & 1) != 0 ? "Menu " + Status.frontendStage : "Race " + Status.racePhase;
        speedKmh = Status.speedMetresPerSecond * 3.6f;
    }
    private void Fail(string message)
    {
        failure = message;
        Debug.LogError(message);
        StopNative();
    }
    private void OnGUI()
    {
        if (failure != null) GUI.Box(new Rect(16,16,Math.Min(Screen.width-32,1000),220),"Initial D Unity scene could not start\n\n"+failure);
    }
    public void StopNative()
    {
        if (stopping) return;
        stopping = true;
        wheelFeedback?.Dispose();
        if (sound != null) sound.StopOutput();
        multiplayer?.Dispose();
        controllerDevices.Dispose();
        if (ready)
        {
            if (Idas3SceneShutdown() != 1) Debug.LogError("Unity scene shutdown: " + Idas3Native.Error());
            ready = false;
        }
    }
    private void OnApplicationQuit() { StopNative(); }
    private void OnDestroy() { StopNative(); if (instance == this) instance = null; }
}
