using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.HID;
using UnityEngine.InputSystem.Layouts;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.InputSystem.Utilities;

// Opt-in, private-save player check. Device events enter Unity's real input
// buffers and the production discovery/profile/capture/native-driving path.
public sealed class Idas3ControllerDevicesSmoke : MonoBehaviour
{
    private static string pendingRoot;
    private static Idas3ControllerDevicesSmoke active;
    private Idas3SceneGame host;
    private string root;
    private Gamepad alpha, beta;
    private Joystick generic, hidPedals, hidWheel;
    private KeyCode physicalKey;
    private int pulse, checks;
    private bool finished;
    private double began;
    private readonly List<string> captures = new List<string>();
    [Serializable] private class Report { public bool passed,shutdownComplete;public int checks;public string error,scope,applicationVersion;public string[] captures; }
    [StructLayout(LayoutKind.Explicit,Size=20)] private struct GenericState : IInputStateTypeInfo
    {
        [FieldOffset(0)] public uint buttons;
        [FieldOffset(4)] public Vector2 stick;
        [FieldOffset(12)] public float twist;
        [FieldOffset(16)] public float pedal;
        public FourCC format => new FourCC('I','D','J','T');
    }
    private static readonly InputDeviceDescription AlphaDescription = Description("IDAS3 Test Alpha", "alpha");
    // A report ID whose bit 4 is set makes Joystick's undescribed inherited
    // trigger appear pressed. Deliberate gaps also test report-relative offsets.
    [StructLayout(LayoutKind.Explicit,Size=12)] private struct HidState : IInputStateTypeInfo
    {
        [FieldOffset(0)] public byte report;
        [FieldOffset(1)] public ushort x;
        [FieldOffset(4)] public ushort y;
        [FieldOffset(7)] public ushort pedal;
        [FieldOffset(10)] public byte buttons;
        [FieldOffset(11)] public byte hat;
        public FourCC format => new FourCC('H','I','D');
    }
    private static InputDeviceDescription HidDescription(bool withButtons)
    {
        HID.HIDElementDescriptor Element(HID.UsagePage page,int usage,int offset,int size,int maximum) => new HID.HIDElementDescriptor {
            usagePage=page,usage=usage,reportType=HID.HIDReportType.Input,reportId=16,
            reportOffsetInBits=offset,reportSizeInBits=size,logicalMin=0,logicalMax=maximum,flags=HID.HIDElementFlags.Variable
        };
        var elements=new List<HID.HIDElementDescriptor>{
            Element(HID.UsagePage.GenericDesktop,(int)HID.GenericDesktop.X,8,16,65535),
            Element(HID.UsagePage.GenericDesktop,(int)HID.GenericDesktop.Y,32,16,65535),
            Element(HID.UsagePage.GenericDesktop,(int)HID.GenericDesktop.Z,56,16,65535)
        };
        if(withButtons){
            elements.Add(Element(HID.UsagePage.Button,1,80,1,1));
            elements.Add(Element(HID.UsagePage.Button,2,81,1,1));
            var hat=Element(HID.UsagePage.GenericDesktop,(int)HID.GenericDesktop.HatSwitch,88,4,7);
            hat.flags|=HID.HIDElementFlags.NullState;elements.Add(hat);
        }
        return new InputDeviceDescription{interfaceName="HID",manufacturer="Private input test",
            product=withButtons?"IDAS3 Descriptor Wheel":"IDAS3 Descriptor Pedals",serial=withButtons?"hid-wheel":"hid-pedals",
            capabilities=new HID.HIDDeviceDescriptor{vendorId=0x1234,productId=withButtons?0x5679:0x5678,
                usagePage=HID.UsagePage.GenericDesktop,usage=(int)HID.GenericDesktop.Joystick,inputReportSize=12,elements=elements.ToArray()}.ToJson()};
    }
    private void Hid(Joystick device,ushort pedal=65535,byte buttons=0,byte hat=8) =>
        InputSystem.QueueStateEvent(device,new HidState{report=16,x=32768,y=32768,pedal=pedal,buttons=buttons,hat=hat});
    private static readonly InputDeviceDescription BetaDescription = Description("IDAS3 Test Beta", "beta");
    private static readonly InputDeviceDescription GenericDescription = Description("IDAS3 Test Wheel", "wheel");
    private static InputDeviceDescription Description(string name,string serial) => new InputDeviceDescription {
        interfaceName="Idas3Diagnostic",manufacturer="Private input test",product=name,serial=serial
    };
    public static bool Configure(ref string saves)
    {
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-controller-devices-smoke");if(at<0)return false;
        if(at+1>=args.Length)throw new ArgumentException("Controller device check requires a fresh output directory.");
        pendingRoot=Path.GetFullPath(args[at+1]);if(Directory.Exists(pendingRoot)||File.Exists(pendingRoot))throw new IOException("Use a new diagnostic directory.");
        Directory.CreateDirectory(pendingRoot);saves=Path.Combine(pendingRoot,"userdata");Directory.CreateDirectory(saves);
        File.WriteAllText(Path.Combine(saves,"settings.txt"),"0 0 0 0 0 1 1 0\n");File.WriteAllText(Path.Combine(saves,"native_selection.txt"),"0 0\n");
        Screen.SetResolution(1200,720,FullScreenMode.Windowed);AudioListener.volume=0;return true;
    }
    public static void Attach(Idas3SceneGame game)
    {
        if(pendingRoot==null)return;active=game.gameObject.AddComponent<Idas3ControllerDevicesSmoke>();
        active.host=game;active.root=pendingRoot;active.began=Time.realtimeSinceStartupAsDouble;active.StartCoroutine(active.Guard(active.Run()));
    }
    internal static Idas3ControllerDevices CreateProvider()
    {
        if (pendingRoot == null || Array.IndexOf(Environment.GetCommandLineArgs(), "-idas3-controller-devices-smoke") < 0)
            throw new InvalidOperationException("Isolated controller discovery requires the configured controller-device diagnostic.");
        // Do not remove, disable or send state to the user's real devices.
        // This provider merely excludes them from the private diagnostic's
        // discovery and supplies disconnected XInput slots for deterministic
        // fallback/reconnection checks.
        return new Idas3ControllerDevices(() => Time.realtimeSinceStartupAsDouble, NoXInput,
            device => string.Equals(device.description.manufacturer, "Private input test", StringComparison.Ordinal));
    }
    private static uint NoXInput(uint slot, out Idas3Native.PadState state)
    { state = default; return 1167; } // ERROR_DEVICE_NOT_CONNECTED
    internal static void PreparePhysicalInput(ref Func<KeyCode,bool> key)
    { if(active!=null&&!active.finished)key=active.KeyHeld; }
    private bool KeyHeld(KeyCode key)=>physicalKey!=KeyCode.None&&key==physicalKey;
    internal static bool PrepareFrame(ref Idas3Native.FrameInput frame)
    {
        if(active==null)return true;if(active.finished)return false;
        if(active.pulse!=0){frame.SetKey(active.pulse);active.pulse=0;}return true;
    }
    private void Check(bool value,string reason){++checks;if(!value)throw new InvalidOperationException(reason+" flags="+host.Status.flags);}
    private IEnumerator Frames(int count){for(int i=0;i<count;++i)yield return null;}
    private IEnumerator Until(Func<bool> predicate,double seconds,string reason){double end=Time.realtimeSinceStartupAsDouble+seconds;while(!predicate()&&Time.realtimeSinceStartupAsDouble<end)yield return null;Check(predicate(),reason);}
    private IEnumerator Guard(IEnumerator routine)
    {
        var stack=new Stack<IEnumerator>();stack.Push(routine);
        while(stack.Count>0&&!finished){object value=null;Exception error=null;try{if(!stack.Peek().MoveNext()){stack.Pop();continue;}value=stack.Peek().Current;}catch(Exception e){error=e;}
            if(error!=null){Finish(false,error.ToString());yield break;}if(value is IEnumerator child)stack.Push(child);else yield return value;}
    }
    private void Update(){if(!finished&&(host.Failure!=null||Time.realtimeSinceStartupAsDouble-began>180))Finish(false,host.Failure??"Device diagnostic timeout.");}
    private string DeviceKey(string label)
    {
        foreach(var choice in host.ControllerDevices.Choices)if(choice.connected&&choice.label.Contains(label))return choice.key;
        throw new InvalidOperationException("Device was not discovered: "+label);
    }
    private void Pad(Gamepad device,GamepadState value)=>InputSystem.QueueStateEvent(device,value);
    private void Wheel(float pedal=1,float x=0,uint buttons=0)=>InputSystem.QueueStateEvent(generic,new GenericState{pedal=pedal,stick=new Vector2(x,0),buttons=buttons});
    private IEnumerator Select(string key)
    {
        Check(host.ControllerDevices.Select(key),"Could not select controller: "+host.ControllerDevices.LastError);
        yield return Frames(6);
        Check(host.ControllerDevices.SelectedKey==key,"Controller selector changed unexpectedly.");
    }
    private IEnumerator Run()
    {
        yield return Frames(5);Check(host.Ready,"Player did not initialize");host.DiagnosticFocusOverride=true;
        InputSystem.RegisterLayoutMatcher("Gamepad",new InputDeviceMatcher().WithInterface("Idas3Diagnostic").WithProduct("IDAS3 Test Alpha"));
        InputSystem.RegisterLayoutMatcher("Gamepad",new InputDeviceMatcher().WithInterface("Idas3Diagnostic").WithProduct("IDAS3 Test Beta"));
        InputSystem.RegisterLayout(@"{""name"":""Idas3DiagnosticWheel"",""extend"":""Joystick"",""format"":""IDJT"",""controls"":[
            {""name"":""trigger"",""layout"":""Button"",""offset"":0,""bit"":4},
            {""name"":""button2"",""displayName"":""Button 2"",""layout"":""Button"",""offset"":0,""bit"":5},
            {""name"":""extraButton"",""displayName"":""Extra Button"",""layout"":""Button"",""offset"":0,""bit"":12},
            {""name"":""stick"",""layout"":""Stick"",""offset"":4},
            {""name"":""twist"",""layout"":""Axis"",""offset"":12},
            {""name"":""pedal"",""displayName"":""Pedal"",""layout"":""Axis"",""offset"":16}
        ]}",name:"Idas3DiagnosticWheel",matches:new InputDeviceMatcher().WithInterface("Idas3Diagnostic").WithProduct("IDAS3 Test Wheel"));
        alpha=(Gamepad)InputSystem.AddDevice(AlphaDescription);beta=(Gamepad)InputSystem.AddDevice(BetaDescription);generic=(Joystick)InputSystem.AddDevice(GenericDescription);
        Pad(alpha,new GamepadState());Pad(beta,new GamepadState());Wheel();yield return Frames(8);
        string alphaKey=DeviceKey("Alpha"),betaKey=DeviceKey("Beta"),genericKey=DeviceKey("Wheel");
        Check(alphaKey!=betaKey&&betaKey!=genericKey,"Distinct controllers share selector keys");
        yield return Select("automatic");
        Pad(beta,new GamepadState().WithButton(GamepadButton.West));yield return Frames(6);
        Check(host.ControllerDevices.ActiveName.Contains("Beta"),"Automatic mode did not select the controller receiving input");
        Pad(beta,new GamepadState());yield return Frames(6);
        Check(host.ControllerDevices.ActiveName.Contains("Beta"),"A resting generic pedal stole automatic selection");
        Pad(alpha,new GamepadState().WithButton(GamepadButton.North));yield return Frames(6);
        Check(host.ControllerDevices.ActiveName.Contains("Alpha"),"Automatic mode did not switch to a second active controller");
        Pad(alpha,new GamepadState());yield return Frames(6);
        yield return Select(alphaKey);string alphaProfile=host.ControllerDevices.ActiveProfileKey;
        pulse=116;yield return Until(()=>((host.Status.flags&1)==0)&&host.Status.simulationTicks>240,40,"Native race did not start");
        var menu=host.PauseMenu;var bindings=host.ControlBindings;menu.SetOpen(true);menu.SelectTab(3);yield return Frames(5);
        Check(menu.IsOpen&&(host.Status.flags&2)!=0,"Offline controls menu did not pause");yield return Capture("devices");

        menu.SelectBindingColumn(3);
        Pad(alpha,new GamepadState().WithButton(GamepadButton.South));yield return Frames(3);
        menu.BeginBindingCapture(Idas3ControlBindings.ActionId.Accelerate,Idas3ControlBindings.Slot.Controller);yield return Frames(5);
        Check(bindings.IsCapturing,"The initiating held A press rebound itself");
        Pad(alpha,new GamepadState());yield return Frames(4);yield return Capture("capture-alpha");
        Pad(alpha,new GamepadState().WithButton(GamepadButton.East));yield return Frames(4);
        Check(!bindings.IsCapturing&&bindings.Draft.actions[0].pad==Idas3ControlBindings.PadInput.B,"Occupied controller B did not rebind acceleration");
        Check(bindings.Draft.actions[4].pad==Idas3ControlBindings.PadInput.RightTrigger,"Controller conflict did not swap old throttle into Shift up");
        Pad(alpha,new GamepadState());yield return Frames(4);
        Check(bindings.ApplyDraft(),"Alpha bindings did not save: "+bindings.LastError);yield return Frames(4);yield return Capture("alpha-saved");
        menu.SetOpen(false);yield return Frames(5);Pad(alpha,new GamepadState{leftStick=new Vector2(-.25f,0)}.WithButton(GamepadButton.East));yield return Frames(30);
        Check(host.DiagnosticSubmittedInput.rightTrigger==255&&Math.Abs(host.DiagnosticSubmittedInput.thumbLX+8192)<=1,"Alpha rebound button/analog steering did not reach native input");
        Check(host.Status.speedMetresPerSecond>1,"Rebound controller did not drive native car");
        Pad(alpha,new GamepadState());yield return Frames(4);Pad(alpha,new GamepadState().WithButton(GamepadButton.Start));yield return Frames(12);
        Check(menu.IsOpen&&(host.Status.flags&2)!=0,"Held controller Start did not retain pause");Pad(alpha,new GamepadState());yield return Frames(4);menu.SelectTab(3);

        yield return Select(betaKey);string betaProfile=host.ControllerDevices.ActiveProfileKey;Check(betaProfile!=alphaProfile,"Distinct controller profiles merged");
        Check(bindings.Current.actions[0].pad==Idas3ControlBindings.PadInput.RightTrigger,"Alpha mapping overwrote Beta's defaults");
        menu.BeginBindingCapture(Idas3ControlBindings.ActionId.Accelerate,Idas3ControlBindings.Slot.Controller);yield return Frames(4);
        Pad(alpha,new GamepadState().WithButton(GamepadButton.North));yield return Frames(4);
        Check(bindings.IsCapturing,"Input from an unselected controller was captured");
        Pad(beta,new GamepadState().WithButton(GamepadButton.West));yield return Frames(4);
        Check(!bindings.IsCapturing&&bindings.Draft.actions[0].pad==Idas3ControlBindings.PadInput.X,"Beta button did not bind");
        Pad(alpha,new GamepadState());Pad(beta,new GamepadState());yield return Frames(4);
        yield return Select(alphaKey);Check(bindings.Draft.actions[0].pad==Idas3ControlBindings.PadInput.B,"Switching lost Alpha bindings");
        yield return Select(betaKey);Check(bindings.Draft.actions[0].pad==Idas3ControlBindings.PadInput.X,"Switching lost Beta's unsaved draft");
        Check(bindings.ApplyDraft(),"Profile drafts did not save");yield return Frames(4);yield return Capture("beta-saved");
        var reload=new Idas3ControlBindings();reload.Initialize(Path.Combine(root,"userdata"));reload.SelectControllerProfile(alphaProfile,"Alpha");
        Check(reload.Current.actions[0].pad==Idas3ControlBindings.PadInput.B,"Alpha profile did not persist");reload.SelectControllerProfile(betaProfile,"Beta");
        Check(reload.Current.actions[0].pad==Idas3ControlBindings.PadInput.X,"Beta profile did not persist");

        yield return Select("keyboard");menu.SetOpen(false);yield return Frames(5);
        Pad(alpha,new GamepadState().WithButton(GamepadButton.East));Pad(beta,new GamepadState().WithButton(GamepadButton.West));physicalKey=KeyCode.W;yield return Frames(5);
        Check(host.DiagnosticSubmittedInput.padConnected==0&&host.DiagnosticSubmittedInput.rightTrigger==0,"Keyboard-only mode accepted controller input");
        Check((host.DiagnosticSubmittedInput.key2&(1u<<23))!=0,"Keyboard-only mode lost keyboard throttle");physicalKey=KeyCode.None;
        Pad(alpha,new GamepadState());Pad(beta,new GamepadState());menu.SetOpen(true);menu.SelectTab(3);yield return Frames(5);

        yield return Select(genericKey);string genericProfile=host.ControllerDevices.ActiveProfileKey;
        Check(host.ControllerDevices.ActiveIsGeneric,"Joystick was not exposed as a generic controller");yield return Capture("generic-device");
        menu.BeginBindingCapture(Idas3ControlBindings.ActionId.Accelerate,Idas3ControlBindings.Slot.Controller);yield return Frames(8);
        Check(bindings.IsCapturing,"Resting pedal rebound itself or blocked capture");Wheel(-1);yield return Frames(4);
        Check(!bindings.IsCapturing&&bindings.Draft.actions[0].controlPath.Contains("pedal"),"Rest-at-one pedal was not captured");Wheel();yield return Frames(5);
        menu.BeginBindingCapture(Idas3ControlBindings.ActionId.SteerLeft,Idas3ControlBindings.Slot.Controller);yield return Frames(4);Wheel(1,-1);yield return Frames(4);
        Check(!bindings.IsCapturing,"Generic left steering did not capture");Wheel();yield return Frames(4);
        menu.BeginBindingCapture(Idas3ControlBindings.ActionId.SteerRight,Idas3ControlBindings.Slot.Controller);yield return Frames(4);Wheel(1,1);yield return Frames(4);
        Check(!bindings.IsCapturing,"Generic right steering did not capture");Wheel();yield return Frames(4);
        menu.BeginBindingCapture(Idas3ControlBindings.ActionId.Brake,Idas3ControlBindings.Slot.Controller);yield return Frames(4);Wheel(1,0,1u<<12);yield return Frames(4);
        Check(!bindings.IsCapturing&&bindings.Draft.actions[1].controlPath.Contains("extraButton"),"Extra joystick button did not capture");Wheel();yield return Frames(4);
        menu.BeginBindingCapture(Idas3ControlBindings.ActionId.Pause,Idas3ControlBindings.Slot.Controller);yield return Frames(4);Wheel(1,0,1u<<5);yield return Frames(4);
        Check(!bindings.IsCapturing,"Generic pause button did not capture");Wheel();yield return Frames(4);
        Check(bindings.ApplyDraft(),"Generic bindings did not save: "+bindings.LastError);yield return Frames(5);yield return Capture("generic-bound");
        menu.SetOpen(false);yield return Frames(6);
        Check(host.DiagnosticSubmittedInput.rightTrigger==0&&host.DiagnosticSubmittedInput.thumbLX==0,"Resting pedal/steering produced input");
        Wheel(0,-.45f);yield return Frames(5);
        Check(Math.Abs((int)host.DiagnosticSubmittedInput.rightTrigger-128)<=1&&Math.Abs(host.DiagnosticSubmittedInput.thumbLX+14745)<=2,"Generic analog pedal/steering lost proportional travel");
        Wheel(1,0,1u<<12);yield return Frames(4);Check(host.DiagnosticSubmittedInput.leftTrigger==255,"Generic extra button did not brake");
        Wheel();yield return Frames(4);Wheel(1,0,1u<<5);yield return Frames(12);
        Check(menu.IsOpen&&(host.Status.flags&2)!=0,"Rebound generic pause button did not keep pause open");Wheel();yield return Frames(4);menu.SetOpen(false);yield return Frames(5);

        InputSystem.RemoveDevice(generic);generic=null;Pad(alpha,new GamepadState().WithButton(GamepadButton.East));yield return Frames(12);
        Check(host.ControllerDevices.SelectedKey==genericKey&&host.ControllerDevices.UsingFallback&&host.ControllerDevices.ActiveProfileKey==alphaProfile,"Disconnected selected device did not temporarily fall back to the active controller");
        Pad(alpha,new GamepadState());yield return Frames(6);
        int changes=0;System.Action changed=()=>++changes;host.ControllerDevices.ActiveDeviceChanged+=changed;
        yield return Frames(12);host.ControllerDevices.ActiveDeviceChanged-=changed;
        Check(changes==0&&!bindings.SuppressInput,"Fallback repeatedly reset the input release latch");
        Pad(alpha,new GamepadState().WithButton(GamepadButton.East));yield return Frames(6);
        Check(host.DiagnosticSubmittedInput.rightTrigger==255,"Fallback did not use Alpha's saved throttle binding");
        Pad(alpha,new GamepadState());yield return Frames(6);
        var missingRestart=CreateProvider();try{
            missingRestart.Initialize(Path.Combine(root,"userdata"));missingRestart.Tick(false);
            Check(missingRestart.SelectedKey==genericKey&&missingRestart.UsingFallback,"Restart with a missing saved device did not recover a connected controller");
        }finally{missingRestart.Dispose();}
        menu.SetOpen(true);menu.SelectTab(3);yield return Frames(5);yield return Capture("temporary-device");
        InputSystem.RemoveDevice(alpha);alpha=null;yield return Frames(8);
        Check(menu.IsOpen&&host.ControllerDevices.UsingFallback&&host.ControllerDevices.ActiveProfileKey==betaProfile,"Losing the fallback while paused prevented recovery to another controller; open="+menu.IsOpen+" fallback="+host.ControllerDevices.UsingFallback+" active="+host.ControllerDevices.ActiveName+" profile="+host.ControllerDevices.ActiveProfileKey+" expected="+betaProfile);
        InputSystem.RemoveDevice(beta);beta=null;yield return Frames(8);
        Check(host.ControllerDevices.Controls.Count==0&&!host.ControllerDevices.UsingFallback&&!bindings.SuppressInput,"Removing all controllers left input blocked");
        menu.SetOpen(false);yield return Frames(6);physicalKey=KeyCode.W;yield return Frames(6);
        Check((host.DiagnosticSubmittedInput.key2&(1u<<23))!=0,"Missing selected controller blocked keyboard throttle");
        physicalKey=KeyCode.None;yield return Frames(6);
        alpha=(Gamepad)InputSystem.AddDevice(AlphaDescription);beta=(Gamepad)InputSystem.AddDevice(BetaDescription);
        Pad(alpha,new GamepadState());Pad(beta,new GamepadState());yield return Frames(12);
        Check(host.ControllerDevices.SelectedKey==genericKey&&host.ControllerDevices.UsingFallback,"Replacement controller failed to recover input without changing the saved preference");
        generic=(Joystick)InputSystem.AddDevice(GenericDescription);Wheel();yield return Frames(12);
        Check(host.ControllerDevices.ActiveProfileKey==genericProfile&&host.ControllerDevices.SelectedKey==genericKey&&!host.ControllerDevices.UsingFallback,"Reconnect lost selected controller/profile");
        Pad(alpha,new GamepadState());Wheel(-1);yield return Frames(6);Check(host.DiagnosticSubmittedInput.rightTrigger==255,"Reconnected generic controller lost its mapping");Wheel();yield return Frames(4);
        var reopened=CreateProvider();try{reopened.Initialize(Path.Combine(root,"userdata"));reopened.Tick(false);Check(reopened.SelectedKey==genericKey,"Selected controller did not persist");}finally{reopened.Dispose();}
        menu.SetOpen(true);menu.SelectTab(3);yield return Frames(5);yield return Capture("reconnected");
        yield return Select("automatic");
        Check(host.ControllerDevices.ActiveProfileKey==genericProfile,"Automatic selection lost the current generic controller");
        InputSystem.RemoveDevice(generic);generic=null;yield return Frames(8);
        Check(host.ControllerDevices.Controls.Count==0,"Locked automatic mode took another controller after disconnect");
        generic=(Joystick)InputSystem.AddDevice(GenericDescription);Wheel();yield return Frames(12);
        Check(menu.IsOpen&&host.ControllerDevices.ActiveProfileKey==genericProfile,"Automatic controller did not reconnect while paused");
        Wheel(1,0,1u<<5);yield return Frames(5);Wheel();yield return Frames(4);
        Check(menu.IsOpen&&menu.OptionsVisible&&menu.DiagnosticSelection==0,"Reconnected controller could not return to the Controls category");
        Wheel(1,0,1u<<5);yield return Frames(5);Wheel();yield return Frames(4);
        Check(menu.IsOpen&&!menu.OptionsVisible,"Reconnected controller pause/back could not leave Controls");
        Wheel(1,0,1u<<5);yield return Frames(5);Wheel();yield return Frames(4);
        Check(!menu.IsOpen,"Reconnected controller could not resume the race");
        yield return VerifyHidDescriptors();
        Finish(true,null);
    }
    private IEnumerator VerifyHidDescriptors()
    {
        var menu=host.PauseMenu;var bindings=host.ControlBindings;
        menu.SetOpen(true);menu.SelectTab(3);Wheel();yield return Frames(4);
        // Reproduce the unarmed capture state with a real, latched button.
        Wheel(1,0,1u<<4);yield return Frames(4);
        menu.BeginBindingCapture(Idas3ControlBindings.ActionId.Accelerate,Idas3ControlBindings.Slot.Controller);yield return Frames(4);
        Check(bindings.IsCapturing,"Held selector should leave capture waiting for release");
        physicalKey=KeyCode.Escape;yield return Frames(3);
        Check(!bindings.IsCapturing,"Escape could not cancel before capture armed");physicalKey=KeyCode.None;yield return Frames(5);
        Check(!bindings.SuppressInput&&menu.IsOpen&&menu.OptionsVisible,"Cancelling left a grey menu or activated the held confirm button");
        yield return Capture("cancel-held-wheel");Wheel();yield return Frames(4);
        // Mapped Pause/Online must retain their held edges too, not just raw A.
        Idas3ControllerControl onlineControl=null;
        foreach(var control in host.ControllerDevices.Controls)if(control.path=="extraButton")onlineControl=control;
        Check(bindings.TrySetDraftControl(Idas3ControlBindings.ActionId.Online,onlineControl,1,0)&&bindings.ApplyDraft(),"Could not prepare latched online shortcut");
        menu.BeginBindingCapture(Idas3ControlBindings.ActionId.Accelerate,Idas3ControlBindings.Slot.Controller);
        Wheel(1,0,(1u<<5)|(1u<<12));yield return Frames(4);
        Check(bindings.IsCapturing,"Shortcut cancellation fixture armed through held buttons");
        bindings.CancelCapture();yield return Frames(5);
        Check(!bindings.SuppressInput&&menu.IsOpen&&menu.OptionsVisible,"Latched pause/online shortcut fired after mouse cancellation");
        bindings.BeginCapture(Idas3ControlBindings.ActionId.Accelerate,Idas3ControlBindings.Slot.Controller,Time.realtimeSinceStartupAsDouble-16);
        yield return Frames(5);
        Check(!bindings.IsCapturing&&!bindings.SuppressInput&&menu.IsOpen&&menu.OptionsVisible,"Latched pause/online shortcut fired after capture timeout");
        yield return Capture("timeout-held-shortcuts");Wheel();yield return Frames(4);

        hidPedals=(Joystick)InputSystem.AddDevice(HidDescription(false));Hid(hidPedals);yield return Frames(8);
        Check(hidPedals.layout.StartsWith("HID::",StringComparison.Ordinal),"HID test must use Unity's generated descriptor layout");
        Check(hidPedals.trigger.isPressed,"Report does not exercise inherited phantom trigger");
        yield return Select(DeviceKey("Descriptor Pedals"));
        Check(host.ControllerDevices.Controls.Count==3,"Descriptor-only pedals exposed phantom controls");
        foreach(var control in host.ControllerDevices.Controls)Check(!control.button,"Axis-only descriptor created a button");
        Check(host.ControllerDevices.TryRead(out var raw)&&raw.buttons==0,"Phantom trigger leaked into raw menu buttons");
        Check(!bindings.SuppressInput,"Phantom trigger blocked the device release guard");
        menu.BeginBindingCapture(Idas3ControlBindings.ActionId.Accelerate,Idas3ControlBindings.Slot.Controller);yield return Frames(5);
        Check(bindings.IsCapturing&&bindings.CapturePrompt.StartsWith("Press a button"),"Descriptor pedal capture did not arm");
        yield return Capture("hid-pedal-capture");Hid(hidPedals,0);yield return Frames(5);
        Check(!bindings.IsCapturing&&bindings.Draft.actions[0].controlPath=="z"&&bindings.Draft.actions[0].controlDirection==-1,"Descriptor reversed pedal did not bind its analog axis");
        Hid(hidPedals);yield return Frames(4);Check(bindings.ApplyDraft(),"Descriptor pedal binding did not save");
        menu.SetOpen(false);yield return Frames(4);Hid(hidPedals,32768);yield return Frames(5);
        Check(Math.Abs((int)host.DiagnosticSubmittedInput.rightTrigger-128)<=1,"Descriptor pedal lost proportional input");
        Hid(hidPedals);yield return Frames(4);Check(host.DiagnosticSubmittedInput.rightTrigger==0,"Descriptor pedal rest was not neutral");
        menu.SetOpen(true);menu.SelectTab(3);yield return Frames(4);
        // The opposite pedal polarity must be captured from its measured rest.
        Hid(hidPedals,0);yield return Frames(4);
        menu.BeginBindingCapture(Idas3ControlBindings.ActionId.Accelerate,Idas3ControlBindings.Slot.Controller);yield return Frames(4);
        Hid(hidPedals);yield return Frames(4);
        Check(!bindings.IsCapturing&&bindings.Draft.actions[0].controlDirection==1&&bindings.Draft.actions[0].controlRest<-.99f,"Rest-at-minus-one descriptor pedal did not bind");
        Hid(hidPedals,0);yield return Frames(4);Check(bindings.ApplyDraft(),"Opposite pedal polarity did not save");

        hidWheel=(Joystick)InputSystem.AddDevice(HidDescription(true));Hid(hidWheel);yield return Frames(8);
        yield return Select(DeviceKey("Descriptor Wheel"));
        int buttons=0;foreach(var control in host.ControllerDevices.Controls)if(control.button)++buttons;
        Check(buttons==6,"Descriptor filtering removed a real button or hat direction");
        menu.BeginBindingCapture(Idas3ControlBindings.ActionId.Brake,Idas3ControlBindings.Slot.Controller);yield return Frames(4);
        Hid(hidWheel,65535,1);yield return Frames(4);
        Check(!bindings.IsCapturing&&bindings.Draft.actions[1].controlButton,"Real HID button could not bind");
        Check(host.ControllerDevices.TryRead(out raw)&&(raw.buttons&0x1000)!=0,"Real HID button lost menu confirm fallback");
        Hid(hidWheel);yield return Frames(4);Check(bindings.ApplyDraft(),"Real HID button mapping did not save");
        Hid(hidWheel,65535,0,0);yield return Frames(4);
        Check(host.ControllerDevices.TryRead(out raw)&&(raw.buttons&1)!=0&&(raw.buttons&0x3000)==0,"Real HID hat lost direction or generated confirm/back");
        Hid(hidWheel);yield return Frames(4);yield return Capture("hid-wheel-bound");
    }
    private IEnumerator Capture(string name)
    {
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-input-check-no-captures")>=0)yield break;
        // Capture the real camera/UI passes offscreen, as in ControlsSmoke;
        // a hidden player's desktop backbuffer need not have usable pixels.
        var camera=host.GetComponent<Camera>();var scene=host.GetComponent<Idas3SceneRenderer>();var ui=host.GetComponent<Idas3UnityUi>();var menu=host.PauseMenu;
        var previous=camera.targetTexture;var old=RenderTexture.active;Texture2D picture=null;
        var target=new RenderTexture(Screen.width,Screen.height,24,RenderTextureFormat.ARGB32){antiAliasing=1};
        try{
            Check(target.Create(),"Controls capture target creation failed");
            camera.targetTexture=target;scene.ApplyFrame();ui.ApplyFrame();var cameras=new List<Camera>();
            foreach(var item in Resources.FindObjectsOfTypeAll<Camera>())if(item!=null&&item.enabled&&item.gameObject.activeInHierarchy&&item.targetTexture==target)cameras.Add(item);
            cameras.Sort((a,b)=>a.depth.CompareTo(b.depth));foreach(var item in cameras)item.Render();
            camera.targetTexture=previous;scene.ApplyFrame();ui.ApplyFrame();menu.RequestDiagnosticCapture(target);
            yield return Until(()=>menu.DiagnosticCaptureReady,5,"Controls menu did not repaint");yield return new WaitForEndOfFrame();
            RenderTexture.active=target;picture=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);
            picture.ReadPixels(new Rect(0,0,target.width,target.height),0,0);picture.Apply();
            string file="devices-"+name+".png";File.WriteAllBytes(Path.Combine(root,file),picture.EncodeToPNG());captures.Add(file);
        }finally{
            menu.CancelDiagnosticCapture();camera.targetTexture=previous;scene.ApplyFrame();ui.ApplyFrame();RenderTexture.active=old;
            if(picture!=null)Destroy(picture);target.Release();Destroy(target);
        }
    }
    private void Finish(bool passed,string error)
    {
        if(finished)return;finished=true;bool stopped=false;
        try{foreach(var device in new InputDevice[]{alpha,beta,generic,hidPedals,hidWheel})if(device!=null&&device.added)InputSystem.RemoveDevice(device);host.DiagnosticFocusOverride=null;host.StopNative();stopped=!host.Ready;}
        catch(Exception e){passed=false;error=(error??"")+e;}
        File.WriteAllText(Path.Combine(root,"report.json"),JsonUtility.ToJson(new Report{passed=passed&&stopped,shutdownComplete=stopped,checks=checks,error=error,captures=captures.ToArray(),applicationVersion=Application.version,
            scope="Private actual Unity player. Simulated gamepads, a custom wheel, and autogenerated HID wheel/pedal descriptors use Unity state events and production discovery, selection, per-device profiles, binding capture, pause and native driving. Covers missing saved-device fallback at startup and during play/menus, keyboard recovery with all controllers disconnected, stable fallback input, preference preservation, occupied-button swap, extra joystick buttons, reversed resting pedal, steering directions, reconnect, keyboard-only mode, inherited phantom HID trigger rejection, descriptor buttons/hats, both pedal polarities, and Escape/timeout/mouse cancellation with held shortcuts. Diagnostic discovery excludes physical devices. Does not exercise physical USB/Bluetooth hardware. "+(captures.Count>0?"Includes rendered UI captures.":"Input-only run; UI rendering not checked.")},true));
        Debug.Log((passed?"PASS":"FAIL")+" controller devices "+error);
#if UNITY_EDITOR
        UnityEditor.EditorApplication.isPlaying=false;
#else
        Application.Quit(passed?0:1);
#endif
    }
}
