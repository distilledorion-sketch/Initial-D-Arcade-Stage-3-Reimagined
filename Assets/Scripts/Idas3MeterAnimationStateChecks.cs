using System;
using UnityEngine;
using Meter=Idas3ArcadeMeterCatalog.Meter;
using Curve=Idas3ArcadeMeterCatalog.Curve;

public static class Idas3MeterAnimationStateChecks
{
    public static void Run()=>Debug.Log(RunChecks());
    public static string RunChecks(){
        int checks=0;
        void Check(bool value,string message){++checks;if(!value)throw new InvalidOperationException("Meter animation state: "+message);}
        void Near(float actual,float expected,string message)=>Check(Math.Abs(actual-expected)<.00002f,message);
        var meter=new Meter{id=66};
        var other=new Meter{id=75};
        var curve=new Curve{animation="Anim_Gear_Change_INST",animationStart=0,animationEnd=24000,
            playbackStart=0,playbackEnd=36000,ticksPerSecond=24000};
        var telemetry=new Idas3ArcadeHud.Telemetry{size=40,version=2,flags=1,gear=3};
        var state=new Idas3MeterAnimationState();
        Check(!state.TryProgress(curve,out _),"uninitialized renderer emits a pulse");
        state.Update(meter,telemetry,8);
        Check(!state.TryProgress(curve,out _),"initial telemetry emits a pulse");
        state.Update(meter,telemetry,9);
        Check(!state.TryProgress(curve,out _),"unchanged gear emits a pulse");
        telemetry.gear=4;state.Update(meter,telemetry,10);
        Check(state.TryProgress(curve,out var progress),"gear transition does not start a pulse");Near(progress,0,"gear transition has wrong initial phase");
        state.Update(meter,telemetry,10.5f);
        Check(state.TryProgress(curve,out progress),"gear pulse ended early");Near(progress,.5f,"pulse does not follow elapsed seconds");
        state.Update(meter,telemetry,11.25f);
        Check(state.TryProgress(curve,out progress),"authored playback tail was discarded");Near(progress,1,"playback tail does not hold final key");
        state.Update(meter,telemetry,11.5f);
        Check(!state.TryProgress(curve,out _),"pulse persists beyond exclusive playback end");
        telemetry.gear=3;state.Update(meter,telemetry,12);
        Check(state.TryProgress(curve,out progress),"downshift does not start a pulse");Near(progress,0,"downshift failed to restart");
        telemetry.gear=2;state.Update(meter,telemetry,12.2f);
        Check(state.TryProgress(curve,out progress),"successive gear change lost");Near(progress,0,"successive change does not restart");
        state.Update(other,telemetry,12.3f);
        Check(!state.TryProgress(curve,out _),"meter change manufactures a pulse");
        telemetry.gear=3;state.Update(other,telemetry,13);
        Check(state.TryProgress(curve,out _),"new meter cannot observe later changes");
        telemetry.gear=4;state.Update(other,telemetry,12);
        Check(!state.TryProgress(curve,out _),"time rewind manufactures a pulse");
        telemetry.gear=5;state.Update(other,telemetry,13);
        telemetry.flags=0;state.Update(other,telemetry,13.1f);
        Check(!state.TryProgress(curve,out _),"telemetry loss retains pulse");
        telemetry.flags=1;telemetry.gear=6;state.Update(other,telemetry,13.2f);
        Check(!state.TryProgress(curve,out _),"telemetry recovery manufactures pulse");
        telemetry.gear=5;state.Update(other,telemetry,14);
        state.Update(other,telemetry,float.NaN);
        Check(!state.TryProgress(curve,out _),"nonfinite time retains pulse");
        state.Update(other,telemetry,15);telemetry.gear=4;state.Update(other,telemetry,16);
        state.Update(other,telemetry,float.PositiveInfinity);
        Check(!state.TryProgress(curve,out _),"infinite time retains pulse");
        state.Update(other,telemetry,17);telemetry.gear=3;state.Update(other,telemetry,18);
        telemetry.gear=-1;state.Update(other,telemetry,18.1f);
        Check(!state.TryProgress(curve,out _),"invalid gear retains pulse");
        telemetry.gear=3;state.Update(meter,telemetry,19);telemetry.gear=4;state.Update(meter,telemetry,20);
        state.Update(null,telemetry,20.1f);
        Check(!state.TryProgress(curve,out _),"missing meter retains pulse");
        state.Update(meter,telemetry,21);telemetry.gear=3;state.Update(meter,telemetry,22);state.Reset();
        Check(!state.TryProgress(curve,out _),"explicit reset retains pulse");
        telemetry.gear=3;state.Update(meter,telemetry,23);
        telemetry.gear=4;state.Update(meter,telemetry,23);
        Check(!state.TryProgress(curve,out _),"frozen clock starts a gear event");
        state.Update(meter,telemetry,24);
        Check(!state.TryProgress(curve,out _),"resuming clock invents a deferred gear event");

        Near(Idas3MeterAnimationState.DurationSeconds(curve),1.5f,"duration does not use playback range");
        var fallback=new Curve{animation="Anim_Gear_Change_INST",animationStart=0,animationEnd=12000,ticksPerSecond=0};
        Near(Idas3MeterAnimationState.DurationSeconds(fallback),.5f,"legacy curve fallback duration is wrong");
        Near(Idas3MeterAnimationState.DurationSeconds(null),0,"null duration is nonzero");
        foreach(int fps in new[]{30,60,144}){
            var frameState=new Idas3MeterAnimationState();telemetry.gear=2;frameState.Update(meter,telemetry,0);
            for(int frame=1;frame<fps;frame++)frameState.Update(meter,telemetry,frame/(float)fps);
            telemetry.gear=3;frameState.Update(meter,telemetry,1);
            for(int frame=1;frame<=fps/2;frame++)frameState.Update(meter,telemetry,1+frame/(float)fps);
            Check(frameState.TryProgress(curve,out progress),"pulse missing at "+fps+" FPS");Near(progress,.5f,"frame-count dependent phase at "+fps+" FPS");
        }
        var loop=new Curve{animation="LEDAnimation00_Pengin",animationStart=0,animationEnd=240000,
            playbackStart=0,playbackEnd=300000,ticksPerSecond=24000};
        state.Update(meter,telemetry,26);
        Check(state.TryProgress(loop,out progress),"source loop missing");Near(progress,.1f,"loop ignores its own source duration");
        var fresh=new Idas3MeterAnimationState();fresh.Update(meter,telemetry,26);
        Check(fresh.TryProgress(loop,out var freshProgress),"fresh ambient loop missing");Near(progress,freshProgress,"ambient phase depends on renderer lifetime");
        state.Update(meter,telemetry,36);
        Check(state.TryProgress(loop,out progress),"loop playback tail missing");Near(progress,1,"loop playback tail failed to hold final key");
        Check(!state.TryProgress(new Curve{animation="Anim_AccelPin_INST"},out _),"helper overrides telemetry-driven curves");
        Check(!state.TryProgress(new Curve{animation="Anim_Gear_Change_INST",animationEnd=float.NaN},out _),"invalid curve emits progress");
        return "Meter animation state checks passed: "+checks;
    }
}
