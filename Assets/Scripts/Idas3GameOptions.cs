using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

// Settings are independent of the pause menu and native race owner. Tests can
// supply a fake platform to exercise validation, persistence and display expiry.
public sealed class Idas3GameOptions
{
    [Serializable] public sealed class Values
    {
        public int version=1;
        public float masterVolume=1,musicVolume=1,engineVolume=1,effectsVolume=1;
        public int displayMode,width=1280,height=720;
        public bool vSync=true;
        public int frameRateLimit=60,antiAliasing=4,defaultCamera,controllerResponse;
        public int steeringSettingsVersion=1;
        public float steeringDeadzoneGamepad=.1f,steeringDeadzonePrevious=.13f,steeringDeadzoneWheel;
        public float steeringSmoothing;
        public bool wheelForceFeedback,wheelFeedbackInvert;
        public float wheelFeedbackStrength=.35f;
        public string wheelFeedbackDevice="";
        public bool showFps,muteWhenUnfocused;
        public bool discordPresence=true;
        public bool communityTimes=true;
        public bool replayTimeAttack=true,replayOnline,replayLegend;
        public bool TimeAttackReplayRequired=>communityTimes||replayTimeAttack;
        public int rainDetail,importedSceneryDetail;
        public float SteeringDeadzone {
            get=>controllerResponse==1?steeringDeadzonePrevious:controllerResponse==2?steeringDeadzoneWheel:steeringDeadzoneGamepad;
            set{if(controllerResponse==1)steeringDeadzonePrevious=value;else if(controllerResponse==2)steeringDeadzoneWheel=value;else steeringDeadzoneGamepad=value;}
        }
        public Values Clone() => (Values)MemberwiseClone();
    }
    public struct ResolutionChoice
    {
        public int width,height;
        public ResolutionChoice(int width,int height){this.width=width;this.height=height;}
        public override string ToString()=>width+" × "+height;
    }
    public interface IPlatform
    {
        int Width {get;}
        int Height {get;}
        int DisplayMode {get;}
        double Now {get;}
        ResolutionChoice[] Resolutions {get;}
        void Apply(Values previous,Values next,bool displayChanged);
    }
    private sealed class UnityPlatform : IPlatform
    {
        public int Width=>Screen.width;
        public int Height=>Screen.height;
        public int DisplayMode=>Screen.fullScreenMode==FullScreenMode.Windowed?0:
            Screen.fullScreenMode==FullScreenMode.ExclusiveFullScreen?2:1;
        public double Now=>Time.realtimeSinceStartupAsDouble;
        public ResolutionChoice[] Resolutions {
            get{
                var choices=new List<ResolutionChoice>();
                Action<int,int> add=(w,h)=>{
                    if(w<640||h<360)return;
                    if(!choices.Exists(item=>item.width==w&&item.height==h))choices.Add(new ResolutionChoice(w,h));
                };
                foreach(var resolution in Screen.resolutions)add(resolution.width,resolution.height);
                add(Screen.width,Screen.height);add(640,360);add(960,540);add(1280,720);add(1600,900);add(1920,1080);
                choices.Sort((a,b)=>{int pixels=((long)a.width*a.height).CompareTo((long)b.width*b.height);
                    return pixels!=0?pixels:a.width.CompareTo(b.width);});
                return choices.ToArray();
            }
        }
        public void Apply(Values previous,Values next,bool displayChanged){
            if(QualitySettings.antiAliasing!=next.antiAliasing)QualitySettings.antiAliasing=next.antiAliasing;
            if(QualitySettings.vSyncCount!=(next.vSync?1:0))QualitySettings.vSyncCount=next.vSync?1:0;
            int cap=next.frameRateLimit==0?-1:next.frameRateLimit;
            if(Application.targetFrameRate!=cap)Application.targetFrameRate=cap;
            if(displayChanged){
                var mode=next.displayMode==0?FullScreenMode.Windowed:
                    next.displayMode==2?FullScreenMode.ExclusiveFullScreen:FullScreenMode.FullScreenWindow;
                Screen.SetResolution(next.width,next.height,mode);
            }
        }
    }
    private readonly IPlatform platform;
    private Values current,draft,rollback;
    private string file;
    private double confirmationDeadline,lastNow;
    public event Action<Values> Changed;
    public Values Current=>current;
    public Values Draft=>draft;
    public string FilePath=>file;
    public string LastError {get;private set;}
    public bool DisplayConfirmationPending=>rollback!=null;
    public double SecondsRemaining=>DisplayConfirmationPending?Math.Max(0,confirmationDeadline-lastNow):0;
    public bool HasUnsavedChanges=>current!=null&&draft!=null&&!Equivalent(current,Normalize(draft));
    public ResolutionChoice[] AvailableResolutions=>platform.Resolutions;
    public Idas3GameOptions(IPlatform environment=null){platform=environment??new UnityPlatform();}
    public Values Defaults(){
        return new Values{width=Math.Max(640,platform.Width),height=Math.Max(360,platform.Height),
            displayMode=Math.Max(0,Math.Min(2,platform.DisplayMode))};
    }
    public void Initialize(string saveRoot){
        if(string.IsNullOrWhiteSpace(saveRoot))throw new ArgumentException("An options save directory is required.",nameof(saveRoot));
        file=Path.Combine(Path.GetFullPath(saveRoot),"game-options.json");
        current=Defaults();LastError=null;rollback=null;
        bool loadedSaved=false;
        if(File.Exists(file)){
            try{
                // Initialize missing fields explicitly. The zero marker lets
                // old JSON migrate without treating saved zero deadzones as
                // missing once these settings have been written.
                var loaded=new Values{version=0,steeringSettingsVersion=0};
                JsonUtility.FromJsonOverwrite(File.ReadAllText(file),loaded);
                if(loaded==null||loaded.version!=1)throw new InvalidDataException("Unsupported options format.");
                current=Normalize(loaded);loadedSaved=true;
            }catch(Exception error){LastError="Could not load options; using defaults. "+error.Message;}
        }
        draft=current.Clone();lastNow=platform.Now;
        // A missing file must not overwrite the host's diagnostic resolution,
        // vSync or frame cap. Normal first-run defaults already match the game.
        if(loadedSaved){var live=Defaults();platform.Apply(live,current,DisplayChanged(live,current));}
        Changed?.Invoke(current);
    }
    public void BeginEdit(){
        EnsureInitialized();
        if(DisplayConfirmationPending)RevertDisplay();
        else{
            // Window resizing and F11 can change the live display without an
            // options apply. Remember that baseline in memory, not on disk.
            current.width=Math.Max(640,Math.Min(8192,platform.Width));
            current.height=Math.Max(360,Math.Min(8192,platform.Height));
            current.displayMode=Math.Max(0,Math.Min(2,platform.DisplayMode));
        }
        draft=current.Clone();
    }
    public void ResetDraft(){EnsureInitialized();draft=Defaults();}
    // Presets edit the draft: Apply and the existing display rollback still own
    // activation. Race physics, audio, controls and save progression are untouched.
    public void SetPerformancePreset(int preset){
        EnsureInitialized();
        if(preset<0||preset>2)throw new ArgumentOutOfRangeException(nameof(preset));
        draft.rainDetail=preset==0?0:1;draft.importedSceneryDetail=preset;
        draft.antiAliasing=preset==0?4:preset==1?2:0;
        if(preset!=0){draft.width=preset==1?1280:960;draft.height=preset==1?720:540;draft.vSync=false;draft.frameRateLimit=60;}
    }
    public static int PerformancePreset(Values v){
        if(v.rainDetail==0&&v.importedSceneryDetail==0&&v.antiAliasing==4)return 0;
        if(v.vSync||v.frameRateLimit!=60)return -1;
        if(v.rainDetail==1&&v.importedSceneryDetail==1&&v.antiAliasing==2&&v.width==1280&&v.height==720)return 1;
        if(v.rainDetail==1&&v.importedSceneryDetail==2&&v.antiAliasing==0&&v.width==960&&v.height==540)return 2;
        return -1;
    }
    public bool ApplyDraft(){
        EnsureInitialized();if(DisplayConfirmationPending)return false;
        LastError=null;var next=Normalize(draft);var previous=current.Clone();
        bool displayChanged=DisplayChanged(previous,next);
        try{
            platform.Apply(previous,next,displayChanged);current=next;draft=next.Clone();Changed?.Invoke(current);
            if(displayChanged){rollback=previous;lastNow=platform.Now;confirmationDeadline=lastNow+15;}
            else SaveCurrent();
            return true;
        }catch(Exception error){
            LastError="Could not apply options. "+error.Message;
            Restore(previous);return false;
        }
    }
    public bool ConfirmDisplay(){
        if(!DisplayConfirmationPending)return false;
        try{SaveCurrent();rollback=null;LastError=null;return true;}
        catch(Exception error){LastError="Could not save display settings. "+error.Message;RevertDisplay();return false;}
    }
    public void RevertDisplay(){
        if(!DisplayConfirmationPending)return;
        var previous=rollback;rollback=null;Restore(previous);
    }
    public void Tick(double now){
        lastNow=now;if(DisplayConfirmationPending&&now>=confirmationDeadline)RevertDisplay();
    }
    private void Restore(Values previous){
        var changed=DisplayChanged(current,previous);
        try{platform.Apply(current,previous,changed);}
        catch(Exception error){LastError=(LastError==null?"":LastError+" ")+"Could not restore the display. "+error.Message;}
        current=previous.Clone();draft=current.Clone();Changed?.Invoke(current);
    }
    private void SaveCurrent(){
        Directory.CreateDirectory(Path.GetDirectoryName(file));
        string temporary=file+".tmp";
        File.WriteAllText(temporary,JsonUtility.ToJson(current,true));
        if(File.Exists(file))File.Replace(temporary,file,file+".previous");else File.Move(temporary,file);
    }
    private void EnsureInitialized(){if(current==null||file==null)throw new InvalidOperationException("Options have not been initialized.");}
    private static float Volume(float value)=>float.IsNaN(value)||float.IsInfinity(value)?1:Math.Max(0,Math.Min(1,value));
    public static Values Normalize(Values source){
        if(source==null)throw new ArgumentNullException(nameof(source));var value=source.Clone();value.version=1;
        value.masterVolume=Volume(value.masterVolume);value.musicVolume=Volume(value.musicVolume);
        value.engineVolume=Volume(value.engineVolume);value.effectsVolume=Volume(value.effectsVolume);
        value.displayMode=Math.Max(0,Math.Min(2,value.displayMode));value.width=Math.Max(640,Math.Min(8192,value.width));
        value.height=Math.Max(360,Math.Min(8192,value.height));
        if(value.antiAliasing!=0&&value.antiAliasing!=2&&value.antiAliasing!=4&&value.antiAliasing!=8)value.antiAliasing=4;
        if(value.frameRateLimit!=0)value.frameRateLimit=Math.Max(30,Math.Min(360,value.frameRateLimit));
        value.defaultCamera=value.defaultCamera==1?1:0;
        value.rainDetail=Math.Max(0,Math.Min(1,value.rainDetail));
        value.importedSceneryDetail=Math.Max(0,Math.Min(2,value.importedSceneryDetail));
        if(value.controllerResponse<0||value.controllerResponse>2)value.controllerResponse=0;
        if(value.steeringSettingsVersion<1){value.steeringDeadzoneGamepad=.1f;value.steeringDeadzonePrevious=.13f;value.steeringDeadzoneWheel=0;}
        value.steeringSettingsVersion=1;
        value.steeringDeadzoneGamepad=Deadzone(value.steeringDeadzoneGamepad,.1f);
        value.steeringDeadzonePrevious=Deadzone(value.steeringDeadzonePrevious,.13f);
        value.steeringDeadzoneWheel=Deadzone(value.steeringDeadzoneWheel,0);
        value.steeringSmoothing=float.IsNaN(value.steeringSmoothing)||float.IsInfinity(value.steeringSmoothing)?0:Math.Max(0,Math.Min(1,value.steeringSmoothing));
        value.wheelFeedbackStrength=float.IsNaN(value.wheelFeedbackStrength)||float.IsInfinity(value.wheelFeedbackStrength)?.35f:Math.Max(0,Math.Min(1,value.wheelFeedbackStrength));
        if(value.wheelFeedbackDevice==null||value.wheelFeedbackDevice.Length>512||value.wheelFeedbackDevice.IndexOf('\0')>=0)value.wheelFeedbackDevice="";
        return value;
    }
    private static float Deadzone(float value,float fallback)=>float.IsNaN(value)||float.IsInfinity(value)?fallback:Math.Max(0,Math.Min(.3f,value));
    public static bool DisplayChanged(Values a,Values b)=>a.displayMode!=b.displayMode||a.width!=b.width||a.height!=b.height;
    public static bool Equivalent(Values a,Values b)=>a!=null&&b!=null&&
        a.masterVolume==b.masterVolume&&a.musicVolume==b.musicVolume&&a.engineVolume==b.engineVolume&&a.effectsVolume==b.effectsVolume&&
        !DisplayChanged(a,b)&&a.vSync==b.vSync&&a.frameRateLimit==b.frameRateLimit&&a.antiAliasing==b.antiAliasing&&
        a.defaultCamera==b.defaultCamera&&a.controllerResponse==b.controllerResponse&&
        a.steeringSettingsVersion==b.steeringSettingsVersion&&a.steeringDeadzoneGamepad==b.steeringDeadzoneGamepad&&
        a.steeringDeadzonePrevious==b.steeringDeadzonePrevious&&a.steeringDeadzoneWheel==b.steeringDeadzoneWheel&&
        a.steeringSmoothing==b.steeringSmoothing&&
        a.wheelForceFeedback==b.wheelForceFeedback&&a.wheelFeedbackStrength==b.wheelFeedbackStrength&&
        a.wheelFeedbackInvert==b.wheelFeedbackInvert&&a.wheelFeedbackDevice==b.wheelFeedbackDevice&&
        a.showFps==b.showFps&&a.muteWhenUnfocused==b.muteWhenUnfocused&&a.communityTimes==b.communityTimes&&
        a.discordPresence==b.discordPresence&&a.replayTimeAttack==b.replayTimeAttack&&a.replayOnline==b.replayOnline&&a.replayLegend==b.replayLegend&&
        a.rainDetail==b.rainDetail&&a.importedSceneryDetail==b.importedSceneryDetail;
}
