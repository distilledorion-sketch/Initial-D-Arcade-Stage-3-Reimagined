using System;
using UnityEngine;
using Meter=Idas3ArcadeMeterCatalog.Meter;
using Curve=Idas3ArcadeMeterCatalog.Curve;
using Owner=Idas3ArcadeMeterCatalog.Owner;

public static class Idas3HalloweenLanternChecks
{
    public static void Run()=>Debug.Log(RunChecks());
    public static string RunChecks(){
        int checks=0;
        void Check(bool value,string message){++checks;if(!value)throw new InvalidOperationException("Halloween lantern: "+message);}
        void Near(float actual,float expected,string message)=>Check(Math.Abs(actual-expected)<.0001f,message);
        var curve=new Curve{animation="Anim_DriftLamp_InOut_INST",property="Rotation",owner=new Owner{name="Cantera"},
            animationStart=0,animationEnd=30000,playbackStart=0,playbackEnd=30000,ticksPerSecond=24000,
            times=new[]{0f,10000,20000,30000},values=new[]{0f,5,-2.5f,0}};
        var meter=new Meter{id=83};var state=new Idas3HalloweenLanternAnimation();
        var input=new Idas3ArcadeHud.Telemetry{version=2,flags=9,driftOpacity=1};
        state.Update(meter,input,0);Check(!state.TryProgress(curve,out _),"initial drift creates an event");
        input.flags=1;state.Update(meter,input,1);Check(state.TryProgress(curve,out var p),"drift exit does not play");Near(p,1,"exit is not reversed");
        state.Update(meter,input,1.5f);Check(state.TryProgress(curve,out p),"exit ends too early");Near(p,.6f,"exit ignores elapsed seconds");
        state.Update(meter,input,2.25f);Check(!state.TryProgress(curve,out _),"exit persists past source duration");
        input.flags=9;state.Update(meter,input,3);Check(state.TryProgress(curve,out p),"entry missing");Near(p,0,"entry is not forward");
        state.Update(meter,input,3.5f);Check(state.TryProgress(curve,out p),"entry ends too early");Near(p,.4f,"entry phase wrong");
        float angle=Idas3ImportedMeter.Evaluate(curve,p);Check(Math.Abs(angle)>1,"authored swing has no visible angle");
        for(int i=0;i<200;++i)state.Update(meter,input,3.5f);
        Check(state.TryProgress(curve,out p),"frozen clock lost active motion");Near(p,.4f,"frozen clock advanced motion");
        input.driftOpacity=0;state.Update(meter,input,3.6f);Check(state.TryProgress(curve,out p),"opacity was mistaken for a drift event");Near(p,.48f,"opacity change restarted motion");
        input.flags=1;state.Update(meter,input,3.6f);Check(!state.TryProgress(curve,out _),"same-time correction invented an exit");
        input.flags=9;state.Update(meter,input,4);state.Update(meter,input,3);Check(!state.TryProgress(curve,out _),"clock rewind retained swing");
        input.flags=1;state.Update(meter,input,5);input.flags=0;state.Update(meter,input,5.1f);Check(!state.TryProgress(curve,out _),"telemetry loss retained motion");
        input.flags=9;state.Update(meter,input,5.2f);Check(!state.TryProgress(curve,out _),"telemetry recovery invented entry");
        input.flags=1;state.Update(meter,input,6);state.Update(new Meter{id=83},input,6.1f);Check(!state.TryProgress(curve,out _),"style replacement retained motion");
        state.Update(meter,input,7);input.flags=9;state.Update(meter,input,8);state.Update(meter,input,float.NaN);Check(!state.TryProgress(curve,out _),"invalid clock retained motion");
        state.Update(meter,input,9);input.flags=1;state.Update(meter,input,10);state.Reset();Check(!state.TryProgress(curve,out _),"Reset retained motion");
        state.Update(new Meter{id=82},input,11);input.flags=9;state.Update(new Meter{id=82},input,12);Check(!state.TryProgress(curve,out _),"non-Halloween meter animated");
        foreach(int fps in new[]{30,60,144}){
            state.Reset();input.flags=1;state.Update(meter,input,0);input.flags=9;state.Update(meter,input,1);
            for(int i=1;i<=fps/2;++i)state.Update(meter,input,1+i/(float)fps);
            Check(state.TryProgress(curve,out p),"entry absent at "+fps+" Hz");Near(p,.4f,"frame dependent entry at "+fps+" Hz");
            input.flags=1;state.Update(meter,input,3);
            for(int i=1;i<=fps/2;++i)state.Update(meter,input,3+i/(float)fps);
            Check(state.TryProgress(curve,out p),"exit absent at "+fps+" Hz");Near(p,.6f,"frame dependent exit at "+fps+" Hz");
        }
        var unrelated=new Curve{animation="Anim_DriftLamp_Blink_INST",property="Rotation",owner=new Owner{name="Cantera"}};
        Check(!state.TryProgress(unrelated,out _),"unavailable source drift grade was guessed");
        unrelated.animation=curve.animation;unrelated.owner.name="Other";
        Check(!state.TryProgress(unrelated,out _),"another owner was animated");
        unrelated.owner.name="Cantera";unrelated.property="RenderOpacity";
        Check(!state.TryProgress(unrelated,out _),"helper overrode native lamp alpha");
        return "Halloween lantern checks passed: "+checks;
    }
}
