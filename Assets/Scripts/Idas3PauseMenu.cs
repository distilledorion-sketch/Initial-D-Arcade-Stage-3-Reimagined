using System;
using UnityEngine;

// The host owns pause/input/network behavior. This view emits explicit commands
// and remains usable while an online race continues at its normal source rate.
public sealed class Idas3PauseMenu : MonoBehaviour
{
    public enum Command {None,Resume,Restart,ReturnToCourse,LeaveOnline,Quit,Retire,FullTune,Replays}
    private const float Width=1040,Height=680;
    private static readonly Color Ink=new Color32(11,12,15,255),Panel=new Color32(22,24,29,255);
    private static readonly Color Raised=new Color32(34,36,42,255),Edge=new Color32(65,68,76,255);
    private static readonly Color Red=new Color32(222,35,49,255),Muted=new Color32(164,166,175,255);
    private static readonly Color PromptYellow=new Color32(255,221,44,255);
    private static readonly string[] Tabs={"AUDIO","GRAPHICS","GAMEPLAY","CONTROLS","WHEEL","RECORDS","REPLAYS","HUD"};
    public string ReplayStatus {get;set;}="Finished recordings are saved on this computer.";
    public string CommunityStatus {get;set;}="Community times ready.";
    public Idas3Updates Updates {get;set;}
    private static readonly string[] DisplayModes={"WINDOWED","BORDERLESS","FULLSCREEN"};
    private static readonly string[] CameraModes={"BUMPER","CHASE","NATURAL"};
    private static readonly string[] ControllerResponses={"FLYCAST GAMEPAD","PREVIOUS","FLYCAST WHEEL"};
    private static readonly int[] FrameCaps={0,30,60,90,120,144,165,240,360},AaValues={0,2,4,8};
    private Idas3GameOptions options;
    private Idas3ControlBindings bindings;
    private Idas3ControllerDevices controllerDevices;
    private Idas3WheelFeedback wheelFeedback;
    private int bindingColumn;
    private bool wheelNavigation,wheelEditing;
    private bool bindingChoice;
    private int bindingChoiceSelection;
    internal bool BindingChoiceVisible=>bindingChoice;
    internal bool WheelEditing=>wheelEditing;
    internal void SetWheelNavigation(bool enabled){if(wheelNavigation!=enabled)wheelEditing=false;wheelNavigation=enabled;}
    private Idas3ControlBindings.ActionId captureAction;
    private Idas3ControlBindings.Slot captureSlot;
    private GUIStyle title,heading,label,small,wrapped,button,bindingButton,attractPromptStyle;
    private bool online,canRetire,canRestart,showOptions,previousCursorVisible;
    private bool attractPromptRequested;
    private string attractControlLabel="VIEW CHANGE";
    private CursorLockMode previousCursorLock;
    private string courseName="MOUNTAIN PASS",notice="";
    private int tab,selection,blockThroughFrame=-1,modalSelection;
    private Command queued,pending;
    private RenderTexture diagnosticTarget;
    private string frameRateText="";
    private double frameRateSampleStart;
    private int frameRateSampleCount;
    public event Action<bool> OpenChanged;
    public bool IsOpen {get;private set;}
    public bool AttractOptions {get;private set;}
    public bool AttractPromptVisible=>attractPromptRequested&&!IsOpen;
    public float AttractHoldProgress {get;private set;}
    public bool BlocksGameInput=>IsOpen||Time.frameCount<=blockThroughFrame;
    public float FrameRate {get;set;}
    public int SelectedTab=>tab;
    internal int DiagnosticSelection=>selection;
    public bool OptionsVisible=>showOptions;
    internal Idas3HudEditor HudEditor { get; private set; }
    internal Idas3HudCustomization HudCustomization { get; private set; }
    internal bool CustomizingHud=>HudCustomization!=null&&HudCustomization.IsOpen;
    internal bool EditingLayout=>HudEditor!=null&&HudEditor.IsOpen;
    internal bool EditingHud=>CustomizingHud||EditingLayout;
    internal void OpenHudEditor(){if(EditingHud)return;if(HudEditor==null)HudEditor=gameObject.AddComponent<Idas3HudEditor>();HudEditor.Open(options,this);}
    internal void OpenHudEditor(Idas3GameOptions.Values initial,Action<bool,Idas3GameOptions.Values> onClosed,int initialGroup)
    {
        if(EditingLayout||!CustomizingHud)return;
        if(HudEditor==null)HudEditor=gameObject.AddComponent<Idas3HudEditor>();
        HudEditor.Open(options,this,initial,(saved,layout)=>{blockThroughFrame=Time.frameCount+1;onClosed?.Invoke(saved,layout);},initialGroup);
    }
    internal void OpenHudCustomization(){if(EditingHud)return;if(HudCustomization==null)HudCustomization=gameObject.AddComponent<Idas3HudCustomization>();HudCustomization.Open(options,this);}
    internal void HudCustomizationClosed(bool saved){blockThroughFrame=Time.frameCount+1;notice=saved?"HUD CUSTOMIZATION SAVED":"";}
    public bool FullTuneAvailable {get;set;}
    public bool DiagnosticCaptureReady {get;private set;}
    public int DiagnosticRepaints {get;private set;}
    public void Initialize(Idas3GameOptions owner){options=owner??throw new ArgumentNullException(nameof(owner));}
    public void InitializeBindings(Idas3ControlBindings owner){bindings=owner??throw new ArgumentNullException(nameof(owner));}
    public void InitializeWheelFeedback(Idas3WheelFeedback owner){wheelFeedback=owner??throw new ArgumentNullException(nameof(owner));}
    public void InitializeControllerDevices(Idas3ControllerDevices owner){
        if(controllerDevices!=null)controllerDevices.ActiveDeviceChanged-=ControllerDeviceChanged;
        controllerDevices=owner??throw new ArgumentNullException(nameof(owner));
        controllerDevices.ActiveDeviceChanged+=ControllerDeviceChanged;
    }
    private void ControllerDeviceChanged(){
        bindings?.CancelCapture();blockThroughFrame=Time.frameCount+1;
        notice="Controller changed. Draft bindings are kept. APPLY saves all profiles.";
    }
    public void SetContext(bool isOnline,bool restartAllowed,string track,bool retireAllowed=false){online=isOnline;canRetire=retireAllowed&&!isOnline;canRestart=restartAllowed;courseName=string.IsNullOrWhiteSpace(track)?"MOUNTAIN PASS":track;}
    public void SetAttractPrompt(bool visible,float progress,string controlLabel){
        attractPromptRequested=visible;
        AttractHoldProgress=visible&&!float.IsNaN(progress)&&!float.IsInfinity(progress)?Mathf.Clamp01(progress):0;
        string hint=string.IsNullOrWhiteSpace(controlLabel)?"VIEW CHANGE":controlLabel.Trim();
        attractControlLabel=hint.StartsWith("VIEW CHANGE",StringComparison.OrdinalIgnoreCase)?hint:"VIEW CHANGE ("+hint+")";
    }
    public void OpenAttractOptions(){
        if(IsOpen)return;
        SetOpen(true,true);
    }
    public void SetOpen(bool open)=>SetOpen(open,false);
    private void SetOpen(bool open,bool attractContext){
        blockThroughFrame=Time.frameCount+1;if(open==IsOpen)return;
        IsOpen=open;bindingChoice=false;
        if(open){
            AttractOptions=attractContext;
            previousCursorVisible=Cursor.visible;previousCursorLock=Cursor.lockState;
            Cursor.lockState=CursorLockMode.None;Cursor.visible=true;
            showOptions=attractContext;selection=0;wheelEditing=false;pending=Command.None;notice="";options?.BeginEdit();bindings?.BeginEdit();
            if(attractContext){tab=0;queued=Command.None;AttractHoldProgress=0;}
        }else{
            if(CustomizingHud)HudCustomization.Close(false);
            if(HudEditor!=null&&HudEditor.IsOpen)HudEditor.Close(false);
            // Returning to driving must preserve a held pedal or steering axis.
            // The host separately guards the closing menu button against reuse.
            options?.RevertDisplay();bindings?.CancelCapture();bindings?.CancelEdit(false);pending=Command.None;
            if(AttractOptions){options?.BeginEdit();showOptions=false;queued=Command.None;AttractHoldProgress=0;}
            Cursor.lockState=previousCursorLock;Cursor.visible=previousCursorVisible;
        }
        // Keep the context available to the host through the closing callback.
        OpenChanged?.Invoke(open);
        if(!open)AttractOptions=false;
    }
    public void SelectTab(int index){
        if(!IsOpen)SetOpen(true);
        if(bindings!=null&&bindings.IsCapturing)return;
        if(!showOptions){options.BeginEdit();bindings?.BeginEdit();}showOptions=true;tab=Wrap(index,Tabs.Length);selection=1;wheelEditing=false;notice="";
        if(tab==3&&controllerDevices!=null)bindingColumn=controllerDevices.Controls.Count>0?3:0;
        if(tab==4)wheelFeedback?.RefreshDevices();
    }
    public void Navigate(int delta){
        if(EditingLayout){HudEditor.Navigate(delta);return;}
        if(CustomizingHud){HudCustomization.Navigate(delta);return;}
        if(EditingHud)return;
        if(!IsOpen||delta==0||BindingInputBlocked)return;
        if(bindingChoice){bindingChoiceSelection=Wrap(bindingChoiceSelection+Math.Sign(delta),3);return;}
        if(Modal){modalSelection=1-modalSelection;return;}
        wheelEditing=false;
        if(showOptions&&selection==0){ChangeCategory(delta);return;}
        selection=showOptions?1+Wrap(selection-1+Math.Sign(delta),Rows+3):Wrap(selection+Math.Sign(delta),online?4:5);
    }
    public void NavigateHorizontal(int delta){
        if(EditingLayout){HudEditor.NavigateHorizontal(delta,wheelNavigation);return;}
        if(CustomizingHud){if(wheelNavigation)HudCustomization.Navigate(delta);else HudCustomization.NavigateHorizontal(delta);return;}
        if(EditingHud)return;
        if(!IsOpen||delta==0||BindingInputBlocked)return;
        if(bindingChoice){bindingChoiceSelection=Wrap(bindingChoiceSelection+Math.Sign(delta),3);return;}
        if(Modal){modalSelection=1-modalSelection;return;}
        if(wheelNavigation&&!wheelEditing){Navigate(delta);return;}
        if(!showOptions)return;
        if(selection==0){ChangeCategory(delta);return;}
        if(selection<=Rows){if(tab==3){if(DeviceRowSelected)ChangeControllerDevice(Math.Sign(delta));else bindingColumn=Wrap(bindingColumn+Math.Sign(delta),4);}else Adjust(selection-1,Math.Sign(delta));}
    }
    public void Activate(){
        if(EditingLayout){HudEditor.Activate();return;}
        if(CustomizingHud){HudCustomization.Activate();return;}
        if(EditingHud)return;
        if(!IsOpen||BindingInputBlocked)return;
        if(bindingChoice){ActivateBindingChoice();return;}
        if(options.DisplayConfirmationPending){if(modalSelection==1)KeepDisplay();else options.RevertDisplay();return;}
        if(pending!=Command.None){if(modalSelection==1){queued=pending;pending=Command.None;}else pending=Command.None;return;}
        if(!showOptions){MainAction(selection);return;}
        if(selection==0){selection=1;return;}
        if(tab==2&&selection==7){if(FullTuneAvailable)queued=Command.FullTune;return;}
        if(tab==2&&selection==8){if(AttractOptions)Updates?.Activate();return;}
        if(tab==5&&selection==2){Application.OpenURL(Idas3CommunityTimes.ServiceUrl);return;}
        if(tab==6&&selection==1){queued=Command.Replays;return;}
        if(tab==7&&selection==1){OpenHudCustomization();return;}
        if(tab==7&&selection==2){OpenHudEditor();return;}
        if(wheelNavigation&&selection<=Rows){
            if(!wheelEditing){wheelEditing=true;return;}
            if(tab!=3||DeviceRowSelected){wheelEditing=false;return;}
        }
        if(selection<=Rows){if(tab==3){if(DeviceRowSelected)ChangeControllerDevice(1);else OpenBindingChoice((Idas3ControlBindings.ActionId)(selection-BindingFirstSelection),(Idas3ControlBindings.Slot)bindingColumn);}else Adjust(selection-1,1);return;}
        if(selection==Rows+1)ResetDefaults();
        else if(selection==Rows+2)Apply();else Back();
    }
    public void Back(){
        if(EditingLayout){HudEditor.Back();return;}
        if(CustomizingHud){HudCustomization.Back();return;}
        if(EditingHud){HudEditor.Close(false);return;}
        if(!IsOpen)return;
        if(bindingChoice){bindingChoice=false;return;}
        if(bindings!=null&&bindings.IsCapturing){bindings.CancelCapture();notice="Binding unchanged.";return;}
        if(options.DisplayConfirmationPending){options.RevertDisplay();notice="Display change reverted.";return;}
        if(pending!=Command.None){pending=Command.None;return;}
        if(wheelEditing){wheelEditing=false;return;}
        if(showOptions&&selection!=0){selection=0;notice="";return;}
        if(AttractOptions){SetOpen(false);return;}
        if(showOptions){options.BeginEdit();bindings?.CancelEdit(false);showOptions=false;selection=1;notice="";return;}
        queued=Command.Resume;
    }
    public bool TryConsumeCommand(out Command command){command=queued;queued=Command.None;return command!=Command.None;}
    public void RequestDiagnosticCapture(RenderTexture target){
        if((!IsOpen&&!AttractPromptVisible)||target==null)throw new InvalidOperationException("A visible pause menu or attract prompt and render target are required.");
        diagnosticTarget=target;DiagnosticCaptureReady=false;
    }
    public void CancelDiagnosticCapture(){diagnosticTarget=null;}
    private bool BindingInputBlocked=>bindings!=null&&(bindings.IsCapturing||bindings.SuppressInput);
    private bool Modal=>bindingChoice||pending!=Command.None||options.DisplayConfirmationPending||(bindings!=null&&bindings.IsCapturing);
    private int BindingFirstSelection=>controllerDevices!=null?2:1;
    private bool DeviceRowSelected=>controllerDevices!=null&&selection==1;
    private int Rows=>tab==7?12:tab==6?4:tab==5?2:tab==0?5:tab==4?4:tab==2?10:tab==1?8:bindings!=null?10+BindingFirstSelection-1:0;
    private static int Wrap(int value,int count)=>(value%count+count)%count;
    private void Update(){
        double now=Time.realtimeSinceStartupAsDouble;options?.Tick(now);
        if(options==null||!options.Current.showFps){frameRateSampleStart=now;frameRateSampleCount=0;return;}
        ++frameRateSampleCount;
        double elapsed=now-frameRateSampleStart;
        if(elapsed>=.25){
            double fps=frameRateSampleCount/elapsed;
            frameRateText=Math.Round(fps)+" FPS  /  "+(1000/fps).ToString("0.0",System.Globalization.CultureInfo.InvariantCulture)+" ms";
            frameRateSampleStart=now;frameRateSampleCount=0;
        }
    }
    private void OnDestroy(){if(controllerDevices!=null)controllerDevices.ActiveDeviceChanged-=ControllerDeviceChanged;if(IsOpen){Cursor.lockState=previousCursorLock;Cursor.visible=previousCursorVisible;}options?.RevertDisplay();bindings?.CancelCapture();}
    private void MainAction(int index){
        if(AttractOptions){Back();return;}
        if(index==0){queued=Command.Resume;return;}
        if(index==1){SelectTab(0);selection=0;return;}
        if(online){Confirm(index==2?Command.LeaveOnline:Command.Quit);return;}
        if(index==2){if(canRestart&&!online)Confirm(Command.Restart);return;}
        if(index==3){Confirm(canRetire?Command.Retire:Command.ReturnToCourse);return;}
        Confirm(Command.Quit);
    }
    private void Confirm(Command command){pending=command;modalSelection=0;}
    private void ChangeCategory(int delta){
        tab=Wrap(tab+Math.Sign(delta),Tabs.Length);
        if(tab==3&&controllerDevices!=null)bindingColumn=controllerDevices.Controls.Count>0?3:0;
        if(tab==4)wheelFeedback?.RefreshDevices();
    }
    internal bool CategoryFocused=>showOptions&&selection==0;
    private void Apply(){
        if(tab==3){
            if(bindings==null){notice="Control bindings are unavailable.";return;}
            notice=bindings.ApplyDraft()?"CONTROLS SAVED — all edited device profiles":bindings.LastError??"Could not save controls.";return;
        }
        if(options.ApplyDraft()){
            modalSelection=0;notice=options.DisplayConfirmationPending?"Review the new display settings.":"OPTIONS SAVED";
        }else notice=options.LastError??"Could not apply options.";
    }
    private void ResetDefaults(){
        if(tab==3)bindings?.ResetDraft();else options.ResetDraft();
        notice="Defaults selected. Apply to save.";
    }
    public void BeginBindingCapture(Idas3ControlBindings.ActionId action,Idas3ControlBindings.Slot slot){
        if(bindings==null||BindingInputBlocked)return;
        if((int)action<0||(int)action>=10||(int)slot<0||(int)slot>=4)return;
        if(slot==Idas3ControlBindings.Slot.Controller&&controllerDevices!=null&&controllerDevices.Controls.Count==0){notice="No controller active. Connect one or choose a connected device above.";return;}
        if(!IsOpen||!showOptions||tab!=3)SelectTab(3);
        selection=(int)action+BindingFirstSelection;bindingColumn=(int)slot;captureAction=action;captureSlot=slot;notice="";
        bindings.BeginCapture(action,slot,Time.realtimeSinceStartupAsDouble);
    }
    private void OpenBindingChoice(Idas3ControlBindings.ActionId action,Idas3ControlBindings.Slot slot){
        if(bindings==null||BindingInputBlocked)return;
        captureAction=action;captureSlot=slot;bindingChoiceSelection=0;bindingChoice=true;
    }
    private void ActivateBindingChoice(){
        bindingChoice=false;
        if(bindingChoiceSelection==0)BeginBindingCapture(captureAction,captureSlot);
        else if(bindingChoiceSelection==1){
            bool cleared=bindings.ClearDraft(captureAction,captureSlot);
            notice=cleared?"Binding cleared. Apply to save.":bindings.LastError??"Could not clear this binding.";
        }
    }
    public void SelectBindingColumn(int column){
        if(!IsOpen||BindingInputBlocked||column<0||column>3)return;
        if(!showOptions||tab!=3)SelectTab(3);
        bindingColumn=column;if(selection<BindingFirstSelection||selection>Rows)selection=BindingFirstSelection;
    }
    public void ChangeControllerDevice(int direction){
        if(controllerDevices==null||direction==0||BindingInputBlocked)return;
        var choices=controllerDevices.Choices;if(choices.Count==0)return;
        int at=0;for(int i=0;i<choices.Count;++i)if(choices[i].key==controllerDevices.SelectedKey){at=i;break;}
        selection=1;
        if(!controllerDevices.Select(choices[Wrap(at+Math.Sign(direction),choices.Count)].key))notice=controllerDevices.LastError??"Could not select that controller.";
    }
    private void KeepDisplay(){if(options.ConfirmDisplay())notice="OPTIONS SAVED";else notice=options.LastError??"Display change reverted.";}
    private void Adjust(int row,int direction){
        var v=options.Draft;
        if(tab==6){
            if(row==1&&!v.communityTimes)v.replayTimeAttack=!v.replayTimeAttack;
            if(row==2)v.replayOnline=!v.replayOnline;
            if(row==3)v.replayLegend=!v.replayLegend;
            return;
        }
        if(tab==0){
            if(row==0)v.masterVolume=Mathf.Clamp01(v.masterVolume+direction*.05f);
            if(row==1)v.musicVolume=Mathf.Clamp01(v.musicVolume+direction*.05f);
            if(row==2)v.engineVolume=Mathf.Clamp01(v.engineVolume+direction*.05f);
            if(row==3)v.tireVolume=Mathf.Clamp01(v.tireVolume+direction*.05f);
            if(row==4)v.effectsVolume=Mathf.Clamp01(v.effectsVolume+direction*.05f);
        }else if(tab==7){
            if(row==0){OpenHudCustomization();return;}
            if(row==1){OpenHudEditor();return;}row-=2;
            if(row==1)v.minimapZoom=Wrap(v.minimapZoom-direction,3);
            int group=row==0?5:row==2?1:row==3?2:row==4?3:row==5?6:row==6?7:row==7?4:row==8?9:row==9?8:0;
            if(group!=0)v.SetHudSizePercent(group,v.HudSizePercent(group)+Math.Sign(direction));
        }else if(tab==1){
            if(row==0)v.displayMode=Wrap(v.displayMode+direction,3);
            if(row==1){var resolutions=options.AvailableResolutions;if(resolutions.Length==0)return;
                int at=Array.FindIndex(resolutions,item=>item.width==v.width&&item.height==v.height);
                var size=resolutions[Wrap(Math.Max(0,at)+direction,resolutions.Length)];v.width=size.width;v.height=size.height;}
            if(row==2)v.vSync=!v.vSync;
            if(row==3)v.frameRateLimit=FrameCaps[Wrap(Math.Max(0,Array.IndexOf(FrameCaps,v.frameRateLimit))+direction,FrameCaps.Length)];
            if(row==4)v.antiAliasing=AaValues[Wrap(Math.Max(0,Array.IndexOf(AaValues,v.antiAliasing))+direction,AaValues.Length)];
            if(row==5)options.SetPerformancePreset(Wrap(Idas3GameOptions.PerformancePreset(v)+direction,3));
            if(row==6)v.rainDetail=Wrap(v.rainDetail+direction,2);
            if(row==7)v.importedSceneryDetail=Wrap(v.importedSceneryDetail+direction,3);
        }else if(tab==2){
            if(row==0)v.defaultCamera=Wrap(v.defaultCamera+direction,CameraModes.Length);
            if(row==8)v.discordPresence=!v.discordPresence;
            if(row==9)v.aiDifficulty=Wrap(v.aiDifficulty+direction,3);
            if(row==1)v.showFps=!v.showFps;
            if(row==2)v.muteWhenUnfocused=!v.muteWhenUnfocused;
            if(row==3)v.controllerResponse=Wrap(v.controllerResponse+direction,ControllerResponses.Length);
            if(row==4)v.SteeringDeadzone=Mathf.Clamp(Mathf.Round(v.SteeringDeadzone*100)+direction,0,30)/100f;
            if(row==5)v.steeringSmoothing=Mathf.Clamp(Mathf.Round(v.steeringSmoothing*100)+direction,0,100)/100f;
        }else if(tab==5){
            if(row==0)v.communityTimes=!v.communityTimes;
        }else if(tab==4){
            if(row==0)v.wheelForceFeedback=!v.wheelForceFeedback;
            if(row==1&&wheelFeedback!=null){
                var choices=wheelFeedback.Choices;if(choices.Count>0){
                    int at=0;for(int i=0;i<choices.Count;++i)if(choices[i].id==v.wheelFeedbackDevice){at=i;break;}
                    v.wheelFeedbackDevice=choices[Wrap(at+direction,choices.Count)].id;
                }
            }
            if(row==2)v.wheelFeedbackStrength=Mathf.Clamp(Mathf.Round(v.wheelFeedbackStrength*100)+direction,0,100)/100f;
            if(row==3)v.wheelFeedbackInvert=!v.wheelFeedbackInvert;
        }
        notice="";
    }
    private void Styles(){
        if(label!=null)return;
        label=new GUIStyle(GUI.skin.label){fontSize=18,padding=new RectOffset(0,0,0,0),clipping=TextClipping.Clip};label.normal.textColor=Color.white;
        small=new GUIStyle(label){fontSize=13};wrapped=new GUIStyle(small){wordWrap=true};
        heading=new GUIStyle(label){fontSize=24,fontStyle=FontStyle.Bold};
        title=new GUIStyle(heading){fontSize=40,fontStyle=FontStyle.BoldAndItalic};
        button=new GUIStyle(label){fontSize=16,fontStyle=FontStyle.Bold,alignment=TextAnchor.MiddleCenter};
        bindingButton=new GUIStyle(button){fontSize=13};
        attractPromptStyle=new GUIStyle(button){fontSize=13};
    }
    private static void Fill(Rect rect,Color color){var before=GUI.color;GUI.color=color;GUI.DrawTexture(rect,Texture2D.whiteTexture);GUI.color=before;}
    private static void Frame(Rect rect,Color color){Fill(new Rect(rect.x,rect.y,rect.width,1),color);Fill(new Rect(rect.x,rect.yMax-1,rect.width,1),color);Fill(new Rect(rect.x,rect.y,1,rect.height),color);Fill(new Rect(rect.xMax-1,rect.y,1,rect.height),color);}
    private void Text(Rect rect,string value,GUIStyle style,Color? color=null){var before=GUI.contentColor;GUI.contentColor=color??(style==small||style==wrapped?Muted:Color.white);GUI.Label(rect,value??"",style);GUI.contentColor=before;}
    private bool Button(Rect rect,string value,bool active=false,bool enabled=true,bool primary=false,GUIStyle textStyle=null){
        bool hover=enabled&&rect.Contains(Event.current.mousePosition);
        Fill(rect,!enabled?Panel:primary?Red:hover||active?Raised:Panel);Frame(rect,active?Red:Edge);
        if(active)Fill(new Rect(rect.x,rect.y,4,rect.height),Red);
        Text(rect,value,textStyle??button,enabled?Color.white:Muted*.7f);
        bool before=GUI.enabled;GUI.enabled=before&&enabled;bool pressed=GUI.Button(rect,GUIContent.none,GUIStyle.none);GUI.enabled=before;return pressed;
    }
    private void OnGUI(){
        if(HudEditor!=null&&HudEditor.IsOpen)return;
        if(options==null)return;
        if(Updates!=null&&Updates.WindowVisible)return;
        if(!IsOpen&&(!AttractPromptVisible&&!options.Current.showFps||Event.current.type!=EventType.Repaint))return;
        // The host polls keyboard/gamepad input and forwards one menu action.
        // Consume IMGUI keys before focused buttons or sliders can handle the
        // same key again. Pointer events still reach the normal GUI controls.
        if(IsOpen&&(Event.current.type==EventType.KeyDown||Event.current.type==EventType.KeyUp))Event.current.Use();
        Styles();
        var oldMatrix=GUI.matrix;GUI.depth=-11000;
        bool diagnostic=(IsOpen||AttractPromptVisible)&&diagnosticTarget!=null&&Event.current.type==EventType.Repaint;
        var previousTarget=RenderTexture.active;
        if((IsOpen||AttractPromptVisible)&&Event.current.type==EventType.Repaint)++DiagnosticRepaints;
        if(diagnostic){RenderTexture.active=diagnosticTarget;GL.PushMatrix();GL.LoadPixelMatrix(0,Screen.width,Screen.height,0);}
        try{
            if(CustomizingHud){HudCustomization.Draw(wheelNavigation);return;}
            if(!IsOpen){
                if(AttractPromptVisible)DrawAttractPrompt();
                GUI.matrix=oldMatrix;
                if(options.Current.showFps&&frameRateText.Length>0){
                    var box=new Rect(Screen.width*.5f-75,8,150,25);Fill(box,new Color(0,0,0,.72f));
                    Text(box,frameRateText,button);
                }
                return;
            }
            Fill(new Rect(0,0,Screen.width,Screen.height),new Color(0,0,0,.82f));
            float scale=Mathf.Min(1.5f,Mathf.Min(Screen.width/(Width+32),Screen.height/(Height+24)));
            GUI.matrix=Matrix4x4.TRS(new Vector3((Screen.width-Width*scale)*.5f,(Screen.height-Height*scale)*.5f,0),Quaternion.identity,new Vector3(scale,scale,1));
            Fill(new Rect(0,0,Width,Height),Ink);Frame(new Rect(0,0,Width,Height),Edge);Fill(new Rect(0,0,Width,5),Red);
            Fill(new Rect(0,108,Width,1),Edge);
            Text(new Rect(30,18,630,52),AttractOptions?"GAME SETTINGS":showOptions?"OPTIONS":online?"RACE MENU":"PAUSED",title);
            Text(new Rect(33,77,660,23),AttractOptions?"INITIAL D  /  ATTRACT SCREEN":"INITIAL D  /  "+courseName.ToUpperInvariant(),small);
            if(AttractOptions)Text(new Rect(750,33,238,27),"BACK TO ATTRACT",small);
            else if(online){Fill(new Rect(728,37,9,9),Red);Text(new Rect(750,29,238,26),"LIVE RACE CONTINUES",button);}
            else Text(new Rect(782,33,198,27),"TAKE A BREATHER",small);
            Text(new Rect(710,77,295,23),Updates!=null&&Updates.State==Idas3Updates.CheckState.Available?"UPDATE AVAILABLE — GAMEPLAY":"v"+Application.version,small,Updates!=null&&Updates.State==Idas3Updates.CheckState.Available?PromptYellow:Muted);
            if(Button(new Rect(984,22,34,35),"×")){Back();if(!IsOpen)return;}
            bool enabled=GUI.enabled;GUI.enabled=enabled&&!Modal&&!BindingInputBlocked;
            if(showOptions)OptionsView();else MainView();
            GUI.enabled=enabled;
            Fill(new Rect(24,630,Width-48,1),Edge);
            Text(new Rect(32,646,974,22),showOptions&&tab==3?"↑ ↓  DEVICE / ACTION    ← →  DEVICE / SLOT    ENTER / A  REBIND    ESC / B  BACK":"↑ ↓  SELECT    ← →  CHANGE    ENTER / A  CONFIRM    ESC / B  BACK",small);
            if(bindingChoice)BindingChoiceView();
            else if(bindings!=null&&bindings.IsCapturing)BindingCaptureView();
            else if(options.DisplayConfirmationPending)DisplayConfirmation();else if(pending!=Command.None)ExitConfirmation();
        }finally{
            GUI.matrix=oldMatrix;
            if(diagnostic){GL.PopMatrix();RenderTexture.active=previousTarget;diagnosticTarget=null;DiagnosticCaptureReady=true;}
        }
    }
    private void DrawAttractPrompt(){
        var safe=Screen.safeArea;
        safe=safe.width>0&&safe.height>0?new Rect(safe.x,Screen.height-safe.yMax,safe.width,safe.height):new Rect(0,0,Screen.width,Screen.height);
        // Size this secondary hint in screen pixels, independently of the large
        // arcade artwork. Integer bounds and real padding protect glyph edges.
        float scale=Mathf.Clamp(safe.height/720f,.75f,2f);
        float margin=Mathf.Ceil(16*scale),padding=Mathf.Ceil(10*scale);
        float artworkScale=Mathf.Min(Screen.width/640f,Screen.height/480f);
        float artworkLeft=(Screen.width-640*artworkScale)*.5f;
        float left=Mathf.Max(safe.xMin+margin,artworkLeft+180*artworkScale);
        float right=Mathf.Floor(safe.xMax-margin),bottom=Mathf.Floor(safe.yMax-12*scale);
        float available=right-left-2*padding;
        if(available<=0)return;
        attractPromptStyle.fontSize=Mathf.Clamp(Mathf.RoundToInt(13*scale),10,20);
        string text="HOLD "+attractControlLabel.ToUpperInvariant()+" FOR OPTIONS";
        var content=new GUIContent(text);
        Vector2 size=attractPromptStyle.CalcSize(content);
        while(size.x>available&&attractPromptStyle.fontSize>6){
            --attractPromptStyle.fontSize;size=attractPromptStyle.CalcSize(content);
        }
        float width=Mathf.Ceil(size.x)+2*padding,height=Mathf.Ceil(size.y)+Mathf.Ceil(8*scale)+2;
        var rect=new Rect(right-width,bottom-height,width,height);
        GUI.matrix=Matrix4x4.identity;
        Fill(rect,new Color(0,0,0,.58f));
        Text(new Rect(rect.x+padding-2,rect.y+2,rect.width-2*padding+4,rect.height-4),text,attractPromptStyle,PromptYellow);
        if(Updates!=null&&Updates.State==Idas3Updates.CheckState.Available){
            var updateRect=new Rect(rect.x,rect.y-height-4,rect.width,height);
            Fill(updateRect,new Color(0,0,0,.75f));
            Text(new Rect(updateRect.x+padding-2,updateRect.y+2,updateRect.width-2*padding+4,updateRect.height-4),"UPDATE AVAILABLE — OPTIONS > GAMEPLAY",attractPromptStyle,PromptYellow);
        }
        if(AttractHoldProgress>0){
            Fill(new Rect(rect.x,rect.yMax-2,rect.width,2),Edge);
            Fill(new Rect(rect.x,rect.yMax-2,Mathf.Round(rect.width*AttractHoldProgress),2),PromptYellow);
        }
    }
    private void MainView(){
        string[] actions=online?new[]{"RESUME","OPTIONS","LEAVE ONLINE BATTLE","QUIT GAME"}:
            new[]{"RESUME","OPTIONS","RESTART RACE",canRetire?"RETIRE RACE":"RETURN TO COURSE SELECT","QUIT GAME"};
        for(int i=0;i<actions.Length;++i){
            bool allowed=online||i!=2||canRestart;
            if(Button(new Rect(30,139+i*87,320,65),actions[i],selection==i,allowed,i==0))MainAction(i);
        }
        Fill(new Rect(378,139,631,412),Panel);Fill(new Rect(378,139,5,66),Red);
        Text(new Rect(404,160,573,37),online?"THE BATTLE IS STILL ON":"YOUR NEXT CORNER CAN WAIT",heading);
        Text(new Rect(405,215,554,72),online?"Opening this menu does not pause either driver. Return to the road when you are ready.":"Your race is paused. Adjust your settings, check the controls, or return to the road.",wrapped);
        SummaryRow(308,"CAMERA",CameraModes[options.Current.defaultCamera]);
        SummaryRow(370,"DISPLAY",options.Current.width+" × "+options.Current.height+"  /  "+DisplayModes[options.Current.displayMode]);
        SummaryRow(432,"AUDIO",Mathf.RoundToInt(options.Current.masterVolume*100)+"% MASTER VOLUME");
        Text(new Rect(405,562,573,45),online?"Leaving ends your participation in this battle.":"Restarting or leaving discards the current race attempt.",wrapped);
    }
    private void SummaryRow(float y,string name,string value){
        Fill(new Rect(405,y-8,573,1),Edge);Text(new Rect(405,y+1,180,22),name,small);
        Text(new Rect(570,y-2,405,31),value,label);
    }
    private void OptionsView(){
        const float categoryTop=139,categoryStep=48,categoryHeight=39;
        for(int i=0;i<Tabs.Length;++i){var rect=new Rect(30,categoryTop+i*categoryStep,207,categoryHeight);if(Button(rect,Tabs[i],tab==i))SelectTab(i);if(selection==0&&tab==i)Frame(rect,Color.white);}
        Text(new Rect(31,categoryTop+Tabs.Length*categoryStep+5,205,88),wheelNavigation?"STEERING: SELECT\nACCEL: EDIT / DONE\nBRAKE: BACK\nAPPLY to save":"CHOOSE CATEGORY\n↑ ↓ / STEERING\nCONFIRM to edit\nBACK to categories",wrapped);
        Fill(new Rect(262,130,748,413),Panel);
        Text(new Rect(282,144,660,36),tab==3?"CONTROLLER & KEYBOARD":Tabs[tab],heading);
        if(selection==0)Frame(new Rect(276,139,716,42),Red);
        if(wheelEditing)Text(new Rect(760,148,225,27),"STEERING: CHANGE",small,Color.white);
        var v=options.Draft;
        if(tab==0){
            VolumeRow(0,"MASTER",v.masterVolume,value=>v.masterVolume=value);
            VolumeRow(1,"MUSIC",v.musicVolume,value=>v.musicVolume=value);
            VolumeRow(2,"ENGINE",v.engineVolume,value=>v.engineVolume=value);
            VolumeRow(3,"TIRE SQUEAL",v.tireVolume,value=>v.tireVolume=value);
            VolumeRow(4,"EFFECTS / VOICES",v.effectsVolume,value=>v.effectsVolume=value);
            Text(new Rect(288,497,683,29),"Changes take effect when you choose APPLY.",small);
        }else if(tab==7){
            if(Button(new Rect(278,180,714,29),"CUSTOMIZE",selection==1)){selection=1;OpenHudCustomization();}
            if(Button(new Rect(278,210,714,29),"EDIT HUD LAYOUT",selection==2)){selection=2;OpenHudEditor();}
            ChoiceRow(2,"MINIMAP SIZE",v.HudSizePercent(5)+"%");
            ChoiceRow(3,"MINIMAP ZOOM OUT",new[]{"WIDEST (50%)","WIDER (75%)","ORIGINAL"}[v.minimapZoom]);
            ChoiceRow(4,"TIME / SECTION TIMES",v.HudSizePercent(1)+"%");
            ChoiceRow(5,"SPEEDOMETER / GEAR",v.HudSizePercent(2)+"%");
            ChoiceRow(6,"TIME ATTACK RECORDS",v.HudSizePercent(3)+"%");
            ChoiceRow(7,"LEGEND OPPONENT PANEL",v.HudSizePercent(6)+"%");
            ChoiceRow(8,"ONLINE OPPONENT PANEL",v.HudSizePercent(7)+"%");
            ChoiceRow(9,"REAR-VIEW MIRROR",v.HudSizePercent(4)+"%");
            ChoiceRow(10,"TIME EXTENDED",v.HudSizePercent(9)+"%");
            ChoiceRow(11,"ACCEPTING CHALLENGERS",v.HudSizePercent(8)+"%");
        }else if(tab==1){
            ChoiceRow(0,"DISPLAY MODE",DisplayModes[v.displayMode]);ChoiceRow(1,"RESOLUTION",v.width+" × "+v.height);
            ChoiceRow(2,"VERTICAL SYNC",v.vSync?"ON":"OFF");ChoiceRow(3,"FRAME LIMIT",v.frameRateLimit==0?"UNLIMITED":v.frameRateLimit+" FPS");
            ChoiceRow(4,"ANTI-ALIASING",v.antiAliasing==0?"OFF":v.antiAliasing+"× MSAA");
            int preset=Idas3GameOptions.PerformancePreset(v);
            ChoiceRow(5,"QUALITY PRESET",preset<0?"CUSTOM":new[]{"ORIGINAL","BALANCED","LOW"}[preset]);
            ChoiceRow(6,"WEATHER & SPRAY",new[]{"FULL","REDUCED"}[v.rainDetail]);
            ChoiceRow(7,"IMPORTED SCENERY",new[]{"ORIGINAL","BALANCED","LOW"}[v.importedSceneryDetail]);
            string help=selection==6?"Balanced: 720p / 2x AA. Low: 540p / no AA. Both use reduced effects and scenery, capped at 60 FPS. Mirror and gameplay stay enabled.":
                selection==7?"Reduced uses fewer rain and spray particles. Rain stays visible and wet grip is unchanged.":
                selection==8?"Lower detail uses simpler trees sooner and draws less distant scenery on Hakone and Sadamine. Roads and collision stay the same.":
                selection==2?"Resolution changes the entire game image and window size in windowed mode. Confirm display changes within 15 seconds.":
                v.vSync?"VSync synchronizes to your display. Turn it off to use the frame limit.":"Display changes must be confirmed within 15 seconds.";
            Text(new Rect(288,504,687,40),help,wrapped);
        }else if(tab==2){
            ChoiceRow(0,"DEFAULT CAMERA",CameraModes[v.defaultCamera]);ChoiceRow(1,"SHOW FRAME RATE",v.showFps?"ON":"OFF");
            ChoiceRow(2,"MUTE WHEN UNFOCUSED",v.muteWhenUnfocused?"ON":"OFF");
            ChoiceRow(3,"CONTROLLER RESPONSE",ControllerResponses[v.controllerResponse]);
            SliderRow(4,"STEERING DEADZONE",v.SteeringDeadzone,.3f,value=>v.SteeringDeadzone=value);
            SliderRow(5,"STEERING SMOOTHING",v.steeringSmoothing,1,value=>v.steeringSmoothing=value);
            Text(new Rect(290,395,294,27),"FULL TUNE",label);
            if(Button(new Rect(595,394,387,29),"999999 POINTS + UPGRADES",selection==7,FullTuneAvailable)){selection=7;queued=Command.FullTune;}
            Text(new Rect(290,429,294,27),"GAME UPDATES",label);
            if(Button(new Rect(595,428,387,29),Updates?.ButtonLabel??"CHECK FOR UPDATES",selection==8,AttractOptions&&Updates!=null&&Updates.CanActivate)){selection=8;Updates.Activate();}
            ChoiceRow(8,"DISCORD RICH PRESENCE",v.discordPresence?"ON":"OFF");
            ChoiceRow(9,"AI DRIVER DIFFICULTY",new[]{"NORMAL","HARD (+5% PACE)","EXPERT (+10% PACE)"}[v.aiDifficulty]);
            string help=selection==7?(FullTuneAvailable?"Choose a save, then a make and car for upgrades.":"Finish the current screen and leave online play to use Full Tune."):
                selection==8?(!AttractOptions?"Return to the title screen to check for updates.":Updates?.Message??"Update checking is unavailable."):"Deadzone is saved per controller response. APPLY saves changes.";
            Text(new Rect(288,536,687,16),selection==10?"Legend of the Streets only. Bunta Challenge keeps its original difficulty.":selection==9?"Shares game activity with the Discord desktop app. APPLY saves your choice.":help,small);
        }else if(tab==5){
            ChoiceRow(0,"COMMUNITY TIMES",v.communityTimes?"ON":"OFF");
            if(Button(new Rect(595,257,387,35),"VIEW SHARED RANKINGS",selection==2))Application.OpenURL(Idas3CommunityTimes.ServiceUrl);
            Text(new Rect(288,344,681,72),"Only new completed Time Attacks with a replay can be uploaded. Previous saved times cannot be uploaded.",wrapped);
            Text(new Rect(288,422,681,65),CommunityStatus,wrapped);
            Text(new Rect(288,493,681,55),"New times include a driving replay for admin review. No Steam linking. OFF stops uploads; existing submissions remain.",wrapped);
        }else if(tab==6){
            if(Button(new Rect(282,197,692,39),"OPEN REPLAY LIBRARY",selection==1))queued=Command.Replays;
            ChoiceRow(1,"TIME ATTACK",v.communityTimes?"ON — REQUIRED FOR SHARING":v.replayTimeAttack?"ON":"OFF");
            ChoiceRow(2,"ONLINE BATTLES",v.replayOnline?"ON":"OFF");
            ChoiceRow(3,"LEGEND OF THE STREETS",v.replayLegend?"ON":"OFF");
            Text(new Rect(288,436,681,40),"Recording choices apply to the next race. Community Times always requires a Time Attack replay, even if optional recording is off.",wrapped);
            Text(new Rect(288,481,681,55),ReplayStatus,wrapped);
        }else if(tab==3)ControlsView();else{
            ChoiceRow(0,"FORCE FEEDBACK",v.wheelForceFeedback?"ON":"OFF");
            ChoiceRow(1,"DEVICE",WheelDeviceName(v.wheelFeedbackDevice));
            SliderRow(2,"STRENGTH",v.wheelFeedbackStrength,1,value=>v.wheelFeedbackStrength=value);
            ChoiceRow(3,"INVERT FORCE",v.wheelFeedbackInvert?"ON":"OFF");
            Text(new Rect(288,439,681,87),wheelFeedback?.StatusText??"Wheel feedback is unavailable.",wrapped);
        }
        bool unsaved=tab==3?bindings!=null&&bindings.HasUnsavedChanges:options.HasUnsavedChanges;
        string error=tab==3?bindings?.LastError:options.LastError;
        string captureError=tab==3?bindings?.CaptureError:null;
        string status=!string.IsNullOrEmpty(error)?error:!string.IsNullOrEmpty(captureError)?captureError:!string.IsNullOrEmpty(notice)?notice:tab==3&&!string.IsNullOrEmpty(bindings?.LastNotice)?bindings.LastNotice:unsaved?"UNSAVED CHANGES — APPLY to save; leaving settings discards changes.":"Settings are saved on this computer.";
        Text(new Rect(274,552,730,25),status,small,unsaved?Color.white:Muted);
        if(Button(new Rect(262,583,177,35),"RESET DEFAULTS",selection==Rows+1)){
            ResetDefaults();
        }
        if(Button(new Rect(458,583,254,35),"APPLY",selection==Rows+2,true,true))Apply();
        if(Button(new Rect(731,583,279,35),"BACK",selection==Rows+3))Back();
    }
    private void VolumeRow(int row,string name,float value,Action<float> set){
        float y=200+row*56;var rect=new Rect(278,y-5,714,55);if(selection==row+1)Frame(rect,Red);
        Text(new Rect(290,y+5,231,32),name,label);
        float before=value;value=GUI.HorizontalSlider(new Rect(557,y+13,317,20),value,0,1);
        if(!Mathf.Approximately(before,value)){selection=row+1;set(value);notice="";}
        Text(new Rect(900,y+5,78,33),Mathf.RoundToInt(value*100)+"%",button);
    }
    private string WheelDeviceName(string id){
        if(wheelFeedback!=null)foreach(var choice in wheelFeedback.Choices)if(choice.id==id)return choice.name;
        return string.IsNullOrEmpty(id)?"AUTOMATIC":"DISCONNECTED DEVICE";
    }
    private void SliderRow(int row,string name,float value,float maximum,Action<float> set){
        if(tab==2){
            float compactY=188+row*34;if(selection==row+1)Frame(new Rect(278,compactY,714,37),Red);
            Text(new Rect(290,compactY+7,294,27),name,label);
            float compactValue=GUI.HorizontalSlider(new Rect(595,compactY+13,279,20),value,0,maximum);
            if(!Mathf.Approximately(compactValue,value)){selection=row+1;set(Mathf.Round(compactValue*100)/100f);notice="";}
            Text(new Rect(899,compactY+5,79,27),Mathf.RoundToInt(compactValue*100)+"%",button);return;
        }
        float y=194+row*(tab==2?48:59);if(selection==row+1)Frame(new Rect(278,y,714,tab==2?46:49),Red);
        Text(new Rect(290,y+13,294,31),name,label);
        float next=GUI.HorizontalSlider(new Rect(595,y+18,279,20),value,0,maximum);
        if(!Mathf.Approximately(next,value)){selection=row+1;set(Mathf.Round(next*100)/100f);notice="";}
        Text(new Rect(899,y+8,79,33),Mathf.RoundToInt(next*100)+"%",button);
    }
    private void ChoiceRow(int row,string name,string value){
        if(tab==1||tab==2||tab==7){
            float compactY=tab==7?180+row*30:tab==2?188+row*34:194+row*38;
            if(selection==row+1)Frame(new Rect(278,compactY,714,tab==7?29:37),Red);
            Text(new Rect(290,compactY+3,294,27),name,label);
            if(Button(new Rect(595,compactY+2,34,tab==7?27:29),"‹")){selection=row+1;Adjust(row,-1);}
            Text(new Rect(636,compactY+5,304,27),value,button);
            if(Button(new Rect(948,compactY+2,34,tab==7?27:29),"›")){selection=row+1;Adjust(row,1);}
            return;
        }
        float y=194+row*(tab==2?48:59);if(selection==row+1)Frame(new Rect(278,y,714,tab==2?46:49),Red);
        Text(new Rect(290,y+13,294,31),name,label);
        if(Button(new Rect(595,y+7,34,35),"‹")){selection=row+1;Adjust(row,-1);}
        Text(new Rect(636,y+8,304,33),value,button);
        if(Button(new Rect(948,y+7,34,35),"›")){selection=row+1;Adjust(row,1);}
    }
    private void ControlsView(){
        if(bindings==null){Text(new Rect(288,206,690,45),"Control bindings are unavailable.",wrapped);return;}
        bool deviceControls=controllerDevices!=null;
        if(deviceControls){
            var choices=controllerDevices.Choices;int selected=-1;
            for(int i=0;i<choices.Count;++i)if(choices[i].key==controllerDevices.SelectedKey){selected=i;break;}
            string choice=selected>=0?choices[selected].label:controllerDevices.SelectedKey;
            Text(new Rect(288,190,83,23),"DEVICE",small);
            if(Button(new Rect(378,183,34,31),"‹",DeviceRowSelected,choices.Count>1))ChangeControllerDevice(-1);
            if(Button(new Rect(420,183,520,31),choice,DeviceRowSelected,choices.Count>1))ChangeControllerDevice(1);
            if(Button(new Rect(948,183,34,31),"›",DeviceRowSelected,choices.Count>1))ChangeControllerDevice(1);
            string active=controllerDevices.Controls.Count>0?(controllerDevices.UsingFallback?"TEMPORARY DEVICE  /  ":"ACTIVE  /  ")+controllerDevices.ActiveName:controllerDevices.SelectedKey=="keyboard"?"Keyboard only. Select a controller above to edit its bindings.":"Controller disconnected. Keyboard controls remain available.";
            Text(new Rect(288,218,694,22),active,small);
        }
        float headerY=deviceControls?241:192,firstY=deviceControls?270:220,rowHeight=deviceControls?25:29;
        Text(new Rect(288,headerY,169,23),"ACTION",small);
        string[] columns={"KEYBOARD 1","KEYBOARD 2","KEYBOARD 3","CONTROLLER"};
        for(int col=0;col<4;++col)if(Button(new Rect(col==3?804:462+col*114,headerY,col==3?180:110,24),columns[col],bindingColumn==col,true,false,bindingButton))SelectBindingColumn(col);
        for(int row=0;row<10;++row){
            float y=firstY+row*rowHeight;var action=(Idas3ControlBindings.ActionId)row;
            Text(new Rect(288,y+5,169,23),Idas3ControlBindings.ActionName(action),small,selection==row+BindingFirstSelection?Color.white:Muted);
            for(int col=0;col<4;++col){
                var slot=(Idas3ControlBindings.Slot)col;
                var cell=new Rect(col==3?804:462+col*114,y,col==3?180:110,deviceControls?23:26);
                if(Button(cell,bindings.BindingName(action,slot),selection==row+BindingFirstSelection&&bindingColumn==col,!deviceControls||col!=3||controllerDevices.Controls.Count>0,false,bindingButton))OpenBindingChoice(action,slot);
            }
        }
        Text(new Rect(288,deviceControls?524:514,702,18),"Fixed menu controls: arrows / D-pad, Enter / A, Esc / B. F1 always opens online.",small);
    }
    private void BindingChoiceView(){
        ModalFrame(Idas3ControlBindings.ActionName(captureAction).ToUpperInvariant(),"Choose REBIND to assign a control, CLEAR to remove this slot, or BACK.\nRebinding waits up to 15 seconds. Leave controls at rest to cancel without assigning anything.");
        string[] choices={"REBIND","CLEAR","BACK"};
        for(int i=0;i<3;++i)if(Button(new Rect(247+i*184,404,174,48),choices[i],bindingChoiceSelection==i)){bindingChoiceSelection=i;ActivateBindingChoice();}
    }
    private void BindingCaptureView(){
        bool controller=captureSlot==Idas3ControlBindings.Slot.Controller;
        string slot=controller?"CONTROLLER  /  "+(controllerDevices?.ActiveName??bindings.ActiveControllerProfileLabel):"KEYBOARD "+((int)captureSlot+1)+"  /  shared across devices";
        string prompt=!string.IsNullOrEmpty(bindings.CaptureError)?bindings.CaptureError:bindings.CapturePrompt;
        ModalFrame("REBIND "+Idas3ControlBindings.ActionName(captureAction).ToUpperInvariant(),slot+"\n"+prompt+"\n"+(controller?"Assigned controller controls swap actions. APPLY saves your changes.":"Esc cancels. Keyboard conflicts must be cleared before reassignment."));
        if(Button(new Rect(247,404,253,48),"CLEAR SLOT")){
            bool cleared=bindings.ClearDraft(captureAction,captureSlot);bindings.CancelCapture();
            notice=cleared?"Binding cleared. Apply to save.":bindings.LastError??"Could not clear this binding.";
        }
        if(Button(new Rect(519,404,274,48),"CANCEL",true)){
            bindings.CancelCapture();notice="Binding unchanged.";
        }
    }
    private void ModalFrame(string headingText,string body){
        Fill(new Rect(0,0,Width,Height),new Color(0,0,0,.88f));Fill(new Rect(218,202,604,278),Panel);Frame(new Rect(218,202,604,278),Edge);
        Fill(new Rect(218,202,604,4),Red);Text(new Rect(248,229,545,44),headingText,heading);
        Text(new Rect(248,287,539,88),body,wrapped);
    }
    private void DisplayConfirmation(){
        ModalFrame("KEEP THESE DISPLAY SETTINGS?","Confirm within "+Mathf.CeilToInt((float)options.SecondsRemaining)+" seconds.\nUnconfirmed changes will be reverted automatically.");
        if(Button(new Rect(247,404,253,48),"REVERT",modalSelection==0))options.RevertDisplay();
        if(Button(new Rect(519,404,274,48),"KEEP CHANGES",modalSelection==1,true,true))KeepDisplay();
    }
    private void ExitConfirmation(){
        bool retiring=pending==Command.Retire||(pending==Command.ReturnToCourse&&canRetire);
        string headingText=retiring?"RETIRE THIS RACE?":pending==Command.Restart?"RESTART THIS RACE?":pending==Command.Quit?"QUIT INITIAL D?":pending==Command.LeaveOnline?"LEAVE THIS ONLINE BATTLE?":"RETURN TO COURSE SELECT?";
        string body=retiring?"This counts as a time-out loss. After the results and rival dialogue, you can retry or choose another course.":pending==Command.LeaveOnline?"You will disconnect from the other driver and leave this battle.":online?"You will disconnect from the other driver. Your saved settings and progress will remain.":"The current race attempt will be discarded. Your saved progress will remain.";
        ModalFrame(headingText,body);
        if(Button(new Rect(247,404,253,48),"CANCEL",modalSelection==0))pending=Command.None;
        if(Button(new Rect(519,404,274,48),"CONFIRM",modalSelection==1,true,true)){queued=pending;pending=Command.None;}
    }
}
