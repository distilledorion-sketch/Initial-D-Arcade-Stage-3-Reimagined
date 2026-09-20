using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;
using Idas3.Multiplayer;

// Explicit isolated diagnostic only. Online checks use two normal LAN clients.
public sealed class Idas3PauseSmoke : MonoBehaviour
{
    private static string pendingRoot;
    private static Idas3PauseSmoke active;
    private Idas3SceneGame host;
    private Idas3GameOptions options;
    private Idas3PauseMenu menu;
    private string root;
    private int checks,pulse;
    private uint padPulse;
    private bool online,finished,driving;
    private double began;
    private readonly List<Observation> observations=new List<Observation>();
    private readonly List<string> captures=new List<string>();
    private readonly List<int> verifiedControllerResponses=new List<int>();
    private readonly List<float> verifiedSteeringSmoothing=new List<float>();
    [Serializable] private class Observation {
        public string name;public ulong localTicks,remoteTicks;public long received,sent;
        public uint nativeFlags;public double elapsed;public bool menuOpen,applicationFocused;
    }
    [Serializable] private class Report {
        public string schema="idas3-pause-options-smoke-v1",error,scope;
        public bool passed,online,shutdownComplete;public int checks;public double seconds;
        public Observation[] observations;public string[] captures;public Idas3GameOptions.Values options;
        public int[] verifiedControllerResponses;
        public float[] verifiedSteeringSmoothing;
    }
    private sealed class OptionsTestPlatform : Idas3GameOptions.IPlatform {
        public int Width=>1280;public int Height=>720;public int DisplayMode=>0;public double Now=>0;
        public Idas3GameOptions.ResolutionChoice[] Resolutions=>new[]{new Idas3GameOptions.ResolutionChoice(1280,720)};
        public void Apply(Idas3GameOptions.Values previous,Idas3GameOptions.Values next,bool displayChanged){}
    }
    [StructLayout(LayoutKind.Sequential)] private struct NativeOptions {
        public uint size,version;public float masterGain,musicGain,engineGain,effectsGain;
        public uint cameraView,paused,managedPauseOverlay,reserved;
    }
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneGetOptions(ref NativeOptions value);
    public static bool Configure(ref string saves){
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-pause-smoke");if(at<0)return false;
        if(at+1>=args.Length)throw new ArgumentException("Pause diagnostic needs a new output directory.");
        pendingRoot=Path.GetFullPath(args[at+1]);if(Directory.Exists(pendingRoot)||File.Exists(pendingRoot))throw new IOException("Use a new pause diagnostic directory.");
        Directory.CreateDirectory(pendingRoot);saves=Path.Combine(pendingRoot,"userdata");Directory.CreateDirectory(saves);
        File.WriteAllText(Path.Combine(saves,"settings.txt"),"0 0 0 0 0 1 1 0\n");File.WriteAllText(Path.Combine(saves,"native_selection.txt"),"0 0\n");
        File.WriteAllText(Path.Combine(pendingRoot,"ISOLATED_PAUSE_TEST.txt"),"Private diagnostic saves; no ordinary profile loaded.\n");
        Screen.SetResolution(1200,720,FullScreenMode.Windowed);AudioListener.volume=0;return true;
    }
    public static void Attach(Idas3SceneGame game,Idas3GameOptions settings,Idas3PauseMenu pause){
        if(pendingRoot==null)return;active=game.gameObject.AddComponent<Idas3PauseSmoke>();
        active.host=game;active.options=settings;active.menu=pause;active.root=pendingRoot;active.began=Time.realtimeSinceStartupAsDouble;
        active.StartCoroutine(active.Guard(active.Offline()));
    }
    internal static bool PreparePhysicalInput(ref Idas3ControlBindings.PadState pad){
        if(active==null||active.finished)return false;
        // Controller pause now has one owner: the actual binding mapper. Do
        // not bypass it by adding a raw Start bit after action translation.
        pad=new Idas3ControlBindings.PadState{connected=true,buttons=(ushort)active.padPulse};return true;
    }
    internal static bool PrepareFrame(ref Idas3Native.FrameInput frame){
        if(active==null)return true;if(active.finished)return !active.online;
        if(!active.online){uint focused=frame.flags;frame=new Idas3Native.FrameInput{size=(uint)Marshal.SizeOf<Idas3Native.FrameInput>(),flags=focused,deltaSeconds=Math.Min(Time.unscaledDeltaTime,.25)};
            if(active.driving)frame.SetKey(87);}
        if(active.pulse!=0){frame.SetKey(active.pulse);active.pulse=0;}
        if(active.padPulse!=0){frame.padConnected=1;frame.padButtons|=active.padPulse;active.padPulse=0;}
        return true;
    }
    private void Check(bool ok,string reason){++checks;if(!ok)throw new InvalidOperationException(reason+" flags="+host.Status.flags+" ticks="+host.Status.simulationTicks);}
    private IEnumerator Frames(int count){for(int i=0;i<count;++i)yield return null;}
    private IEnumerator Until(Func<bool> predicate,double seconds,string reason){double end=Time.realtimeSinceStartupAsDouble+seconds;while(!predicate()&&Time.realtimeSinceStartupAsDouble<end)yield return null;Check(predicate(),reason);}
    private IEnumerator Guard(IEnumerator routine){
        var stack=new Stack<IEnumerator>();stack.Push(routine);
        while(stack.Count>0&&!finished){object value=null;Exception failure=null;try{if(!stack.Peek().MoveNext()){stack.Pop();continue;}value=stack.Peek().Current;}catch(Exception e){failure=e;}
            if(failure!=null){Finish(false,failure.ToString());yield break;}if(value is IEnumerator child)stack.Push(child);else yield return value;}
    }
    private void Update(){if(active==this&&!online&&!finished){if(host.Failure!=null)Finish(false,host.Failure);else if(Time.realtimeSinceStartupAsDouble-began>150)Finish(false,"Pause diagnostic timeout.");}}
    private void Observe(string name,Idas3MultiplayerSession session=null){observations.Add(new Observation{name=name,elapsed=Time.realtimeSinceStartupAsDouble-began,
        localTicks=session==null?host.Status.simulationTicks:session.LocalSnapshot.raceTicks,remoteTicks=session==null?0:session.RemoteSnapshot.raceTicks,
        received=session==null?0:session.RemoteSnapshotsReceived,sent=session==null?0:session.SnapshotsSent,nativeFlags=host.Status.flags,menuOpen=menu.IsOpen,applicationFocused=Application.isFocused});}
    private IEnumerator Offline(){
        yield return Frames(5);Check(host.Ready,"Game initialized");host.DiagnosticFocusOverride=true;
        Check(options.Current.controllerResponse==0&&Idas3Native.Idas3SceneGetControllerResponse()==0,"Fresh game did not default to Flycast gamepad response");
        Check(host.ControllerDevices.Select("keyboard"),"Could not isolate injected pause controls");yield return Frames(3);driving=true;pulse=116;
        yield return Until(()=>((host.Status.flags&1)==0)&&host.Status.simulationTicks>200,35,"Offline race did not start");
        var wheelState=new Idas3Native.WheelState{size=(uint)Marshal.SizeOf<Idas3Native.WheelState>()};
        Check(wheelState.size==40&&Idas3Native.Idas3SceneGetWheelState(ref wheelState)==1&&wheelState.version==1,"Wheel telemetry native ABI");
        var wheelAgain=new Idas3Native.WheelState{size=40};
        Check(Idas3Native.Idas3SceneGetWheelState(ref wheelAgain)==1&&wheelAgain.simulationTicks==wheelState.simulationTicks,"Reading wheel telemetry advanced the simulation");
        var invalidWheel=new Idas3Native.WheelState{size=39};
        Check(Idas3Native.Idas3SceneGetWheelState(ref invalidWheel)==0,"Wheel telemetry accepted an invalid ABI size");
        pulse=27;yield return Frames(5);Check(menu.IsOpen&&(host.Status.flags&2)!=0,"Escape did not open offline pause");
        wheelState.size=40;Check(Idas3Native.Idas3SceneGetWheelState(ref wheelState)==1&&wheelState.flags==0,"Paused source advertised active force feedback");
        ulong frozen=host.Status.simulationTicks;Observe("offline-paused");yield return new WaitForSecondsRealtime(1.2f);
        Check(host.Status.simulationTicks==frozen,"Offline pause advanced the source solver");Check(menu.BlocksGameInput,"Pause does not block driving input");
        yield return Capture("pause");yield return OptionsChecks();
        menu.SetOpen(false);yield return Until(()=>host.Status.simulationTicks>frozen+30,5,"Closing pause did not resume offline race");
        host.DiagnosticFocusOverride=false;yield return Frames(5);frozen=host.Status.simulationTicks;
        yield return new WaitForSecondsRealtime(.8f);Check((host.Status.flags&2)!=0&&host.Status.simulationTicks==frozen,"Offline focus loss did not freeze race");
        Observe("offline-unfocused");host.DiagnosticFocusOverride=true;yield return Frames(3);menu.SetOpen(false);
        yield return Until(()=>host.Status.simulationTicks>frozen+20,5,"Offline race did not resume after focus recovery");
        padPulse=0x10;yield return Frames(5);Check(menu.IsOpen&&(host.Status.flags&2)!=0,"Controller Start did not open offline pause");
        Observe("offline-start-paused");menu.SetOpen(false);host.DiagnosticFocusOverride=null;Finish(true,null);
    }
    private IEnumerator OptionsChecks(Idas3MultiplayerSession session=null){
        Idas3WheelFeedbackChecks.Run(root);
        string bindingsBefore=JsonUtility.ToJson(host.ControlBindings.Current);
        for(int tab=0;tab<5;++tab){menu.SelectTab(tab);yield return Frames(3);yield return Capture(new[]{"audio","graphics","gameplay","controls","wheel-disabled"}[tab]);}
        yield return ControllerResponseChecks();
        yield return SteeringSmoothingChecks(session);
        yield return WheelOptionsChecks();
        options.BeginEdit();var draft=options.Draft;draft.masterVolume=.55f;draft.musicVolume=.35f;draft.engineVolume=.75f;draft.effectsVolume=.45f;
        draft.vSync=false;draft.frameRateLimit=90;draft.antiAliasing=2;draft.defaultCamera=1;draft.showFps=true;draft.muteWhenUnfocused=false;
        Check(options.ApplyDraft(),"Options Apply failed: "+options.LastError);yield return Frames(3);
        Check(QualitySettings.antiAliasing==2&&QualitySettings.vSyncCount==0&&Application.targetFrameRate==90,"Actual Unity quality settings did not change");
        var native=new NativeOptions{size=(uint)Marshal.SizeOf<NativeOptions>()};Check(native.size==40&&Idas3SceneGetOptions(ref native)==1,"Options native ABI");
        Check(native.masterGain==draft.masterVolume&&native.musicGain==draft.musicVolume&&native.engineGain==draft.engineVolume&&native.effectsGain==draft.effectsVolume,"Native category gains differ from applied values");
        Check(native.cameraView==1,"Native chase default did not apply");Check(native.managedPauseOverlay==1,"Managed pause ownership missing");
        Check(File.Exists(options.FilePath),"Applied settings were not saved");var saved=JsonUtility.FromJson<Idas3GameOptions.Values>(File.ReadAllText(options.FilePath));
        Check(Idas3GameOptions.Equivalent(saved,options.Current),"Saved settings do not match applied settings");
        var reloaded=new Idas3GameOptions();reloaded.Initialize(Path.GetDirectoryName(options.FilePath));Check(Idas3GameOptions.Equivalent(reloaded.Current,options.Current),"Settings did not survive loading a new settings owner");
        if(!online){
            int oldWidth=options.Current.width,oldHeight=options.Current.height;string before=File.ReadAllText(options.FilePath);
            options.BeginEdit();options.Draft.width=oldWidth==1280?1200:1280;options.Draft.height=720;
            Check(options.ApplyDraft()&&options.DisplayConfirmationPending,"Resolution change requires Keep confirmation");
            yield return Frames(12);menu.SelectTab(1);yield return Capture("graphics-confirmation");
            Check(File.ReadAllText(options.FilePath)==before,"Unconfirmed display change persisted");options.RevertDisplay();yield return Frames(12);
            Check(options.Current.width==oldWidth&&options.Current.height==oldHeight&&!options.DisplayConfirmationPending,"Display revert failed");
            options.BeginEdit();options.Draft.width=oldWidth==1280?1200:1280;options.Draft.height=720;
            Check(options.ApplyDraft()&&options.ConfirmDisplay(),"Display Keep failed");yield return Frames(12);
            saved=JsonUtility.FromJson<Idas3GameOptions.Values>(File.ReadAllText(options.FilePath));Check(Idas3GameOptions.Equivalent(saved,options.Current),"Confirmed display settings not saved");
        }
        Check(JsonUtility.ToJson(host.ControlBindings.Current)==bindingsBefore,"Gameplay or wheel options changed controller bindings");
        Observe("options-applied");
    }
    private IEnumerator ControllerResponseChecks(){
        var platform=new OptionsTestPlatform();var fresh=new Idas3GameOptions(platform);
        fresh.Initialize(Path.Combine(root,"response-fresh-options"));
        Check(fresh.Defaults().controllerResponse==0&&fresh.Current.controllerResponse==0,"New options did not default to Flycast gamepad response");
        Check(fresh.Current.steeringSettingsVersion==1&&fresh.Current.steeringDeadzoneGamepad==.1f&&fresh.Current.steeringDeadzonePrevious==.13f&&fresh.Current.steeringDeadzoneWheel==0,"Steering deadzone defaults differ by response");
        string legacyRoot=Path.Combine(root,"response-legacy-options");Directory.CreateDirectory(legacyRoot);
        File.WriteAllText(Path.Combine(legacyRoot,"game-options.json"),"{\"version\":1,\"masterVolume\":0.65,\"width\":1280,\"height\":720,\"defaultCamera\":1,\"showFps\":true,\"antiAliasing\":8}");
        var legacy=new Idas3GameOptions(platform);legacy.Initialize(legacyRoot);
        Check(legacy.LastError==null&&legacy.Current.controllerResponse==0&&legacy.Current.masterVolume==.65f,"Legacy options without response did not load with default response and preserve existing fields");
        Check(legacy.Current.steeringSettingsVersion==1&&legacy.Current.steeringDeadzoneGamepad==.1f&&legacy.Current.steeringDeadzonePrevious==.13f&&legacy.Current.steeringDeadzoneWheel==0,"Old JSON did not migrate steering deadzones");
        Check(legacy.Current.defaultCamera==1&&legacy.Current.showFps&&legacy.Current.antiAliasing==8,"Steering migration reset existing camera or graphics settings");
        Check(!legacy.Current.wheelForceFeedback&&legacy.Current.wheelFeedbackStrength==.35f&&legacy.Current.wheelFeedbackDevice=="","Old JSON lost disabled wheel defaults");
        fresh.BeginEdit();fresh.Draft.steeringDeadzoneGamepad=fresh.Draft.steeringDeadzonePrevious=fresh.Draft.steeringDeadzoneWheel=0;
        Check(fresh.ApplyDraft(),"Could not save explicit zero deadzones");
        var zeroReload=new Idas3GameOptions(platform);zeroReload.Initialize(Path.Combine(root,"response-fresh-options"));
        Check(zeroReload.Current.steeringSettingsVersion==1&&zeroReload.Current.steeringDeadzoneGamepad==0&&zeroReload.Current.steeringDeadzonePrevious==0&&zeroReload.Current.steeringDeadzoneWheel==0,"Saved zero deadzones were treated as missing settings");
        foreach(float invalid in new[]{float.NaN,float.PositiveInfinity,float.NegativeInfinity}){
            var invalidSettings=fresh.Current.Clone();invalidSettings.steeringDeadzoneGamepad=invalidSettings.steeringDeadzonePrevious=invalidSettings.steeringDeadzoneWheel=invalid;
            var normalized=Idas3GameOptions.Normalize(invalidSettings);
            Check(normalized.steeringDeadzoneGamepad==.1f&&normalized.steeringDeadzonePrevious==.13f&&normalized.steeringDeadzoneWheel==0,"Nonfinite deadzones did not use per-response defaults");
        }
        var bounded=fresh.Current.Clone();bounded.steeringDeadzoneGamepad=-1;bounded.steeringDeadzonePrevious=1;bounded.steeringDeadzoneWheel=.2f;
        bounded=Idas3GameOptions.Normalize(bounded);Check(bounded.steeringDeadzoneGamepad==0&&bounded.steeringDeadzonePrevious==.3f&&bounded.steeringDeadzoneWheel==.2f,"Steering deadzone bounds altered another profile");
        foreach(Action<Idas3GameOptions.Values> change in new Action<Idas3GameOptions.Values>[]{
            v=>v.steeringDeadzoneGamepad=.2f,v=>v.steeringDeadzonePrevious=.2f,v=>v.steeringDeadzoneWheel=.2f,
            v=>v.wheelForceFeedback=true,v=>v.wheelFeedbackStrength=.2f,v=>v.wheelFeedbackInvert=true,v=>v.wheelFeedbackDevice="another-device"}){
            var changed=fresh.Current.Clone();change(changed);Check(!Idas3GameOptions.Equivalent(fresh.Current,changed),"New steering or wheel option is absent from equality");
        }
        foreach(int invalid in new[]{-1,3,int.MinValue,int.MaxValue}){
            var invalidOptions=fresh.Current.Clone();invalidOptions.controllerResponse=invalid;
            Check(Idas3GameOptions.Normalize(invalidOptions).controllerResponse==0,"Invalid saved controller response did not normalize to default");
        }
        var different=fresh.Current.Clone();different.controllerResponse=1;
        Check(!Idas3GameOptions.Equivalent(fresh.Current,different),"Controller response is missing from options equality");
        Check(Idas3Native.Idas3SceneGetControllerResponse()==options.Current.controllerResponse,"Native response does not match options at entry");

        // Use the same row navigation and APPLY action as the actual menu.
        // The existing controls tab and binding drafts are not edited here.
        foreach(int expected in new[]{1,2,0}){
            menu.SelectTab(2);menu.Navigate(1);menu.Navigate(1);menu.Navigate(1);
            int prior=options.Current.controllerResponse;
            for(int attempt=0;options.Draft.controllerResponse!=expected&&attempt<3;++attempt)menu.NavigateHorizontal(1);
            Check(options.Draft.controllerResponse==expected,"Gameplay response row could not select "+expected);
            Check(Idas3Native.Idas3SceneGetControllerResponse()==prior,"Draft controller response applied before APPLY");
            Check(options.HasUnsavedChanges==(prior!=expected),"Controller response change was not tracked as an unsaved option");
            float nativeBefore=Idas3Native.Idas3SceneGetSteeringDeadzone();
            float expectedDeadzone=Mathf.Round(options.Draft.SteeringDeadzone*100+1)/100f;
            menu.Navigate(1);menu.NavigateHorizontal(1);
            Check(Mathf.Abs(options.Draft.SteeringDeadzone-expectedDeadzone)<.000001f,"Gameplay deadzone row did not adjust by one percent");
            Check(Idas3Native.Idas3SceneGetSteeringDeadzone()==nativeBefore,"Draft steering deadzone applied before APPLY");
            yield return Frames(2);yield return Capture("gameplay-response-"+expected);
            // Gameplay now has a sixth row (steering smoothing) before its footer.
            menu.Navigate(1);menu.Navigate(1);menu.Navigate(1);menu.Activate();yield return Frames(3);
            Check(options.LastError==null&&options.Current.controllerResponse==expected&&!options.HasUnsavedChanges,"Menu APPLY failed for controller response "+expected);
            Check(Idas3Native.Idas3SceneGetControllerResponse()==expected,"Native response did not change through menu APPLY: "+expected);
            Check(Mathf.Abs(Idas3Native.Idas3SceneGetSteeringDeadzone()-expectedDeadzone)<.000001f,"Native deadzone did not change through menu APPLY");
            Check(File.Exists(options.FilePath),"Controller response apply did not save options");
            var saved=JsonUtility.FromJson<Idas3GameOptions.Values>(File.ReadAllText(options.FilePath));
            Check(saved.controllerResponse==expected,"Controller response was not persisted: "+expected);
            Check(saved.SteeringDeadzone==expectedDeadzone,"Active profile deadzone was not saved");
            var reloaded=new Idas3GameOptions(platform);reloaded.Initialize(Path.GetDirectoryName(options.FilePath));
            Check(reloaded.LastError==null&&reloaded.Current.controllerResponse==expected,"Controller response did not survive reloading: "+expected);
            Check(reloaded.Current.SteeringDeadzone==expectedDeadzone,"Active profile deadzone did not survive reloading");
            foreach(int invalid in new[]{-1,3,int.MinValue,int.MaxValue}){
                Check(Idas3Native.Idas3SceneSetControllerResponse(invalid)!=1,"Native response setter accepted invalid value "+invalid);
                Check(Idas3Native.Idas3SceneGetControllerResponse()==expected,"Rejected native response changed active setting");
            }
            foreach(float invalid in new[]{-.01f,.31f,float.NaN,float.PositiveInfinity,float.NegativeInfinity}){
                Check(Idas3Native.Idas3SceneSetSteeringDeadzone(invalid)!=1,"Native steering deadzone accepted an invalid value");
                Check(Mathf.Abs(Idas3Native.Idas3SceneGetSteeringDeadzone()-expectedDeadzone)<.000001f,"Rejected native deadzone changed the active setting");
            }
            verifiedControllerResponses.Add(expected);
        }
        Check(options.Current.steeringDeadzoneGamepad==.11f&&options.Current.steeringDeadzonePrevious==.14f&&options.Current.steeringDeadzoneWheel==.01f,"Switching response overwrote another profile's saved deadzone");
        Observe("controller-response-applied");
    }
    private IEnumerator SteeringSmoothingChecks(Idas3MultiplayerSession session){
        var platform=new OptionsTestPlatform();string freshRoot=Path.Combine(root,"smoothing-fresh-options");
        var fresh=new Idas3GameOptions(platform);fresh.Initialize(freshRoot);
        Check(fresh.Defaults().steeringSmoothing==0&&fresh.Current.steeringSmoothing==0,"Steering smoothing must default to off");
        Check(options.Current.steeringSmoothing==0&&Idas3Native.Idas3SceneGetSteeringSmoothing()==0,"Default or response changes enabled steering smoothing");
        string legacyRoot=Path.Combine(root,"smoothing-legacy-options");Directory.CreateDirectory(legacyRoot);
        File.WriteAllText(Path.Combine(legacyRoot,"game-options.json"),"{\"version\":1,\"steeringSettingsVersion\":1,\"controllerResponse\":2,\"steeringDeadzoneGamepad\":0.17,\"steeringDeadzonePrevious\":0.22,\"steeringDeadzoneWheel\":0.03,\"masterVolume\":0.65,\"width\":1280,\"height\":720,\"defaultCamera\":1,\"showFps\":true}");
        var legacy=new Idas3GameOptions(platform);legacy.Initialize(legacyRoot);
        Check(legacy.LastError==null&&legacy.Current.steeringSmoothing==0,"Existing options without smoothing must load with smoothing off");
        Check(legacy.Current.controllerResponse==2&&legacy.Current.steeringDeadzoneGamepad==.17f&&legacy.Current.steeringDeadzonePrevious==.22f&&legacy.Current.steeringDeadzoneWheel==.03f&&legacy.Current.masterVolume==.65f&&legacy.Current.defaultCamera==1&&legacy.Current.showFps,"Adding smoothing changed existing saved options");
        foreach(float invalid in new[]{float.NaN,float.PositiveInfinity,float.NegativeInfinity}){
            var value=fresh.Current.Clone();value.steeringSmoothing=invalid;
            Check(Idas3GameOptions.Normalize(value).steeringSmoothing==0,"Nonfinite smoothing did not normalize to off");
        }
        var bounded=fresh.Current.Clone();bounded.steeringSmoothing=-1;
        Check(Idas3GameOptions.Normalize(bounded).steeringSmoothing==0,"Negative smoothing did not clamp to zero");
        bounded.steeringSmoothing=2;Check(Idas3GameOptions.Normalize(bounded).steeringSmoothing==1,"Smoothing above 100 percent did not clamp");
        fresh.BeginEdit();fresh.Draft.steeringSmoothing=.37f;
        Check(fresh.HasUnsavedChanges&&!Idas3GameOptions.Equivalent(fresh.Current,fresh.Draft),"Smoothing is missing from dirty tracking or equality");
        Check(fresh.ApplyDraft(),"Private smoothing settings did not save");
        foreach(int response in new[]{1,2,0}){
            fresh.BeginEdit();fresh.Draft.controllerResponse=response;Check(fresh.ApplyDraft(),"Private response selection failed");
            var reload=new Idas3GameOptions(platform);reload.Initialize(freshRoot);
            Check(reload.LastError==null&&reload.Current.steeringSmoothing==.37f&&reload.Current.controllerResponse==response,"Global smoothing did not survive profile selection and reload");
        }
        fresh.BeginEdit();fresh.Draft.steeringSmoothing=0;Check(fresh.ApplyDraft(),"Explicit zero smoothing did not save");
        var zeroReload=new Idas3GameOptions(platform);zeroReload.Initialize(freshRoot);
        Check(zeroReload.Current.steeringSmoothing==0,"Saved zero smoothing did not survive reloading");

        // Reach the real sixth Gameplay row, including footer navigation, and
        // use its same one-percent controller actions. No input bindings or
        // live force-feedback settings are changed by this diagnostic.
        menu.SelectTab(2);for(int row=0;row<5;++row)menu.Navigate(1);
        string beforeCancel=File.ReadAllText(options.FilePath);
        menu.NavigateHorizontal(-1);Check(options.Draft.steeringSmoothing==0&&!options.HasUnsavedChanges,"Smoothing slider escaped its lower bound");
        menu.NavigateHorizontal(1);Check(options.Draft.steeringSmoothing==.01f&&options.HasUnsavedChanges,"Smoothing row did not increment by one percent");
        yield return Frames(3);
        Check(options.Current.steeringSmoothing==0&&Idas3Native.Idas3SceneGetSteeringSmoothing()==0,"Draft smoothing applied before APPLY");
        Check(File.ReadAllText(options.FilePath)==beforeCancel,"Draft smoothing was saved before APPLY");
        menu.Back();Check(menu.CategoryFocused&&options.HasUnsavedChanges,"Returning to categories lost pending settings");menu.Back();
        Check(options.Draft.steeringSmoothing==0&&!options.HasUnsavedChanges&&options.Current.steeringSmoothing==0,"BACK did not discard draft smoothing");
        Check(Idas3Native.Idas3SceneGetSteeringSmoothing()==0&&File.ReadAllText(options.FilePath)==beforeCancel,"Cancel changed active or saved smoothing");
        // BACK deliberately blocks held controls until the next released input
        // sample. Give the normal binding poll time to observe that release.
        yield return Frames(3);
        Check(!host.ControlBindings.SuppressInput,"BACK release latch did not clear before reopening smoothing options");
        foreach(float expected in new[]{.01f,.37f,1f,0f}){
            menu.SelectTab(2);for(int row=0;row<5;++row)menu.Navigate(1);
            float previous=options.Current.steeringSmoothing;
            for(int step=0;Mathf.Abs(options.Draft.steeringSmoothing-expected)>.000001f&&step<101;++step)
                menu.NavigateHorizontal(options.Draft.steeringSmoothing<expected?1:-1);
            Check(Mathf.Abs(options.Draft.steeringSmoothing-expected)<.000001f,"Smoothing row could not reach requested percentage");
            if(expected==1){menu.NavigateHorizontal(1);Check(options.Draft.steeringSmoothing==1,"Smoothing slider escaped its upper bound");}
            Check(options.HasUnsavedChanges&&options.Current.steeringSmoothing==previous&&Idas3Native.Idas3SceneGetSteeringSmoothing()==previous,"Smoothing draft changed native/current values before APPLY");
            if(expected==.37f||expected==0){yield return Frames(2);yield return Capture(expected==0?"gameplay-smoothing-off":"gameplay-smoothing");}
            menu.Navigate(1);menu.Navigate(1);menu.Activate();yield return Frames(3);
            Check(options.LastError==null&&!options.HasUnsavedChanges&&Mathf.Abs(options.Current.steeringSmoothing-expected)<.000001f,"Smoothing menu APPLY failed");
            Check(Mathf.Abs(Idas3Native.Idas3SceneGetSteeringSmoothing()-expected)<.000001f,"Smoothing menu APPLY did not update native value");
            var saved=JsonUtility.FromJson<Idas3GameOptions.Values>(File.ReadAllText(options.FilePath));
            var reloaded=new Idas3GameOptions(platform);reloaded.Initialize(Path.GetDirectoryName(options.FilePath));
            Check(saved.steeringSmoothing==expected&&reloaded.LastError==null&&reloaded.Current.steeringSmoothing==expected,"Applied smoothing did not persist and reload");
            if(expected==.37f){
                foreach(float invalid in new[]{-.01f,1.01f,float.NaN,float.PositiveInfinity,float.NegativeInfinity}){
                    Check(Idas3Native.Idas3SceneSetSteeringSmoothing(invalid)!=1,"Native smoothing accepted an invalid value");
                    Check(Idas3Native.Idas3SceneGetSteeringSmoothing()==expected,"Rejected smoothing changed the active native setting");
                }
                if(session!=null){Check(menu.IsOpen&&(host.Status.flags&2)==0,"Smoothing options paused the online race");yield return MeasureOnline(session,"online-steering-smoothing");}
            }
            verifiedSteeringSmoothing.Add(expected);
        }
        Check(!options.Current.wheelForceFeedback,"Smoothing diagnostic enabled live force feedback");
        Observe("steering-smoothing-restored-off",session);
    }
    private IEnumerator WheelOptionsChecks(){
        var platform=new OptionsTestPlatform();var fresh=new Idas3GameOptions(platform);fresh.Initialize(Path.Combine(root,"wheel-fresh-options"));
        Check(!fresh.Current.wheelForceFeedback&&fresh.Current.wheelFeedbackStrength==.35f&&!fresh.Current.wheelFeedbackInvert&&fresh.Current.wheelFeedbackDevice=="","Wheel settings must start disabled, automatic, at35 percent");
        foreach(float invalid in new[]{float.NaN,float.PositiveInfinity,float.NegativeInfinity}){
            var candidate=fresh.Current.Clone();candidate.wheelFeedbackStrength=invalid;
            Check(Idas3GameOptions.Normalize(candidate).wheelFeedbackStrength==.35f,"Nonfinite wheel strength did not use its default");
        }
        var limits=fresh.Current.Clone();limits.wheelFeedbackStrength=2;limits.wheelFeedbackDevice="bad\0device";
        var normalized=Idas3GameOptions.Normalize(limits);Check(normalized.wheelFeedbackStrength==1&&normalized.wheelFeedbackDevice=="","Invalid wheel strength/device were not normalized");
        limits.wheelFeedbackStrength=-1;Check(Idas3GameOptions.Normalize(limits).wheelFeedbackStrength==0,"Negative wheel strength was not clamped");
        // The live diagnostic never enables forces or asks hardware to move.
        // Enabled persistence is checked only through a platform with no FFB.
        fresh.BeginEdit();fresh.Draft.wheelForceFeedback=true;fresh.Draft.wheelFeedbackStrength=.42f;fresh.Draft.wheelFeedbackInvert=true;fresh.Draft.wheelFeedbackDevice="private-test-device";
        Check(fresh.HasUnsavedChanges&&fresh.ApplyDraft(),"Private wheel settings did not save");
        var privateReload=new Idas3GameOptions(platform);privateReload.Initialize(Path.Combine(root,"wheel-fresh-options"));
        Check(Idas3GameOptions.Equivalent(privateReload.Current,fresh.Current),"Private wheel options did not survive reloading");

        Check(!options.Current.wheelForceFeedback,"Live diagnostic must leave wheel feedback disabled");
        menu.SelectTab(4);Check(menu.SelectedTab==4,"Wheel tab is unreachable");
        menu.Back();menu.NavigateHorizontal(1);Check(menu.SelectedTab==0,"Wheel-to-audio tab wrap failed");
        menu.NavigateHorizontal(-1);Check(menu.SelectedTab==4,"Audio-to-wheel tab wrap failed");menu.Activate();
        menu.NavigateHorizontal(1);Check(options.Draft.wheelForceFeedback&&!options.Current.wheelForceFeedback,"Wheel enable toggle did not remain a draft");
        menu.NavigateHorizontal(-1);Check(!options.Draft.wheelForceFeedback,"Could not leave live wheel feedback disabled");
        menu.Navigate(1);menu.NavigateHorizontal(1);string device=options.Draft.wheelFeedbackDevice;
        menu.Navigate(1);float strength=Mathf.Round(options.Draft.wheelFeedbackStrength*100+1)/100f;menu.NavigateHorizontal(1);
        Check(options.Draft.wheelFeedbackStrength==strength,"Wheel strength row did not change by one percent");
        menu.Navigate(1);menu.NavigateHorizontal(1);bool invert=options.Draft.wheelFeedbackInvert;
        Check(invert&&!options.Current.wheelFeedbackInvert&&options.HasUnsavedChanges,"Wheel invert did not remain an unsaved draft");
        yield return Frames(2);yield return Capture("wheel-configured-disabled");
        menu.Navigate(1);menu.Navigate(1);menu.Activate();yield return Frames(3);
        Check(options.LastError==null&&!options.Current.wheelForceFeedback&&options.Current.wheelFeedbackStrength==strength&&options.Current.wheelFeedbackInvert==invert&&options.Current.wheelFeedbackDevice==device,"Wheel menu APPLY did not save its four rows");
        var reloaded=new Idas3GameOptions(platform);reloaded.Initialize(Path.GetDirectoryName(options.FilePath));
        Check(Idas3GameOptions.Equivalent(reloaded.Current,options.Current),"Wheel menu options did not survive reloading");
        Observe("wheel-options-applied-disabled");
    }
    public static IEnumerator VerifyOnline(Idas3SceneGame host,Idas3MultiplayerSession session,string directory,string role){
        var test=host.gameObject.AddComponent<Idas3PauseSmoke>();active=test;test.online=true;test.host=host;test.options=host.GameOptions;test.menu=host.PauseMenu;
        test.root=directory;test.began=Time.realtimeSinceStartupAsDouble;
        yield return test.Online(session,role);test.finished=true;test.WriteReport(true,null,false);active=null;Destroy(test);
    }
    private IEnumerator Online(Idas3MultiplayerSession session,string role){
        host.DiagnosticFocusOverride=true;Check(host.ControllerDevices.Select("keyboard"),"Could not isolate injected online pause controls");yield return Frames(3);
        pulse=27;yield return Frames(5);Check(menu.IsOpen,"Online Escape did not open options overlay");
        Check((host.Status.flags&2)==0,"Online Escape paused native race");yield return MeasureOnline(session,"online-overlay");
        yield return OptionsChecks(session);menu.SetOpen(false);yield return Frames(3);
        host.DiagnosticFocusOverride=false;yield return MeasureOnline(session,"online-unfocused");
        Check((host.Status.flags&4)==0,"Forced managed focus loss was not forwarded as native unfocused state");
        host.DiagnosticFocusOverride=true;padPulse=0x10;yield return Frames(5);
        Check(menu.IsOpen,"Online controller Start did not open options overlay");Check((host.Status.flags&2)==0,"Online Start paused native race");
        yield return Capture("online-start");yield return MeasureOnline(session,"online-start-overlay");
        menu.SetOpen(false);host.DiagnosticFocusOverride=null;Observe("online-resumed",session);
    }
    private IEnumerator MeasureOnline(Idas3MultiplayerSession session,string name){
        var local=session.LocalSnapshot.raceTicks;var remote=session.RemoteSnapshot.raceTicks;long received=session.RemoteSnapshotsReceived,sent=session.SnapshotsSent;
        double start=Time.realtimeSinceStartupAsDouble;Observe(name+"-before",session);
        while(Time.realtimeSinceStartupAsDouble-start<1.2){Check((host.Status.flags&2)==0,"Online native pause flag became set");yield return null;}
        Check(session.IsRacing&&session.RaceReleased,"Online race was interrupted");Check(session.LocalSnapshot.raceTicks>local+45&&session.RemoteSnapshot.raceTicks>remote+45,"Both source clocks must advance during options/focus loss");
        Check(session.RemoteSnapshotsReceived>received+15&&session.SnapshotsSent>sent+15,"Snapshot exchange stopped while options open/unfocused");Observe(name+"-after",session);
    }
    private IEnumerator Capture(string name){
        bool displayConfirmation=options.DisplayConfirmationPending;
        var camera=host.GetComponent<Camera>();var scene=host.GetComponent<Idas3SceneRenderer>();var ui=host.GetComponent<Idas3UnityUi>();var previous=camera.targetTexture;
        var target=new RenderTexture(Screen.width,Screen.height,24,RenderTextureFormat.ARGB32){name="Actual pause OnGUI capture",antiAliasing=1};
        Check(target.Create(),"Pause capture target failed");camera.targetTexture=target;scene.ApplyFrame();ui.ApplyFrame();
        var cameras=new List<Camera>();foreach(var item in Resources.FindObjectsOfTypeAll<Camera>())if(item!=null&&item.enabled&&item.gameObject.activeInHierarchy&&item.targetTexture==target)cameras.Add(item);
        cameras.Sort((a,b)=>a.depth.CompareTo(b.depth));foreach(var item in cameras)item.Render();
        camera.targetTexture=previous;scene.ApplyFrame();ui.ApplyFrame();menu.RequestDiagnosticCapture(target);
        yield return Until(()=>menu.DiagnosticCaptureReady,5,"Pause menu did not repaint capture target "+name);yield return new WaitForEndOfFrame();
        var old=RenderTexture.active;RenderTexture.active=target;var image=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);
        image.ReadPixels(new Rect(0,0,target.width,target.height),0,0);image.Apply();RenderTexture.active=old;
        var pixels=image.GetPixels32();int visible=0;foreach(var color in pixels)if(Math.Max(color.r,Math.Max(color.g,color.b))>24)++visible;
        File.WriteAllBytes(Path.Combine(root,"options-"+name+".png"),image.EncodeToPNG());captures.Add("options-"+name+".png");
        Check(visible>image.width*image.height/100&&menu.DiagnosticRepaints>0,"Options image is blank or has no actual menu repaint");
        float scale=Mathf.Min(1.5f,Mathf.Min(image.width/1072f,image.height/704f));
        float left=(image.width-1040*scale)*.5f,top=(image.height-680*scale)*.5f;
        int bestRed=0,samples=0,header=Mathf.RoundToInt(top+(displayConfirmation?204:2)*scale);
        float headerLeft=displayConfirmation?238:20,headerRight=displayConfirmation?802:1020;
        // RenderTexture readback orientation differs between graphics paths.
        // Require the long red header at its known edge, in either row order.
        for(int delta=-3;delta<=3;++delta)for(int orientation=0;orientation<2;++orientation){
            int red=0; samples=0;int row=Mathf.Clamp(orientation==0?header+delta:image.height-1-header+delta,0,image.height-1);
            for(int x=Mathf.RoundToInt(left+headerLeft*scale);x<left+headerRight*scale;x+=4){var pixel=pixels[row*image.width+x];++samples;if(pixel.r>160&&pixel.g<110&&pixel.b<120)++red;}
            bestRed=Math.Max(bestRed,red);
        }
        Check(samples>0&&bestRed>samples*.7f,"Capture "+name+" does not contain the actual "+(displayConfirmation?"confirmation":"pause/options")+" header");
        menu.CancelDiagnosticCapture();Destroy(image);target.Release();Destroy(target);
    }
    private void WriteReport(bool passed,string error,bool stopped){File.WriteAllText(Path.Combine(root,online?"pause-options-report.json":"report.json"),JsonUtility.ToJson(new Report{
        passed=passed,error=error,online=online,shutdownComplete=stopped,checks=checks,seconds=Time.realtimeSinceStartupAsDouble-began,options=options.Current,
        observations=observations.ToArray(),captures=captures.ToArray(),verifiedControllerResponses=verifiedControllerResponses.ToArray(),verifiedSteeringSmoothing=verifiedSteeringSmoothing.ToArray(),scope="Actual game/options owners with isolated saves; keyboard/controller input traverses the normal SceneGame pause router. Controller response/deadzone checks use normal gameplay-row navigation and APPLY, native setters/getters, per-response persistence, bounds and isolated legacy JSON migration. Steering smoothing uses the sixth Gameplay slider, one-percent navigation, BACK cancellation, APPLY, native range checks, legacy-off defaults and global save/reload; online clocks and packets are checked while smoothing options remain open. Wheel rows and persistence are tested with live feedback disabled; enabled persistence uses a platform without hardware output. Focus loss uses its diagnostic override through normal managed/native gates. Options images use real OnGUI Repaint on AA1 target. Online snapshots use normal two-client LAN transport; this is not Steam internet or OS Alt-Tab event coverage."},true));}
    private void Finish(bool passed,string error){
        if(finished)return;finished=true;host.DiagnosticFocusOverride=null;bool stopped=false;
        try{host.StopNative();stopped=!host.Ready;}catch(Exception e){error=(error??"")+e.Message;passed=false;}
        WriteReport(passed&&stopped,error,stopped);Debug.Log((passed?"PASS":"FAIL")+" pause options "+error);
#if UNITY_EDITOR
        UnityEditor.EditorApplication.isPlaying=false;
#else
        Application.Quit(passed?0:1);
#endif
    }
}
