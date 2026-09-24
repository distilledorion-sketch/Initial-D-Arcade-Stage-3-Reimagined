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
        public float masterVolume=1,musicVolume=1,engineVolume=1,effectsVolume=1,tireVolume=1;
        public int audioSettingsVersion=1;
        public int displayMode,width=1280,height=720;
        public bool vSync=true;
        public int frameRateLimit=60,antiAliasing=4,defaultCamera,controllerResponse;
        public int steeringSettingsVersion=1;
        public int aiDifficulty;
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
        public int hudMeterStyle; // Stable catalog ID: 0 = original, 1 = Stuttgart.
        public int hudOrnamentId; // Stable source ornament ID; 0 = off.
        public bool hudShiftLights=true,hudPedalIndicators=true;
        public int hudNameplateStyle; // 0 = off, 1 = driver plate.
        public const int HudLayoutGroupCount=11;
        public Vector2[] hudPositions=new Vector2[HudLayoutGroupCount]; // Normalized offsets from each original anchor.
        public int[] hudSizePercent=new int[HudLayoutGroupCount]; // Zero retains the legacy size preset.
        public static int HudPositionGroup(int group)=>group==6||group==7?3:group;
        public Vector2 HudOffset(int group){group=HudPositionGroup(group);return group!=0&&hudPositions!=null&&group>=0&&group<hudPositions.Length?hudPositions[group]:Vector2.zero;}
        public void SetHudOffset(int group,Vector2 offset){
            group=HudPositionGroup(group);if(group<=0||group>=HudLayoutGroupCount)return;
            if(hudPositions==null)hudPositions=new Vector2[HudLayoutGroupCount];else if(hudPositions.Length!=HudLayoutGroupCount)Array.Resize(ref hudPositions,HudLayoutGroupCount);
            hudPositions[group]=offset;
            if(group==3)hudPositions[6]=hudPositions[7]=offset;
        }
        public int hudTimerSize=2,hudSpeedometerSize=2,hudRecordsSize=2,hudLegendSize=2;
        public int hudTimeExtensionSize=2;
        public int hudOrnamentSize=2;
        public int hudOnlineSize=2,hudMirrorSize=2,hudMessagesSize=2,hudChallengersSize=2;
        public int HudSizePercent(int group) {
            if(group<=0||group>=HudLayoutGroupCount)return 100;
            if(hudSizePercent!=null&&group<hudSizePercent.Length&&hudSizePercent[group]!=0)
                return Math.Max(group==5?100:50,Math.Min(150,hudSizePercent[group]));
            if(group==5)return 100+25*Math.Max(0,Math.Min(2,minimapSize));
            int size=group==1?hudTimerSize:group==2?hudSpeedometerSize:group==3?hudRecordsSize:
                group==4?hudMirrorSize:group==6?hudLegendSize:group==7?hudOnlineSize:group==8?hudChallengersSize:group==9?hudTimeExtensionSize:group==10?hudOrnamentSize:hudMessagesSize;
            return 50+25*Math.Max(0,Math.Min(4,size));
        }
        public void SetHudSizePercent(int group,int percent) {
            if(group<=0||group>=HudLayoutGroupCount)return;
            if(hudSizePercent==null)hudSizePercent=new int[HudLayoutGroupCount];
            else if(hudSizePercent.Length!=HudLayoutGroupCount)Array.Resize(ref hudSizePercent,HudLayoutGroupCount);
            hudSizePercent[group]=Math.Max(group==5?100:50,Math.Min(150,percent));
        }
        public void ResetHudSize(int group) {
            if(group<=0||group>=HudLayoutGroupCount)return;
            if(hudSizePercent!=null&&group<hudSizePercent.Length)hudSizePercent[group]=0;
            switch(group){
                case 1:hudTimerSize=2;break;case 2:hudSpeedometerSize=2;break;case 3:hudRecordsSize=2;break;
                case 4:hudMirrorSize=2;break;case 5:minimapSize=0;break;case 6:hudLegendSize=2;break;
                case 7:hudOnlineSize=2;break;case 8:hudChallengersSize=2;break;case 9:hudTimeExtensionSize=2;break;
                case 10:hudOrnamentSize=2;break;
            }
        }
        public float HudGroupScale(int group) {
            // The native minimap already contains its legacy preset scale.
            // Apply only the remaining ratio, including to its clipping bounds.
            return group==5?(float)HudSizePercent(group)/(100+25*Math.Max(0,Math.Min(2,minimapSize))):HudSizePercent(group)/100f;
        }
        public int minimapSize; // 0 = original, 1 = 125%, 2 = 150%.
        public int minimapZoom=2; // Zoom-out only: 50%, 75%, original 100%.
        public float SteeringDeadzone {
            get=>controllerResponse==1?steeringDeadzonePrevious:controllerResponse==2?steeringDeadzoneWheel:steeringDeadzoneGamepad;
            set{if(controllerResponse==1)steeringDeadzonePrevious=value;else if(controllerResponse==2)steeringDeadzoneWheel=value;else steeringDeadzoneGamepad=value;}
        }
        public Values Clone(){var copy=(Values)MemberwiseClone();copy.hudPositions=hudPositions==null?new Vector2[HudLayoutGroupCount]:(Vector2[])hudPositions.Clone();copy.hudSizePercent=hudSizePercent==null?new int[HudLayoutGroupCount]:(int[])hudSizePercent.Clone();return copy;}
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
            Idas3FramePacing.Configure(next.vSync,next.frameRateLimit);
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
                var loaded=new Values{version=0,steeringSettingsVersion=0,audioSettingsVersion=0};
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
    // The customization panel commits its own transaction while retaining any
    // unrelated edits in the main settings draft, including pending display edits.
    public bool ApplyHudCustomization(Values appearance,bool includeLayout=false){
        EnsureInitialized();if(DisplayConfirmationPending)return false;
        var pending=draft.Clone();draft=current.Clone();CopyHudCustomization(appearance,draft);
        if(includeLayout)CopyHudLayout(appearance,draft);
        bool applied=ApplyDraft();
        if(applied){CopyHudCustomization(current,pending);if(includeLayout)CopyHudLayout(current,pending);}
        draft=pending;return applied;
    }
    public static void CopyHudCustomization(Values from,Values to){
        to.hudMeterStyle=from.hudMeterStyle;to.hudOrnamentId=from.hudOrnamentId;to.hudShiftLights=from.hudShiftLights;
        to.hudPedalIndicators=from.hudPedalIndicators;to.hudNameplateStyle=from.hudNameplateStyle;
    }
    public static void CopyHudLayout(Values from,Values to){
        to.hudPositions=from.hudPositions==null?new Vector2[Values.HudLayoutGroupCount]:(Vector2[])from.hudPositions.Clone();
        to.hudSizePercent=from.hudSizePercent==null?new int[Values.HudLayoutGroupCount]:(int[])from.hudSizePercent.Clone();
        to.hudTimerSize=from.hudTimerSize;to.hudSpeedometerSize=from.hudSpeedometerSize;
        to.hudRecordsSize=from.hudRecordsSize;to.hudLegendSize=from.hudLegendSize;
        to.hudOnlineSize=from.hudOnlineSize;to.hudMirrorSize=from.hudMirrorSize;
        to.hudMessagesSize=from.hudMessagesSize;to.hudChallengersSize=from.hudChallengersSize;
        to.hudTimeExtensionSize=from.hudTimeExtensionSize;to.hudOrnamentSize=from.hudOrnamentSize;
        to.minimapSize=from.minimapSize;to.minimapZoom=from.minimapZoom;
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
        if(value.audioSettingsVersion<1)value.tireVolume=value.engineVolume;
        value.tireVolume=Volume(value.tireVolume);value.audioSettingsVersion=1;
        value.displayMode=Math.Max(0,Math.Min(2,value.displayMode));value.width=Math.Max(640,Math.Min(8192,value.width));
        value.height=Math.Max(360,Math.Min(8192,value.height));
        if(value.antiAliasing!=0&&value.antiAliasing!=2&&value.antiAliasing!=4&&value.antiAliasing!=8)value.antiAliasing=4;
        if(value.frameRateLimit!=0)value.frameRateLimit=Math.Max(30,Math.Min(360,value.frameRateLimit));
        if(value.defaultCamera<0||value.defaultCamera>2)value.defaultCamera=0;
        value.aiDifficulty=Math.Max(0,Math.Min(2,value.aiDifficulty));
        if(!Idas3ArcadeMeterCatalog.IsValidStyle(value.hudMeterStyle))value.hudMeterStyle=0;
        if(!Idas3OrnamentCatalog.IsValid(value.hudOrnamentId))value.hudOrnamentId=0;
        if(value.hudNameplateStyle<0||value.hudNameplateStyle>1)value.hudNameplateStyle=0;
        // Preserve an existing Time Attack placement; otherwise inherit a previously moved battle panel.
        var shared=value.HudOffset(3);
        if(shared==Vector2.zero&&value.hudPositions!=null){
            foreach(int group in new[]{6,7})if(group<value.hudPositions.Length&&value.hudPositions[group]!=Vector2.zero){shared=value.hudPositions[group];break;}
        }
        value.SetHudOffset(3,shared);
        var positions=new Vector2[Values.HudLayoutGroupCount];
        for(int i=0;i<positions.Length;++i){var point=value.HudOffset(i);
            positions[i]=new Vector2(float.IsNaN(point.x)||float.IsInfinity(point.x)?0:Mathf.Clamp(point.x,-1,1),float.IsNaN(point.y)||float.IsInfinity(point.y)?0:Mathf.Clamp(point.y,-1,1));}
        value.hudPositions=positions;
        var sizes=new int[Values.HudLayoutGroupCount];
        for(int i=1;i<sizes.Length;++i)
            if(value.hudSizePercent!=null&&i<value.hudSizePercent.Length&&value.hudSizePercent[i]!=0)sizes[i]=value.HudSizePercent(i);
        value.hudSizePercent=sizes;
        value.hudTimeExtensionSize=Math.Max(0,Math.Min(4,value.hudTimeExtensionSize));
        value.hudOrnamentSize=Math.Max(0,Math.Min(4,value.hudOrnamentSize));
        value.hudTimerSize=Math.Max(0,Math.Min(4,value.hudTimerSize));
        value.hudSpeedometerSize=Math.Max(0,Math.Min(4,value.hudSpeedometerSize));
        value.hudRecordsSize=Math.Max(0,Math.Min(4,value.hudRecordsSize));
        value.hudLegendSize=Math.Max(0,Math.Min(4,value.hudLegendSize));
        value.hudOnlineSize=Math.Max(0,Math.Min(4,value.hudOnlineSize));
        value.hudMirrorSize=Math.Max(0,Math.Min(4,value.hudMirrorSize));
        value.hudMessagesSize=Math.Max(0,Math.Min(4,value.hudMessagesSize));
        value.hudChallengersSize=Math.Max(0,Math.Min(4,value.hudChallengersSize));
        value.minimapZoom=Math.Max(0,Math.Min(2,value.minimapZoom));
        value.minimapSize=Math.Max(0,Math.Min(2,value.minimapSize));
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
    private static bool SameHudPositions(Values a,Values b){for(int i=0;i<Values.HudLayoutGroupCount;++i)if(a.HudOffset(i)!=b.HudOffset(i))return false;return true;}
    private static bool SameHudSizes(Values a,Values b){for(int i=1;i<Values.HudLayoutGroupCount;++i)if(a.HudSizePercent(i)!=b.HudSizePercent(i))return false;return true;}
    public static bool Equivalent(Values a,Values b)=>a!=null&&b!=null&&
        a.masterVolume==b.masterVolume&&a.musicVolume==b.musicVolume&&a.engineVolume==b.engineVolume&&a.effectsVolume==b.effectsVolume&&a.tireVolume==b.tireVolume&&a.audioSettingsVersion==b.audioSettingsVersion&&
        !DisplayChanged(a,b)&&a.vSync==b.vSync&&a.frameRateLimit==b.frameRateLimit&&a.antiAliasing==b.antiAliasing&&
        a.aiDifficulty==b.aiDifficulty&&a.defaultCamera==b.defaultCamera&&a.controllerResponse==b.controllerResponse&&
        a.steeringSettingsVersion==b.steeringSettingsVersion&&a.steeringDeadzoneGamepad==b.steeringDeadzoneGamepad&&
        a.steeringDeadzonePrevious==b.steeringDeadzonePrevious&&a.steeringDeadzoneWheel==b.steeringDeadzoneWheel&&
        a.steeringSmoothing==b.steeringSmoothing&&
        a.wheelForceFeedback==b.wheelForceFeedback&&a.wheelFeedbackStrength==b.wheelFeedbackStrength&&
        a.wheelFeedbackInvert==b.wheelFeedbackInvert&&a.wheelFeedbackDevice==b.wheelFeedbackDevice&&
        a.showFps==b.showFps&&a.muteWhenUnfocused==b.muteWhenUnfocused&&a.communityTimes==b.communityTimes&&
        a.discordPresence==b.discordPresence&&a.replayTimeAttack==b.replayTimeAttack&&a.replayOnline==b.replayOnline&&a.replayLegend==b.replayLegend&&
        a.hudMeterStyle==b.hudMeterStyle&&a.hudOrnamentId==b.hudOrnamentId&&a.hudShiftLights==b.hudShiftLights&&a.hudPedalIndicators==b.hudPedalIndicators&&a.hudNameplateStyle==b.hudNameplateStyle&&
        SameHudPositions(a,b)&&SameHudSizes(a,b)&&a.hudOrnamentSize==b.hudOrnamentSize&&a.hudTimeExtensionSize==b.hudTimeExtensionSize&&a.hudTimerSize==b.hudTimerSize&&a.hudSpeedometerSize==b.hudSpeedometerSize&&a.hudRecordsSize==b.hudRecordsSize&&a.hudLegendSize==b.hudLegendSize&&a.hudOnlineSize==b.hudOnlineSize&&a.hudMirrorSize==b.hudMirrorSize&&a.hudMessagesSize==b.hudMessagesSize&&a.hudChallengersSize==b.hudChallengersSize&&
        a.minimapSize==b.minimapSize&&a.minimapZoom==b.minimapZoom&&a.rainDetail==b.rainDetail&&a.importedSceneryDetail==b.importedSceneryDetail;
}
