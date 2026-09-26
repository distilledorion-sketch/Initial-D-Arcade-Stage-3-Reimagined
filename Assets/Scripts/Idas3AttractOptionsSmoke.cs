using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;

// Explicit private-save diagnostic. View Change is polled through the normal
// binding owner; the source attract frontend is never replaced with a fixture.
public sealed class Idas3AttractOptionsSmoke : MonoBehaviour
{
    private static bool ReportsCheck => Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-reports-check")>=0;
    private static bool PointerCheck => Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-pointer-live-check")>=0;
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneModeFlowFixture(int scene);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneModeFlowValue(int field);
    private static string pendingRoot;
    private static Idas3AttractOptionsSmoke active;
    private Idas3SceneGame host;
    private Idas3PauseMenu menu;
    private Idas3GameOptions options;
    private string root;
    private KeyCode physicalKey;
    private ushort buttons;
    private bool padConnected,finished,heldAccelerator,heldSteering;
    private byte throttle;
    private short steeringAxis;
    private static bool OptionsExitCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-options-exit-check")>=0;
    private static bool UpdatesCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-updates-check")>=0;
    private static bool HudCustomizationCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-hud-customization-check")>=0;
    private static bool HudEdgePlacementCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-hud-edge-placement-check")>=0;
    private int pulse,checks;
    private double began;
    private double presentationDelta;
    private readonly List<string> captures=new List<string>();
    private readonly List<CaptureDimensions> captureDimensions=new List<CaptureDimensions>();
    private readonly List<string> observations=new List<string>();
    [Serializable] private sealed class CaptureDimensions {
        public string file;public int requestedWidth,requestedHeight,actualWidth,actualHeight;
        public bool requestedSizeSupported;
    }
    [Serializable] private sealed class Report {
        public string schema="idas3-attract-options-smoke-v1",applicationVersion,error,scope;
        public bool passed,shutdownComplete;public int checks,finalFrontendStage;public double seconds;
        public string[] captures,observations;public CaptureDimensions[] captureDimensions;public Idas3GameOptions.Values options;
    }
    private sealed class OptionsTestPlatform : Idas3GameOptions.IPlatform {
        public int Width=>1200;public int Height=>720;public int DisplayMode=>0;public double Now=>0;
        public Idas3GameOptions.ResolutionChoice[] Resolutions=>new[]{new Idas3GameOptions.ResolutionChoice(1200,720)};
        public void Apply(Idas3GameOptions.Values previous,Idas3GameOptions.Values next,bool displayChanged){}
    }
    public static bool Configure(ref string saves){
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-attract-options-smoke");if(at<0)return false;
        if(at+1>=args.Length)throw new ArgumentException("Attract options diagnostic needs a fresh output directory.");
        pendingRoot=Path.GetFullPath(args[at+1]);
        if(Directory.Exists(pendingRoot)||File.Exists(pendingRoot))throw new IOException("Use a new attract options diagnostic directory.");
        Directory.CreateDirectory(pendingRoot);saves=Path.Combine(pendingRoot,"userdata");Directory.CreateDirectory(saves);
        File.WriteAllText(Path.Combine(saves,"settings.txt"),"0 0 0 0 0 1 1 0\n");
        File.WriteAllText(Path.Combine(saves,"native_selection.txt"),"0 0\n");
        File.WriteAllText(Path.Combine(pendingRoot,"ISOLATED_ATTRACT_OPTIONS_TEST.txt"),"Private saves and synthetic controls; no ordinary profile or hardware force output.\n");
        Screen.SetResolution(1200,720,FullScreenMode.Windowed);AudioListener.volume=0;return true;
    }
    public static void Attach(Idas3SceneGame game){
        if(pendingRoot==null)return;
        active=game.gameObject.AddComponent<Idas3AttractOptionsSmoke>();active.host=game;
        active.menu=game.PauseMenu;active.options=game.GameOptions;active.root=pendingRoot;
        active.began=Time.realtimeSinceStartupAsDouble;active.StartCoroutine(active.Guard(active.Run()));
    }
    internal static bool PreparePhysicalInput(ref Func<KeyCode,bool> key,ref Idas3ControlBindings.PadState pad){
        if(active==null||active.finished)return false;
        key=active.KeyHeld;pad=new Idas3ControlBindings.PadState{connected=active.padConnected,buttons=active.buttons,rightTrigger=active.throttle,thumbLX=active.steeringAxis};return true;
    }
    private bool KeyHeld(KeyCode key)=>(physicalKey!=KeyCode.None&&key==physicalKey)||(heldAccelerator&&key==KeyCode.W)||(heldSteering&&key==KeyCode.D);
    internal static bool PrepareFrame(ref Idas3Native.FrameInput frame){
        if(active==null)return true;if(active.finished)return false;
        if(active.presentationDelta>0)frame.deltaSeconds=active.presentationDelta;
        if(active.pulse!=0){frame.SetKey(active.pulse);active.pulse=0;}return true;
    }
    private void Check(bool ok,string message){++checks;if(!ok)throw new InvalidOperationException(message+" stage="+host.Status.frontendStage+" flags="+host.Status.flags);}
    private IEnumerator Frames(int count){for(int i=0;i<count;++i)yield return null;}
    private IEnumerator Delay(double seconds){double end=Time.realtimeSinceStartupAsDouble+seconds;while(Time.realtimeSinceStartupAsDouble<end)yield return null;}
    private IEnumerator Until(Func<bool> condition,double seconds,string message){double end=Time.realtimeSinceStartupAsDouble+seconds;while(!condition()&&Time.realtimeSinceStartupAsDouble<end)yield return null;Check(condition(),message);}
    private IEnumerator Release(){physicalKey=KeyCode.None;buttons=0;yield return Frames(4);Check(!host.ControlBindings.SuppressInput,"Neutral input did not release the binding gate");}
    private IEnumerator Guard(IEnumerator routine){
        var stack=new Stack<IEnumerator>();stack.Push(routine);
        while(stack.Count>0&&!finished){object value=null;Exception failure=null;
            try{if(!stack.Peek().MoveNext()){stack.Pop();continue;}value=stack.Peek().Current;}catch(Exception e){failure=e;}
            if(failure!=null){Finish(false,failure.ToString());yield break;}if(value is IEnumerator child)stack.Push(child);else yield return value;
        }
    }
    private void Update(){if(!finished&&(host.Failure!=null||Time.realtimeSinceStartupAsDouble-began>(ReportsCheck||PointerCheck||HudCustomizationCheck?300:45)))Finish(false,host.Failure??"Attract options diagnostic timeout.");}
    private IEnumerator PointerRegression(){
        padConnected=true;yield return Release();menu.OpenAttractOptions();menu.SelectTab(2);
        int camera=options.Draft.defaultCamera;bool fps=options.Draft.showFps;
        File.WriteAllText(Path.Combine(root,"pointer-phase.txt"),"settings");
        yield return Until(()=>options.Draft.showFps!=fps,90,"Real mouse did not change FPS setting");
        Check(options.Draft.defaultCamera==camera&&menu.IsOpen,"Mouse activated controller-highlighted camera instead of FPS");
        yield return Capture("pointer-settings");menu.SetOpen(false);yield return Release();
        var session=host.MultiplayerSession;session.ConfigureLocalTest(28475,"MOUSE TEST");session.HostRoom();
        yield return Until(()=>session.InLobby,15,"Local isolated lobby opened");
        var online=host.GetComponent<Idas3.Multiplayer.Idas3MultiplayerMenu>();online.SetOpen(true);yield return Frames(8);
        for(int i=0;i<40&&online.ControllerSelection!="course-option:BOOST";++i){
            online.ProcessMenuNavigation(0,0,false,false,false,Time.realtimeSinceStartupAsDouble);
            online.ProcessMenuNavigation(0,1,false,false,false,Time.realtimeSinceStartupAsDouble);yield return Frames(2);
        }
        Check(online.ControllerSelection=="course-option:BOOST","Controller highlights Boost before mouse target differs");
        File.WriteAllText(Path.Combine(root,"pointer-phase.txt"),"online-collisions");
        yield return Until(()=>!session.CollisionsEnabled,90,"Mouse did not toggle collisions");
        Check(session.BoostEnabled&&session.InLobby,"Mouse activated wrong controller-highlighted online action");
        Check(online.ControllerSelection=="course-option:CAR COLLISIONS","Clicked online action takes controller focus");
        File.WriteAllText(Path.Combine(root,"pointer-phase.txt"),"open-music");
        yield return Until(()=>host.RaceMusicMenu.IsOpen,60,"Mouse opens song selection");
        int initial=host.RaceMusicMenu.HighlightedTrackId,chosen=int.MinValue;host.RaceMusicMenu.Selected+=id=>chosen=id;
        File.WriteAllText(Path.Combine(root,"pointer-phase.txt"),"choose-music");
        yield return Until(()=>chosen!=int.MinValue,60,"Mouse chooses song");
        Check(chosen!=initial,"Mouse selected a different song from controller highlight");
        Check(!host.RaceMusicMenu.IsOpen,"Mouse selected song only once and closed chooser");
        observations.Add("Real mouse clicks with injected connected pad: settings row, lobby collisions while Boost focused, and song distinct from initial highlight.");
        session.LeaveRoom();Finish(true,null);
    }
    private void CheckTitle(string message){Check((host.Status.flags&1u)!=0&&host.Status.frontendStage==0&&(host.Status.flags&2u)==0,message);}
    private IEnumerator OptionsExitRegression(){
        CheckTitle("Options exit regression starts at title");
        physicalKey=KeyCode.C;yield return ExpectOpen("options-exit");physicalKey=KeyCode.None;yield return Frames(5);
        options.Draft.masterVolume=.6f;Check(options.ApplyDraft(),"Attract options apply");
        padConnected=true;steeringAxis=22000;yield return Frames(3);menu.Back();yield return Frames(12);
        Check(!menu.IsOpen&&!host.ControlBindings.SuppressInput,"Attract options retained a binding lock");
        Check((host.DiagnosticSubmittedInput.flags&2u)==0&&host.DiagnosticSubmittedInput.thumbLX==22000,"Attract exit swallowed held steering");
        observations.Add("Attract options edited/applied/closed while stick remains deflected; native input resumes");
        steeringAxis=0;pulse=13;yield return Until(()=>host.Status.frontendStage!=0,5,"Keyboard did not start after options exit");
        pulse=116;yield return Until(()=>host.Status.racePhase==2&&(host.Status.flags&1u)==0,35,"Quick race did not start");
        menu.SetOpen(true);yield return Frames(4);menu.SelectTab(0);options.Draft.engineVolume=.7f;Check(options.ApplyDraft(),"Race options apply");
        throttle=255;heldSteering=true;yield return Frames(3);menu.Back();menu.SetOpen(false);yield return Frames(12);
        Check(!host.ControlBindings.SuppressInput&&(host.DiagnosticSubmittedInput.flags&2u)==0,"Race options retained a global lock");
        Check(host.DiagnosticSubmittedInput.rightTrigger==255&&(host.DiagnosticSubmittedInput.key2&(1u<<(68-64)))!=0,"Held pedal and steering did not resume");
        var callback=typeof(Idas3SceneGame).GetMethod("MusicVisibilityChanged",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
        Check(callback!=null,"Music close callback exists");callback.Invoke(host,new object[]{false});yield return Frames(12);
        Check((host.DiagnosticSubmittedInput.flags&2u)==0&&host.DiagnosticSubmittedInput.rightTrigger==255,"Music exit waited forever for held pedal");
        ulong before=host.Status.simulationTicks;yield return Frames(12);Check(host.Status.simulationTicks>before,"Race stalled after options/music return");
        observations.Add("Race options apply/back/resume and music close callback retain held throttle and steering");
        Finish(true,null);
    }
    private IEnumerator ReportsRegression(){
        CheckTitle("Start regression begins at title");padConnected=true;
        for(int i=0;i<16;++i){
            buttons=0x10;yield return Frames(2);Check((host.Status.flags&8u)!=0,"Repeated Start exited native frontend");
            Check((host.DiagnosticSubmittedInput.key0&(1u<<27))==0,"Controller Start also emitted Escape in frontend");
            buttons=0;yield return Frames(3);
        }
        yield return Until(()=>host.Status.frontendStage!=0,5,"Start did not confirm original title");
        observations.Add("16 repeated controller Start presses kept original frontend running without Escape");
        pulse=116;yield return Until(()=>host.Status.racePhase==2&&(host.Status.flags&1u)==0,45,"Quick Time Attack did not start");
        yield return Delay(1);
        yield return SceneCapture("time-attack-live-hud");
        buttons=0x10;yield return Frames(3);Check(menu.IsOpen,"Controller Start did not pause race");
        buttons=0;yield return Frames(5);buttons=0x10;yield return Frames(3);Check(!menu.IsOpen,"Controller Start did not resume race");
        buttons=0;yield return Frames(5);
        yield return HeldAcceleratorRegression();
        File.WriteAllText(Path.Combine(root,"ISOLATED_MODE_FLOW_TEST.txt"),"Private report regression fixture permission. No normal saves.\n");
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-presentation-check")>=0)yield return FrameCounterCheck();
        Check(Idas3SceneModeFlowFixture(-1)==1,"Native completed section/timeout regression: "+Idas3Native.Error());
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-presentation-check")>=0){
            for(int course=0;course<9;++course){
                Check(Idas3SceneModeFlowFixture(21+course)==1,"Conquered fixture: "+Idas3Native.Error());
                yield return Frames(2);yield return SceneCapture("conquered-"+course);
            }
            foreach(int course in new[]{0,3,4})for(int phase=1;phase<=3;++phase){
                Check(Idas3SceneModeFlowFixture(21+9*phase+course)==1,"Conquered entrance fixture: "+Idas3Native.Error());
                yield return Frames(2);yield return SceneCapture("conquered-"+course+"-entrance-"+phase);
            }
        }
        observations.Add("Native App finish gates: Myogi, Usui, Akina; final section totals and natural timeout checked");
        Finish(true,null);
    }
    private IEnumerator FrameCounterCheck(){
        var before=options.Current.Clone();
        var rows=new List<string>{"show_fps,frames,mean_ms,p95_ms"};
        foreach(bool show in new[]{false,true,true,false}){
            options.BeginEdit();options.Draft.showFps=show;options.Draft.vSync=false;options.Draft.frameRateLimit=240;
            Check(options.ApplyDraft(),"FPS counter options apply");yield return Frames(60);
            var samples=new double[180];double last=Time.realtimeSinceStartupAsDouble;
            for(int i=0;i<samples.Length;++i){yield return null;double now=Time.realtimeSinceStartupAsDouble;samples[i]=(now-last)*1000;last=now;}
            double total=0;foreach(double value in samples)total+=value;Array.Sort(samples);
            rows.Add(show+",180,"+(total/180).ToString("F3",System.Globalization.CultureInfo.InvariantCulture)+","+samples[171].ToString("F3",System.Globalization.CultureInfo.InvariantCulture));
        }
        options.BeginEdit();options.Draft.showFps=before.showFps;options.Draft.vSync=before.vSync;options.Draft.frameRateLimit=before.frameRateLimit;
        Check(options.ApplyDraft(),"Restore private performance settings");
        File.WriteAllLines(Path.Combine(root,"fps-counter-abba.csv"),rows);
    }
    private IEnumerator HeldAcceleratorRegression(){
        // Generic wheel/pedal controls use the same resume path as gamepads.
        var wheel=new Idas3ControlBindings();wheel.Initialize(Path.Combine(root,"wheel-bindings"));
        wheel.SelectControllerProfile("test-wheel","Synthetic wheel",true);
        var pedal=new Idas3ControllerControl{path="axis/pedal",label="Pedal",minimum=-1,maximum=1,value=-1};
        var steering=new Idas3ControllerControl{path="axis/steer",label="Steering",minimum=-1,maximum=1,value=0};
        var controls=new[]{pedal,steering};
        wheel.Poll(k=>false,default,0,controls);wheel.BeginEdit();
        Check(wheel.TrySetDraftControl(Idas3ControlBindings.ActionId.Accelerate,pedal,1,-1),"Bind wheel pedal");
        Check(wheel.TrySetDraftControl(Idas3ControlBindings.ActionId.SteerRight,steering,1,0),"Bind wheel steering");
        Check(wheel.ApplyDraft(),"Save private wheel profile");wheel.Poll(k=>false,default,1,controls);
        pedal.value=1;steering.value=.5f;wheel.Poll(k=>false,default,2,controls);
        wheel.BeginEdit();wheel.CancelEdit(false);wheel.Poll(k=>false,default,3,controls);
        var wheelFrame=new Idas3Native.FrameInput();wheel.ApplyDriving(ref wheelFrame);
        Check(!wheel.SuppressInput&&wheelFrame.rightTrigger==255&&wheelFrame.thumbLX>0,"Held wheel pedal/steering survive resume");
        wheel.BeginEdit();wheel.CancelEdit();wheel.Poll(k=>false,default,4,controls);
        Check(wheel.SuppressInput,"Normal binding-edit release protection must remain enabled");
        observations.Add("Synthetic generic wheel pedal and steering preserved; normal binding-edit release guard retained");
        foreach(int input in new[]{0,1,2}){
            yield return Release();throttle=0;heldAccelerator=heldSteering=false;
            if(input==2){
                host.ControlBindings.BeginEdit();
                Check(host.ControlBindings.TrySetDraftPad(Idas3ControlBindings.ActionId.Accelerate,Idas3ControlBindings.PadInput.A),"Bind accelerator to confirm button");
                Check(host.ControlBindings.ApplyDraft(),"Save private accelerator binding");yield return Release();
            }
            physicalKey=KeyCode.Escape;yield return Frames(3);Check(menu.IsOpen,"Pause opens before held throttle test");
            physicalKey=KeyCode.None;yield return Frames(5);
            heldAccelerator=input==0;throttle=(byte)(input==1?255:0);buttons=(ushort)(input==2?0x1000:0);
            // Hold the pedal while resuming. For A, this same edge activates Resume.
            if(input!=2)physicalKey=KeyCode.Escape;
            yield return Frames(4);Check(!menu.IsOpen,"Held throttle did not permit resume");
            physicalKey=KeyCode.None;heldSteering=true;yield return Frames(5);
            Check(!host.ControlBindings.SuppressInput,"Held accelerator locked binding input after resume");
            var submitted=host.DiagnosticSubmittedInput;
            Check(input==0?(submitted.key2&(1u<<(87-64)))!=0:submitted.rightTrigger==255,"Held accelerator was lost after resume");
            Check((submitted.key2&(1u<<(68-64)))!=0,"Steering was blocked by held accelerator after resume");
            ulong before=host.Status.simulationTicks;yield return Frames(8);
            Check(host.Status.simulationTicks>before&&!menu.IsOpen,"Race must advance without reopening pause");
            observations.Add("Held accelerator resume passed: "+(input==0?"keyboard W":input==1?"controller trigger":"rebound controller A/Resume conflict"));
            heldAccelerator=heldSteering=false;throttle=0;buttons=0;yield return Release();
        }
    }
    private IEnumerator SceneCapture(string name){
        yield return Frames(2);
        var camera=host.GetComponent<Camera>();var previous=camera.targetTexture;
        var target=new RenderTexture(Screen.width,Screen.height,24,RenderTextureFormat.ARGB32);Check(target.Create(),"HUD render target");
        camera.targetTexture=target;host.GetComponent<Idas3SceneRenderer>().ApplyFrame();host.GetComponent<Idas3UnityUi>().ApplyFrame();
        var cameras=new List<Camera>();foreach(var c in Resources.FindObjectsOfTypeAll<Camera>())if(c!=null&&c.enabled&&c.gameObject.activeInHierarchy&&c.targetTexture==target)cameras.Add(c);
        cameras.Sort((a,b)=>a.depth.CompareTo(b.depth));foreach(var c in cameras)c.Render();
        var old=RenderTexture.active;RenderTexture.active=target;var picture=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);
        picture.ReadPixels(new Rect(0,0,target.width,target.height),0,0);picture.Apply();RenderTexture.active=old;
        if(name.StartsWith("conquered-",StringComparison.Ordinal)){
            int side=Mathf.FloorToInt((picture.width-picture.height*4f/3f)*.5f);
            var pixels=picture.GetPixels32();bool clean=true;
            for(int y=0;y<picture.height&&clean;++y)for(int x=0;x<side-1;++x){
                var left=pixels[y*picture.width+x];var right=pixels[y*picture.width+picture.width-1-x];
                if(left.r>1||left.g>1||left.b>1||right.r>1||right.g>1||right.b>1){clean=false;break;}
            }
            Check(clean,"Conquered animation escaped its original screen aperture");
        }
        File.WriteAllBytes(Path.Combine(root,name+".png"),picture.EncodeToPNG());captures.Add(name+".png");
        camera.targetTexture=previous;host.GetComponent<Idas3SceneRenderer>().ApplyFrame();host.GetComponent<Idas3UnityUi>().ApplyFrame();
        Destroy(picture);target.Release();Destroy(target);
    }
    private float NativeMaster(){var value=new Idas3Native.Options{size=(uint)Marshal.SizeOf<Idas3Native.Options>()};Check(Idas3Native.Idas3SceneGetOptions(ref value)==1,"Native options unavailable");return value.masterGain;}
    private void CheckClosed(string message){
        Check(!menu.IsOpen&&!menu.AttractOptions,message);CheckTitle("Closing attract options left Title or paused the source");
        Check(!menu.TryConsumeCommand(out var unused),"Attract options queued a race or quit command");
    }
    private IEnumerator ExpectOpen(string input){
        yield return Until(()=>menu.IsOpen,2,"Held "+input+" did not open attract options");
        Check(menu.AttractOptions&&menu.SelectedTab==0,"Attract hold did not open Audio options directly");
        Check(!menu.AttractPromptVisible,"Attract prompt remained visible under options");
        CheckTitle("Opening attract options changed Title or paused native state");observations.Add(input+"-opened-audio");
    }
    private IEnumerator NativeInstallerRegression(){
        string fixture=Path.Combine(root,"native-installer"),game=Path.Combine(fixture,"game"),session=Path.Combine(fixture,"session");
        Directory.CreateDirectory(game);Directory.CreateDirectory(session);File.WriteAllText(Path.Combine(fixture,"ISOLATED_UPDATE_TEST.txt"),"Private fixture");
        var files=new[]{"InitialDUnity.exe","UnityPlayer.dll","InitialDUnity_Data/globalgamemanagers","InitialDUnity_Data/Managed/Assembly-CSharp.dll"};
        foreach(string name in files){string path=Path.Combine(game,name);Directory.CreateDirectory(Path.GetDirectoryName(path));File.WriteAllText(path,"old");}
        Directory.CreateDirectory(Path.Combine(game,"userdata"));File.WriteAllText(Path.Combine(game,"userdata/card.json"),"save");
        string archive=Path.Combine(session,"game.zip");
        using(var zip=System.IO.Compression.ZipFile.Open(archive,System.IO.Compression.ZipArchiveMode.Create))foreach(string name in files){using(var stream=new StreamWriter(zip.CreateEntry(name).Open()))stream.Write("new");}
        var preparation=System.Threading.Tasks.Task.Run(()=>Idas3UpdateStaging.Prepare(game,session,archive,Idas3UpdateStaging.Hash(archive),false,"1.0.0","1.0.1",0,0,json=>JsonUtility.FromJson<Idas3UpdateStaging.Patch>(json)));
        while(!preparation.IsCompleted)yield return null;
        string plan=preparation.GetAwaiter().GetResult();
        var asset=Resources.Load<TextAsset>("UpdateInstaller.exe");Check(asset!=null,"Native installer bundled in player");
        string helper=Path.Combine(session,"install.exe");File.WriteAllBytes(helper,asset.bytes);
        var start=new System.Diagnostics.ProcessStartInfo(helper){Arguments="\""+plan+"\" --test",UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=session};
        using(var process=System.Diagnostics.Process.Start(start)){
            yield return Until(()=>process.HasExited,20,"Bundled native installer did not finish");
            Check(process.ExitCode==0,"Bundled native installer applied update: "+(File.Exists(Path.Combine(session,"error.txt"))?File.ReadAllText(Path.Combine(session,"error.txt")):""));
        }
        foreach(string name in files)Check(File.ReadAllText(Path.Combine(game,name))=="new","Native installed "+name);
        Check(File.ReadAllText(Path.Combine(game,"userdata/card.json"))=="save","Native install preserved personal save");
    }
    private IEnumerator SimulateUpdateNetworkFailure(){yield return null;throw new IOException("Controlled interrupted download");}
    private IEnumerator SimulateUpdateTransfer(bool fail){yield return null;if(fail)throw new Idas3Updates.PatchUnavailableException("Controlled patch verification failure");}
    private IEnumerator UpdatesRegression(){
        Idas3UpdateChecks.Run(Check);
        yield return NativeInstallerRegression();
        var updates=menu.Updates;Check(updates!=null,"Update service attached");
        if(updates.State==Idas3Updates.CheckState.Idle){
            updates.CheckNow();Check(updates.State==Idas3Updates.CheckState.Checking,"Live anonymous GitHub request began");
            updates.CheckNow();Check(!updates.CanCheck,"Duplicate requests blocked");
        }else Check(Idas3Updates.StartupFinished&&!updates.WindowVisible,"Startup gate finished before native game initialized");
        yield return Until(()=>updates.State!=Idas3Updates.CheckState.Checking,12,"Update request did not time out or finish");
        Check(updates.State==Idas3Updates.CheckState.Current,"Live GitHub Windows release parsed: "+updates.Message);
        observations.Add("Live anonymous GitHub latest release: "+updates.AvailableVersion);
        Check(!updates.CanCheck,"Manual recheck cooldown enforced");
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-updates-download-check")>=0){
            // Exercise the real download/hash/helper failure path using the
            // public 128-byte checksum file as an intentionally invalid ZIP.
            // This cannot pass the helper's archive preflight or replace files.
            string executable=Path.Combine(Path.GetDirectoryName(Application.dataPath),"InitialDUnity.exe");
            byte[] before=File.ReadAllBytes(executable);
            var fixture=JsonUtility.FromJson<Idas3Updates.Release>(Idas3UpdateChecks.Fixture("v99.0.0"));
            fixture.assets[0].size=128;fixture.assets[0].digest="sha256:dbbbb3dfe4dfa8819bed4ec6cb8a3baf0e1af53963d9c077b39b2a56cdfea9da";
            updates.ApplyResponse(200,JsonUtility.ToJson(fixture));
            typeof(Idas3Updates).GetField("downloadUrl",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic).SetValue(updates,
                Idas3Updates.RepositoryUrl+"/releases/download/v0.3.95-community-replays.5/SHA256SUMS.txt");
            updates.Activate();updates.AcceptUpdate();
            Check(updates.State==Idas3Updates.CheckState.Preparing||updates.State==Idas3Updates.CheckState.Downloading,"Confirmed update starts cache preparation or file download");
            yield return Until(()=>updates.State==Idas3Updates.CheckState.Unavailable,40,"Invalid update did not report an error");
            Check(updates.Message.Contains("archive")||updates.Message.Contains("Central Directory"),"Downloaded/checksummed data reached ZIP validation: "+updates.Message);
            Check(Convert.ToBase64String(before)==Convert.ToBase64String(File.ReadAllBytes(executable)),"Invalid archive leaves installed executable intact");
            updates.ContinueToGame();Check(host.Ready&&!updates.WindowVisible,"Continue after rejected update");
            observations.Add("Real GitHub file download, background SHA-256 verification and external helper rejection of invalid ZIP; no installed files changed.");
            Finish(true,null);yield break;
        }
        updates.ApplyResponse(0,"",true);
        Check(updates.State==Idas3Updates.CheckState.Unavailable&&host.Ready,"Offline failure leaves game ready");
        physicalKey=KeyCode.C;yield return ExpectOpen("updates-keyboard-C");yield return Release();
        menu.SelectTab(2);for(int i=0;i<7;i++)menu.Navigate(1);
        Check(menu.DiagnosticSelection==8,"Game Updates accessible by navigation");
        observations.Add("Before menu capture: updateWindow="+updates.WindowVisible+" menuOpen="+menu.IsOpen+" menuRepaints="+menu.DiagnosticRepaints);
        Check(!updates.WindowVisible,"Update modal remained open after live check");
        yield return Capture("updates-offline");
        updates.ApplyResponse(200,Idas3UpdateChecks.Fixture("v99.0.0"));
        Check(updates.State==Idas3Updates.CheckState.Available&&updates.CanActivate,"New version enables download action");
        int installs=0;updates.InstallOverride=()=>installs++;
        pulse=13;yield return Frames(5);Check(updates.WindowVisible&&installs==0,"Keyboard confirm opens Yes/No prompt without installing");
        yield return new WaitForEndOfFrame();
        var dialogImage=ScreenCapture.CaptureScreenshotAsTexture();File.WriteAllBytes(Path.Combine(root,"update-yes-no.png"),dialogImage.EncodeToPNG());Destroy(dialogImage);
        updates.HandleWindowInput(false,false,true,false);Check(!updates.WindowVisible&&installs==0,"Default No continues without downloading");yield return Release();
        pulse=13;yield return Frames(5);Check(updates.WindowVisible,"Can reopen update prompt");
        updates.HandleWindowInput(true,false,false,false);updates.HandleWindowInput(true,false,true,false);Check(installs==1&&!updates.DiagnosticRepair,"Update requests normal installation once");updates.ContinueToGame();
        updates.Activate();updates.HandleWindowInput(true,false,true,false);Check(installs==2&&updates.DiagnosticRepair,"Full Repair explicitly selects the full package");updates.ContinueToGame();
        updates.ApplyResponse(200,Idas3UpdateChecks.Fixture("v"+Application.version));updates.Activate();
        Check(updates.WindowVisible&&updates.State==Idas3Updates.CheckState.Current,"Current build can open repair");
        updates.HandleWindowInput(true,false,true,false);Check(installs==3&&updates.DiagnosticRepair,"Same-version Full Repair available");updates.ContinueToGame();
        updates.ApplyResponse(200,Idas3UpdateChecks.Fixture("v99.0.0"));
        yield return Release();padConnected=true;buttons=0x1000;yield return Frames(5);
        Check(updates.WindowVisible,"Controller confirm opens update prompt");updates.ContinueToGame();yield return Release();
        menu.SetWheelNavigation(true);menu.Activate();Check(updates.WindowVisible&&!menu.WheelEditing,"Wheel confirm invokes update prompt");updates.ContinueToGame();menu.SetWheelNavigation(false);
        updates.InstallOverride=null;
        var patchFixture=JsonUtility.FromJson<Idas3Updates.Release>(Idas3UpdateChecks.Fixture("v99.0.0"));
        var patchAsset=new Idas3Updates.Asset{name="Initial-D-Update-from-"+Application.version+"-to-99.0.0-Patch.zip",size=50,state="uploaded",digest="sha256:"+new string('b',64)};
        patchAsset.browser_download_url=Idas3Updates.RepositoryUrl+"/releases/download/v99.0.0/"+patchAsset.name;
        patchFixture.assets=new[]{patchAsset,patchFixture.assets[0]};
        var attempts=new List<bool>();
        updates.DownloadOverride=patch=>{attempts.Add(patch);return SimulateUpdateTransfer(patch);};
        updates.ApplyResponse(200,JsonUtility.ToJson(patchFixture));updates.AcceptUpdate();yield return Frames(6);
        Check(attempts.Count==2&&attempts[0]&&!attempts[1],"Patch verification failure falls back once to full download");
        attempts.Clear();updates.DownloadOverride=patch=>{attempts.Add(patch);return SimulateUpdateTransfer(false);};
        updates.ApplyResponse(200,JsonUtility.ToJson(patchFixture));updates.AcceptUpdate();yield return Frames(4);
        Check(attempts.Count==1&&attempts[0],"Healthy update downloads only patch");
        attempts.Clear();updates.AcceptRepair();yield return Frames(4);
        Check(attempts.Count==1&&!attempts[0],"Full Repair bypasses patch");
        attempts.Clear();updates.DownloadOverride=patch=>{attempts.Add(patch);return SimulateUpdateTransfer(true);};
        updates.ApplyResponse(200,JsonUtility.ToJson(patchFixture));updates.AcceptUpdate();yield return Frames(6);
        Check(attempts.Count==2&&updates.State==Idas3Updates.CheckState.Unavailable,"Failed full fallback stops without a retry loop");
        attempts.Clear();updates.DownloadOverride=patch=>{attempts.Add(patch);return SimulateUpdateNetworkFailure();};
        updates.ApplyResponse(200,JsonUtility.ToJson(patchFixture));updates.AcceptUpdate();yield return Frames(4);
        Check(attempts.Count==1&&attempts[0]&&updates.State==Idas3Updates.CheckState.Unavailable,"Network failure does not trigger a large full download");
        updates.DownloadOverride=null;updates.ApplyResponse(200,Idas3UpdateChecks.Fixture("v99.0.0"));
        Check(!options.HasUnsavedChanges,"Update action does not change game settings");
        foreach(var size in new[]{new Vector2Int(640,480),new Vector2Int(1280,720),new Vector2Int(1920,800)}){
            yield return Resize(size.x,size.y,false);yield return Capture("updates-available-"+size.x+"x"+size.y,size.x,size.y);
        }
        menu.Navigate(-1);Check(menu.DiagnosticSelection==7,"Full Tune remains next to updates");
        menu.Back();Check(menu.CategoryFocused,"Back returns to categories");menu.Back();yield return Release();
        yield return Until(()=>menu.AttractPromptVisible,3,"Returned to title prompt");yield return Capture("updates-title-notice");
        pulse=13;yield return Until(()=>host.Status.frontendStage!=0,8,"Game can start after update check");
        Check(!menu.IsOpen&&!menu.AttractPromptVisible,"Update notice stays out of gameplay");
        Finish(true,null);
    }
    private IEnumerator BrowseHudPickerTo(int style){
        var customization=menu.HudCustomization;
        Check(customization!=null&&customization.PickerOpen,"Meter picker is open before browsing");
        int count=Idas3ArcadeMeterCatalog.Count;
        for(int step=0;step<count&&customization.Draft.hudMeterStyle!=style;++step){
            menu.Navigate(1);yield return Frames(1);
        }
        Check(customization.Draft.hudMeterStyle==style&&customization.PickerIndex==Idas3ArcadeMeterCatalog.IndexOfStyle(style),"Picker navigation reaches saved style "+style);
    }
    private void SelectHudCustomizationRow(int row){
        var panel=menu.HudCustomization;
        Check(panel!=null&&panel.IsOpen&&!panel.PickerOpen,"HUD customization actions are available");
        for(int step=0;step<12&&panel.SelectedRow!=row;++step)menu.Navigate(1);
        Check(panel.SelectedRow==row,"HUD customization navigation reaches action "+row);
    }
    private void CheckHudPreviewPixels(string name){
        var image=new Texture2D(2,2,TextureFormat.RGB24,false);
        try{
            Check(image.LoadImage(File.ReadAllBytes(Path.Combine(root,name+".png"))),"HUD preview capture can be decoded");
            float scale=Mathf.Min(1.5f,Mathf.Min(image.width/1072f,image.height/704f));
            float originX=(image.width-1040*scale)*.5f,originY=(image.height-680*scale)*.5f;
            int left=Mathf.CeilToInt(originX+404*scale),right=Mathf.FloorToInt(originX+988*scale);
            int top=Mathf.CeilToInt(originY+192*scale),bottom=Mathf.FloorToInt(originY+460*scale),visible=0;
            for(int y=top;y<bottom;++y)for(int x=left;x<right;++x){
                Color32 pixel=image.GetPixel(x,image.height-1-y);
                if(pixel.r>48||pixel.g>48||pixel.b>48)++visible;
            }
            Check(visible>(right-left)*(bottom-top)/200,"Selected artwork appears inside the actual OnGUI preview: "+name);
        }finally{Destroy(image);}
    }
    private IEnumerator HudCatalogPickerRegression(){
        int count=Idas3ArcadeMeterCatalog.Count;
        Check(count==88,"Meter picker contains Original plus all 87 recovered meters");
        var seen=new HashSet<int>();
        for(int index=0;index<count;++index)Check(seen.Add(Idas3ArcadeMeterCatalog.StyleAt(index)),"Meter picker saved IDs are unique");
        const int analogStyle=3,characterStyle=51;
        int lastStyle=Idas3ArcadeMeterCatalog.StyleAt(count-1);
        Check(lastStyle==91&&Idas3ArcadeMeterCatalog.SourceId(lastStyle)==89,"Last picker entry retains recovered source ID 89");
        menu.Activate();yield return Frames(2);menu.Activate();
        var customization=menu.HudCustomization;
        Check(customization.PickerOpen&&customization.PickerIndex==1&&customization.PickerScroll==0,"Picker opens on saved Stuttgart within the initial rows");
        yield return Capture("meter-picker-initial");
        yield return BrowseHudPickerTo(analogStyle);menu.Activate();yield return Frames(2);
        Check(!customization.PickerOpen&&options.Current.hudMeterStyle==1,"Imported analog preview leaves saved Stuttgart unchanged");
        yield return Capture("imported-analog-customization");CheckHudPreviewPixels("imported-analog-customization");
        menu.Activate();yield return BrowseHudPickerTo(characterStyle);
        Check(customization.PickerScroll>0&&customization.PickerIndex>=customization.PickerScroll&&customization.PickerIndex<customization.PickerScroll+9,"Browsing reveals selected meter in scrolled picker");
        yield return Capture("meter-picker-scrolled");menu.Activate();yield return Frames(2);
        yield return Capture("imported-character-customization");CheckHudPreviewPixels("imported-character-customization");
        menu.Activate();yield return BrowseHudPickerTo(lastStyle);
        Check(customization.PickerIndex==count-1&&Mathf.Approximately(customization.PickerScroll,count-9),"Last catalog entry scrolls into the final viewport");
        yield return Capture("meter-picker-last");menu.Activate();
        SelectHudCustomizationRow(6);menu.Activate();yield return Frames(3);
        Check(!menu.CustomizingHud&&options.Current.hudMeterStyle==lastStyle,"Apply persists the high saved meter ID from the picker");
        var reload=new Idas3GameOptions(new OptionsTestPlatform());reload.Initialize(Path.GetDirectoryName(options.FilePath));
        Check(reload.Current.hudMeterStyle==lastStyle,"High meter ID survives options reload");
        string saved=File.ReadAllText(options.FilePath);
        menu.Activate();yield return Frames(2);menu.Activate();
        Check(customization.PickerIndex==count-1,"Reopened picker locates the saved high meter ID");
        menu.Navigate(1);
        Check(customization.PickerIndex==0&&customization.Draft.hudMeterStyle==0&&customization.PickerScroll==0,"Picker wraps from last meter to Original");
        menu.Back();Check(menu.CustomizingHud&&!customization.PickerOpen,"Back dismisses picker before cancelling customization");
        menu.Back();yield return Frames(3);
        Check(!menu.CustomizingHud&&options.Current.hudMeterStyle==lastStyle&&File.ReadAllText(options.FilePath)==saved,"Cancel restores prior high meter appearance and saved file");
        menu.Activate();yield return Frames(2);menu.Activate();yield return BrowseHudPickerTo(1);menu.Activate();
        SelectHudCustomizationRow(6);menu.Activate();yield return Frames(3);
        Check(options.Current.hudMeterStyle==1&&options.Current.hudNameplateStyle==1,"Picker restores Stuttgart for existing layout and live telemetry checks");
        observations.Add("Actual OnGUI catalog picker: 88 unique styles, initial/scrolled/final rows, Infinity and Reimu previews, high style91 Apply/reload, wrap to Original, Cancel preservation, Stuttgart restored.");
    }
    private IEnumerator BrowseOrnamentPickerTo(int id){
        var customization=menu.HudCustomization;
        Check(customization!=null&&customization.OrnamentPicker,"Ornament picker is open before browsing");
        int count=Idas3OrnamentCatalog.Count,target=Idas3OrnamentCatalog.IndexOf(id);
        int forward=(target-customization.PickerIndex+count)%count,backward=(customization.PickerIndex-target+count)%count;
        int direction=forward<=backward?1:-1;
        for(int step=0;step<count&&customization.Draft.hudOrnamentId!=id;++step){menu.Navigate(direction);yield return Frames(1);}
        Check(customization.Draft.hudOrnamentId==id&&customization.PickerIndex==target,"Ornament navigation reaches recovered item "+id);
    }
    private IEnumerator OrnamentPickerRegression(){
        int count=Idas3OrnamentCatalog.Count,firstId=Idas3OrnamentCatalog.IdAt(1),lastId=Idas3OrnamentCatalog.IdAt(count-1);
        Check(count==281&&Idas3OrnamentCatalog.IdAt(0)==0,"Ornament picker contains Off and all 280 recovered models");
        Check(options.Current.hudOrnamentId==0&&!Idas3OrnamentRenderer.PreviewLoaded,"Ornaments are opt-in with no idle preview renderer");
        menu.Activate();menu.NavigateHorizontal(-1);menu.Navigate(1);menu.Activate();
        var customization=menu.HudCustomization;
        Check(customization.OrnamentPicker&&customization.PickerIndex==0&&customization.Draft.hudMeterStyle==0,"Original HUD retains access to the ornament picker");
        yield return Capture("ornament-picker-initial");
        yield return BrowseOrnamentPickerTo(firstId);menu.Activate();yield return Frames(2);
        yield return Capture("ornament-ae86-preview");CheckHudPreviewPixels("ornament-ae86-preview");
        Check(Idas3OrnamentRenderer.PreviewLoaded&&Idas3OrnamentRenderer.ResidentTextureCount>0,"The private ornament preview loads recovered model resources");
        Check(options.Current.hudMeterStyle==1&&options.Current.hudOrnamentId==0,"Preview does not change saved meter or ornament");
        menu.Activate();yield return BrowseOrnamentPickerTo(Idas3OrnamentCatalog.IdAt(12));
        Check(customization.PickerScroll>0&&customization.PickerIndex>=customization.PickerScroll&&customization.PickerIndex<customization.PickerScroll+9,"Scrolled ornament picker keeps the selected model visible");
        yield return Capture("ornament-picker-scrolled");
        yield return BrowseOrnamentPickerTo(lastId);
        Check(customization.PickerIndex==count-1&&Mathf.Approximately(customization.PickerScroll,count-9),"Final ornament is visible in the final scrollbar viewport");
        yield return Capture("ornament-picker-last");CheckHudPreviewPixels("ornament-picker-last");
        menu.Activate();SelectHudCustomizationRow(6);menu.Activate();yield return Frames(3);
        Check(!menu.CustomizingHud&&options.Current.hudOrnamentId==lastId&&options.Current.hudMeterStyle==0,"Apply saves the final ornament with the original HUD");
        Check(!Idas3OrnamentRenderer.PreviewLoaded&&!host.GetComponent<Idas3UnityUi>().OrnamentVisible,"Closing the picker releases its renderer and the ornament stays out of attract screens");
        var reload=new Idas3GameOptions(new OptionsTestPlatform());reload.Initialize(Path.GetDirectoryName(options.FilePath));
        Check(reload.Current.hudOrnamentId==lastId&&reload.Current.hudMeterStyle==0,"Ornament and original meter selections survive reload");
        string saved=File.ReadAllText(options.FilePath);
        menu.Activate();menu.Navigate(1);menu.Activate();
        Check(customization.OrnamentPicker&&customization.PickerIndex==count-1,"Reopened ornament picker locates the saved final entry");
        menu.Navigate(1);yield return Frames(2);
        Check(customization.PickerIndex==0&&customization.Draft.hudOrnamentId==0&&customization.PickerScroll==0,"Ornaments wrap through Off");
        yield return Capture("ornament-off-preview");
        Check(!Idas3OrnamentRenderer.PreviewLoaded,"Off releases the private preview model");
        menu.Back();Check(menu.CustomizingHud&&!customization.PickerOpen&&customization.SelectedRow==4,"Back closes the ornament picker and returns to its row");
        menu.Back();yield return Frames(2);
        Check(!menu.CustomizingHud&&options.Current.hudOrnamentId==lastId&&File.ReadAllText(options.FilePath)==saved,"Cancel preserves the saved ornament and settings file");
        menu.Activate();menu.NavigateHorizontal(1);for(int row=0;row<4;++row)menu.Navigate(1);menu.Activate();
        yield return BrowseOrnamentPickerTo(firstId);menu.Activate();SelectHudCustomizationRow(6);menu.Activate();yield return Frames(3);
        Check(options.Current.hudMeterStyle==1&&options.Current.hudNameplateStyle==1&&options.Current.hudOrnamentId==firstId,"Ornament selection preserves meter options and restores Stuttgart for live checks");
        observations.Add("Actual OnGUI ornament picker: 281 entries including Off, initial/scrolled/final screenshots, AE86 recovered model preview, Original HUD compatibility, high-ID Apply/reload, Off resource release, Cancel preservation and first ornament selected for live driving.");
    }
    private IEnumerator HudPlacementRegression(){
        var baseline=options.Current.Clone();string saved=File.ReadAllText(options.FilePath);
        menu.Activate();SelectHudCustomizationRow(8);yield return Capture("customization-edit-layout");menu.Activate();yield return Frames(2);
        var editor=menu.HudEditor;
        Check(editor!=null&&editor.IsOpen&&menu.CustomizingHud,"Edit Layout opens inside the customization transaction");
        // Exercise the actual preview geometry, not just the saved percentages.
        int[] resizeGroups={1,2,3,6,7,4,5,8,9,10};
        var initialLayout=editor.Draft.Clone();
        for(int index=0;index<resizeGroups.Length;++index){
            int group=resizeGroups[index];editor.SelectGroup(index);editor.SetSelectedSizePercent(112);editor.Refresh();
            Check(editor.Bounds(group,out var beforeFine),"Missing preview bounds for fine sizing group "+group);
            var beforeOffset=editor.Draft.HudOffset(group);var otherSizes=editor.Draft.Clone();
            editor.ResizeSelected(1);editor.Refresh();
            Check(editor.Draft.HudSizePercent(group)==113&&editor.Bounds(group,out var afterFine)&&Mathf.Abs(afterFine.width/beforeFine.width-113f/112f)<.003f,"A fine adjustment did not produce proportional geometry for group "+group);
            Check(editor.Draft.HudOffset(group)==beforeOffset,"Resizing changed the saved position");
            foreach(int other in resizeGroups)if(other!=group)Check(editor.Draft.HudSizePercent(other)==otherSizes.HudSizePercent(other),"Resizing changed another HUD group");
            editor.SetSelectedSizePercent(150);editor.ResizeSelected(1);
            Check(editor.Draft.HudSizePercent(group)==150,"Upper size limit wrapped to the minimum");
            editor.SetSelectedSizePercent(group==5?100:50);editor.ResizeSelected(-1);
            Check(editor.Draft.HudSizePercent(group)==(group==5?100:50),"Lower size limit wrapped to the maximum");
        }
        // Minimap presets are baked into native geometry. Fine resizing must
        // also work for old 125% and 150% saves without applying size twice.
        editor.SelectGroup(6);
        for(int preset=0;preset<3;++preset){
            editor.Draft.minimapSize=preset;editor.Draft.hudSizePercent[5]=0;editor.Refresh();
            Check(editor.Bounds(5,out var originalMap),"Missing legacy minimap bounds");
            editor.SetSelectedSizePercent(137);editor.Refresh();
            Check(editor.Bounds(5,out var resizedMap)&&Mathf.Abs(resizedMap.width/originalMap.width-137f/(100+25*preset))<.003f,"Fine minimap sizing double-scaled its legacy preset");
        }
        Idas3GameOptions.CopyHudLayout(initialLayout,editor.Draft);editor.SelectGroup(1);editor.Refresh();
        bool meterReady=editor.Bounds(2,out var meterBefore),chainReady=editor.Bounds(10,out var chainBefore);
        Check(meterReady&&chainReady,"Both custom meter and keychain have draggable preview bounds at attract");
        editor.SelectGroup(1);editor.MoveSelected(new Vector2(-90,-30));editor.ResizeSelected(1);editor.Refresh();
        Check(editor.Draft.HudSizePercent(2)==baseline.HudSizePercent(2)+1,"Meter resizing still jumps by a preset");
        editor.SetSelectedSizePercent(113);editor.Refresh();
        var meterOffset=editor.Draft.HudOffset(2);
        Check(editor.Draft.HudOffset(10)==baseline.HudOffset(10),"Moving the meter does not move the keychain");
        editor.SelectGroup(9);editor.MoveSelected(new Vector2(-180,85));editor.ResizeSelected(1);editor.Refresh();
        Check(editor.Draft.HudSizePercent(10)==baseline.HudSizePercent(10)+1,"Keychain resizing still jumps by a preset");
        editor.SetSelectedSizePercent(117);editor.Refresh();
        Check(editor.Draft.HudOffset(2)==meterOffset&&editor.Bounds(10,out var chainAfter)&&chainAfter.x<chainBefore.x&&chainAfter.y>chainBefore.y&&chainAfter.width>chainBefore.width,"Keychain moves and resizes independently of the tachometer");
        var moved=editor.Draft.Clone();
        yield return editor.Capture(Path.Combine(root,"hud-layout-meter-keychain.png"));
        editor.Close(true);yield return Frames(2);
        Check(menu.CustomizingHud&&!menu.EditingLayout&&File.ReadAllText(options.FilePath)==saved&&Idas3GameOptions.Equivalent(options.Current,baseline),"Nested Done leaves the save file and live options unchanged");
        Check(menu.HudCustomization.Draft.HudOffset(10)==moved.HudOffset(10)&&menu.HudCustomization.Draft.HudSizePercent(10)==117,"Nested Done returns exact keychain size to the customization draft");
        menu.Back();yield return Frames(2);
        Check(!menu.CustomizingHud&&File.ReadAllText(options.FilePath)==saved,"Outer Cancel discards both placement changes");

        menu.Activate();SelectHudCustomizationRow(8);menu.Activate();yield return Frames(2);editor=menu.HudEditor;
        editor.SelectGroup(9);editor.Refresh();var beforeKeyboard=editor.Draft.HudOffset(10);
        menu.Navigate(1);menu.NavigateHorizontal(-1);editor.Refresh();
        Check(editor.Draft.HudOffset(10).x<beforeKeyboard.x,"Keyboard/controller Move X changes keychain position");
        menu.SetWheelNavigation(true);menu.NavigateHorizontal(1);menu.Activate();menu.NavigateHorizontal(1);menu.Activate();menu.SetWheelNavigation(false);editor.Refresh();
        Check(editor.Draft.HudOffset(10).y>beforeKeyboard.y,"Wheel steering and confirm can move the keychain vertically");
        menu.Navigate(1);menu.NavigateHorizontal(1);
        Check(editor.Draft.HudSizePercent(10)==baseline.HudSizePercent(10)+1,"Controller resize must advance by one percent");
        menu.SetWheelNavigation(true);menu.Activate();menu.NavigateHorizontal(-1);menu.Activate();menu.SetWheelNavigation(false);
        Check(editor.Draft.HudSizePercent(10)==baseline.HudSizePercent(10),"Wheel resize must decrease by one percent");
        editor.ResetSelected();editor.Refresh();
        Check(editor.Draft.HudOffset(10)==Vector2.zero&&editor.Draft.HudSizePercent(10)==100,"Reset selected restores keychain defaults");
        editor.MoveSelected(new Vector2(-180,85));editor.SetSelectedSizePercent(117);editor.Refresh();
        editor.SelectGroup(1);editor.MoveSelected(new Vector2(-90,-30));editor.SetSelectedSizePercent(113);editor.Refresh();
        moved=editor.Draft.Clone();editor.Close(true);yield return Frames(2);SelectHudCustomizationRow(6);menu.Activate();yield return Frames(2);
        Check(!menu.CustomizingHud&&options.Current.HudOffset(2)==moved.HudOffset(2)&&options.Current.HudOffset(10)==moved.HudOffset(10)&&options.Current.HudSizePercent(10)==117&&options.Current.HudSizePercent(2)==113,"Apply saves both independent positions and precise sizes");
        var reload=new Idas3GameOptions(new OptionsTestPlatform());reload.Initialize(Path.GetDirectoryName(options.FilePath));
        Check(reload.Current.HudOffset(2)==moved.HudOffset(2)&&reload.Current.HudOffset(10)==moved.HudOffset(10)&&reload.Current.HudSizePercent(10)==117&&reload.Current.HudSizePercent(2)==113,"Meter and keychain exact sizes survive restart");

        menu.Activate();SelectHudCustomizationRow(4);menu.NavigateHorizontal(-1);SelectHudCustomizationRow(8);menu.Activate();yield return Frames(2);editor=menu.HudEditor;
        Check(!editor.Bounds(10,out _)&&editor.Bounds(2,out _),"Keychain Off removes its layout target without hiding the meter");
        menu.Back();menu.Back();yield return Frames(2);
        Check(options.Current.hudOrnamentId==baseline.hudOrnamentId,"Cancelling nested Off preview keeps the saved keychain");
        observations.Add("Nested Edit Layout: custom meter and keychain preview/move/resize independently; keyboard/controller and wheel movement, reset, Done versus Apply, outer Cancel, Off and restart persistence verified.");
        observations.Add("Fine resize: all ten editable HUD groups change proportionally by 1%, retain independent offsets and sizes, and clamp without wrapping. Precise 113% meter and 117% keychain survive Apply and restart. Minimap legacy 100/125/150% presets resize without double scaling.");
    }
    private void CheckOrnamentRenderPixels(Idas3OrnamentRenderer renderer){
        var target=renderer.Output as RenderTexture;
        Check(target!=null&&target.IsCreated()&&renderer.PartCount>0,"Selected ornament has genuine mesh parts and a rendered viewport");
        var previous=RenderTexture.active;var image=new Texture2D(target.width,target.height,TextureFormat.RGBA32,false);
        try{
            RenderTexture.active=target;image.ReadPixels(new Rect(0,0,target.width,target.height),0,0);image.Apply();
            int opaque=0,transparent=0;
            foreach(var pixel in image.GetPixels32()){if(pixel.a>32)++opaque;else ++transparent;}
            Check(opaque>target.width*target.height/1000&&transparent>target.width*target.height/2,"Recovered ornament geometry is visible against a transparent viewport");
        }finally{RenderTexture.active=previous;Destroy(image);}
    }
    private IEnumerator OrnamentLiveRegression(Idas3UnityUi ui){
        int id=options.Current.hudOrnamentId;
        Check(id!=0&&ui.OrnamentVisible&&ui.OrnamentRenderer!=null&&ui.OrnamentRenderer.SelectedId==id,"Saved ornament appears during a live race");
        Check(Idas3OrnamentRenderer.Read(out var first)&&(first.flags&1)!=0&&first.simulationTicks>0,"Ornament consumes active native car-motion telemetry");
        Check(Idas3OrnamentRenderer.Read(out var repeat)&&repeat.simulationTicks==first.simulationTicks&&repeat.flags==first.flags&&repeat.car==first.car&&repeat.x==first.x&&repeat.y==first.y&&repeat.z==first.z&&repeat.yaw==first.yaw,"Repeated ornament telemetry reads do not mutate the simulation tick or vehicle pose");
        CheckOrnamentRenderPixels(ui.OrnamentRenderer);
        var bounds=Idas3OrnamentRenderer.ScreenBounds(Screen.width,Screen.height);
        Check(bounds.y<=0&&bounds.yMax<Screen.height*.4f&&bounds.xMin>0&&bounds.xMax<Screen.width,"The default ornament anchor remains at the screen top");
        var placed=Idas3OrnamentRenderer.ScreenBounds(Screen.width,Screen.height,options.Current);
        Check(ui.HudBounds(10,out var liveBounds)&&Vector2.Distance(liveBounds.position,placed.position)<.01f&&Mathf.Abs(liveBounds.width-placed.width)<.01f,"Live keychain render uses the saved position and size");
        options.BeginEdit();options.Draft.hudMeterStyle=0;Check(options.ApplyDraft(),"Original HUD can be selected while keeping the ornament");yield return Frames(3);
        Check(!ui.ArcadeMeterVisible&&ui.OrnamentVisible&&ui.HudBounds(2,out var original)&&original.width>10,"Original instruments and ornament coexist");
        yield return SceneCapture("ornament-original-hud-live");
        float maximumSwing=Quaternion.Angle(Quaternion.identity,ui.OrnamentRenderer.Swing);
        float maximumCurve=0;var chainStart=ui.OrnamentRenderer.ChainAttachment;
        heldSteering=true;
        try{
            for(int frame=0;frame<45;++frame){yield return null;float angle=Quaternion.Angle(Quaternion.identity,ui.OrnamentRenderer.Swing);Check(!float.IsNaN(angle)&&!float.IsInfinity(angle),"Live ornament swing is finite");maximumSwing=Mathf.Max(maximumSwing,angle);
                var chain=ui.OrnamentRenderer.Motion;var start=chain.ChainPoint(0);var axis=(chain.AttachmentPosition-start).normalized;
                for(int node=1;node<Idas3OrnamentMotion.ChainNodeCount-1;++node){var delta=chain.ChainPoint(node)-start;maximumCurve=Mathf.Max(maximumCurve,(delta-axis*Vector3.Dot(delta,axis)).magnitude);}
            }
        }finally{heldSteering=false;}
        Check(Idas3OrnamentRenderer.Read(out var after)&&after.simulationTicks>first.simulationTicks,"Native ornament telemetry advances with driving");
        Check((new Vector3(after.x,after.y,after.z)-new Vector3(first.x,first.y,first.z)).sqrMagnitude>.0001f,"Ornament telemetry tracks the moving car position");
        Check(maximumSwing>.05f&&maximumSwing<40f,"The hanging model responds to driving with bounded swing");
        Check(maximumCurve>.0001f,"Live car motion bends the chain instead of rotating one rigid assembly");
        Check(Vector3.Distance(chainStart,ui.OrnamentRenderer.ChainAttachment)>.0001f,"The actual skinned attachment moves with the chain");
        yield return HighFrameRateHudRegression(ui);
        yield return SceneCapture("ornament-driving-swing");
        menu.SetOpen(true);yield return Frames(3);
        bool readPaused=Idas3OrnamentRenderer.Read(out var paused);
        Check(menu.IsOpen&&readPaused&&(paused.flags&2)!=0,"Normal race pause is reflected in ornament telemetry");
        var pausedSwing=ui.OrnamentRenderer.Swing;var pausedAttachment=ui.OrnamentRenderer.ChainAttachment;var pausedRevision=ui.OrnamentRenderer.Motion.Revision;yield return Frames(12);
        Check(Idas3OrnamentRenderer.Read(out var pausedAgain)&&pausedAgain.simulationTicks==paused.simulationTicks&&pausedAgain.x==paused.x&&pausedAgain.y==paused.y&&pausedAgain.z==paused.z&&pausedAgain.yaw==paused.yaw,"Paused vehicle tick and pose remain fixed");
        Check(Quaternion.Angle(pausedSwing,ui.OrnamentRenderer.Swing)<.001f,"The ornament pose freezes while the race is paused");
        Check(ui.OrnamentRenderer.Motion.Revision==pausedRevision&&Vector3.Distance(pausedAttachment,ui.OrnamentRenderer.ChainAttachment)<.000001f,"Pause freezes chain joints and their actual skinned attachment");
        menu.Back();yield return Frames(6);
        Check(!menu.IsOpen&&Idas3OrnamentRenderer.Read(out var resumed)&&(resumed.flags&2)==0&&resumed.simulationTicks>paused.simulationTicks,"Normal Resume restarts ornament telemetry without a new selection");
        options.BeginEdit();options.Draft.hudMeterStyle=1;Check(options.ApplyDraft(),"Custom meter can be restored alongside the ornament");yield return Frames(3);
        Check(ui.ArcadeMeterVisible&&ui.OrnamentVisible,"Imported meter and ornament render together");
        yield return SceneCapture("ornament-stuttgart-live");
        options.BeginEdit();options.Draft.hudOrnamentId=0;Check(options.ApplyDraft(),"Ornament can be disabled during a live race");yield return Frames(3);
        Check(!ui.OrnamentVisible&&Idas3OrnamentRenderer.ResidentTextureCount==0&&!Idas3OrnamentRenderer.PreviewLoaded,"Turning the ornament off hides it and releases its textures");
        yield return SceneCapture("ornament-off-live");
        observations.Add("Native car-motion telemetry is read-only and advances during actual driving; normal Pause freezes tick, car pose and ornament swing, and Resume restarts sampling; selected recovered meshes render with transparency at the screen top, Original and Stuttgart HUD coexist, steering/acceleration produce finite bounded swing (max "+maximumSwing.ToString("F2")+" degrees), and Off releases live resources. Hanging orientation is recorded in full-screen captures for visual review.");
    }
    private IEnumerator HighFrameRateHudRegression(Idas3UnityUi ui){
        var rows=new List<string>{"fps,frame,tick,alpha,rpm,rawRpm,chainX,chainY"};
        bool originalSteering=heldSteering;heldSteering=true;
        try{
            foreach(int fps in new[]{30,60,120,144,240}){
                presentationDelta=1.0/fps;
                int duplicateTicks=0,movingSubframes=0,tachoSubframes=0;
                ulong previousTick=ulong.MaxValue;Vector3 previousEnd=Vector3.zero;float previousRpm=0;
                for(int frame=0;frame<48;++frame){
                    yield return null;
                    Check(Idas3OrnamentRenderer.ReadTiming(out var timing)&&(timing.flags&1)!=0,"Live presentation timing is available");
                    Check(timing.alpha>=0&&timing.alpha<=1,"Native interpolation phase is bounded");
                    Check(Idas3ArcadeHud.Read(out var meter),"Interpolated tachometer telemetry is available");
                    var end=ui.OrnamentRenderer.ChainAttachment;
                    if(timing.simulationTicks==previousTick){
                        ++duplicateTicks;
                        if(Vector3.Distance(end,previousEnd)>.0000001f)++movingSubframes;
                        if(Mathf.Abs(meter.rpm-previousRpm)>.0001f)++tachoSubframes;
                    }
                    rows.Add(string.Format(System.Globalization.CultureInfo.InvariantCulture,"{0},{1},{2},{3:F6},{4:F4},{5:F4},{6:F7},{7:F7}",fps,frame,timing.simulationTicks,timing.alpha,meter.rpm,host.Status.rpm,end.x,end.y));
                    previousTick=timing.simulationTicks;previousEnd=end;previousRpm=meter.rpm;
                }
                if(fps>60){
                    Check(duplicateTicks>0,"High-FPS frames occur between native ticks at "+fps);
                    Check(movingSubframes>0,"Rendered chain moves between native ticks at "+fps);
                    Check(tachoSubframes>0,"RPM presentation moves between native ticks at "+fps);
                }
            }
        }finally{presentationDelta=0;heldSteering=originalSteering;File.WriteAllLines(Path.Combine(root,"hud-subframes.csv"),rows);}
        observations.Add("30/60/120/144/240 FPS input cadence: actual native render phase, tachometer RPM and skinned ornament attachment sampled. At each cadence above60, RPM and chain movement change on rendered frames with no new simulation tick.");
    }
    private IEnumerator HudCustomizationRegression(){
        Check(options.Current.hudMeterStyle==0,"Original HUD is the default");
        Check(Idas3ArcadeHud.Available,"All matching meter resources are bundled");
        Check(Idas3ArcadeHud.TachMaximum(7800)==8000&&Idas3ArcadeHud.TachMaximum(8500)==9000&&Idas3ArcadeHud.TachMaximum(9500)==10000&&Idas3ArcadeHud.TachMaximum(11000)==13000,"Tach scales match car RPM ranges");
        menu.OpenAttractOptions();menu.SelectTab(7);yield return Capture("hud-settings");
        menu.Activate();Check(menu.CustomizingHud,"Customize opens from HUD category");
        menu.NavigateHorizontal(1);menu.Navigate(1);menu.Navigate(1);menu.Navigate(1);menu.Activate();
        Check(options.Current.hudMeterStyle==0,"Live preview does not apply appearance");
        yield return Capture("stuttgart-customization");
        menu.Back();Check(options.Current.hudMeterStyle==0&&!menu.CustomizingHud,"Cancel preserves original appearance");
        menu.Activate();menu.NavigateHorizontal(1);for(int i=0;i<3;i++)menu.Navigate(1);menu.Activate();
        SelectHudCustomizationRow(6);menu.Activate();
        Check(!menu.CustomizingHud&&options.Current.hudMeterStyle==1&&options.Current.hudNameplateStyle==1,"Apply commits custom meter and nameplate");
        var reload=new Idas3GameOptions(new OptionsTestPlatform());reload.Initialize(Path.GetDirectoryName(options.FilePath));
        Check(reload.Current.hudMeterStyle==1&&reload.Current.hudNameplateStyle==1,"Appearance survives reload");
        yield return HudCatalogPickerRegression();
        yield return OrnamentPickerRegression();
        yield return HudPlacementRegression();
        menu.Navigate(1);menu.Activate();var editor=menu.HudEditor;
        Check(editor!=null&&editor.IsOpen,"Layout editor remains available");editor.SelectGroup(1);editor.Refresh();
        Check(editor.Bounds(2,out var before)&&before.width>250,"Custom meter has draggable layout bounds");
        var timer=editor.Draft.HudOffset(1);editor.MoveSelected(new Vector2(-90,-30));editor.ResizeSelected(1);editor.Refresh();
        Check(editor.Bounds(2,out var after)&&after.width>before.width&&editor.Draft.HudOffset(1)==timer,"Move and resize change only the meter group");
        yield return editor.Capture(Path.Combine(root,"stuttgart-layout.png"));editor.Close(false);
        yield return HudEdgePlacementRegression(true);
        foreach(var size in new[]{new Vector2Int(640,480),new Vector2Int(1920,800)}){
            yield return Resize(size.x,size.y,false);menu.OpenHudCustomization();yield return Capture("customization-"+size.x);
            SelectHudCustomizationRow(8);menu.Activate();yield return Frames(2);editor=menu.HudEditor;editor.SelectGroup(9);editor.Refresh();
            Check(editor.Bounds(10,out var keychainRect)&&keychainRect.xMin>=0&&keychainRect.xMax<=Screen.width+.01f&&keychainRect.yMax<=Screen.height+.01f,"Keychain remains reachable after aspect ratio change");
            yield return editor.Capture(Path.Combine(root,"hud-layout-"+size.x+".png"));editor.Close(false);menu.Back();
        }
        yield return Resize(1200,720,false);menu.SetOpen(false);yield return Release();
        pulse=13;yield return Until(()=>host.Status.frontendStage!=0,8,"Game can start after HUD settings");
        pulse=116;yield return Until(()=>host.Status.racePhase==2&&(host.Status.flags&1u)==0,35,"Quick race started for live meter");
        heldAccelerator=true;yield return Frames(90);
        var ui=host.GetComponent<Idas3UnityUi>();Check(ui.ArcadeMeterVisible,"Custom meter replaces original instrument group in race");
        Check(Idas3ArcadeHud.Read(out var telemetry)&&telemetry.gear>=1&&telemetry.gear<=6&&telemetry.throttle>.5f,"Native gear and pedal telemetry follows driving");
        Check(!float.IsNaN(telemetry.rpm)&&telemetry.rpm>=0&&Mathf.Abs(telemetry.speedKmh-host.Status.speedMetresPerSecond*3.6f)<5&&Mathf.Abs(telemetry.rpm-host.Status.rpm)<2500,"Interpolated speed and RPM remain close to live simulation, including gear changes");
        yield return SceneCapture("stuttgart-live-race");
        yield return OrnamentLiveRegression(ui);
        foreach(int style in new[]{3,51}){
            options.BeginEdit();options.Draft.hudMeterStyle=style;Check(options.ApplyDraft(),"Imported meter can be selected during a live race");yield return Frames(3);
            bool importedLive=Idas3ArcadeHud.Read(out var importedTelemetry);
            Check(ui.ArcadeMeterVisible&&importedLive&&(importedTelemetry.flags&1)!=0&&importedTelemetry.throttle>.5f,"Imported meter uses active native race telemetry");
            // Presentation can be up to one tick behind current physics. The
            // exact endpoint/bounds equivalence is checked by the native live
            // regression; this verifies catalog changes retain live sampling.
            Check(!float.IsNaN(importedTelemetry.rpm)&&importedTelemetry.rpm>=0&&Mathf.Abs(importedTelemetry.speedKmh-host.Status.speedMetresPerSecond*3.6f)<5&&Mathf.Abs(importedTelemetry.rpm-host.Status.rpm)<2500,"Imported meter interpolation stays close to the live vehicle after changing style");
            yield return SceneCapture(style==3?"imported-live-race-infinity":"imported-live-race-reimu");
        }
        options.BeginEdit();options.Draft.hudMeterStyle=1;Check(options.ApplyDraft(),"Stuttgart is restored after imported live examples");yield return Frames(2);heldAccelerator=false;
        options.BeginEdit();options.Draft.hudShiftLights=false;options.Draft.hudPedalIndicators=false;options.Draft.hudNameplateStyle=0;Check(options.ApplyDraft(),"Optional indicators can be disabled");yield return Frames(2);
        yield return SceneCapture("stuttgart-minimal-race");
        options.BeginEdit();options.Draft.hudMeterStyle=0;Check(options.ApplyDraft(),"Original meter can be restored");yield return Frames(2);
        Check(!ui.ArcadeMeterVisible&&ui.HudBounds(2,out var original)&&original.width>10,"Original meter is restored without restarting");
        yield return SceneCapture("original-restored-race");
        int beforeTeardown=Idas3ImportedMeter.ResidentTextureCount;
        var teardownHost=new GameObject("HUD preview teardown smoke");
        var teardownOptions=new Idas3GameOptions(new OptionsTestPlatform());teardownOptions.Initialize(Path.Combine(root,"preview-teardown-settings"));teardownOptions.Draft.hudMeterStyle=3;
        var teardownMenu=teardownHost.AddComponent<Idas3PauseMenu>();teardownMenu.Initialize(teardownOptions);
        var teardownPanel=teardownHost.AddComponent<Idas3HudCustomization>();teardownPanel.Open(teardownOptions,teardownMenu);
        Check(Idas3ArcadeHud.PreparePreview(teardownPanel.Draft,.125f)&&Idas3ImportedMeter.ResidentTextureCount>beforeTeardown,"Player teardown fixture acquires imported preview textures");
        yield return Frames(1);Destroy(teardownHost);yield return Frames(2);
        Check(Idas3ArcadeHud.PreviewSpriteCount==0&&Idas3ImportedMeter.ResidentTextureCount==beforeTeardown,"Actual Play Mode OnDestroy releases imported preview resources");
        observations.Add("Source meter31 Stuttgart artwork, dynamic tach scale, live vehicle telemetry, optional pedal/shift/nameplate controls, private Apply/Cancel/reload, layout and small/ultrawide captures.");
        Finish(true,null);
    }
    private IEnumerator HudEdgePlacementRegression(bool capture){
        // Exercise the actual editor, live composed bounds and draft offsets.
        int savedStyle=options.Current.hudMeterStyle;
        menu.OpenHudEditor();yield return Frames(2);var editor=menu.HudEditor;
        Check(editor!=null&&editor.IsOpen,"HUD placement editor opened");
        editor.Draft.hudMeterStyle=86;editor.SelectGroup(1);editor.ResetSelected();editor.Refresh();
        Check(editor.Bounds(2,out var initial),"Youmu has live editor bounds");
        foreach(float direction in new[]{1f,-1f}){
            editor.MoveSelected(Vector2.one*(direction*100000));editor.Refresh();
            Check(editor.Bounds(2,out var edge),"Youmu retained its bounds at the screen edge");
            Check(direction>0?edge.xMax>Screen.width&&edge.yMax>Screen.height:edge.xMin<0&&edge.yMin<0,
                "Youmu decoration still pins the dial away from the screen edge");
            float visibleWidth=Mathf.Min(Screen.width,edge.xMax)-Mathf.Max(0,edge.xMin);
            float visibleHeight=Mathf.Min(Screen.height,edge.yMax)-Mathf.Max(0,edge.yMin);
            Check(visibleWidth>edge.width*.7f&&visibleHeight>edge.height*.7f,"Youmu became unreachable");
            editor.MoveSelected(Vector2.one*(-direction*80));editor.Refresh();
            Check(editor.Bounds(2,out var back)&&Mathf.Abs(back.x-edge.x+direction*80)<.1f&&Mathf.Abs(back.y-edge.y+direction*80)<.1f,
                "Youmu cannot move back from the screen edge");
        }
        if(capture)yield return editor.Capture(Path.Combine(root,"youmu-layout.png"));editor.Close(false);
        Check(options.Current.hudMeterStyle==savedStyle,"Cancelling the Youmu layout changed saved appearance");
    }
    private IEnumerator Run(){
        yield return Frames(5);Check(host.Ready,"Player initialized");host.DiagnosticFocusOverride=true;
        Check(host.ControllerDevices.Select("keyboard"),"Could not isolate physical-input injection");yield return Release();
        if(HudEdgePlacementCheck){menu.OpenAttractOptions();yield return HudEdgePlacementRegression(false);menu.SetOpen(false);Finish(true,null);yield break;}
        if(HudCustomizationCheck){yield return HudCustomizationRegression();yield break;}
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-ai-options-check")>=0){
            Check(options.Current.aiDifficulty==0,"Default AI must remain Normal");
            Check(Idas3GameOptions.Normalize(new Idas3GameOptions.Values{aiDifficulty=-1}).aiDifficulty==0,"Negative AI setting");
            Check(Idas3GameOptions.Normalize(new Idas3GameOptions.Values{aiDifficulty=99}).aiDifficulty==2,"AI setting cap");
            menu.OpenAttractOptions();menu.SelectTab(2);for(int i=0;i<9;++i)menu.Navigate(1);
            menu.NavigateHorizontal(1);Check(options.Draft.aiDifficulty==1,"Controller did not choose Hard");
            menu.NavigateHorizontal(1);Check(options.Draft.aiDifficulty==2,"Controller did not choose Expert");
            Check(options.Current.aiDifficulty==0,"Unapplied difficulty leaked");menu.Navigate(1);menu.Navigate(1);menu.Activate();menu.Navigate(-1);menu.Navigate(-1);
            Check(options.Current.aiDifficulty==2&&Idas3SceneModeFlowValue(34)==2,"Difficulty not applied to native owner");
            var reloaded=new Idas3GameOptions(new OptionsTestPlatform());reloaded.Initialize(Path.GetDirectoryName(options.FilePath));
            Check(reloaded.Current.aiDifficulty==2,"Difficulty did not persist");
            yield return Frames(3);
            observations.Add("AI capture: open="+menu.IsOpen+" attract="+menu.AttractOptions+" updates="+(menu.Updates!=null&&menu.Updates.WindowVisible)+" repaints="+menu.DiagnosticRepaints);
            Check(menu.IsOpen,"AI settings closed before capture");
            if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-ai-no-capture")<0){
            var testCamera=host.GetComponent<Camera>();var previousTarget=testCamera.targetTexture;
            var testTarget=new RenderTexture(Screen.width,Screen.height,24);testTarget.Create();testCamera.targetTexture=testTarget;
            try{yield return Capture("ai-gameplay-options");}finally{testCamera.targetTexture=previousTarget;testTarget.Release();Destroy(testTarget);}
            }
            menu.NavigateHorizontal(1);Check(options.Draft.aiDifficulty==0,"Expert should wrap to Normal");menu.Back();menu.Back();
            Check(options.Current.aiDifficulty==2,"Cancel changed saved difficulty");Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-hud-editor-check")>=0){
            menu.OpenAttractOptions();menu.SelectTab(7);menu.Navigate(1);menu.Activate();yield return Frames(3);
            var editor=menu.HudEditor;Check(editor!=null&&editor.IsOpen,"Live editor did not open from HUD settings");
            Check(editor.Draft.HudGroupScale(0)==1&&editor.Draft.HudOffset(0)==Vector2.zero,"Race announcements must remain fixed");
            var legacy=new Idas3GameOptions.Values();legacy.hudPositions[6]=new Vector2(-.1f,.15f);
            var migrated=Idas3GameOptions.Normalize(legacy);Check(migrated.HudOffset(3)==legacy.hudPositions[6]&&migrated.HudOffset(7)==migrated.HudOffset(3),"Legacy battle position was lost");
            legacy.hudPositions[3]=new Vector2(-.2f,.05f);migrated=Idas3GameOptions.Normalize(legacy);
            Check(migrated.HudOffset(6)==legacy.hudPositions[3],"Existing Time Attack position must take precedence");
            var groups=new[]{1,2,3,6,7,4,5,8,9};
            for(int i=0;i<groups.Length;++i){
                editor.SelectGroup(i);editor.Refresh();Check(editor.Bounds(groups[i],out var bounds)&&bounds.width>5&&bounds.height>5,"Missing draggable preview group "+groups[i]);
                var before=editor.Draft.Clone();editor.MoveSelected((new Vector2(Screen.width*.5f,Screen.height*.5f)-bounds.center)*.2f);
                Check(editor.Draft.HudOffset(groups[i])!=before.HudOffset(groups[i]),"Drag did not move group "+groups[i]);
                for(int other=1;other<10;++other)if(Idas3GameOptions.Values.HudPositionGroup(other)!=Idas3GameOptions.Values.HudPositionGroup(groups[i]))Check(editor.Draft.HudOffset(other)==before.HudOffset(other),"Drag moved an unrelated group");
                Check(editor.Draft.HudOffset(3)==editor.Draft.HudOffset(6)&&editor.Draft.HudOffset(3)==editor.Draft.HudOffset(7),"Time Attack and battle panels must share a position");
                Check(options.Current.HudOffset(groups[i])==Vector2.zero,"Preview changed saved layout before Save");
                editor.ResizeSelected(1);editor.Refresh();
            }
            editor.SetThirdPerson(false);yield return editor.Capture(Path.Combine(root,"editor-bumper.png"));
            editor.SetThirdPerson(true);Check(editor.ThirdPerson,"Third-person preview switch failed");yield return editor.Capture(Path.Combine(root,"editor-third-person.png"));
            foreach(var size in new[]{new Vector2Int(640,480),new Vector2Int(1920,800)}){yield return Resize(size.x,size.y,false);yield return Frames(2);yield return editor.Capture(Path.Combine(root,"editor-"+size.x+".png"));}
            yield return Resize(1200,720,false);
            editor.SelectGroup(2);editor.Refresh();yield return editor.Capture(Path.Combine(root,"shared-time-attack.png"));
            editor.SelectGroup(3);editor.Refresh();yield return editor.Capture(Path.Combine(root,"shared-legend.png"));
            editor.SelectGroup(4);editor.Refresh();yield return editor.Capture(Path.Combine(root,"shared-online.png"));
            var expected=editor.Draft.Clone();editor.Close(true);Check(!editor.IsOpen&&menu.IsOpen,"Save did not return to HUD settings");
            for(int i=0;i<groups.Length;++i)Check(options.Current.HudOffset(groups[i])==expected.HudOffset(groups[i]),"Saved HUD position mismatch");
            var reloaded=new Idas3GameOptions(new OptionsTestPlatform());reloaded.Initialize(Path.GetDirectoryName(options.FilePath));
            Check(Idas3GameOptions.Equivalent(reloaded.Current,options.Current),"HUD layout did not survive reload");
            menu.OpenHudEditor();yield return Frames(2);editor=menu.HudEditor;editor.SelectGroup(0);editor.ResetSelected();editor.Close(false);
            Check(options.Current.HudOffset(1)==expected.HudOffset(1),"Cancel saved the reset layout");
            Check(options.Draft.HudOffset(1)==expected.HudOffset(1),"Cancel changed the pending draft");
            menu.OpenHudEditor();yield return Frames(2);editor=menu.HudEditor;editor.SelectGroup(3);editor.ResetSelected();
            Check(editor.Draft.HudOffset(3)==Vector2.zero&&editor.Draft.HudOffset(6)==Vector2.zero&&editor.Draft.HudOffset(7)==Vector2.zero,"Reset must reset the shared panel position");
            Check(editor.Draft.HudOffset(2)==expected.HudOffset(2),"Panel reset moved speedometer");editor.Close(false);
            menu.SetOpen(false);Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-hud-options-check")>=0){
            Check(options.Current.minimapSize==0&&options.Current.minimapZoom==2,"Existing/default settings must retain original map");
            var oldHud=new Idas3GameOptions.Values();JsonUtility.FromJsonOverwrite("{\"version\":1,\"minimapSize\":1}",oldHud);Check(oldHud.minimapZoom==2&&oldHud.hudTimerSize==2&&oldHud.hudSpeedometerSize==2&&oldHud.hudOnlineSize==2,"Old settings must retain original zoom and independent HUD sizes");
            Check(Idas3GameOptions.Normalize(new Idas3GameOptions.Values{minimapZoom=-1}).minimapZoom==0,"Negative zoom not clamped");
            Check(Idas3GameOptions.Normalize(new Idas3GameOptions.Values{minimapZoom=99}).minimapZoom==2,"Previous zoom-in setting must return to original");
            Check(Idas3GameOptions.Normalize(new Idas3GameOptions.Values{minimapSize=-1}).minimapSize==0,"Negative size not clamped");
            Check(Idas3GameOptions.Normalize(new Idas3GameOptions.Values{minimapSize=99}).minimapSize==2,"Oversize not clamped");
            menu.OpenAttractOptions();menu.SelectTab(7);Check(menu.SelectedTab==7,"HUD category unavailable");menu.Navigate(1);menu.Navigate(1);
            menu.NavigateHorizontal(1);Check(options.Draft.HudSizePercent(5)==101&&options.HasUnsavedChanges,"101% selection not dirty");
            menu.Activate();Check(options.Draft.HudSizePercent(5)==102,"Confirm did not advance by one percent");
            menu.Navigate(1);menu.NavigateHorizontal(1);Check(options.Draft.minimapZoom==1,"Right should zoom out to wider");menu.NavigateHorizontal(1);Check(options.Draft.minimapZoom==0,"Zoom out did not select 50%");
            var sizeGroups=new[]{1,2,3,6,7,4,9,8};
            for(int row=0;row<sizeGroups.Length;++row){
                menu.Navigate(1);menu.NavigateHorizontal(1);
                for(int other=0;other<sizeGroups.Length;++other){
                    Check(options.Draft.HudSizePercent(sizeGroups[other])==(other<=row?101:100),"HUD row changed a different group: "+sizeGroups[other]);
                }
            }
            menu.Navigate(1);menu.Navigate(1);menu.Activate();Check(options.Current.HudSizePercent(5)==102&&options.Current.minimapZoom==0,"HUD Apply lost map settings");
            var hudReloaded=new Idas3GameOptions(new OptionsTestPlatform());hudReloaded.Initialize(Path.GetDirectoryName(options.FilePath));
            foreach(int group in sizeGroups)Check(hudReloaded.Current.HudSizePercent(group)==101,"Independent HUD setting did not survive reload: "+group);
            if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-hud-options-no-capture")<0)yield return Capture("hud-settings");
            menu.SelectTab(7);menu.Navigate(1);menu.Navigate(1);
            options.Draft.SetHudSizePercent(5,150);menu.NavigateHorizontal(1);Check(options.Draft.HudSizePercent(5)==150,"Size wrapped at the upper bound");
            menu.NavigateHorizontal(-1);Check(options.Draft.HudSizePercent(5)==149,"Reverse size selection did not decrease one percent");
            options.Draft.SetHudSizePercent(5,100);menu.NavigateHorizontal(-1);Check(options.Draft.HudSizePercent(5)==100,"Size wrapped at the lower bound");
            menu.SetOpen(false);Finish(true,null);yield break;
        }
        if(PointerCheck){yield return PointerRegression();yield break;}
        if(OptionsExitCheck){yield return OptionsExitRegression();yield break;}
        if(ReportsCheck){yield return ReportsRegression();yield break;}
        if(UpdatesCheck){yield return UpdatesRegression();yield break;}
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-discord-check")>=0){
            Idas3DiscordChecks.Run(Check);
            if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-discord-live-check")>=0){
                using(var rpc=new DiscordRPC.DiscordRpcClient(Idas3DiscordPresence.ApplicationId,autoEvents:false)){
                    bool connected=false,accepted=false;rpc.OnReady+=(s,m)=>connected=true;rpc.OnPresenceUpdate+=(s,m)=>accepted=true;
                    Check(rpc.Initialize(),"Discord transport initialized in Unity player");
                    double end=Time.realtimeSinceStartupAsDouble+12;
                    while(!connected&&Time.realtimeSinceStartupAsDouble<end){rpc.Invoke();yield return null;}
                    Check(connected,"Discord accepted application ID in Unity player");
                    rpc.SetPresence(Idas3DiscordPresence.Build(new Idas3DiscordPresence.Description{details="Testing Rich Presence",state=Idas3DiscordPresence.GameTitle},DateTime.UtcNow));
                    end=Time.realtimeSinceStartupAsDouble+8;
                    while(!accepted&&Time.realtimeSinceStartupAsDouble<end){rpc.Invoke();yield return null;}
                    Check(accepted,"Discord acknowledged logo, activity and leaderboard button");
                }
            }
            menu.OpenAttractOptions();menu.SelectTab(2);
            for(int i=0;i<8;i++)menu.Navigate(1);
            Check(menu.DiagnosticSelection==9,"Discord toggle is controller accessible");menu.Activate();Check(!options.Draft.discordPresence,"Confirm toggles presence");
            menu.Navigate(1);menu.Navigate(1);menu.Activate();Check(!options.Current.discordPresence,"Apply persists presence off");
            menu.SelectTab(2);for(int i=0;i<8;i++)menu.Navigate(1);
            foreach(var size in new[]{new Vector2Int(640,480),new Vector2Int(1280,720)}){yield return Resize(size.x,size.y,false);yield return Capture("discord-gameplay-"+size.x,size.x,size.y);}
            menu.Back();menu.Back();CheckTitle("Discord settings preserve original title");Finish(true,null);yield break;
        }
        CheckTitle("Diagnostic did not begin in original attract mode");
        Check(!options.Current.wheelForceFeedback,"Diagnostic must leave force feedback disabled");
        yield return Until(()=>menu.AttractPromptVisible,2,"Attract options prompt missing");yield return Capture("attract-prompt");
        foreach(var size in new[]{new Vector2Int(640,480),new Vector2Int(1024,768),new Vector2Int(1280,720),new Vector2Int(1920,800)}){
            yield return Resize(size.x,size.y,false);
            Check(menu.AttractPromptVisible&&!menu.IsOpen,"Resolution change lost the attract prompt");
            yield return Capture("attract-prompt-"+size.x+"x"+size.y,size.x,size.y);
        }
        yield return Resize(1200,720,true);
        physicalKey=KeyCode.C;yield return Delay(.15);Check(!menu.IsOpen,"Short View Change tap opened options");
        yield return Release();Check(menu.AttractHoldProgress==0,"Short tap did not reset hold progress");CheckTitle("Short View Change started a game");

        physicalKey=KeyCode.C;yield return Delay(.25);
        Check(!menu.IsOpen&&menu.AttractHoldProgress>0&&menu.AttractHoldProgress<1,"View hold progress did not advance before its threshold");
        host.DiagnosticFocusOverride=false;yield return Delay(.12);
        Check(!menu.IsOpen&&menu.AttractHoldProgress==0,"Focus loss did not cancel a partial hold");
        physicalKey=KeyCode.None;host.DiagnosticFocusOverride=true;yield return Release();
        observations.Add("focus-interruption-reset-hold");

        double holdBegan=Time.realtimeSinceStartupAsDouble;physicalKey=KeyCode.C;
        yield return Delay(.30);Check(!menu.IsOpen,"Attract hold opened before its threshold");yield return ExpectOpen("keyboard-C");
        Check(Time.realtimeSinceStartupAsDouble-holdBegan>=.60,"Attract hold opened before approximately0.65 seconds");
        var unchanged=options.Current.Clone();yield return Delay(.75);
        Check(menu.IsOpen&&menu.AttractOptions&&menu.SelectedTab==0&&!options.HasUnsavedChanges&&Idas3GameOptions.Equivalent(unchanged,options.Current),"Held View Change repeated or activated an options control");
        yield return Release();Check(menu.CategoryFocused,"Settings did not start at category list");menu.Activate();menu.NavigateHorizontal(-1);float applied=options.Draft.masterVolume;
        Check(applied<options.Current.masterVolume&&options.HasUnsavedChanges,"Audio row did not edit its draft");
        Check(NativeMaster()==unchanged.masterVolume,"Audio draft changed native gain before Apply");
        // Audio has four rows: from selection1, five Down actions reach Apply6.
        for(int i=0;i<6;++i)menu.Navigate(1);menu.Activate();yield return Frames(4);
        Check(options.LastError==null&&!options.HasUnsavedChanges&&options.Current.masterVolume==applied&&NativeMaster()==applied,"Attract Audio Apply failed");
        var loaded=new Idas3GameOptions(new OptionsTestPlatform());loaded.Initialize(Path.GetDirectoryName(options.FilePath));
        Check(loaded.LastError==null&&loaded.Current.masterVolume==applied,"Attract Audio Apply did not persist");
        yield return Capture("attract-audio-applied");menu.Back();Check(menu.CategoryFocused,"Back did not return to categories");menu.Back();yield return Release();CheckClosed("Back did not close attract Audio options");

        var bindings=host.ControlBindings;bindings.BeginEdit();
        Check(bindings.TrySetDraftKey(Idas3ControlBindings.ActionId.Camera,Idas3ControlBindings.Slot.Primary,KeyCode.L)&&bindings.ApplyDraft(),"Private View Change key rebind failed");
        yield return Release();physicalKey=KeyCode.C;yield return Delay(.80);
        Check(!menu.IsOpen,"Old Camera key still opened options after rebinding");CheckTitle("Old Camera key left attract mode");yield return Release();
        physicalKey=KeyCode.L;yield return ExpectOpen("remapped-keyboard-L");yield return Release();
        menu.Activate();menu.NavigateHorizontal(-1);Check(options.HasUnsavedChanges,"Cancel fixture did not change a draft");
        string saved=File.ReadAllText(options.FilePath);physicalKey=KeyCode.L;yield return Frames(3);menu.Back();menu.Back();yield return Delay(.80);
        CheckClosed("Back did not close remapped-key options");
        Check(options.Current.masterVolume==applied&&NativeMaster()==applied&&File.ReadAllText(options.FilePath)==saved,"Back applied or saved a cancelled Audio draft");
        Check(!menu.IsOpen,"Continuing to hold View Change reopened cancelled options");yield return Release();
        observations.Add("back-cancel-and-held-release-gate");

        bindings.BeginEdit();Check(bindings.TrySetDraftPad(Idas3ControlBindings.ActionId.Camera,Idas3ControlBindings.PadInput.Y)&&bindings.ApplyDraft(),"Could not restore standard Camera button");
        padConnected=true;yield return Release();buttons=0x8000;yield return ExpectOpen("controller-Y");
        yield return Delay(.70);Check(!options.HasUnsavedChanges&&menu.SelectedTab==0,"Held controller View Change activated options controls");
        yield return Release();menu.Back();yield return Release();CheckClosed("Controller-opened options did not close");

        // A is also the source menu's normal confirm button. Binding View
        // Change to it must consume the pending hold before native confirmation.
        bindings.BeginEdit();Check(bindings.TrySetDraftPad(Idas3ControlBindings.ActionId.Camera,Idas3ControlBindings.PadInput.A)&&bindings.ApplyDraft(),"Could not bind Camera to the controller confirm button");
        yield return Release();buttons=0x1000;yield return Delay(.20);Check(!menu.IsOpen,"Controller A short hold opened too early");CheckTitle("Bound View Change A leaked into native Title confirm");
        yield return ExpectOpen("remapped-controller-A");yield return Delay(.70);
        Check(menu.AttractOptions&&menu.SelectedTab==0&&!options.HasUnsavedChanges,"Held remapped A activated an options button");
        yield return Capture("attract-controller-remap");yield return Release();menu.Back();yield return Release();CheckClosed("Remapped controller options did not close");
        Check(!options.Current.wheelForceFeedback,"Attract tests enabled hardware force feedback");

        pulse=13;yield return Until(()=>host.Status.frontendStage!=0,8,"Released Enter did not start the normal game flow");
        yield return Frames(3);Check(!menu.IsOpen&&!menu.AttractPromptVisible,"Attract options prompt leaked outside Title");
        observations.Add("normal-enter-starts-after-release");Finish(true,null);
    }
    private IEnumerator Resize(int width,int height,bool requireExact){
        Screen.SetResolution(width,height,FullScreenMode.Windowed);
        double deadline=Time.realtimeSinceStartupAsDouble+1.5;
        // A desktop may clamp a window larger than its usable display. Record
        // the actual size in that case rather than labeling it as ultrawide.
        while((Screen.width!=width||Screen.height!=height)&&Time.realtimeSinceStartupAsDouble<deadline)yield return null;
        yield return Until(()=>Screen.width>0&&Screen.height>0&&host.Status.width==Screen.width&&host.Status.height==Screen.height,
            2,"Native rendering did not follow the resized window");
        yield return Frames(3);
        Check(host.Status.width==Screen.width&&host.Status.height==Screen.height,"Window/native output changed during resize settling");
        if(requireExact)Check(Screen.width==width&&Screen.height==height,"Could not restore the original behavioral-test window");
        observations.Add("resolution-request-"+width+"x"+height+"-actual-"+Screen.width+"x"+Screen.height);
    }
    private IEnumerator Capture(string name,int requestedWidth=0,int requestedHeight=0){
        var camera=host.GetComponent<Camera>();var scene=host.GetComponent<Idas3SceneRenderer>();var ui=host.GetComponent<Idas3UnityUi>();var previous=camera.targetTexture;
        var target=new RenderTexture(Screen.width,Screen.height,24,RenderTextureFormat.ARGB32){name="Actual attract options OnGUI capture",antiAliasing=1};
        Check(target.Create(),"Attract capture target failed");camera.targetTexture=target;scene.ApplyFrame();ui.ApplyFrame();
        var cameras=new List<Camera>();foreach(var item in Resources.FindObjectsOfTypeAll<Camera>())if(item!=null&&item.enabled&&item.gameObject.activeInHierarchy&&item.targetTexture==target)cameras.Add(item);
        cameras.Sort((a,b)=>a.depth.CompareTo(b.depth));foreach(var item in cameras)item.Render();
        camera.targetTexture=previous;scene.ApplyFrame();ui.ApplyFrame();int repaints=menu.DiagnosticRepaints;menu.RequestDiagnosticCapture(target);
        yield return Until(()=>menu.DiagnosticCaptureReady,4,"Actual attract OnGUI capture did not repaint");yield return new WaitForEndOfFrame();
        var old=RenderTexture.active;RenderTexture.active=target;var image=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);
        image.ReadPixels(new Rect(0,0,target.width,target.height),0,0);image.Apply();RenderTexture.active=old;
        int visible=0;foreach(var color in image.GetPixels32())if(color.r>24||color.g>24||color.b>24)++visible;
        File.WriteAllBytes(Path.Combine(root,name+".png"),image.EncodeToPNG());captures.Add(name+".png");
        int expectedWidth=requestedWidth>0?requestedWidth:image.width,expectedHeight=requestedHeight>0?requestedHeight:image.height;
        captureDimensions.Add(new CaptureDimensions{file=name+".png",requestedWidth=expectedWidth,requestedHeight=expectedHeight,
            actualWidth=image.width,actualHeight=image.height,requestedSizeSupported=image.width==expectedWidth&&image.height==expectedHeight});
        Check(visible>image.width*image.height/100&&menu.DiagnosticRepaints>repaints,"Attract capture is blank or lacks actual GUI repaint");
        menu.CancelDiagnosticCapture();Destroy(image);target.Release();Destroy(target);
    }
    private void Finish(bool passed,string error){
        if(finished)return;finished=true;physicalKey=KeyCode.None;buttons=0;host.DiagnosticFocusOverride=null;bool stopped=false;
        int finalStage=host.Status.frontendStage;
        try{host.StopNative();stopped=!host.Ready;}catch(Exception e){error=(error??"")+e.Message;passed=false;}
        var report=new Report{passed=passed&&stopped,shutdownComplete=stopped,error=error,applicationVersion=Application.version,
            checks=checks,seconds=Time.realtimeSinceStartupAsDouble-began,finalFrontendStage=finalStage,options=options.Current,
            captures=captures.ToArray(),captureDimensions=captureDimensions.ToArray(),observations=observations.ToArray(),scope=ReportsCheck?"Private-save native/Unity regression: repeated synthetic controller Start, race pause/resume with continuously held keyboard/trigger/rebound A acceleration and steering, original live TA HUD capture, controlled-position finish gates and natural timeout; physical controllers not tested.":"Actual original attract frontend and managed options with private saves. Prompt captures request 640x480, 1024x768, 1280x720 and 1920x800 and report actual dimensions before restoring 1200x720. Synthetic physical keyboard/controller input traverses normal bindings and hold routing, including remapped confirm-button conflict and focus interruption. Apply, Back and persistence use normal options owners. Captures use actual OnGUI Repaint; no guest runtime, race fixture, native pause, or hardware force output."};
        if(PointerCheck)report.scope="Actual Unity settings, local lobby, and music chooser; real OS mouse clicks while a synthetic connected controller highlights a different control. Private saves; no physical controller hardware validation.";
        if(HudCustomizationCheck)report.scope="Standalone player with private saves: actual OnGUI 88-meter and 281-entry ornament pickers, initial/scrolled/final rows, recovered artwork/3D preview pixel checks, high-ID Apply/reload and Cancel, Original HUD compatibility, Stuttgart layout move/resize, small/ultrawide captures, and live native quick-race telemetry. Actual ornament mesh parts, transparent render target, screen-top bounds, movement-responsive swing, and Off resource release are checked. Programmatic normal menu navigation and synthetic keyboard driving; no physical controller or every-car validation.";
        if(HudEdgePlacementCheck)report.scope="Hidden standalone Unity player with private saves: actual HUD editor and composed Youmu bounds, edge placement in both directions, visible reachability, movement back from edges and Cancel preservation. No OnGUI pixel capture or OS mouse input.";
        if(OptionsExitCheck)report.scope="Actual Unity host with private saves and injected keyboard/controller input: attract options apply/close with held axis, keyboard Start, race options apply/back/resume with held throttle/steering, and music visibility close callback. No physical wheel or menu pixel verification.";
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-discord-check")>=0)report.scope="Discord activity state mapping, native snapshot, UTF8 limits, replay descriptions, settings persistence and controller navigation; actual Gameplay captures at 640x480 and 1280x720. Optional live flag checks Discord READY and activity acknowledgement from this Unity player.";
        if(UpdatesCheck)report.scope="GitHub release/version/checksum validation, live anonymous latest-release request, request cooldown, controlled offline/newer-release responses, keyboard/controller/wheel access to Update / Full Repair / Later, same-version repair and patch/full fallback state transitions with controlled transfer failures, options/title captures, and return to game. Installation intercepted here and tested separately by installer fixtures. Private saves only.";
        File.WriteAllText(Path.Combine(root,"report.json"),JsonUtility.ToJson(report,true));Debug.Log((report.passed?"PASS":"FAIL")+" attract options "+error);
#if UNITY_EDITOR
        UnityEditor.EditorApplication.isPlaying=false;
#else
        Application.Quit(report.passed?0:1);
#endif
    }
}
