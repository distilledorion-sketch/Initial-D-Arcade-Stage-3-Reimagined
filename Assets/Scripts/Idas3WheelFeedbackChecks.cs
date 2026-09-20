using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

// Fake output boundary only. These checks never load/acquire a physical wheel.
//
// The force model itself is no longer tested here: it moved to the ported
// cabinet board and its command owner, which are covered natively by
// original_ffb_tests and original_ffb_owner_tests. What remains here is the
// managed half -- when a wheel may be driven at all, and that the telemetry
// handed across the boundary is the driving state and nothing invented.
internal static class Idas3WheelFeedbackChecks
{
    private sealed class Backend : Idas3WheelFeedback.IBackend {
        public List<Idas3WheelFeedback.DeviceChoice> devices=new List<Idas3WheelFeedback.DeviceChoice>{
            new Idas3WheelFeedback.DeviceChoice{id="test-wheel",name="Test wheel",vendorId=11,productId=22}};
        public int sends,stops,shutdowns;public string lastDevice;public bool fail,discoveryFailure;
        public Idas3WheelFeedback.CabinetRequest last;
        public List<Idas3WheelFeedback.DeviceChoice> Discover(){if(discoveryFailure)throw new InvalidOperationException("Discovery failed");return new List<Idas3WheelFeedback.DeviceChoice>(devices);}
        public bool Send(string id,Idas3WheelFeedback.CabinetRequest request){++sends;lastDevice=id;last=request;return !fail;}
        public void Stop(){++stops;}public void Shutdown(){++shutdowns;}public string Status=>"Mock driver unavailable";
    }
    [Serializable] private class Report {public bool passed,physicalOutputSent;public int checks;public string scope;}
    internal static void Run(string root){
        int checks=0;void Check(bool ok,string message){++checks;if(!ok)throw new InvalidOperationException("Wheel feedback: "+message);}
        var state=new Idas3Native.WheelState{size=40,version=1,simulationTicks=1,speed=35,steering=.6f,flags=1};

        // ---- telemetry packing: units, clamps and refusals ----
        Check(Idas3WheelFeedback.Pack(state,7,1f/60,1,false,out var packed),"Valid driving telemetry must pack");
        Check(Math.Abs(packed.speedKmh-35*3.6f)<.001f,"Speed must reach the cabinet board in km/h");
        Check(packed.steering==.6f&&packed.driving,"Steering must cross the boundary unchanged");
        Check(!packed.wallContact&&packed.impact==0,"Wall contact must not be invented");
        Check(packed.deltaSeconds>0,"Frame time must cross the boundary");
        Check(packed.carIndex==7,"The car must reach the board, which limits force per car");
        var wall=state;wall.flags|=2;wall.wallLateral=-.2f;wall.impact=.5f;
        Check(Idas3WheelFeedback.Pack(wall,7,1f/60,1,false,out var packedWall),"Wall telemetry must pack");
        Check(packedWall.wallContact&&packedWall.wallLateral==-.2f&&packedWall.impact==.5f,"Wall telemetry must cross unchanged");
        Check(Idas3WheelFeedback.Pack(state,7,1f/60,1,true,out var inverted)&&inverted.invert,"Inversion must reach the board, not be applied here");
        var overSteered=state;overSteered.steering=4;
        Check(Idas3WheelFeedback.Pack(overSteered,7,1f/60,1,false,out var clamped)&&clamped.steering==1,"Out-of-range steering must be clamped, not passed on");
        foreach(var bad in new[]{float.NaN,float.PositiveInfinity}){
            var invalid=state;invalid.steering=bad;
            Check(!Idas3WheelFeedback.Pack(invalid,7,1f/60,1,false,out _),"Nonfinite telemetry must be refused before the boundary");
            var invalidWall=state;invalidWall.wallLateral=bad;
            Check(!Idas3WheelFeedback.Pack(invalidWall,7,1f/60,1,false,out _),"Nonfinite wall telemetry must be refused");
        }
        Check(!Idas3WheelFeedback.Pack(state,7,0,1,false,out _),"A zero frame time must be refused");
        Check(!Idas3WheelFeedback.Pack(state,7,1f/60,0,false,out _),"Zero strength must be refused rather than sent as silence");
        var parked=state;parked.flags=0;
        Check(!Idas3WheelFeedback.Pack(parked,7,1f/60,1,false,out _),"Telemetry from outside driving must be refused");

        // ---- lifecycle ----
        var settings=new Idas3GameOptions.Values{wheelForceFeedback=true};
        var input=new Idas3WheelFeedback.InputIdentity{connected=true,key="wheel-input",vendorId=11,productId=22};
        var backend=new Backend();using(var output=new Idas3WheelFeedback(backend)){
            output.Update(settings,input,state,7,false,0);Check(backend.sends==0,"Menu sent wheel force");
            output.Update(settings,input,state,7,true,0);Check(backend.sends==1&&backend.lastDevice=="test-wheel","Unique active wheel was not selected");
            Check(backend.last.driving&&backend.last.strength==settings.wheelFeedbackStrength,"The player's strength setting must reach the board");
            output.Update(settings,input,state,7,false,.02);Check(backend.stops==1,"Opening menu did not release wheel");
            state.simulationTicks++;output.Update(settings,input,state,7,true,.04);Check(backend.sends==2,"Driving did not resume output");
            output.Update(settings,input,state,7,true,.2);int staleCount=backend.sends;
            for(int i=0;i<5;++i)output.Update(settings,input,state,7,true,.3+i*.1);
            Check(backend.sends==staleCount,"Stale simulation restarted force without a new tick");
            state.simulationTicks++;output.Update(settings,input,state,7,true,.9);Check(backend.sends==staleCount+1,"Fresh simulation did not recover");
            input.connected=false;output.Update(settings,input,state,7,true,1);Check(backend.stops>=3,"Disconnect did not stop output");
            input.connected=true;settings.wheelFeedbackDevice="missing";state.simulationTicks++;output.Update(settings,input,state,7,true,1.1);
            Check(backend.sends==staleCount+1,"Missing explicit device fell back to another wheel");
            settings.wheelFeedbackDevice="test-wheel";state.simulationTicks++;output.Update(settings,input,state,7,true,1.2);
            settings.wheelForceFeedback=false;output.Update(settings,input,state,7,true,1.3);Check(backend.stops>=4,"Disabling feedback did not release wheel");
        }Check(backend.shutdowns==1,"Shutdown did not dispose native output");

        // A frame whose telemetry goes bad mid-race must release the wheel.
        var broken=new Backend();settings.wheelForceFeedback=true;settings.wheelFeedbackDevice="";
        using(var output=new Idas3WheelFeedback(broken)){
            output.Update(settings,input,state,7,true,0);Check(broken.sends==1,"Driving did not start output");
            var poisoned=state;poisoned.simulationTicks++;poisoned.headingError=float.NaN;
            output.Update(settings,input,poisoned,7,true,.05);
            Check(broken.sends==1&&broken.stops>=1,"Nonfinite telemetry reached the board instead of stopping it");
        }

        var ambiguous=new Backend();ambiguous.devices.Add(new Idas3WheelFeedback.DeviceChoice{id="second",name="Same model",vendorId=11,productId=22});
        using(var output=new Idas3WheelFeedback(ambiguous)){output.Update(settings,input,state,7,true,0);Check(ambiguous.sends==0,"Ambiguous automatic selection energized a wheel");}
        var noIdentity=new Backend();var unknown=input;unknown.vendorId=0;
        using(var output=new Idas3WheelFeedback(noIdentity)){output.Update(settings,unknown,state,7,true,0);Check(noIdentity.sends==0,"Unidentified input selected the first feedback device");}
        var failure=new Backend{fail=true};using(var output=new Idas3WheelFeedback(failure)){
            output.Update(settings,input,state,7,true,0);Check(failure.sends==1&&failure.stops==1,"Failed driver output not stopped");
            for(int i=1;i<20;++i){state.simulationTicks++;output.Update(settings,input,state,7,true,i*.04);}
            Check(failure.sends==1,"Driver retry backoff was bypassed");
            state.simulationTicks++;output.Update(settings,input,state,7,true,1.1);Check(failure.sends==2,"Driver retry did not recover after its delay");
        }
        var scanFailure=new Backend();using(var output=new Idas3WheelFeedback(scanFailure)){
            output.Update(settings,input,state,7,true,0);Check(scanFailure.sends==1,"Discovery regression did not start output");
            scanFailure.discoveryFailure=true;state.simulationTicks++;output.Update(settings,input,state,7,true,2.1);
            Check(scanFailure.stops==1&&output.Choices.Count==1,"Failed discovery retained a feedback device");
            for(int i=1;i<15;++i){state.simulationTicks++;output.Update(settings,input,state,7,true,2.1+i*.1);Check(scanFailure.sends==1,"Failed discovery resumed stale device output");}
            scanFailure.discoveryFailure=false;state.simulationTicks++;output.Update(settings,input,state,7,true,4.2);
            Check(scanFailure.sends==2,"Successful rediscovery did not recover output");
        }
        var diagnostic=new Backend();using(var output=new Idas3WheelFeedback(diagnostic,true)){
            output.Update(settings,input,state,7,true,0);Check(diagnostic.sends==0,"Automated diagnostic enabled physical output");
        }
        File.WriteAllText(Path.Combine(root,"wheel-feedback-model-report.json"),JsonUtility.ToJson(new Report{
            passed=true,physicalOutputSent=false,checks=checks,scope="Managed wheel owner with a fake output backend: telemetry units, clamps and refusals at the native boundary, lifecycle, stale ticks, device identity, no fallback, retry and diagnostic guards. The force model itself is the ported cabinet board and is covered by original_ffb and original_ffb_owner; no physical wheel verification."},true));
    }
}
