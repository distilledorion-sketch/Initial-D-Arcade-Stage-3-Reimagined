using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.Layouts;
using UnityEngine.InputSystem.LowLevel;

// Opt-in player check with private saves. Injects physical inputs before the
// production binding mapper, then checks the packet actually sent to native.
public sealed class Idas3ControlsSmoke : MonoBehaviour
{
    private static string pendingRoot;
    private static Idas3ControlsSmoke active;
    private Idas3SceneGame host;
    private string root;
    private KeyCode physicalKey;
    private Idas3ControlBindings.PadState physicalPad;
    private int pulse,checks;
    private bool finished;
    private bool useHardwareProvider;
    private Gamepad syntheticPad;
    private static readonly InputDeviceDescription TestPadDescription=new InputDeviceDescription{
        interfaceName="Idas3Diagnostic",manufacturer="Private input test",product="IDAS3 Controls Test",serial="controls-smoke"};
    private double started;
    private readonly List<string> captures=new List<string>();
    [Serializable] private class Report {public bool passed,shutdownComplete;public int checks;public string error,scope;public string[] captures;}
    public static bool Configure(ref string saves)
    {
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-controls-smoke");if(at<0)return false;
        if(at+1>=args.Length)throw new ArgumentException("Controls diagnostic needs a new output directory.");
        pendingRoot=Path.GetFullPath(args[at+1]);if(Directory.Exists(pendingRoot)||File.Exists(pendingRoot))throw new IOException("Use a fresh controls diagnostic directory.");
        Directory.CreateDirectory(pendingRoot);saves=Path.Combine(pendingRoot,"userdata");Directory.CreateDirectory(saves);
        File.WriteAllText(Path.Combine(saves,"settings.txt"),"0 0 0 0 0 1 1 0\n");File.WriteAllText(Path.Combine(saves,"native_selection.txt"),"0 0\n");
        Screen.SetResolution(1200,720,FullScreenMode.Windowed);AudioListener.volume=0;return true;
    }
    public static void Attach(Idas3SceneGame game)
    {
        if(pendingRoot==null)return;active=game.gameObject.AddComponent<Idas3ControlsSmoke>();active.host=game;active.root=pendingRoot;
        active.started=Time.realtimeSinceStartupAsDouble;active.StartCoroutine(active.Guard(active.Run()));
    }
    internal static bool PreparePhysicalInput(ref Func<KeyCode,bool> key,ref Idas3ControlBindings.PadState pad)
    {
        if(active==null||active.finished)return false;key=active.KeyHeld;if(active.useHardwareProvider)return false;
        pad=active.physicalPad;return true;
    }
    private bool KeyHeld(KeyCode key)=>physicalKey!=KeyCode.None&&key==physicalKey;
    internal static bool PrepareFrame(ref Idas3Native.FrameInput frame)
    {
        if(active==null)return true;if(active.finished)return false;
        if(active.pulse!=0){frame.SetKey(active.pulse);active.pulse=0;}return true;
    }
    private void Check(bool ok,string reason){++checks;if(!ok)throw new InvalidOperationException(reason);}
    private IEnumerator Frames(int count){for(int i=0;i<count;++i)yield return null;}
    private IEnumerator Until(Func<bool> predicate,double seconds,string reason){double end=Time.realtimeSinceStartupAsDouble+seconds;while(!predicate()&&Time.realtimeSinceStartupAsDouble<end)yield return null;Check(predicate(),reason);}
    private IEnumerator Guard(IEnumerator routine)
    {
        var stack=new Stack<IEnumerator>();stack.Push(routine);
        while(stack.Count>0&&!finished){object value=null;Exception failure=null;try{if(!stack.Peek().MoveNext()){stack.Pop();continue;}value=stack.Peek().Current;}catch(Exception e){failure=e;}
            if(failure!=null){Finish(false,failure.ToString());yield break;}if(value is IEnumerator child)stack.Push(child);else yield return value;}
    }
    private void Update(){if(!finished&&(host.Failure!=null||Time.realtimeSinceStartupAsDouble-started>180))Finish(false,host.Failure??"Controls diagnostic timeout.");}
    private IEnumerator Run()
    {
        yield return Frames(5);Check(host.Ready,"Player initialized");host.DiagnosticFocusOverride=true;
        InputSystem.RegisterLayoutMatcher("Gamepad",new InputDeviceMatcher().WithInterface("Idas3Diagnostic").WithProduct("IDAS3 Controls Test"));
        syntheticPad=(Gamepad)InputSystem.AddDevice(TestPadDescription);InputSystem.QueueStateEvent(syntheticPad,new GamepadState());yield return Frames(6);
        string deviceKey=null;foreach(var choice in host.ControllerDevices.Choices)if(choice.connected&&choice.label.Contains("IDAS3 Controls Test"))deviceKey=choice.key;
        Check(deviceKey!=null&&host.ControllerDevices.Select(deviceKey),"Diagnostic controller was not selectable");yield return Frames(5);
        string controllerProfile=host.ControllerDevices.ActiveProfileKey;
        var bindings=host.ControlBindings;var menu=host.PauseMenu;
        // Rebinding the driving stick must leave native menu navigation raw.
        // Both left-stick values stay below navigation thresholds so this
        // checks the actual submitted packet without changing menu selection.
        Check((host.Status.flags&1)!=0,"Menu axis check must run before the race");bindings.BeginEdit();
        Check(bindings.TrySetDraftPad(Idas3ControlBindings.ActionId.SteerLeft,Idas3ControlBindings.PadInput.RightStickLeft)&&
            bindings.TrySetDraftPad(Idas3ControlBindings.ActionId.SteerRight,Idas3ControlBindings.PadInput.RightStickRight)&&bindings.ApplyDraft(),"Could not prepare menu binding isolation");
        physicalPad=new Idas3ControlBindings.PadState{connected=true};yield return Frames(4);
        physicalPad=new Idas3ControlBindings.PadState{connected=true,thumbLX=-12345,thumbLY=6789,thumbRX=22222,thumbRY=-2468};yield return Frames(3);
        var menuPacket=host.DiagnosticSubmittedInput;
        Check((host.Status.flags&1)!=0&&menuPacket.thumbLX==-12345&&menuPacket.thumbLY==6789&&menuPacket.thumbRX==22222&&menuPacket.thumbRY==-2468,
            "Driving stick pairing or response altered the native menu packet");
        physicalPad=new Idas3ControlBindings.PadState{connected=true};bindings.BeginEdit();bindings.ResetDraft();
        Check(bindings.ApplyDraft(),"Could not restore diagnostic controls after menu isolation");yield return Frames(4);pulse=116;
        yield return Until(()=>((host.Status.flags&1)==0)&&host.Status.simulationTicks>240,40,"Race did not start");
        VerifyXInputDiscovery();VerifySteeringAxisPairs();VerifyCaptureRecovery();
        yield return Frames(5);
        physicalPad=new Idas3ControlBindings.PadState{connected=true,buttons=0x10};yield return Frames(12);
        Check(menu.IsOpen&&(host.Status.flags&2)!=0,"Holding controller Start must leave offline pause open");
        ulong pausedTicks=host.Status.simulationTicks;yield return Frames(8);
        Check(menu.IsOpen&&host.Status.simulationTicks==pausedTicks,"Held Start toggled pause twice or advanced offline simulation");
        physicalPad=new Idas3ControlBindings.PadState{connected=true};yield return Frames(3);
        physicalPad.buttons=0x10;yield return Frames(12);
        Check(!menu.IsOpen&&(host.Status.flags&2)==0,"Second Start press must resume and remain resumed while held");
        physicalPad=new Idas3ControlBindings.PadState{connected=true};yield return Frames(4);
        physicalKey=KeyCode.Escape;yield return Frames(3);physicalKey=KeyCode.None;yield return Frames(3);
        Check(menu.IsOpen&&(host.Status.flags&2)!=0,"Physical Escape did not open native offline pause");
        menu.SelectTab(3);yield return Frames(3);yield return Capture("defaults");
        menu.BeginBindingCapture(Idas3ControlBindings.ActionId.Accelerate,Idas3ControlBindings.Slot.Primary);
        yield return Frames(3);Check(bindings.IsCapturing,"Controls cell did not enter capture");yield return Capture("capture");
        physicalKey=KeyCode.A;yield return Frames(3);Check(bindings.IsCapturing&&!string.IsNullOrEmpty(bindings.CaptureError),"Conflicting steering key was accepted silently");
        physicalKey=KeyCode.None;yield return Frames(3);physicalKey=KeyCode.L;yield return Frames(3);
        Check(!bindings.IsCapturing,"New keyboard key was not captured");Check(host.DiagnosticSubmittedInput.key2==0,"Capture key leaked into native driving packet");
        host.DiagnosticFocusOverride=false;yield return Frames(3);Check(bindings.SuppressInput,"Focus loss cleared the capture release guard");
        host.DiagnosticFocusOverride=true;yield return Frames(3);Check(bindings.SuppressInput&&host.DiagnosticSubmittedInput.key2==0,"Held captured key leaked after refocus");
        physicalKey=KeyCode.None;yield return Frames(3);
        Check(bindings.ClearDraft(Idas3ControlBindings.ActionId.Accelerate,Idas3ControlBindings.Slot.Secondary),"Could not clear alternate acceleration key");
        Check(bindings.ApplyDraft(),"Could not save keyboard binding: "+bindings.LastError);
        var reload=new Idas3ControlBindings();reload.Initialize(Path.Combine(root,"userdata"));
        Check(reload.Current.actions[0].key1==KeyCode.L&&reload.Current.actions[0].key2==KeyCode.None,"Bindings did not survive reload");
        yield return Frames(3);yield return Capture("saved-keyboard");
        menu.SetOpen(false);yield return Frames(5);physicalKey=KeyCode.W;yield return Frames(6);
        Check((host.DiagnosticSubmittedInput.key2&(1u<<23))==0,"Old W mapping still accelerates");
        physicalKey=KeyCode.L;yield return Frames(45);
        Check((host.DiagnosticSubmittedInput.key2&(1u<<23))!=0,"L did not produce native throttle");
        Check(host.Status.speedMetresPerSecond>1,"New binding did not drive the native car");physicalKey=KeyCode.None;
        physicalPad=new Idas3ControlBindings.PadState{connected=true,rightTrigger=117,thumbLX=-12345};yield return Frames(4);
        Check(host.DiagnosticSubmittedInput.rightTrigger==117&&host.DiagnosticSubmittedInput.thumbLX==-12345,"Original analog precision changed");
        physicalPad=default;physicalKey=KeyCode.Escape;yield return Frames(3);physicalKey=KeyCode.None;yield return Frames(3);menu.SelectTab(3);
        useHardwareProvider=true;InputSystem.QueueStateEvent(syntheticPad,new GamepadState());yield return Frames(4);
        menu.BeginBindingCapture(Idas3ControlBindings.ActionId.Accelerate,Idas3ControlBindings.Slot.Controller);yield return Frames(3);
        Check(bindings.IsCapturing,"Controller cell did not enter real device capture");
        InputSystem.QueueStateEvent(syntheticPad,new GamepadState().WithButton(GamepadButton.South));yield return Frames(3);
        Check(!bindings.IsCapturing&&bindings.Draft.actions[0].pad==Idas3ControlBindings.PadInput.A,"Controller A was not captured");InputSystem.QueueStateEvent(syntheticPad,new GamepadState());yield return Frames(3);
        Check(bindings.ApplyDraft(),"Controller assignment did not save");yield return Frames(3);yield return Capture("saved-controller");
        reload=new Idas3ControlBindings();reload.Initialize(Path.Combine(root,"userdata"));reload.SelectControllerProfile(controllerProfile,"Controls test");
        Check(reload.Current.actions[0].pad==Idas3ControlBindings.PadInput.A&&reload.Current.actions[0].key1==KeyCode.L,"Controller profile and shared keyboard did not reload");
        menu.SetOpen(false);yield return Frames(5);InputSystem.QueueStateEvent(syntheticPad,new GamepadState{rightTrigger=1});yield return Frames(4);
        Check(host.DiagnosticSubmittedInput.rightTrigger==0,"Old RT still accelerates after rebinding");
        InputSystem.QueueStateEvent(syntheticPad,new GamepadState().WithButton(GamepadButton.South));yield return Frames(4);
        Check(host.DiagnosticSubmittedInput.rightTrigger==255,"Assigned controller A does not accelerate");
        InputSystem.QueueStateEvent(syntheticPad,new GamepadState());yield return Frames(4);host.DiagnosticFocusOverride=false;yield return Frames(4);
        Check(host.DiagnosticSubmittedInput.rightTrigger==0&&host.DiagnosticSubmittedInput.padButtons==0,"Focus loss kept a bound input held");
        host.DiagnosticFocusOverride=true;yield return Frames(4);
        menu.SetOpen(false);yield return Frames(5);
        Check(!menu.IsOpen&&(host.Status.flags&2)==0,"Focus recovery must resume before testing a new device");
        // Exercise Unity's actual device event buffer and production provider,
        // including a newly connected non-XInput gamepad and its Start button.
        InputSystem.QueueStateEvent(syntheticPad,new GamepadState());yield return Frames(4);
        InputSystem.QueueStateEvent(syntheticPad,new GamepadState{leftStick=new Vector2(-.4f,0)}.WithButton(GamepadButton.South));yield return Frames(6);
        Check(host.DiagnosticSubmittedInput.rightTrigger==255&&Math.Abs(host.DiagnosticSubmittedInput.thumbLX+13107)<=1,"Unity HID provider did not reach rebound throttle and native analog steering");
        InputSystem.QueueStateEvent(syntheticPad,new GamepadState());yield return Frames(4);
        InputSystem.QueueStateEvent(syntheticPad,new GamepadState().WithButton(GamepadButton.Start));yield return Frames(12);
        Check(menu.IsOpen&&(host.Status.flags&2)!=0,"Unity gamepad Start did not keep pause open");
        InputSystem.QueueStateEvent(syntheticPad,new GamepadState());yield return Frames(3);
        InputSystem.QueueStateEvent(syntheticPad,new GamepadState().WithButton(GamepadButton.Start));yield return Frames(12);
        Check(!menu.IsOpen,"Unity gamepad Start did not resume");
        InputSystem.QueueStateEvent(syntheticPad,new GamepadState());yield return Frames(4);
        InputSystem.RemoveDevice(syntheticPad);syntheticPad=null;yield return Frames(3);
        syntheticPad=(Gamepad)InputSystem.AddDevice(TestPadDescription);InputSystem.QueueStateEvent(syntheticPad,new GamepadState().WithButton(GamepadButton.South));yield return Frames(6);
        Check(host.DiagnosticSubmittedInput.rightTrigger==0,"Reconnected held button bypassed the device release guard");
        InputSystem.QueueStateEvent(syntheticPad,new GamepadState());yield return Frames(4);
        InputSystem.QueueStateEvent(syntheticPad,new GamepadState().WithButton(GamepadButton.South));yield return Frames(6);
        Check(host.DiagnosticSubmittedInput.rightTrigger==255,"Reconnected Unity gamepad did not drive");
        InputSystem.QueueStateEvent(syntheticPad,new GamepadState());yield return Frames(4);
        Finish(true,null);
    }
    private void VerifySteeringAxisPairs()
    {
        var sample=new Idas3ControlBindings.PadState{connected=true,leftTrigger=33,rightTrigger=117,
            thumbLX=-12345,thumbLY=23456,thumbRX=14567,thumbRY=-24567};
        Idas3Native.FrameInput Read(Idas3ControlBindings mapper,Idas3ControlBindings.PadState state,IReadOnlyList<Idas3ControllerControl> axes=null)
        {
            mapper.Poll(_=>false,state,0,axes);var frame=new Idas3Native.FrameInput();mapper.ApplyDriving(ref frame);return frame;
        }
        Idas3ControlBindings Standard(string name,Idas3ControlBindings.PadInput left,Idas3ControlBindings.PadInput right)
        {
            var mapper=new Idas3ControlBindings();mapper.Initialize(Path.Combine(root,"steering-axis-pairs",name));
            Check(mapper.TrySetDraftPad(Idas3ControlBindings.ActionId.SteerLeft,left)&&
                mapper.TrySetDraftPad(Idas3ControlBindings.ActionId.SteerRight,right)&&mapper.ApplyDraft(),"Could not prepare steering pair "+name);
            Read(mapper,new Idas3ControlBindings.PadState{connected=true});return mapper;
        }
        var lefts=new[]{Idas3ControlBindings.PadInput.LeftStickLeft,Idas3ControlBindings.PadInput.RightStickLeft,
            Idas3ControlBindings.PadInput.LeftStickDown,Idas3ControlBindings.PadInput.RightStickDown};
        var rights=new[]{Idas3ControlBindings.PadInput.LeftStickRight,Idas3ControlBindings.PadInput.RightStickRight,
            Idas3ControlBindings.PadInput.LeftStickUp,Idas3ControlBindings.PadInput.RightStickUp};
        int[] expectedX={-12345,14567,23456,-24567},expectedY={23456,-24567,-12345,14567};
        for(int i=0;i<lefts.Length;++i)
        {
            var mapper=Standard("standard-"+i,lefts[i],rights[i]);var packet=Read(mapper,sample);
            Check(packet.thumbLX==expectedX[i]&&packet.thumbLY==expectedY[i],"Rebound steering did not carry its own perpendicular axis "+i);
            Check(packet.leftTrigger==33&&packet.rightTrigger==117&&packet.thumbRX==sample.thumbRX&&packet.thumbRY==sample.thumbRY,"Axis pairing changed other analog channels");
            var unrelated=sample;
            if(i==0||i==2){unrelated.thumbRX=short.MinValue;unrelated.thumbRY=short.MaxValue;}
            else{unrelated.thumbLX=short.MaxValue;unrelated.thumbLY=short.MinValue;}
            var independent=Read(mapper,unrelated);
            Check(independent.thumbLX==packet.thumbLX&&independent.thumbLY==packet.thumbLY,"Unrelated stick changed virtual steering radius "+i);
            packet=Read(mapper,default);Check(packet.padConnected==0&&packet.thumbLX==0&&packet.thumbLY==0,"Disconnected steering retained a radial axis");
        }
        var inverted=Standard("inverted",rights[0],lefts[0]);var invertPacket=Read(inverted,sample);
        Check(invertPacket.thumbLX==12345&&invertPacket.thumbLY==23456,"Inverted steering lost the physical perpendicular axis");
        var digital=Standard("digital",Idas3ControlBindings.PadInput.DPadLeft,Idas3ControlBindings.PadInput.DPadRight);
        var digitalSample=sample;digitalSample.buttons=4;var digitalPacket=Read(digital,digitalSample);
        Check(digitalPacket.thumbLX==-32768&&digitalPacket.thumbLY==0,"Digital steering inherited unrelated stick movement");
        var mixed=Standard("mixed",lefts[0],rights[1]);var mixedPacket=Read(mixed,sample);
        Check(mixedPacket.thumbLX==2222&&mixedPacket.thumbLY==0,"Mixed-stick steering fabricated a perpendicular axis");
        var endpoints=Standard("endpoints",lefts[0],rights[0]);var endpointSample=sample;endpointSample.thumbLX=short.MinValue;endpointSample.thumbLY=short.MaxValue;
        var endpointPacket=Read(endpoints,endpointSample);Check(endpointPacket.thumbLX==short.MinValue&&endpointPacket.thumbLY==short.MaxValue,"Raw stick endpoints lost precision");
        endpoints.BeginCapture(Idas3ControlBindings.ActionId.SteerLeft,Idas3ControlBindings.Slot.Controller,0);
        var blockedPacket=Read(endpoints,sample);Check(blockedPacket.thumbLX==0&&blockedPacket.thumbLY==0,"Binding capture leaked radial steering");

        string[] paths={"leftStick/x","leftStick/y","rightStick/x","rightStick/y","wheel/steering"};
        float[] values={-.25f,.5f,.75f,-1f,-.25f};int[] customX={-8192,16384,24575,-32768,-8192},customY={16384,-8192,-32768,24575,0};
        for(int i=0;i<paths.Length;++i)
        {
            var mapper=new Idas3ControlBindings();mapper.Initialize(Path.Combine(root,"steering-axis-pairs","custom-"+i));
            mapper.SelectControllerProfile("custom-axis-test","Private axis test",true);
            var source=new Idas3ControllerControl{path=paths[i],minimum=-1,maximum=1};
            Check(mapper.TrySetDraftControl(Idas3ControlBindings.ActionId.SteerLeft,source,-1,0)&&
                mapper.TrySetDraftControl(Idas3ControlBindings.ActionId.SteerRight,source,1,0)&&mapper.ApplyDraft(),"Could not prepare custom steering pair "+i);
            var axes=new List<Idas3ControllerControl>();for(int j=0;j<paths.Length;++j)axes.Add(new Idas3ControllerControl{path=paths[j],minimum=-1,maximum=1});
            Read(mapper,new Idas3ControlBindings.PadState{connected=true},axes);
            for(int j=0;j<axes.Count;++j)axes[j].value=values[j];var packet=Read(mapper,sample,axes);
            Check(packet.thumbLX==customX[i]&&packet.thumbLY==customY[i],"Custom steering used an unrelated pad axis or lost calibration "+i);
            var unrelated=sample;unrelated.thumbLX=unrelated.thumbLY=unrelated.thumbRX=unrelated.thumbRY=short.MinValue;
            var independent=Read(mapper,unrelated,axes);Check(independent.thumbLX==packet.thumbLX&&independent.thumbLY==packet.thumbLY,"Custom steering radius depended on the physical pad snapshot");
            if(i<4)
            {
                // A missing paired control must not fall back to an unrelated
                // physical stick supplied by a generic device adapter.
                packet=Read(mapper,sample,new[]{axes[i]});Check(packet.thumbLX==customX[i]&&packet.thumbLY==0,"Missing custom perpendicular axis fell back to left-stick Y");
            }
        }
    }
    private void VerifyCaptureRecovery()
    {
        var connected=new Idas3ControlBindings.PadState{connected=true};
        Idas3ControlBindings Generic(string name,out List<Idas3ControllerControl> controls,bool held=true)
        {
            controls=new List<Idas3ControllerControl>{
                new Idas3ControllerControl{path="wheel/selector",button=true,minimum=0,maximum=1,value=held?1:0},
                new Idas3ControllerControl{path="wheel/gas",minimum=-1,maximum=1,value=held?-.6f:1},
                new Idas3ControllerControl{path="wheel/clutch",minimum=-1,maximum=1,value=1}};
            var mapper=new Idas3ControlBindings();mapper.Initialize(Path.Combine(root,"capture-recovery",name));
            mapper.SelectControllerProfile("capture-recovery-wheel","Private capture recovery wheel",true);
            Check(mapper.TrySetDraftControl(Idas3ControlBindings.ActionId.Accelerate,controls[1],-1,1)&&mapper.ApplyDraft(),
                "Could not seed the generic pedal for capture recovery "+name);
            mapper.Poll(_=>false,connected,0,controls);return mapper;
        }
        void HeldMenuEdge(Idas3ControlBindings mapper,string reason)
        {
            var menuPacket=new Idas3Native.FrameInput();mapper.ApplyMenu(ref menuPacket,true,true);
            Check((menuPacket.key0&(1u<<13))!=0,reason+": suppressed capture lost the already-held pedal's menu edge");
            var drivingPacket=new Idas3Native.FrameInput();mapper.ApplyDriving(ref drivingPacket);
            Check(drivingPacket.rightTrigger==0&&drivingPacket.leftTrigger==0&&drivingPacket.thumbLX==0,
                reason+": capture suppression leaked driving input");
        }

        var escape=Generic("escape",out var escapeControls);
        string escapeDraft=JsonUtility.ToJson(escape.Draft),escapeFile=File.ReadAllText(escape.FilePath);
        escape.BeginCapture(Idas3ControlBindings.ActionId.Brake,Idas3ControlBindings.Slot.Controller,10);
        escape.Poll(_=>false,connected,10.1,escapeControls);
        Check(escape.IsCapturing&&escape.SuppressInput,"A latched selector did not leave capture waiting for release");
        HeldMenuEdge(escape,"Pre-arm capture");
        escape.Poll(key=>key==KeyCode.Escape,connected,10.2,escapeControls);
        Check(!escape.IsCapturing&&escape.SuppressInput,"Escape did not cancel before capture could arm, or lost its release guard");
        escape.Poll(key=>key==KeyCode.Escape,connected,10.3,escapeControls);
        Check(escape.SuppressInput&&!escape.PauseHeld,"Held cancel Escape escaped the capture release guard");
        HeldMenuEdge(escape,"Cancelled capture");
        escape.Poll(_=>false,connected,10.4,escapeControls);
        Check(!escape.SuppressInput,"A stuck selector or nonneutral pedal blocked recovery after Escape was released");
        Check(JsonUtility.ToJson(escape.Draft)==escapeDraft&&File.ReadAllText(escape.FilePath)==escapeFile,
            "Escape cancellation changed a binding or its saved file");

        var timeout=Generic("timeout",out var timeoutControls);
        string timeoutDraft=JsonUtility.ToJson(timeout.Draft);
        timeout.BeginCapture(Idas3ControlBindings.ActionId.Brake,Idas3ControlBindings.Slot.Controller,20);
        timeout.Poll(_=>false,connected,20.1,timeoutControls);
        Check(timeout.IsCapturing,"Timeout fixture did not remain in pre-arm capture");
        timeout.Poll(_=>false,connected,35.1,timeoutControls);
        Check(!timeout.IsCapturing&&timeout.SuppressInput&&!string.IsNullOrEmpty(timeout.CaptureError),
            "Pre-arm timeout failed to cancel with its normal message/release guard");
        timeout.Poll(_=>false,connected,35.2,timeoutControls);
        Check(!timeout.SuppressInput&&JsonUtility.ToJson(timeout.Draft)==timeoutDraft,
            "Stuck controller inputs prevented timeout recovery or changed the draft");

        var cancelled=Generic("explicit-cancel",out var cancelControls);
        string cancelDraft=JsonUtility.ToJson(cancelled.Draft);
        cancelled.BeginCapture(Idas3ControlBindings.ActionId.Brake,Idas3ControlBindings.Slot.Controller,40);
        cancelled.Poll(key=>key==KeyCode.Mouse0,connected,40.1,cancelControls);cancelled.CancelCapture();
        Check(!cancelled.IsCapturing&&cancelled.SuppressInput,"Explicit CancelCapture failed to stop capture");
        cancelled.Poll(key=>key==KeyCode.Mouse0,connected,40.2,cancelControls);
        Check(cancelled.SuppressInput,"Explicit cancellation did not guard the held mouse button");
        cancelled.Poll(_=>false,connected,40.3,cancelControls);
        Check(!cancelled.SuppressInput&&JsonUtility.ToJson(cancelled.Draft)==cancelDraft,
            "Explicit cancellation still waited for stuck controller inputs or changed the draft");

        // Accepted bindings retain the stronger release guard; cancellation
        // recovery must not turn a held captured control into immediate input.
        var keyboard=Generic("accepted-keyboard",out var keyControls,false);
        keyboard.BeginCapture(Idas3ControlBindings.ActionId.Headlights,Idas3ControlBindings.Slot.Primary,100);
        keyboard.Poll(_=>false,connected,100.1,keyControls);
        keyboard.Poll(key=>key==KeyCode.L,connected,100.2,keyControls);
        Check(!keyboard.IsCapturing&&keyboard.SuppressInput&&keyboard.Draft.actions[9].key1==KeyCode.L,
            "Keyboard capture did not accept its key with a release guard");
        keyboard.CancelCapture();keyboard.Poll(key=>key==KeyCode.L,connected,100.3,keyControls);
        Check(keyboard.SuppressInput,"CancelCapture weakened an already accepted keyboard release guard");
        keyboard.Poll(_=>false,connected,100.4,keyControls);
        Check(!keyboard.SuppressInput,"Accepted keyboard binding did not unblock after key release");

        foreach(bool button in new[]{true,false})
        {
            var mapper=Generic(button?"accepted-button":"accepted-pedal",out var controls,false);
            int index=button?0:2;mapper.BeginCapture(Idas3ControlBindings.ActionId.Brake,Idas3ControlBindings.Slot.Controller,200);
            mapper.Poll(_=>false,connected,200.1,controls);controls[index].value=button?1:-.6f;
            mapper.Poll(_=>false,connected,200.2,controls);
            Check(!mapper.IsCapturing&&mapper.SuppressInput&&mapper.Draft.actions[1].controlPath==controls[index].path,
                "Generic "+(button?"button":"pedal")+" capture did not retain its accepted control");
            mapper.CancelCapture();mapper.Poll(_=>false,connected,200.3,controls);
            Check(mapper.SuppressInput,"Accepted controller capture unblocked before its control returned to rest");
            controls[index].value=button?0:1;mapper.Poll(_=>false,connected,200.4,controls);
            Check(!mapper.SuppressInput,"Accepted controller capture did not recover after release");
        }
    }
    private void VerifyXInputDiscovery()
    {
        for(int expected=0;expected<4;++expected)
        {
            int slot=expected,calls=0;
            uint Poll(uint candidate,out Idas3Native.PadState state)
            {
                ++calls;state=default;if(candidate!=slot)return 1167;
                state.gamepad=new Idas3Native.Gamepad{buttons=0x10,rightTrigger=117,leftTrigger=33,thumbLX=-12345,thumbLY=32767,thumbRX=-32768};return 0;
            }
            var reader=new Idas3GamepadInput();Check(reader.TryReadXInput(0,Poll,out var first)&&reader.ActiveXInputSlot==slot,"Missed connected XInput slot "+slot);
            Check(first.buttons==0x10&&first.rightTrigger==117&&first.leftTrigger==33&&first.thumbLX==-12345&&first.thumbLY==32767&&first.thumbRX==-32768,"XInput analog/button sample lost precision");
            int before=calls;Check(reader.TryReadXInput(.01,Poll,out _),"Active XInput pad stopped polling");Check(calls==before+1,"Every frame unnecessarily polls disconnected slots");
            slot=-1;Check(!reader.TryReadXInput(.02,Poll,out var absent)&&!absent.connected,"Disconnected XInput input stayed held");
            slot=(expected+1)%4;Check(reader.TryReadXInput(.6,Poll,out _)&&reader.ActiveXInputSlot==slot,"Reconnected XInput controller was locked to its old slot");
        }
    }
    private IEnumerator Capture(string name)
    {
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-input-check-no-captures")>=0)yield break;
        var camera=host.GetComponent<Camera>();var scene=host.GetComponent<Idas3SceneRenderer>();var ui=host.GetComponent<Idas3UnityUi>();var menu=host.PauseMenu;var previous=camera.targetTexture;
        var target=new RenderTexture(Screen.width,Screen.height,24,RenderTextureFormat.ARGB32){antiAliasing=1};Check(target.Create(),"Capture target creation failed");
        camera.targetTexture=target;scene.ApplyFrame();ui.ApplyFrame();var cameras=new List<Camera>();
        foreach(var item in Resources.FindObjectsOfTypeAll<Camera>())if(item!=null&&item.enabled&&item.gameObject.activeInHierarchy&&item.targetTexture==target)cameras.Add(item);
        cameras.Sort((a,b)=>a.depth.CompareTo(b.depth));foreach(var item in cameras)item.Render();
        camera.targetTexture=previous;scene.ApplyFrame();ui.ApplyFrame();menu.RequestDiagnosticCapture(target);
        yield return Until(()=>menu.DiagnosticCaptureReady,5,"Controls menu did not repaint");yield return new WaitForEndOfFrame();
        var old=RenderTexture.active;RenderTexture.active=target;var picture=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);
        picture.ReadPixels(new Rect(0,0,target.width,target.height),0,0);picture.Apply();RenderTexture.active=old;
        string path="controls-"+name+".png";File.WriteAllBytes(Path.Combine(root,path),picture.EncodeToPNG());captures.Add(path);
        menu.CancelDiagnosticCapture();Destroy(picture);target.Release();Destroy(target);
    }
    private void Finish(bool passed,string error)
    {
        if(finished)return;finished=true;host.DiagnosticFocusOverride=null;bool stopped=false;
        if(syntheticPad!=null){InputSystem.RemoveDevice(syntheticPad);syntheticPad=null;}
        try{host.StopNative();stopped=!host.Ready;}catch(Exception e){passed=false;error=(error??"")+e;}
        File.WriteAllText(Path.Combine(root,"report.json"),JsonUtility.ToJson(new Report{passed=passed&&stopped,shutdownComplete=stopped,checks=checks,error=error,captures=captures.ToArray(),
            scope="Private-save Unity player. Synthetic physical input passes through production polling, binding capture, native input mapping and pause gates; real native driving. No physical controller hardware automation. "+(captures.Count>0?"Includes Controls OnGUI screenshots.":"Input-only run; UI rendering not checked.")},true));
        Debug.Log((passed?"PASS":"FAIL")+" controls bindings "+error);
#if UNITY_EDITOR
        UnityEditor.EditorApplication.isPlaying=false;
#else
        Application.Quit(passed?0:1);
#endif
    }
}
