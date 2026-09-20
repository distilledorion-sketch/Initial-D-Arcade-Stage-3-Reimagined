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
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneModeFlowFixture(int scene);
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
    private int pulse,checks;
    private double began;
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
    private void Update(){if(!finished&&(host.Failure!=null||Time.realtimeSinceStartupAsDouble-began>(ReportsCheck?240:45)))Finish(false,host.Failure??"Attract options diagnostic timeout.");}
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
        camera.targetTexture=previous;Destroy(picture);target.Release();Destroy(target);
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
    private IEnumerator UpdatesRegression(){
        Idas3UpdateChecks.Run(Check);
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
            Check(updates.State==Idas3Updates.CheckState.Downloading,"Confirmed update starts file download");
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
        updates.HandleWindowInput(true,false,true,false);Check(installs==1,"Yes requests installation once");updates.ContinueToGame();
        yield return Release();padConnected=true;buttons=0x1000;yield return Frames(5);
        Check(updates.WindowVisible,"Controller confirm opens update prompt");updates.ContinueToGame();yield return Release();
        menu.SetWheelNavigation(true);menu.Activate();Check(updates.WindowVisible&&!menu.WheelEditing,"Wheel confirm invokes update prompt");updates.ContinueToGame();menu.SetWheelNavigation(false);
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
    private IEnumerator Run(){
        yield return Frames(5);Check(host.Ready,"Player initialized");host.DiagnosticFocusOverride=true;
        Check(host.ControllerDevices.Select("keyboard"),"Could not isolate physical-input injection");yield return Release();
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
        for(int i=0;i<5;++i)menu.Navigate(1);menu.Activate();yield return Frames(4);
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
        if(OptionsExitCheck)report.scope="Actual Unity host with private saves and injected keyboard/controller input: attract options apply/close with held axis, keyboard Start, race options apply/back/resume with held throttle/steering, and music visibility close callback. No physical wheel or menu pixel verification.";
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-discord-check")>=0)report.scope="Discord activity state mapping, native snapshot, UTF8 limits, replay descriptions, settings persistence and controller navigation; actual Gameplay captures at 640x480 and 1280x720. Optional live flag checks Discord READY and activity acknowledgement from this Unity player.";
        if(UpdatesCheck)report.scope="GitHub release/version/checksum validation, live anonymous latest-release request, request cooldown, controlled offline/newer-release responses, keyboard/controller/wheel access to Yes/No prompt, explicit Yes and No semantics, options/title captures, and return to game. Installation intercepted here and tested separately by installer fixtures. Private saves only.";
        File.WriteAllText(Path.Combine(root,"report.json"),JsonUtility.ToJson(report,true));Debug.Log((report.passed?"PASS":"FAIL")+" attract options "+error);
#if UNITY_EDITOR
        UnityEditor.EditorApplication.isPlaying=false;
#else
        Application.Quit(report.passed?0:1);
#endif
    }
}
