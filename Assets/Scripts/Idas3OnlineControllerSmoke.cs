using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.Layouts;
using UnityEngine.InputSystem.LowLevel;
using Idas3.Multiplayer;

// Private opt-in regression: provider input must recover after the online
// overlay closes. Steam initialization is an explicit second-run option.
public sealed class Idas3OnlineControllerSmoke : MonoBehaviour
{
    private static string pendingRoot;
    private static bool pendingSteam,pendingRequirePhysical;
    private static int pendingPhysicalSlot=-1;
    private static Idas3OnlineControllerSmoke active;
    private Idas3SceneGame host;
    private Idas3MultiplayerMenu menu;
    private Gamepad gamepad;
    private Joystick menuWheel;
    private string root, profile, stage="initializing";
    private KeyCode physicalKey;
    private int pulse, checks,physicalSlot=-1;
    private bool finished, steam,requirePhysical;
    private double began;
    private readonly List<Observation> observations=new List<Observation>();
    private readonly List<Census> censuses=new List<Census>();
    private readonly List<EndpointCheck> endpointChecks=new List<EndpointCheck>();
    private Census physicalBaseline;
    private static readonly InputDeviceDescription Description=new InputDeviceDescription{
        interfaceName="Idas3Diagnostic",manufacturer="Private input test",product="IDAS3 Online Input Test",serial="online-input-smoke"};

    [Serializable] private sealed class Observation
    {
        public string stage,selectedKey,providerProfile,bindingProfile,activeName,transport,state;
        public int frame,controlCount,rawButtons,rawTrigger,rawX,mappedButtons,mappedTrigger,mappedX;
        public double seconds;
        public uint foregroundProcessId;
        public bool foregroundIsPlayer;
        public ulong sourceTicks;
        public uint nativeFlags;
        public bool actualFocus,focusOverride,providerConnected,mappedConnected,bindingBlocked,menuOpen,menuBlocked,pauseOpen,keyboardW,transportAvailable;
    }
    [Serializable] private sealed class SlotSample
    {
        public string dll,error;
        public int slot;
        public uint result;
        public bool connected;
        public ushort buttons;
        public byte leftTrigger,rightTrigger;
        public short leftX,leftY,rightX,rightY;
    }
    [Serializable] private sealed class Census { public string stage;public double seconds;public SlotSample[] samples; }
    [Serializable] private sealed class EndpointCheck { public string stage;public bool passed;public int baselineConnected;public string[] missing; }
    [Serializable] private sealed class Report
    {
        public bool passed,shutdownComplete,steam,requirePhysical;
        public int checks,physicalSlot;
        public string error,scope;
        public Observation[] observations;
        public Census[] censuses;
        public EndpointCheck[] endpointChecks;
    }
    [DllImport("xinput1_4.dll",EntryPoint="XInputGetState")] private static extern uint ReadXInput14(uint slot,out Idas3Native.PadState state);
    [DllImport("user32.dll")] private static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window,out uint process);

    public static bool Configure(ref string saves)
    {
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-online-controller-smoke");if(at<0)return false;
        if(at+1>=args.Length)throw new ArgumentException("Online controller diagnostic needs a fresh output directory.");
        pendingRoot=Path.GetFullPath(args[at+1]);pendingSteam=Array.IndexOf(args,"-idas3-online-controller-steam")>=0;
        pendingRequirePhysical=Array.IndexOf(args,"-idas3-online-controller-require-physical")>=0;pendingPhysicalSlot=-1;
        int slotAt=Array.IndexOf(args,"-idas3-online-controller-physical-slot");
        if(slotAt>=0)
        {
            if(slotAt+1>=args.Length||!int.TryParse(args[slotAt+1],out pendingPhysicalSlot)||pendingPhysicalSlot<0||pendingPhysicalSlot>3)
                throw new ArgumentException("Physical XInput slot must be 0, 1, 2 or 3.");
            pendingRequirePhysical=true;
        }
        if(Directory.Exists(pendingRoot)||File.Exists(pendingRoot))throw new IOException("Use a fresh online controller diagnostic directory.");
        Directory.CreateDirectory(pendingRoot);saves=Path.Combine(pendingRoot,"userdata");Directory.CreateDirectory(saves);
        File.WriteAllText(Path.Combine(saves,"settings.txt"),"0 0 0 0 0 1 1 0\n");
        File.WriteAllText(Path.Combine(saves,"native_selection.txt"),"0 0\n");
        File.WriteAllText(Path.Combine(pendingRoot,"ISOLATED_ONLINE_INPUT_TEST.txt"),"Private diagnostic profile. LAN-only controller navigation may create then leave an empty local room. No public matchmaking or remote messages.\n");
        Screen.SetResolution(1200,720,FullScreenMode.Windowed);AudioListener.volume=0;return true;
    }
    public static void Attach(Idas3SceneGame game)
    {
        if(pendingRoot==null)return;active=game.gameObject.AddComponent<Idas3OnlineControllerSmoke>();
        active.host=game;active.root=pendingRoot;active.steam=pendingSteam;active.requirePhysical=pendingRequirePhysical;active.physicalSlot=pendingPhysicalSlot;active.began=Time.realtimeSinceStartupAsDouble;
        active.StartCoroutine(active.Guard(active.Run()));
    }
    internal static void PreparePhysicalInput(ref Func<KeyCode,bool> key)
    {if(active!=null&&!active.finished)key=active.KeyHeld;}
    private bool KeyHeld(KeyCode key)=>physicalKey!=KeyCode.None&&key==physicalKey;
    private static bool HasForeground()
    {
        GetWindowThreadProcessId(GetForegroundWindow(),out uint process);
        return process==(uint)System.Diagnostics.Process.GetCurrentProcess().Id;
    }
    internal static bool PrepareFrame(ref Idas3Native.FrameInput frame)
    {
        if(active==null)return true;if(active.finished)return false;
        if(active.pulse!=0){frame.SetKey(active.pulse);active.pulse=0;}return true;
    }
    private void Check(bool value,string error)
    {++checks;if(!value)throw new InvalidOperationException(error+" (stage="+stage+", native flags="+host.Status.flags+")");}
    private IEnumerator Frames(int count){for(int i=0;i<count;++i)yield return null;}
    private IEnumerator Until(Func<bool> condition,double seconds,string reason)
    {double deadline=Time.realtimeSinceStartupAsDouble+seconds;while(!condition()&&Time.realtimeSinceStartupAsDouble<deadline)yield return null;Check(condition(),reason);}
    private IEnumerator Guard(IEnumerator routine)
    {
        var stack=new Stack<IEnumerator>();stack.Push(routine);
        while(stack.Count>0&&!finished)
        {
            object next=null;Exception failure=null;
            try{if(!stack.Peek().MoveNext()){stack.Pop();continue;}next=stack.Peek().Current;}catch(Exception error){failure=error;}
            if(failure!=null){Finish(false,failure.ToString());yield break;}
            if(next is IEnumerator child)stack.Push(child);else yield return next;
        }
    }
    private void Update()
    {
        if(!finished&&(host.Failure!=null||Time.realtimeSinceStartupAsDouble-began>120))Finish(false,host.Failure??"Online controller check timed out.");
    }
    private void Pad(float throttle=0,float x=0)
    {if(gamepad!=null)InputSystem.QueueStateEvent(gamepad,new GamepadState{rightTrigger=throttle,leftStick=new Vector2(x,0)});}
    private IEnumerator Button(GamepadButton button){InputSystem.QueueStateEvent(gamepad,new GamepadState().WithButton(button));yield return Frames(3);Pad();yield return Frames(3);}
    private IEnumerator FocusAction(string suffix){
        for(int i=0;i<120&&(menu.ControllerSelection==null||!menu.ControllerSelection.EndsWith(suffix,StringComparison.Ordinal));++i)yield return Button(GamepadButton.DpadDown);
        Check(menu.ControllerSelection!=null&&menu.ControllerSelection.EndsWith(suffix,StringComparison.Ordinal),"Controller cannot reach "+suffix);
    }
    private IEnumerator ControllerMenus(){
        Pad();yield return Frames(5);yield return Button(GamepadButton.Select);
        Check(menu.IsOpen&&menu.ControllerActionCount>3,"Select did not open navigable Online menu");
        yield return FocusAction(":JOIN WITH ADDRESS");yield return Button(GamepadButton.South);
        yield return FocusAction(":EDIT");yield return Button(GamepadButton.South);
        yield return FocusAction(":1");yield return Button(GamepadButton.South);
        yield return FocusAction(":DONE");yield return Button(GamepadButton.South);
        Check(menu.ControllerActionCount<40,"Controller keyboard did not return to code entry");
        yield return Button(GamepadButton.East);
        yield return FocusAction(":HOST A BATTLE");yield return Button(GamepadButton.South);
        yield return Until(()=>host.MultiplayerSession.InLobby,5,"Controller could not host LAN room");yield return Frames(8);
        yield return FocusAction(":AT  /  CHANGE");yield return Button(GamepadButton.South);
        Check(menu.ControllerSelection!=null,"Lobby focus lost after changing transmission");
        yield return FocusAction(":LEAVE ROOM");yield return Button(GamepadButton.South);
        yield return Until(()=>!host.MultiplayerSession.InLobby,5,"Controller could not leave room");
        yield return Button(GamepadButton.East);Check(!menu.IsOpen,"B did not close Online menu");
        yield return Button(GamepadButton.Start);Check(host.PauseMenu.IsOpen,"Start did not open pause");
        yield return Button(GamepadButton.DpadDown);yield return Button(GamepadButton.South);
        Check(host.PauseMenu.CategoryFocused,"Settings category list lacks controller focus");
        yield return Button(GamepadButton.DpadDown);Check(host.PauseMenu.SelectedTab==1,"Controller did not choose Graphics category");
        yield return Button(GamepadButton.South);Check(!host.PauseMenu.CategoryFocused,"A did not enter category");
        yield return Button(GamepadButton.East);Check(host.PauseMenu.CategoryFocused,"B did not return to categories");
        yield return Button(GamepadButton.DpadDown);Check(host.PauseMenu.SelectedTab==2,"Controller could not select another category");
        yield return Button(GamepadButton.East);yield return Button(GamepadButton.East);Check(!host.PauseMenu.IsOpen,"Controller could not return to race");
    }
    private IEnumerator WheelInput(float steering=0,bool confirm=false,bool back=false){
        InputSystem.QueueDeltaStateEvent(menuWheel.stick,new Vector2(steering,back?1:0));
        using(StateEvent.From(menuWheel,out var press)){menuWheel.trigger.WriteValueIntoEvent(confirm?1f:0f,press);menuWheel.stick.WriteValueIntoEvent(new Vector2(steering,back?1:0),press);InputSystem.QueueEvent(press);}yield return Frames(4);
        using(StateEvent.From(menuWheel,out var release)){menuWheel.trigger.WriteValueIntoEvent(0f,release);menuWheel.stick.WriteValueIntoEvent(Vector2.zero,release);InputSystem.QueueEvent(release);}yield return Frames(4);
    }
    private IEnumerator WheelAction(string text){
        for(int i=0;i<130&&(menu.ControllerSelection==null||!menu.ControllerSelection.Contains(text));++i)yield return WheelInput(1);
        Check(menu.ControllerSelection!=null&&menu.ControllerSelection.Contains(text),"Wheel cannot reach "+text);
        yield return WheelInput(confirm:true);
    }
    private IEnumerator WheelMenus(){
        stage="wheel-menu-navigation";Pad();menuWheel=InputSystem.AddDevice<Joystick>();yield return Frames(8);
        string key=null;foreach(var choice in host.ControllerDevices.Choices)if(choice.label.EndsWith("device "+menuWheel.deviceId))key=choice.key;
        Check(key!=null&&host.ControllerDevices.Select(key),"Synthetic wheel selection");yield return Frames(6);
        Check(host.ControllerDevices.ActiveIsGeneric,"Wheel should use generic device menu path");
        var b=host.ControlBindings;b.BeginEdit();
        foreach(var c in host.ControllerDevices.Controls){
            if(c.path=="trigger")Check(b.TrySetDraftControl(Idas3ControlBindings.ActionId.Accelerate,c,1,0),"Wheel accelerator bind");
            if(c.path=="stick/y")Check(b.TrySetDraftControl(Idas3ControlBindings.ActionId.Brake,c,1,0),"Wheel brake bind");
            if(c.path=="stick/x"){
                Check(b.TrySetDraftControl(Idas3ControlBindings.ActionId.SteerLeft,c,-1,0),"Wheel left bind");
                Check(b.TrySetDraftControl(Idas3ControlBindings.ActionId.SteerRight,c,1,0),"Wheel right bind");
            }
        }
        Check(b.ApplyDraft(),"Apply wheel bindings");yield return WheelInput();
        // Open is setup only. Every category, row, value and exit below uses
        // actual Joystick events through the provider and SceneGame router.
        host.PauseMenu.SetOpen(true);yield return Frames(6);
        yield return WheelInput(1);yield return WheelInput(confirm:true);Check(host.PauseMenu.CategoryFocused,"Wheel enters settings categories");
        yield return WheelInput(confirm:true);yield return WheelInput(1);yield return WheelInput(confirm:true);
        Check(host.PauseMenu.WheelEditing,"Wheel accelerator enters music-volume edit");
        float original=host.GameOptions.Draft.musicVolume;
        yield return WheelInput(-1);Check(host.GameOptions.Draft.musicVolume<original,"Wheel steering edits music volume");
        yield return new WaitForEndOfFrame();var picture=ScreenCapture.CaptureScreenshotAsTexture();File.WriteAllBytes(Path.Combine(root,"wheel-options.png"),picture.EncodeToPNG());Destroy(picture);
        yield return WheelInput(back:true);Check(!host.PauseMenu.WheelEditing,"Brake finishes value edit");
        for(int i=0;i<5;++i)yield return WheelInput(1);yield return WheelInput(confirm:true);Check(!host.GameOptions.HasUnsavedChanges,"Wheel reaches Apply");
        yield return WheelInput(back:true);yield return WheelInput(1);Check(host.PauseMenu.SelectedTab==1,"Wheel selects Graphics category");
        yield return WheelInput(1);yield return WheelInput(1);yield return WheelInput(confirm:true);yield return WheelInput(1);
        yield return WheelInput(confirm:true);yield return WheelInput(confirm:true);Check(host.PauseMenu.BindingChoiceVisible,"Wheel opens binding actions");
        yield return new WaitForEndOfFrame();picture=ScreenCapture.CaptureScreenshotAsTexture();File.WriteAllBytes(Path.Combine(root,"wheel-bindings.png"),picture.EncodeToPNG());Destroy(picture);
        yield return WheelInput(back:true);Check(!host.PauseMenu.BindingChoiceVisible,"Wheel cancels binding actions");
        yield return WheelInput(back:true);yield return WheelInput(back:true);
        yield return WheelInput(back:true);yield return WheelInput(back:true);Check(!host.PauseMenu.IsOpen,"Wheel exits pause");
        menu.SetOpen(true);yield return Frames(6);yield return WheelAction(":JOIN WITH ADDRESS");yield return WheelAction(":EDIT");yield return WheelAction(":1");yield return WheelAction(":DONE");yield return WheelInput(back:true);
        yield return WheelAction(":HOST A BATTLE");yield return Until(()=>host.MultiplayerSession.InLobby,5,"Wheel hosts LAN room");yield return Frames(6);
        yield return WheelAction(":MUSIC  /");Check(host.RaceMusicMenu.IsOpen,"Wheel opens music picker");
        int song=host.RaceMusicMenu.HighlightedTrackId;yield return WheelInput(1);Check(host.RaceMusicMenu.HighlightedTrackId!=song,"Steering selects another song without paddles");
        yield return WheelInput(confirm:true);Check(!host.RaceMusicMenu.IsOpen,"Accelerator confirms music");
        yield return WheelAction(":LEAVE ROOM");yield return Until(()=>!host.MultiplayerSession.InLobby,5,"Wheel leaves lobby");yield return WheelInput(back:true);Check(!menu.IsOpen,"Wheel brake closes Online");
        InputSystem.RemoveDevice(menuWheel);menuWheel=null;
    }
    private void Observe(string name)
    {
        stage=name;host.ControllerDevices.TryRead(out var raw);var mapped=host.DiagnosticSubmittedInput;var session=host.MultiplayerSession;
        GetWindowThreadProcessId(GetForegroundWindow(),out uint foregroundProcess);
        observations.Add(new Observation{stage=name,frame=Time.frameCount,seconds=Time.realtimeSinceStartupAsDouble-began,
            actualFocus=Application.isFocused,focusOverride=host.DiagnosticFocusOverride??false,sourceTicks=host.Status.simulationTicks,nativeFlags=host.Status.flags,
            foregroundProcessId=foregroundProcess,foregroundIsPlayer=foregroundProcess==(uint)System.Diagnostics.Process.GetCurrentProcess().Id,
            selectedKey=host.ControllerDevices.SelectedKey,providerProfile=host.ControllerDevices.ActiveProfileKey,bindingProfile=host.ControlBindings.ActiveControllerProfileKey,
            activeName=host.ControllerDevices.ActiveName,controlCount=host.ControllerDevices.Controls.Count,providerConnected=raw.connected,mappedConnected=mapped.padConnected!=0,
            rawButtons=raw.buttons,rawTrigger=raw.rightTrigger,rawX=raw.thumbLX,mappedButtons=(int)mapped.padButtons,mappedTrigger=(int)mapped.rightTrigger,mappedX=mapped.thumbLX,
            keyboardW=(mapped.key2&(1u<<23))!=0,bindingBlocked=host.ControlBindings.SuppressInput,menuOpen=menu!=null&&menu.IsOpen,
            menuBlocked=menu!=null&&menu.BlocksGameInput,pauseOpen=host.PauseMenu.IsOpen,transport=session.TransportName,state=session.StateName,transportAvailable=session.Available});
    }
    private void CheckDriving(string name,int throttle,int x)
    {
        Observe(name);var sample=observations[observations.Count-1];
        Check(sample.providerConnected&&sample.controlCount>0,"Selected controller disappeared");
        Check(sample.providerProfile==profile&&sample.bindingProfile==profile,"Controller profile changed");
        Check(!sample.bindingBlocked&&!sample.menuOpen&&!sample.menuBlocked&&!sample.pauseOpen,"A closed overlay still blocks driving input");
        if(physicalSlot>=0)
        {
            Check(sample.selectedKey=="xinput:"+physicalSlot&&sample.mappedConnected,"Selected physical XInput slot did not reach the native input packet");
            Check(sample.mappedTrigger==sample.rawTrigger&&sample.mappedX==sample.rawX,"Current live physical analog sample differs from mapped native input");
        }
        else Check(Math.Abs(sample.mappedTrigger-throttle)<=1&&Math.Abs(sample.mappedX-x)<=1,"Controller input did not reach the native driving packet");
    }
    private void CensusInputs(string name)
    {
        var samples=new List<SlotSample>();
        for(int api=0;api<2;++api)for(uint slot=0;slot<4;++slot)
        {
            var sample=new SlotSample{dll=api==0?"xinput9_1_0":"xinput1_4",slot=(int)slot,result=uint.MaxValue};
            try
            {
                Idas3Native.PadState value;sample.result=api==0?Idas3Native.ReadGamepad(slot,out value):ReadXInput14(slot,out value);sample.connected=sample.result==0;
                if(sample.connected){sample.buttons=value.gamepad.buttons;sample.leftTrigger=value.gamepad.leftTrigger;sample.rightTrigger=value.gamepad.rightTrigger;
                    sample.leftX=value.gamepad.thumbLX;sample.leftY=value.gamepad.thumbLY;sample.rightX=value.gamepad.thumbRX;sample.rightY=value.gamepad.thumbRY;}
            }catch(Exception error){sample.error=error.GetType().Name+": "+error.Message;}
            samples.Add(sample);
        }
        censuses.Add(new Census{stage=name,seconds=Time.realtimeSinceStartupAsDouble-began,samples=samples.ToArray()});
    }
    private void CheckPhysicalEndpoints(string name)
    {
        if(!requirePhysical)return;
        var latest=censuses[censuses.Count-1];var missing=new List<string>();int expected=0;
        foreach(var before in physicalBaseline.samples)
        {
            if(!before.connected)continue;++expected;bool found=false;
            foreach(var after in latest.samples)if(after.dll==before.dll&&after.slot==before.slot&&after.connected){found=true;break;}
            if(!found)missing.Add(before.dll+" slot "+before.slot);
        }
        var result=new EndpointCheck{stage=name,passed=expected>0&&missing.Count==0,baselineConnected=expected,missing=missing.ToArray()};endpointChecks.Add(result);
        Check(expected>0,"Physical-controller verification requires an XInput controller connected before online initialization");
        Check(result.passed,"Physical controller endpoint disappeared after online initialization: "+string.Join(", ",result.missing)+". Button and axis changes are allowed; this checks connection only");
    }
    private IEnumerator Run()
    {
        yield return Frames(5);Check(host.Ready,"Game did not initialize");host.DiagnosticFocusOverride=true;
        if(steam||requirePhysical)yield return Until(HasForeground,45,"Bring the private game window to the Windows foreground for the physical/Steam input check");
        yield return Frames(15);
        menu=host.GetComponent<Idas3MultiplayerMenu>();Check(menu!=null,"Online menu missing");
        if(!steam)host.MultiplayerSession.ConfigureLocalTest(29834,"Private controller test");
        string key=null;
        if(physicalSlot>=0){key="xinput:"+physicalSlot;yield return Frames(6);}
        else
        {
            InputSystem.RegisterLayoutMatcher("Gamepad",new InputDeviceMatcher().WithInterface("Idas3Diagnostic").WithProduct("IDAS3 Online Input Test"));
            gamepad=(Gamepad)InputSystem.AddDevice(Description);Pad();yield return Frames(6);
            foreach(var choice in host.ControllerDevices.Choices)if(choice.connected&&choice.label.Contains("IDAS3 Online Input Test"))key=choice.key;
        }
        Check(key!=null&&host.ControllerDevices.Select(key),physicalSlot>=0?"Physical XInput slot was not selectable":"Synthetic controller was not selectable");yield return Frames(5);
        profile=host.ControllerDevices.ActiveProfileKey;Observe("selected");CensusInputs("before-online-initialization");
        physicalBaseline=censuses[censuses.Count-1];CheckPhysicalEndpoints("before-online-initialization");
        if(physicalSlot>=0)
        {
            Check(host.ControllerDevices.TryRead(out _),"The selected physical XInput slot is not connected to the provider");
            yield return Until(()=>!host.ControlBindings.SuppressInput,15,"Release the selected controller's controls before beginning the physical test");
        }
        pulse=116;yield return Until(()=>host.Status.racePhase==2&&(host.Status.flags&1025u)==0,40,"Quick start did not reach the running race");
        Pad(.6f,-.35f);yield return Frames(30);CheckDriving("baseline-controller",153,-11469);
        if(physicalSlot<0)Check(host.Status.speedMetresPerSecond>0,"Controller throttle did not move the native car");

        for(int cycle=0;cycle<3;++cycle)
        {
            stage="cycle-"+cycle+"-opening";
            if(steam||requirePhysical){yield return Until(HasForeground,45,"F1 check requires the actual Windows foreground game window");
                Check(Application.isFocused,"F1 path requires the real player window to be focused");}
            physicalKey=KeyCode.F1;yield return Frames(4);Observe(stage);Check(menu.IsOpen,"Mapped F1 did not open online menu");
            physicalKey=KeyCode.None;yield return Frames(6);
            if(cycle==0)
            {
                CensusInputs("after-first-online-initialization");
                if(steam||requirePhysical)for(int settle=1;settle<=5;++settle)
                {
                    double until=Time.realtimeSinceStartupAsDouble+1;
                    while(Time.realtimeSinceStartupAsDouble<until)yield return null;
                    CensusInputs("steam-settle-"+settle);
                }
                Check(host.MultiplayerSession.Available,steam?"Steam initialization failed: "+host.MultiplayerSession.ErrorText:"LAN transport unavailable");
                CheckPhysicalEndpoints("after-five-second-settle");
            }
            Observe("cycle-"+cycle+"-open");Check(host.DiagnosticSubmittedInput.rightTrigger==0,"Driving leaked through open online menu");
            Check(host.ControllerDevices.TryRead(out var held)&&(physicalSlot>=0||held.rightTrigger==153),"Opening online menu lost the raw selected controller sample");
            // Keep throttle and steering held through every close. Releasing
            // the menu control must restore them without reconnecting a pad.
            if(cycle==0)physicalKey=KeyCode.F1;
            else if(cycle==1)physicalKey=KeyCode.Escape;
            else menu.SetOpen(false);
            yield return Frames(4);Observe("cycle-"+cycle+"-closing");Check(!menu.IsOpen,"Online overlay did not close");
            physicalKey=KeyCode.None;yield return Frames(8);
            if(requirePhysical){CensusInputs("cycle-"+cycle+"-closed-physical");CheckPhysicalEndpoints("cycle-"+cycle+"-closed-physical");}
            CheckDriving("cycle-"+cycle+"-recovered-held",153,-11469);
            Pad();yield return Frames(4);Pad(.3f,.2f);yield return Frames(5);CheckDriving("cycle-"+cycle+"-fresh-controller",77,6553);
            Pad();physicalKey=KeyCode.W;yield return Frames(5);Observe("cycle-"+cycle+"-keyboard");
            Check((host.DiagnosticSubmittedInput.key2&(1u<<23))!=0,"Keyboard throttle did not recover alongside controller");
            physicalKey=KeyCode.None;Pad(.6f,-.35f);yield return Frames(5);
        }
        CensusInputs("after-online-close-recovery");CheckPhysicalEndpoints("after-online-close-recovery");Check(!host.MultiplayerSession.InLobby&&!host.MultiplayerSession.IsRacing,"Diagnostic unexpectedly entered an online room/race");
        if(physicalSlot<0&&!steam){yield return ControllerMenus();if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-online-menu-screens")>=0)yield return Idas3.Multiplayer.Idas3OnlineMenuScreens.Run(root);yield return WheelMenus();}
        Pad();yield return Frames(3);Observe("complete");Finish(true,null);
    }
    private void Finish(bool passed,string error)
    {
        if(finished)return;finished=true;bool stopped=false;
        try{if(host.Ready)Observe(passed?"final":"failure");}catch(Exception){}
        try{if(menuWheel!=null&&menuWheel.added)InputSystem.RemoveDevice(menuWheel);if(gamepad!=null&&gamepad.added)InputSystem.RemoveDevice(gamepad);host.DiagnosticFocusOverride=null;host.StopNative();stopped=!host.Ready;}
        catch(Exception failure){passed=false;error=(error??"")+failure;}
        File.WriteAllText(Path.Combine(root,"report.json"),JsonUtility.ToJson(new Report{passed=passed&&stopped,shutdownComplete=stopped,steam=steam,requirePhysical=requirePhysical,physicalSlot=physicalSlot,checks=checks,error=error,
            observations=observations.ToArray(),censuses=censuses.ToArray(),endpointChecks=endpointChecks.ToArray(),scope=(physicalSlot>=0?
                "Private actual Unity player using the selected physical XInput slot. Live raw-to-native pad connection, throttle and steering are compared before/after F1; no physical input is fabricated or assumed nonzero. ":
                "Private actual Unity player. Synthetic Gamepad events pass through the real provider/bindings/native input; held and fresh analog inputs are compared after closing. ")+
                "F1, Escape and programmatic close paths plus keyboard recovery. Synthetic LAN run uses the explicit diagnostic focus override, then tests Select/A/B navigation, on-screen code entry, empty local room hosting/leaving and settings categories. No peer or public matchmaking. Explicit Steam/physical modes require real foreground focus and do not host rooms. When requirePhysical is true, every baseline connected DLL/slot must remain connected after five-second settling and each close; values may change."},true));
        Debug.Log((passed?"PASS":"FAIL")+" online controller recovery "+error);
#if UNITY_EDITOR
        UnityEditor.EditorApplication.isPlaying=false;
#else
        Application.Quit(passed?0:1);
#endif
    }
}
