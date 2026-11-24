#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;

// Only synthetic Unity devices and new isolated control fixtures. Never polls
// hardware, saves bindings, opens gameplay or loads the native game plugin.
internal static class Idas3PrecisePedalRuntimeChecks
{
    static int checks;
    static void Check(bool ok,string why){++checks;if(!ok)throw new Exception(why);}
    static uint Source(int value){uint x=(uint)value*2;if(x>=0x8000&&x<0x8100)x=0x8100;return Math.Min(x,0xff7f);}
    static Idas3Native.FrameInput Frame()=>new Idas3Native.FrameInput{size=88,flags=1,padConnected=1};
    static Idas3ControlBindings.Values Values(Idas3ControlBindings bindings)=>(Idas3ControlBindings.Values)typeof(Idas3ControlBindings).GetField("current",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(bindings);
    static Idas3ControlBindings New(string root,string name){var b=new Idas3ControlBindings();b.Initialize(Path.Combine(root,name));return b;}
    static Idas3Native.PedalInput Sample(Idas3ControlBindings bindings,Idas3ControlBindings.PadState pad,double now,
        IReadOnlyList<Idas3ControllerControl> controls=null,IReadOnlyList<Idas3ControllerDevices.RigSample> rig=null)
    {
        bindings.Poll(_=>false,pad,now,controls,rig);var f=Frame();bindings.ApplyDriving(ref f,out var p);
        return Idas3PrecisePedals.ForSubmission(f,p,false);
    }
    public static void Run()
    {
        string report=Environment.GetEnvironmentVariable("IDAS3_PRECISE_PEDAL_REPORT");
        if(string.IsNullOrWhiteSpace(report))throw new Exception("Explicit private report directory required");
        report=Path.GetFullPath(report);string verification=Path.GetFullPath(Path.Combine(Application.dataPath,"../Verification"))+Path.DirectorySeparatorChar;
        if(!report.StartsWith(verification,StringComparison.OrdinalIgnoreCase)||Directory.Exists(report))throw new Exception("Use a new Verification report directory");
        Directory.CreateDirectory(report);Gamepad device=null;
        try{
            Check(Marshal.SizeOf<Idas3Native.FrameInput>()==88&&Marshal.SizeOf<Idas3Native.PedalInput>()==16,"Stable base/additive ABI layout");
            checks+=Idas3CabinetStartRuntimeChecks.Run(report);
            checks+=Idas3ForcedQuitRuntimeChecks.Run(report);
            checks+=Idas3FinishInputRuntimeChecks.Run(report);
            checks+=Idas3OnlineFinishInputRuntimeChecks.Run(report);
            device=InputSystem.AddDevice<Gamepad>();var bindings=New(report,"defaults");
            for(int raw=0;raw<32768;++raw){
                InputState.Change(device.rightTrigger,raw/32767f);InputState.Change(device.leftTrigger,(32767-raw)/32767f);
                var physical=Idas3GamepadInput.ReadUnityPad(device);
                Check(physical.preciseTriggers,"Synthetic HID source lost precision flag");
                bindings.Poll(_=>false,physical,raw/60.0);var f=Frame();bindings.ApplyDriving(ref f,out var p);
                Check(f.rightTrigger==physical.rightTrigger&&f.leftTrigger==physical.leftTrigger,"Precise mapping changed old fallback bytes");
                var wire=Idas3PrecisePedals.ForSubmission(f,p,false);
                Check(wire.validMask==3&&wire.throttleAdc==Source(raw)&&wire.brakeAdc==Source(32767-raw),"Actual HID/binding/submission lost source ADC at "+raw);
            }
            for(int left=0;left<256;++left)for(int right=0;right<256;++right){
                var pad=new Idas3ControlBindings.PadState{connected=true,leftTrigger=(byte)left,rightTrigger=(byte)right};
                bindings.Poll(_=>false,pad,600);var f=Frame();bindings.ApplyDriving(ref f,out var p);
                Check(f.leftTrigger==left&&f.rightTrigger==right&&p.pedals.validMask==0,"Native XInput byte path changed");
            }
            var stick=New(report,"stick-pedals");Values(stick).actions[0].pad=Idas3ControlBindings.PadInput.LeftStickUp;Values(stick).actions[1].pad=Idas3ControlBindings.PadInput.LeftStickDown;
            for(int axis=-32768;axis<=32767;++axis){
                var pad=new Idas3ControlBindings.PadState{connected=true,thumbLY=(short)axis};
                var p=Sample(stick,pad,600);int throttle=Math.Max(0,axis),stickBrake=Math.Min(Math.Max(0,-axis),32767);
                Check(p.validMask==3&&p.throttleAdc==Source(throttle)&&p.brakeAdc==Source(stickBrake),"Saved stick pedal mapping lost signed source resolution at "+axis);
                var fallback=Frame();stick.ApplyDriving(ref fallback);
                Check(fallback.rightTrigger==Math.Min(255,(Math.Max(0,axis)*255+16383)/32767)&&fallback.leftTrigger==Math.Min(255,(Math.Max(0,-axis)*255+16383)/32767),"Saved stick pedal fallback changed");
            }
            var precise=new Idas3ControlBindings.PadState{connected=true,preciseTriggers=true,preciseRightTrigger=8289/32767f,rightTrigger=65,preciseLeftTrigger=.8f,leftTrigger=204};
            bindings.Poll(k=>k==KeyCode.W||k==KeyCode.S,precise,601);var keyed=Frame();bindings.ApplyDriving(ref keyed,out var captured);
            Check((keyed.key2&(1u<<('W'%32)))!=0&&(keyed.key2&(1u<<('S'%32)))!=0,"Canonical keyboard pedal precedence lost");
            Check(captured.pedals.throttleAdc==Source(8289),"Keyboard alias destroyed underlying analog source");
            var unfocused=keyed;unfocused.flags=0;Check(Idas3PrecisePedals.ForSubmission(unfocused,captured,false).validMask==0,"Focus loss retained precise controls");
            var disconnected=keyed;disconnected.padConnected=0;Check(Idas3PrecisePedals.ForSubmission(disconnected,captured,false).validMask==0,"Disconnect retained precise controls");
            var replaced=keyed;replaced.rightTrigger=0;Check(Idas3PrecisePedals.ForSubmission(replaced,captured,false).validMask==0,"Replaced packet retained stale ADC");
            Check(Idas3PrecisePedals.ForSubmission(keyed,captured,true).validMask==0,"Diagnostic override retained physical ADC");
            object[] args={keyed};typeof(Idas3SceneGame).GetMethod("NeutralizeControls",BindingFlags.NonPublic|BindingFlags.Static).Invoke(null,args);
            Check(Idas3PrecisePedals.ForSubmission((Idas3Native.FrameInput)args[0],captured,false).validMask==0,"Actual late menu neutralization retained ADC");
            bool unavailable=false;int newCalls=0,oldCalls=0;
            Idas3PrecisePedals.LegacyStep oldStep=(ref Idas3Native.FrameInput f)=>{oldCalls++;Check(f.rightTrigger==65&&f.leftTrigger==204,"Fallback bytes changed");return 7;};
            Idas3PrecisePedals.PreciseStep missing=(ref Idas3Native.FrameInput f,ref Idas3Native.PedalInput p)=>{newCalls++;throw new EntryPointNotFoundException();};
            Check(Idas3PrecisePedals.Submit(ref keyed,captured,false,ref unavailable,oldStep,missing)==7,"Old DLL fallback failed");
            Check(Idas3PrecisePedals.Submit(ref keyed,captured,false,ref unavailable,oldStep,missing)==7&&newCalls==1&&oldCalls==2,"Missing capability not cached");
            unavailable=false;Idas3PrecisePedals.PreciseStep present=(ref Idas3Native.FrameInput f,ref Idas3Native.PedalInput p)=>{Check(p.throttleAdc==Source(8289)&&p.validMask==3,"Available API received wrong ADC");return 9;};
            Check(Idas3PrecisePedals.Submit(ref keyed,captured,false,ref unavailable,oldStep,present)==9&&!unavailable&&oldCalls==2,"New API incorrectly used fallback");
            // Saved custom calibration and inversion, followed by independent
            // HID rig arbitration and reconnect-until-release ownership.
            var custom=New(report,"custom");custom.SelectControllerProfile("wheel","Synthetic wheel",true);
            var v=Values(custom);v.actions[0]=new Idas3ControlBindings.Binding{controlPath="pedal",controlMin=-1,controlMax=1,controlRest=1,controlDirection=-1};
            v.actions[1]=new Idas3ControlBindings.Binding{controlPath="brake",controlMin=-1,controlMax=1,controlRest=-1,controlDirection=1};
            var pedal=new Idas3ControllerControl{path="pedal",value=1};var brake=new Idas3ControllerControl{path="brake",value=-1};
            var controls=new[]{pedal,brake};Sample(custom,default,0,controls);pedal.value=.5f;brake.value=.6f;
            var result=Sample(custom,default,1,controls);Check(result.validMask==3&&result.throttleAdc==Idas3PrecisePedals.Adc(.25f)&&result.brakeAdc==Idas3PrecisePedals.Adc(.8f),"Custom range/inversion changed");
            var profiles=(Dictionary<string,Idas3ControlBindings.ControllerProfile>)typeof(Idas3ControlBindings).GetField("savedProfiles",BindingFlags.NonPublic|BindingFlags.Instance).GetValue(custom);
            var rigActions=Idas3ControlBindings.Defaults().actions;rigActions[0]=new Idas3ControlBindings.Binding{controlPath="gas",controlMin=0,controlMax=1,controlRest=0,controlDirection=1};
            profiles.Add("pedals",new Idas3ControlBindings.ControllerProfile{key="pedals",label="Synthetic pedals",generic=true,actions=rigActions});
            var gas=new Idas3ControllerControl{path="gas",minimum=0,maximum=1,value=.9f};
            var rig=new[]{new Idas3ControllerDevices.RigSample{key="rig1",profile="pedals",controls=new[]{gas}}};
            result=Sample(custom,default,2,controls,rig);Check(result.throttleAdc==Idas3PrecisePedals.Adc(.25f),"Fresh held rig bypassed release latch");
            gas.value=0;Sample(custom,default,3,controls,rig);gas.value=.8f;result=Sample(custom,default,4,controls,rig);
            Check(result.throttleAdc==Idas3PrecisePedals.Adc(.8f),"Higher rig pedal lost high resolution");
            pedal.value=-.8f;result=Sample(custom,default,5,controls,rig);Check(result.throttleAdc==Idas3PrecisePedals.Adc(.9f),"Higher primary pedal lost arbitration");
            pedal.value=1;result=Sample(custom,default,6,controls,null);Check(result.throttleAdc==0,"Disconnected rig retained pedal");
            var triggerActions=Idas3ControlBindings.Defaults().actions;
            profiles.Add("triggerpedals",new Idas3ControlBindings.ControllerProfile{key="triggerpedals",label="Synthetic trigger pedals",generic=true,actions=triggerActions});
            var triggerRig=new[]{new Idas3ControllerDevices.RigSample{key="rig2",profile="triggerpedals",pad=new Idas3ControlBindings.PadState{connected=true,preciseTriggers=true}}};
            Sample(custom,default,6.1,controls,triggerRig);triggerRig[0].pad=precise;
            result=Sample(custom,default,6.2,controls,triggerRig);
            Check(result.throttleAdc==Source(8289),"Rig PadInput trigger lost precision through legacy magnitude");
            typeof(Idas3ControlBindings).GetField("releaseBlocked",BindingFlags.NonPublic|BindingFlags.Instance).SetValue(custom,true);
            pedal.value=.5f;result=Sample(custom,default,7,controls);Check(result.validMask==0,"Capture release blocking retained precise pedals");
            Check(Directory.GetFiles(report,"controls.json",SearchOption.AllDirectories).Length==0,"Test wrote control preferences");
            File.WriteAllText(Path.Combine(report,"checks.txt"),"PASS "+checks+" actual virtual-HID/binding/rig/lifecycle/submission/fallback checks; no physical device polling or game launched.\n");
            Debug.Log("PASS "+checks+" precise pedal checks");
        }finally{if(device!=null)InputSystem.RemoveDevice(device);}
    }
}
#endif
