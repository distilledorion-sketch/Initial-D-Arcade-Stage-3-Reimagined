using System;
using System.IO;
using System.Threading;
using UnityEngine;
using UnityEngine.InputSystem;

// Only fake output devices/writers are driven. Never vibrates physical hardware.
internal static class Idas3ControllerVibrationChecks
{
    private sealed class Target : Idas3ControllerVibration.ITarget {
        public bool connected=true,fail;public int calls;public float low,high;
        public bool Connected=>connected;
        public void Set(float l,float h){++calls;low=l;high=h;if(fail&&l>0)throw new InvalidOperationException("test driver unavailable");}
        public void Dispose(){Set(0,0);}
    }
    public sealed class TestPad : Gamepad {
        public int calls;public float low,high;
        public override void SetMotorSpeeds(float l,float h){++calls;low=l;high=h;}
    }
    private sealed class Platform : Idas3GameOptions.IPlatform {
        public int Width=>1280;public int Height=>720;public int DisplayMode=>0;public double Now=>0;
        public Idas3GameOptions.ResolutionChoice[] Resolutions=>new[]{new Idas3GameOptions.ResolutionChoice(1280,720)};
        public void Apply(Idas3GameOptions.Values before,Idas3GameOptions.Values after,bool displayChanged){}
    }
    [Serializable] private class Report {public bool passed,physicalOutputSent;public int checks;public string scope;}
    internal static void Run(string root){
        int checks=0;void Check(bool ok,string why){++checks;if(!ok)throw new InvalidOperationException("Controller vibration: "+why);}
        var settings=new Idas3GameOptions.Values{controllerVibration=true,controllerVibrationStrength=1};
        var state=new Idas3Native.FeedbackState{size=40,version=1,simulationTicks=1,torqueCode=128,rumbleFrequencyHalfHz=63,flags=1};
        var a=new Target();var b=new Target();
        using(var output=new Idas3ControllerVibration()){
            output.Update(settings,a,state,true,0);Check(a.calls==0,"Silent source fabricated vibration");
            state.rumbleAmplitude=41;state.simulationTicks++;output.Update(settings,a,state,true,.02);
            Check(a.low==.5f&&a.high==.5f,"Arcade sine amplitude was not sent to both motors");
            state.rumbleAmplitude=0;state.simulationTicks++;output.Update(settings,a,state,true,.04);
            Check(a.low==0&&a.high==0,"Source silence retained an invented impact envelope");
            state.rumbleAmplitude=41;state.simulationTicks++;settings.controllerVibrationStrength=.25f;output.Update(settings,a,state,true,.2);
            Check(a.low==.125f&&a.high==.125f,"Device gain did not scale output once");
            output.Update(settings,a,state,false,.21);Check(a.low==0,"Pause/menu/focus gate left motors on");
            state.simulationTicks++;output.Update(settings,a,state,true,.23);Check(a.low>0,"New driving did not resume vibration");
            output.Update(settings,a,state,true,.35);Check(a.low==0,"Stalled simulation left motors on");
            int stopped=a.calls;output.Update(settings,a,state,true,.4);Check(a.calls==stopped,"Stale impact restarted vibration");
            state.simulationTicks++;output.Update(settings,a,state,true,.5);Check(a.low>0,"Fresh telemetry did not recover");
            state.simulationTicks++;output.Update(settings,b,state,true,.52);Check(a.low==0&&b.low>0,"Changing devices did not stop the previous controller");
            b.connected=false;output.Update(settings,b,state,true,.54);Check(b.low==0,"Disconnect left output latched");
            b.connected=true;state.simulationTicks++;output.Update(settings,b,state,true,.56);
            settings.controllerVibration=false;output.Update(settings,b,state,true,.58);Check(b.low==0,"Turning vibration off did not stop output");
            settings.controllerVibration=true;state.simulationTicks++;output.Update(settings,b,state,true,.6);
            state.rumbleAmplitude=48;output.Update(settings,b,state,true,.62);Check(b.low==0,"Invalid source amplitude left output on");
            state.rumbleAmplitude=41;state.simulationTicks++;output.Update(settings,b,state,true,.64);
            settings.controllerVibrationStrength=0;output.Update(settings,b,state,true,.66);Check(b.low==0,"Zero strength did not stop output");
            settings.controllerVibrationStrength=1;state.simulationTicks++;output.Update(settings,b,state,true,.68);
            state.flags=0;output.Update(settings,b,state,true,.7);Check(b.low==0,"Non-driving state left output on");
            state.flags=1;state.simulationTicks++;b.fail=true;output.Update(settings,b,state,true,.72);Check(b.low==0,"Driver failure was not released");
            b.fail=false;state.simulationTicks++;output.Update(settings,b,state,true,1.8);Check(b.low==.5f,"Driver retry did not recover");
        }
        Check(b.low==0,"Shutdown left output on");
        var disabled=new Target();using(var output=new Idas3ControllerVibration(true))output.Update(settings,disabled,state,true,2);
        Check(disabled.calls==0,"Automated diagnostic attempted hardware output");
        // Source state persists until changed; consumed peaks do not get
        // replayed on duplicate render frames or acquire an invented decay.
        var repeated=new Target();using(var output=new Idas3ControllerVibration()){
            state.rumbleAmplitude=21;state.peakRumbleAmplitude=0;output.Update(settings,repeated,state,true,0);
            output.Update(settings,repeated,state,true,.05);Check(repeated.low==.25f,"Unchanged live source amplitude was artificially decayed");
            output.Update(settings,repeated,state,true,.101);Check(repeated.low==0,"Stalled source kept output alive");
            state.simulationTicks++;state.rumbleAmplitude=0;state.peakRumbleAmplitude=41;output.Update(settings,repeated,state,true,.12);
            Check(repeated.low==.5f,"Unread event between render frames was lost");
            output.Update(settings,repeated,state,true,.14);Check(repeated.low==0,"Consumed peak was replayed on a duplicate tick");
        }
        foreach(int fps in new[]{30,60,120,144}){
            var sampled=new Target();using(var output=new Idas3ControllerVibration()){
                bool heard=false,ended=false;
                for(int frame=0;frame<fps/2;++frame){
                    state.simulationTicks=(ulong)(frame*60/fps);state.rumbleAmplitude=0;
                    state.peakRumbleAmplitude=frame==(fps+59)/60?41:0;
                    output.Update(settings,sampled,state,true,frame/(double)fps);
                    heard|=sampled.low==.5f;if(heard&&sampled.low==0)ended=true;
                }
                Check(heard&&ended,"Original short event delivery failed at "+fps+" FPS");
            }
        }
        var throttled=new Target();using(var output=new Idas3ControllerVibration()){
            state.simulationTicks=0;state.rumbleAmplitude=21;state.peakRumbleAmplitude=0;state.rumbleFrequencyHalfHz=63;
            output.Update(settings,throttled,state,true,0);
            state.simulationTicks=1;state.rumbleAmplitude=0;state.peakRumbleAmplitude=47;output.Update(settings,throttled,state,true,.005);
            state.simulationTicks=2;state.peakRumbleAmplitude=0;state.rumbleFrequencyHalfHz=0;output.Update(settings,throttled,state,true,.02);
            Check(throttled.low==46f/80,"Send-rate throttle lost the original event");
            state.simulationTicks=3;state.peakRumbleAmplitude=41;state.rumbleFrequencyHalfHz=63;output.Update(settings,throttled,state,true,.025);
            state.simulationTicks=20;state.peakRumbleAmplitude=0;state.rumbleFrequencyHalfHz=0;output.Update(settings,throttled,state,true,.2);
            Check(throttled.low==0,"An expired pending event revived after a render stall");
        }
        // Exercise Unity's actual selected-device adapter with virtual pads.
        var pad=InputSystem.AddDevice<TestPad>();var other=InputSystem.AddDevice<TestPad>();
        try{
            using(var target=new Idas3ControllerVibration.UnityTarget(pad)){
                target.Set(.2f,.7f);Check(pad.low==.2f&&pad.high==.7f&&other.calls==0,"Unity output reached the wrong pad");
            }
            Check(pad.low==0&&pad.high==0,"Unity target disposal did not stop its motors");
            using(var devices=new Idas3ControllerDevices(()=>0,(uint index,out Idas3Native.PadState raw)=>{raw=default;return 1;},d=>d==pad||d==other)){
                devices.Initialize(Path.Combine(root,"vibration-unity-selection"));
                string key=null;foreach(var choice in devices.Choices)if(choice.key.Contains(":device:"+pad.deviceId))key=choice.key;
                Check(key!=null&&devices.Select(key),"Virtual controller could not be selected");
                Check(devices.VibrationTarget!=null,"Selected Unity controller has no vibration target");
                devices.VibrationTarget.Set(.3f,.3f);Check(pad.low==.3f&&other.calls==0,"Device selection did not own Unity vibration");
                devices.Select("keyboard");Check(devices.VibrationTarget==null,"Keyboard retained a vibration destination");
            }
        }finally{InputSystem.RemoveDevice(pad);InputSystem.RemoveDevice(other);}
        using(var devices=new Idas3ControllerDevices(()=>0,(uint index,out Idas3Native.PadState raw)=>{raw=default;return index==2?0u:1u;},d=>false)){
            devices.Initialize(Path.Combine(root,"vibration-xinput-selection"));devices.Select("xinput:2");
            Check(devices.VibrationTarget is Idas3ControllerVibration.XboxTarget xbox&&xbox.Slot==2,"Native XInput selection targeted the wrong slot");
        }
        uint slotSeen=99;float lowSeen=0;var gate=new object();using(var expired=new ManualResetEventSlim()){
            using(var target=new Idas3ControllerVibration.XboxTarget(2,()=>true,(slot,low,high)=>{lock(gate){slotSeen=slot;lowSeen=low;}if(low==0&&high==0)expired.Set();})){
                target.Set(.4f,.4f);lock(gate)Check(slotSeen==2&&lowSeen==.4f,"XInput adapter changed slot or amplitude");
                Check(expired.Wait(1000),"XInput watchdog did not release stalled output");lock(gate)Check(lowSeen==0,"XInput watchdog left a motor on");
            }
        }
        string directory=Path.Combine(root,"vibration-options");Directory.CreateDirectory(directory);
        File.WriteAllText(Path.Combine(directory,"game-options.json"),"{}");
        var options=new Idas3GameOptions(new Platform());options.Initialize(directory);
        Check(options.Current.controllerVibration&&options.Current.controllerVibrationStrength==.5f,"Legacy settings lost vibration defaults");
        options.BeginEdit();options.Draft.controllerVibration=false;options.Draft.controllerVibrationStrength=.23f;
        Check(options.HasUnsavedChanges&&options.ApplyDraft(),"Vibration settings did not save");
        var reload=new Idas3GameOptions(new Platform());reload.Initialize(directory);
        Check(!reload.Current.controllerVibration&&reload.Current.controllerVibrationStrength==.23f,"Vibration settings did not reload");
        reload.BeginEdit();reload.Draft.controllerVibrationStrength=float.NaN;Check(reload.ApplyDraft()&&reload.Current.controllerVibrationStrength==.5f,"Invalid strength was not sanitized");
        File.WriteAllText(Path.Combine(root,"controller-vibration.json"),JsonUtility.ToJson(new Report{passed=true,physicalOutputSent=false,checks=checks,
            scope="Original current/peak amplitude,30/60/120/144FPS event delivery, active Unity/XInput routing, stop/retry/XInput watchdog lifecycle, diagnostic isolation and saved settings. Unity device commands stop on main-thread stale/focus checks; only native XInput has an independent timer. Physical feel untested."},true));
    }
}
