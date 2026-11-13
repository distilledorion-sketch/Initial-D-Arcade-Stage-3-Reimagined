using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.Layouts;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.InputSystem.Utilities;
using UnityEngine.InputSystem.XInput;

// Explicit editor/player diagnostic with isolated preferences and simulated
// device samples. No real controllers are disabled or sent input/output.
public static class Idas3ControllerReconnectChecks
{
    [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
    private struct WindowsPadState : IInputStateTypeInfo
    {
        public ushort buttons;
        public byte leftTrigger,rightTrigger;
        public short leftX,leftY,rightX,rightY;
        public FourCC format => new FourCC('X','I','N','P');
    }
    private static void CheckAlternateBackends(string root,Action<bool,string> check)
    {
#if UNITY_STANDALONE_WIN || UNITY_EDITOR_WIN
        // Actual Unity XInput layout, with a private device and simulated native
        // slots. No event is sent to any physical controller or Steam process.
        var pad=(Gamepad)InputSystem.AddDevice(new InputDeviceDescription {
            interfaceName="XInput",manufacturer="Private backend check",
            product="Alternate Xbox backend",serial="isolated-backend",
            capabilities="{\"type\":0,\"subType\":1,\"flags\":0}" });
        try {
            check(pad is XInputControllerWindows,"Fixture must use the Windows XInput layout");
            double time=0;bool rawConnected=true;var raw=new Idas3Native.PadState();
            uint Read(uint slot,out Idas3Native.PadState value){value=raw;return slot==0&&rawConnected?0u:1167u;}
            using(var devices=new Idas3ControllerDevices(()=>time,Read,d=>ReferenceEquals(d,pad))){
                devices.Initialize(Path.Combine(root,"alternate-backends"));
                check(devices.ActiveProfileKey=="xinput:slot:0","Prefer the precise native sample at startup");
                string key=null;
                foreach(var choice in devices.Choices)if(choice.connected&&choice.label.Contains("Alternate Xbox backend"))key=choice.key;
                check(key!=null,"An idle native port hid a usable Unity XInput controller");
                time=1;devices.Tick(true);
                InputState.Change(pad,new WindowsPadState{rightTrigger=255,leftX=16000,buttons=0x1000});devices.Tick(true);
                check(devices.ActiveName.Contains("Alternate Xbox backend")&&devices.TryRead(out var p)&&p.rightTrigger==255&&p.thumbLX==16000&&(p.buttons&0x1000)!=0,
                    "Fresh Unity Xbox input cannot take over an idle native port");
                InputState.Change(pad,new WindowsPadState());devices.Tick(true);
                if(key!=null){
                    check(devices.Select(key),"Unity Xbox cannot be explicitly selected");
                    rawConnected=false;time+=1;devices.Tick(false);
                    check(devices.SelectedKey==key&&devices.TryRead(out _),"Native unplug disconnected the selected Unity backend");
                    rawConnected=true;time+=1;devices.Tick(false);
                    check(devices.SelectedKey==key&&devices.TryRead(out _)&&devices.ActiveName.Contains("Alternate Xbox backend"),
                        "Native reconnect hid the explicitly selected Unity backend");
                }
                devices.Select("xinput:0");devices.Select("automatic");int changes=0;
                devices.ActiveDeviceChanged+=()=>++changes;
                for(int frame=0;frame<120;++frame){
                    time+=1.0/60;raw.gamepad.rightTrigger=255;raw.gamepad.thumbLX=16000;
                    InputState.Change(pad,new WindowsPadState{rightTrigger=254,leftX=(short)(frame%2==0?17000:19000)});devices.Tick(true);
                    check(devices.ActiveProfileKey=="xinput:slot:0"&&devices.TryRead(out var held)&&held.rightTrigger==255,
                        "Delayed Unity copy interrupted the held native source at "+frame);
                }
                check(changes==0,"Duplicate backend rearmed the input release guard");
                devices.Select("keyboard");devices.Tick(true);
                check(!devices.TryRead(out _),"Alternate backend overrode Keyboard only");
            }
        } finally { InputSystem.RemoveDevice(pad); }
#endif
    }
    [Serializable] private sealed class Report { public bool passed;public int checks;public string[] failures; }
    public static void Run()
    {
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-reconnect-check-output");
        if(at<0||at+1>=args.Length)throw new ArgumentException("A fresh reconnect diagnostic output folder is required.");
        string root=Path.GetFullPath(args[at+1]);
        if(Directory.Exists(root)||File.Exists(root))throw new IOException("Do not overwrite reconnect evidence.");
        Directory.CreateDirectory(root);
        int checks=0;var failures=new List<string>();
        void Check(bool value,string reason){++checks;if(!value)failures.Add(reason);}
        CheckAlternateBackends(root,Check);
        double now=0;var slots=new Idas3Native.PadState[4];var connected=new bool[4];
        uint Read(uint slot,out Idas3Native.PadState value){value=slots[slot];return connected[slot]?0u:1167u;}
        connected[0]=true;
        using(var devices=new Idas3ControllerDevices(()=>now,Read,device=>false)){
            devices.Initialize(Path.Combine(root,"devices"));
            Check(devices.TryRead(out _)&&devices.ActiveProfileKey=="xinput:slot:0","Initial controller was not selected");
            connected[1]=true;now+=1;devices.Tick(false);slots[1].gamepad.buttons=0x1000;devices.Tick(false);
            Check(devices.ActiveProfileKey=="xinput:slot:0","A live menu controller was replaced by unrelated input");
            connected[0]=false;now+=1;devices.Tick(false);
            Check(devices.TryRead(out _)&&devices.ActiveProfileKey=="xinput:slot:1","Online menu stranded input after the controller changed XInput slots");
            connected[1]=false;now+=1;devices.Tick(false);
            Check(!devices.TryRead(out _),"Disconnected controller retained stale input");
            connected[2]=true;now+=1;devices.Tick(false);
            Check(devices.TryRead(out _)&&devices.ActiveProfileKey=="xinput:slot:2","An open menu could not recover a replacement controller");
            devices.Select("keyboard");devices.Tick(false);
            Check(!devices.TryRead(out _),"Automatic recovery overrode Keyboard only");
        }
        // Physical and Steam Input copies can report the same held controls
        // with slightly different values/timing. They must not repeatedly
        // switch profiles and re-arm the held-input release guard.
        now=0;Array.Clear(slots,0,slots.Length);Array.Clear(connected,0,connected.Length);
        connected[0]=connected[1]=true;
        using(var devices=new Idas3ControllerDevices(()=>now,Read,device=>false)){
            devices.Initialize(Path.Combine(root,"duplicate-pads"));
            int changes=0;devices.ActiveDeviceChanged+=()=>++changes;
            for(int frame=0;frame<360;++frame){
                now=frame/60.0;
                slots[0].gamepad.rightTrigger=255;slots[0].gamepad.thumbLX=16000;
                slots[1].gamepad.rightTrigger=(byte)(frame%3==0?254:255);
                slots[1].gamepad.thumbLX=(short)(frame%2==0?17000:19000);
                devices.Tick(true);
                Check(devices.ActiveProfileKey=="xinput:slot:0"&&devices.TryRead(out var held)&&held.rightTrigger==255,
                    "Duplicate pad interrupted held race input at frame "+frame);
            }
            Check(changes==0,"Duplicate pad repeatedly reset the active binding profile");
            slots[0]=slots[1]=default;now+=.02;devices.Tick(true);
            now+=.6;devices.Tick(true);slots[1].gamepad.buttons=0x1000;devices.Tick(true);
            Check(devices.ActiveProfileKey=="xinput:slot:1","A fresh controller could not take over after neutral");
            Check(devices.Select("xinput:0")&&devices.ActiveProfileKey=="xinput:slot:0","Explicit selection was blocked by the neutral hold");
            connected[0]=false;now+=.01;devices.Tick(false);
            Check(devices.ActiveProfileKey=="xinput:slot:1"&&devices.TryRead(out _),"Unplug recovery waited for neutral");
        }
        var mapper=new Idas3ControlBindings();mapper.Initialize(Path.Combine(root,"bindings"));
        mapper.ControllerDeviceChanged();
        var sample=new Idas3ControlBindings.PadState{connected=true,rightTrigger=255,thumbLX=-20000,buttons=0x1000};
        mapper.Poll(_=>false,sample,0);
        var packet=new Idas3Native.FrameInput{padButtons=sample.buttons};mapper.ApplyMenu(ref packet,false);
        Check(packet.padButtons==0,"Held confirm leaked on reconnection");
        sample.leftTrigger=160;sample.buttons|=0x10;mapper.Poll(k=>k==KeyCode.W,sample,1);
        packet=default;mapper.ApplyDriving(ref packet);
        Check(mapper.PauseHeld&&packet.leftTrigger==160,"Held throttle/steering blocked fresh pause or brake input");
        Check(packet.rightTrigger==0&&packet.thumbLX==0,"Already-held analog controls bypassed their release guard");
        Check((packet.key2&(1u<<23))!=0&&!mapper.SuppressInput,"Reconnection blocked independent keyboard input");
        sample.rightTrigger=0;sample.buttons=0;mapper.Poll(_=>false,sample,2);
        sample.rightTrigger=190;sample.buttons=0x1000;mapper.Poll(_=>false,sample,3);
        packet=default;mapper.ApplyDriving(ref packet);
        Check(packet.rightTrigger==190&&packet.thumbLX==0,"Fresh throttle waited for an unrelated held steering axis");
        var menuPacket=new Idas3Native.FrameInput{padButtons=sample.buttons};mapper.ApplyMenu(ref menuPacket,false);
        Check((menuPacket.padButtons&0x1000)!=0,"Released and re-pressed confirm did not recover independently");
        sample.thumbLX=0;mapper.Poll(_=>false,sample,4);sample.thumbLX=12000;mapper.Poll(_=>false,sample,5);
        packet=default;mapper.ApplyDriving(ref packet);
        Check(packet.thumbLX==12000&&packet.rightTrigger==190,"Independent axis release did not restore driving");
        var pedals=new List<Idas3ControllerControl>{
            new Idas3ControllerControl{path="gas",minimum=-1,maximum=1,value=-1},
            new Idas3ControllerControl{path="brake",minimum=-1,maximum=1,value=1}};
        mapper=new Idas3ControlBindings();mapper.Initialize(Path.Combine(root,"wheel"));mapper.SelectControllerProfile("test-wheel","Test wheel",true);
        Check(mapper.TrySetDraftControl(Idas3ControlBindings.ActionId.Accelerate,pedals[0],-1,1)&&
            mapper.TrySetDraftControl(Idas3ControlBindings.ActionId.Brake,pedals[1],-1,1)&&mapper.ApplyDraft(),"Could not prepare isolated wheel bindings");
        mapper.ControllerDeviceChanged();sample=new Idas3ControlBindings.PadState{connected=true};mapper.Poll(_=>false,sample,0,pedals);
        pedals[1].value=-1;mapper.Poll(_=>false,sample,1,pedals);packet=default;mapper.ApplyDriving(ref packet);
        Check(packet.leftTrigger==255&&packet.rightTrigger==0,"A held wheel pedal blocked a different newly pressed pedal");
        pedals[0].value=1;mapper.Poll(_=>false,sample,2,pedals);pedals[0].value=-1;mapper.Poll(_=>false,sample,3,pedals);
        packet=default;mapper.ApplyDriving(ref packet);
        Check(packet.rightTrigger==255&&packet.leftTrigger==255,"Wheel pedal recovery required all controls to rest together");
        var report=new Report{passed=failures.Count==0,checks=checks,failures=failures.ToArray()};
        File.WriteAllText(Path.Combine(root,"report.json"),JsonUtility.ToJson(report,true));
        if(!report.passed)throw new InvalidOperationException(string.Join("; ",report.failures));
        Debug.Log("Controller reconnect checks passed: "+checks);
    }
}
