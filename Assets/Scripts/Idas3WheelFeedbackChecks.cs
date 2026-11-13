using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using System.Threading;

// Fake output boundary only. These checks never load/acquire a physical wheel.
//
// The source decision equations are checked natively. These checks validate
// command conversion, short-event delivery, and device lifecycle/ownership.
internal static class Idas3WheelFeedbackChecks
{
    private sealed class Backend : Idas3WheelFeedback.IBackend {
        public List<Idas3WheelFeedback.DeviceChoice> devices=new List<Idas3WheelFeedback.DeviceChoice>{
            new Idas3WheelFeedback.DeviceChoice{id="test-wheel",name="Test wheel",vendorId=11,productId=22}};
        public int sends,stops,shutdowns,scans;public string lastDevice;public bool fail,discoveryFailure;
        public Idas3WheelFeedback.CabinetRequest last;
        public List<Idas3WheelFeedback.DeviceChoice> Discover(){++scans;if(discoveryFailure)throw new InvalidOperationException("Discovery failed");return new List<Idas3WheelFeedback.DeviceChoice>(devices);}
        public bool Send(string id,Idas3WheelFeedback.CabinetRequest request){++sends;lastDevice=id;last=request;return !fail;}
        public void Stop(){++stops;}public void Shutdown(){++shutdowns;}public string Status=>"Mock driver unavailable";
    }
    [Serializable] private class Report {public bool passed,physicalOutputSent;public int checks;public string scope;}
    internal static void Run(string root){
        int checks=0;void Check(bool ok,string message){++checks;if(!ok)throw new InvalidOperationException("Wheel feedback: "+message);}
        var state=new Idas3Native.FeedbackState{size=40,version=1,simulationTicks=1,torqueCode=192,damperParameter=35,rumbleFrequencyHalfHz=63,flags=1};

        Check(System.Runtime.InteropServices.Marshal.SizeOf<Idas3Native.FeedbackState>()==40,"Feedback ABI size");
        Check(Idas3WheelFeedback.Pack(state,.5f,false,out var packed),"Valid source snapshot must pack");
        Check(packed.torque==-.5f&&packed.power==.5f,"Restoring polarity or independent device gain changed");
        Check(packed.damperStrength==2f/127&&packed.damperParameter==35f/127,"Damper byte roles changed");
        Check(packed.rumbleIntensity==0&&packed.rumbleFrequencyHz==31.5f,"Vibration was fabricated");
        Check(Idas3WheelFeedback.Pack(state,1,true,out var inverted)&&inverted.torque==.5f,"Invert must reverse the one host polarity conversion");
        var pulse=state;pulse.peakRumbleAmplitude=41;
        Check(Idas3WheelFeedback.Pack(pulse,1,false,out var packedPulse)&&packedPulse.rumbleIntensity==.5f,"Unread peak was dropped");
        pulse.rumbleAmplitude=47;
        Check(Idas3WheelFeedback.Pack(pulse,1,false,out var current)&&current.rumbleIntensity==46f/80,"Current source amplitude must win over a smaller peak");
        pulse.peakRumbleAmplitude=0;pulse.rumbleAmplitude=1;
        Check(Idas3WheelFeedback.Pack(pulse,1,false,out var silent)&&silent.rumbleIntensity==0,"Protocol amplitude one must be silent");
        foreach(var bad in new[]{float.NaN,float.PositiveInfinity}){
            Check(!Idas3WheelFeedback.Pack(state,bad,false,out _),"Nonfinite gain must be refused");
        }
        foreach(int field in new[]{0,1,2,3,4,5}){
            var invalid=state;
            if(field==0)invalid.torqueCode=256;else if(field==1)invalid.damperParameter=-1;
            else if(field==2)invalid.rumbleAmplitude=48;else if(field==3)invalid.peakRumbleAmplitude=-1;
            else if(field==4)invalid.version=2;else {invalid.rumbleAmplitude=2;invalid.rumbleFrequencyHalfHz=0;}
            Check(!Idas3WheelFeedback.Pack(invalid,1,false,out _),"Malformed source snapshot reached output");
        }
        Check(!Idas3WheelFeedback.Pack(state,0,false,out _),"Zero gain must stop");
        var parked=state;parked.flags=0;
        Check(!Idas3WheelFeedback.Pack(parked,1,false,out _),"Inactive snapshot must be refused");

        // ---- lifecycle ----
        var settings=new Idas3GameOptions.Values{wheelForceFeedback=true};
        var input=new Idas3WheelFeedback.InputIdentity{connected=true,key="wheel-input",vendorId=11,productId=22};
        var backend=new Backend();using(var output=new Idas3WheelFeedback(backend)){
            output.Update(settings,input,state,false,0);Check(backend.sends==0,"Menu sent wheel force");
            output.Update(settings,input,state,true,0);Check(backend.sends==1&&backend.lastDevice=="test-wheel","Unique active wheel was not selected");
            Check(backend.last.power==settings.wheelFeedbackStrength,"The player's strength setting must reach the board");
            output.Update(settings,input,state,false,.02);Check(backend.stops==1,"Opening menu did not release wheel");
            state.simulationTicks++;output.Update(settings,input,state,true,.04);Check(backend.sends==2,"Driving did not resume output");
            output.Update(settings,input,state,true,.2);int staleCount=backend.sends;
            for(int i=0;i<5;++i)output.Update(settings,input,state,true,.3+i*.1);
            Check(backend.sends==staleCount,"Stale simulation restarted force without a new tick");
            state.simulationTicks++;output.Update(settings,input,state,true,.9);Check(backend.sends==staleCount+1,"Fresh simulation did not recover");
            input.connected=false;output.Update(settings,input,state,true,1);Check(backend.stops>=3,"Disconnect did not stop output");
            input.connected=true;settings.wheelFeedbackDevice="missing";state.simulationTicks++;output.Update(settings,input,state,true,1.1);
            Check(backend.sends==staleCount+1,"Missing explicit device fell back to another wheel");
            settings.wheelFeedbackDevice="test-wheel";state.simulationTicks++;output.Update(settings,input,state,true,1.2);
            settings.wheelForceFeedback=false;output.Update(settings,input,state,true,1.3);Check(backend.stops>=4,"Disabling feedback did not release wheel");
        }Check(backend.shutdowns==1,"Shutdown did not dispose native output");

        // A frame whose telemetry goes bad mid-race must release the wheel.
        var broken=new Backend();settings.wheelForceFeedback=true;settings.wheelFeedbackDevice="";
        using(var output=new Idas3WheelFeedback(broken)){
            output.Update(settings,input,state,true,0);Check(broken.sends==1,"Driving did not start output");
            var poisoned=state;poisoned.simulationTicks++;poisoned.torqueCode=int.MinValue;
            output.Update(settings,input,poisoned,true,.05);
            Check(broken.sends==1&&broken.stops>=1,"Nonfinite telemetry reached the board instead of stopping it");
        }

        var ambiguous=new Backend();ambiguous.devices.Add(new Idas3WheelFeedback.DeviceChoice{id="second",name="Same model",vendorId=11,productId=22});
        using(var output=new Idas3WheelFeedback(ambiguous)){output.Update(settings,input,state,true,0);Check(ambiguous.sends==0,"Ambiguous automatic selection energized a wheel");}
        var noIdentity=new Backend();var unknown=input;unknown.vendorId=0;
        using(var output=new Idas3WheelFeedback(noIdentity)){output.Update(settings,unknown,state,true,0);Check(noIdentity.sends==0,"Unidentified input selected the first feedback device");}
        var failure=new Backend{fail=true};using(var output=new Idas3WheelFeedback(failure)){
            output.Update(settings,input,state,true,0);Check(failure.sends==1&&failure.stops==1,"Failed driver output not stopped");
            for(int i=1;i<20;++i){state.simulationTicks++;output.Update(settings,input,state,true,i*.04);}
            Check(failure.sends==1,"Driver retry backoff was bypassed");
            state.simulationTicks++;output.Update(settings,input,state,true,1.1);Check(failure.sends==2,"Driver retry did not recover after its delay");
        }
        var scanFailure=new Backend();using(var output=new Idas3WheelFeedback(scanFailure)){
            output.Update(settings,input,state,true,0);Check(scanFailure.sends==1,"Discovery regression did not start output");
            scanFailure.discoveryFailure=true;state.simulationTicks++;output.Update(settings,input,state,true,2.1);
            Check(scanFailure.scans==1&&scanFailure.sends==2,"Active race unnecessarily enumerated wheel drivers");
            output.Stop();output.RefreshDevices();
            Check(scanFailure.stops==1&&output.Choices.Count==1,"Failed discovery retained a feedback device");
            for(int i=1;i<15;++i){state.simulationTicks++;output.Update(settings,input,state,true,2.1+i*.1);Check(scanFailure.sends==2,"Failed discovery resumed stale device output");}
            scanFailure.discoveryFailure=false;state.simulationTicks++;output.Update(settings,input,state,true,4.2);
            Check(scanFailure.sends==3,"Successful rediscovery did not recover output");
        }
        QueuedChecks(Check);
        PeakChecks(Check,settings,input);
        var diagnostic=new Backend();using(var output=new Idas3WheelFeedback(diagnostic,true)){
            output.Update(settings,input,state,true,0);Check(diagnostic.sends==0,"Automated diagnostic enabled physical output");
        }
        File.WriteAllText(Path.Combine(root,"wheel-feedback-model-report.json"),JsonUtility.ToJson(new Report{
            passed=true,physicalOutputSent=false,checks=checks,scope="Managed wheel owner and queued worker with fake drivers: blocked discovery does not stall caller updates, newest continuous state with undelivered vibration peaks, stop cancellation, stale-request expiry, worker shutdown, no periodic scan during healthy output, source command validation, lifecycle, identity and retry guards. No physical wheel or hardware FPS verification."},true));
    }
    private sealed class SlowBackend : Idas3WheelFeedback.IBackend {
        internal readonly ManualResetEvent entered=new ManualResetEvent(false),release=new ManualResetEvent(false),stopped=new ManualResetEvent(false);
        internal int sends,shutdowns,driverThread;internal float last,rumble,frequency;
        public List<Idas3WheelFeedback.DeviceChoice> Discover(){entered.Set();release.WaitOne();return new List<Idas3WheelFeedback.DeviceChoice>();}
        public bool Send(string id,Idas3WheelFeedback.CabinetRequest request){driverThread=Thread.CurrentThread.ManagedThreadId;last=request.torque;rumble=request.rumbleIntensity;frequency=request.rumbleFrequencyHz;Interlocked.Increment(ref sends);return true;}
        public void Stop()=>stopped.Set();public void Shutdown()=>Interlocked.Increment(ref shutdowns);public string Status=>"Test";
    }
    private static void QueuedChecks(Action<bool,string> check){
        var slow=new SlowBackend();var output=new Idas3WheelFeedback.QueuedBackend(slow);
        try{
            output.Discover();check(slow.entered.WaitOne(2000),"Worker discovery never started");
            var watch=System.Diagnostics.Stopwatch.StartNew();
            for(int i=0;i<600;++i)check(output.Send("test",new Idas3WheelFeedback.CabinetRequest{torque=i/600f}),"Mailbox rejected healthy request");
            output.Stop();string status=output.Status;
            check(watch.ElapsedMilliseconds<100,"Blocked discovery stalled render-thread calls");
            slow.release.Set();check(slow.stopped.WaitOne(2000),"Worker did not prioritize stop");
            check(slow.sends==0,"Stop allowed queued steering to play");
            output.Send("test",new Idas3WheelFeedback.CabinetRequest{torque=.75f});
            check(SpinWait.SpinUntil(()=>Volatile.Read(ref slow.sends)>0,2000),"Fresh request did not reach driver");
            check(slow.driverThread!=Thread.CurrentThread.ManagedThreadId,"Driver ran on caller thread");
        }finally{slow.release.Set();output.Shutdown();check(output.WaitForShutdown(2000),"Worker did not shut down");}
        check(slow.shutdowns==1,"Driver shutdown count");
        foreach(bool stale in new[]{false,true}){
            var driver=new SlowBackend();var queue=new Idas3WheelFeedback.QueuedBackend(driver);
            try{
                queue.Discover();check(driver.entered.WaitOne(2000),"Slow discovery did not start");
                for(int i=0;i<100;++i)queue.Send("test",new Idas3WheelFeedback.CabinetRequest{torque=i,rumbleIntensity=i==0?.5f:0,rumbleFrequencyHz=31.5f});
                if(stale)Thread.Sleep(150);
                driver.release.Set();
                check(SpinWait.SpinUntil(()=>Volatile.Read(ref driver.sends)>0||driver.stopped.WaitOne(0),2000),"Mailbox work did not complete");
            }finally{driver.release.Set();queue.Shutdown();check(queue.WaitForShutdown(2000),"Mailbox worker did not stop");}
            check(stale?driver.sends==0:driver.sends==1&&driver.last==99,stale?"Stale queued force reached driver":"Mailbox replayed obsolete force requests");
            if(!stale)check(driver.rumble==.5f,"Newer silent state erased an undelivered rumble peak");
        }
        var aging=new SlowBackend();var agingQueue=new Idas3WheelFeedback.QueuedBackend(aging);
        try{
            agingQueue.Discover();check(aging.entered.WaitOne(2000),"Aging-event discovery did not start");
            agingQueue.Send("test",new Idas3WheelFeedback.CabinetRequest{torque=.1f,rumbleIntensity=.5f,rumbleFrequencyHz=31.5f});
            Thread.Sleep(150);
            agingQueue.Send("test",new Idas3WheelFeedback.CabinetRequest{torque=.2f});
            aging.release.Set();check(SpinWait.SpinUntil(()=>Volatile.Read(ref aging.sends)>0,2000),"Latest continuous state not delivered");
            check(aging.last==.2f&&aging.rumble==0,"Fresh continuous traffic rejuvenated an expired queued pulse");
        }finally{aging.release.Set();agingQueue.Shutdown();check(agingQueue.WaitForShutdown(2000),"Aging-event worker did not stop");}
        // Deterministic expiry boundaries: a newer lower/equal event remains
        // after the old maximum expires, even with a newer silent request.
        for(int scenario=0;scenario<4;++scenario){
            long clockMs=0;var driver=new SlowBackend();
            var queue=new Idas3WheelFeedback.QueuedBackend(driver,()=>Volatile.Read(ref clockMs)/1000.0);
            float newer=scenario==1?.5f:.25f;
            try{
                queue.Discover();check(driver.entered.WaitOne(2000),"Boundary-case worker did not enter discovery");
                queue.Send("test",new Idas3WheelFeedback.CabinetRequest{torque=.1f,rumbleIntensity=.5f,rumbleFrequencyHz=31.5f});
                Interlocked.Exchange(ref clockMs,90);
                queue.Send("test",new Idas3WheelFeedback.CabinetRequest{torque=.2f,rumbleIntensity=newer,rumbleFrequencyHz=42});
                Interlocked.Exchange(ref clockMs,100);
                queue.Send("test",new Idas3WheelFeedback.CabinetRequest{torque=.9f});
                Interlocked.Exchange(ref clockMs,scenario==2?191:scenario==3?100:105);
                driver.release.Set();check(SpinWait.SpinUntil(()=>Volatile.Read(ref driver.sends)>0,2000),"Boundary-case request was not delivered");
                float expected=scenario==2?0:scenario==3?.5f:newer;
                check(driver.last==.9f&&driver.rumble==expected,"Expired maximum erased a newer lower/equal pulse, or an expired pulse survived");
                if(scenario<2)check(driver.frequency==42,"Retained event used the expired event's frequency");
            }finally{driver.release.Set();queue.Shutdown();check(queue.WaitForShutdown(2000),"Boundary-case worker did not stop");}
        }
    }
    private static void PeakChecks(Action<bool,string> check,Idas3GameOptions.Values settings,Idas3WheelFeedback.InputIdentity input){
        settings.wheelForceFeedback=true;settings.wheelFeedbackDevice="test-wheel";
        foreach(int fps in new[]{30,60,120,144}){
            var backend=new Backend();using(var output=new Idas3WheelFeedback(backend)){
                // At30FPS the current state is already silent; only the native
                // unread peak reports the event between render samples.
                var sample=new Idas3Native.FeedbackState{size=40,version=1,flags=1,torqueCode=128,damperParameter=35,rumbleFrequencyHalfHz=63};
                bool heard=false,ended=false;
                for(int frame=0;frame<fps/2;++frame){
                    sample.simulationTicks=(ulong)(frame*60/fps);
                    sample.peakRumbleAmplitude=frame==(fps+59)/60?41:0;
                    int before=backend.sends;output.Update(settings,input,sample,true,frame/(double)fps);
                    if(backend.sends>before){heard|=backend.last.rumbleIntensity==.5f;if(heard&&backend.last.rumbleIntensity==0)ended=true;}
                }
                check(heard,"Short original vibration was lost at "+fps+" FPS");
                check(ended&&backend.last.rumbleIntensity==0,"A source event became a quarter-second envelope");
            }
        }
        var delayed=new Backend();using(var output=new Idas3WheelFeedback(delayed)){
            var sample=new Idas3Native.FeedbackState{size=40,version=1,flags=1,torqueCode=128,rumbleFrequencyHalfHz=63};
            output.Update(settings,input,sample,true,0);
            sample.simulationTicks=1;sample.peakRumbleAmplitude=47;output.Update(settings,input,sample,true,.005);
            sample.simulationTicks=2;sample.peakRumbleAmplitude=0;sample.rumbleFrequencyHalfHz=0;output.Update(settings,input,sample,true,.02);
            check(delayed.last.rumbleIntensity==46f/80,"Send-rate throttle lost the consumed native peak");
            check(delayed.last.rumbleFrequencyHz==31.5f,"A later zero-frequency stop discarded the pending peak's frequency");
            sample.simulationTicks=3;sample.peakRumbleAmplitude=41;sample.rumbleFrequencyHalfHz=63;output.Update(settings,input,sample,true,.025);
            sample.simulationTicks=20;sample.peakRumbleAmplitude=0;sample.rumbleFrequencyHalfHz=0;output.Update(settings,input,sample,true,.2);
            check(delayed.last.rumbleIntensity==0,"An expired undelivered pulse revived after a render stall");
        }
    }
#if UNITY_EDITOR
    public static void RunBatch(){
        const string path="Verification/wheel-effects-20260921/managed";
        Directory.CreateDirectory(path);Run(path);Debug.Log("PASS wheel feedback worker checks");
    }
#endif
}
