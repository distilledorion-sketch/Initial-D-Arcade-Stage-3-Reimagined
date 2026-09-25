using System;
using UnityEngine;
using Meter=Idas3ArcadeMeterCatalog.Meter;
using Layer=Idas3ArcadeMeterCatalog.Layer;
using Curve=Idas3ArcadeMeterCatalog.Curve;

public static class Idas3MeterNeedleTrailChecks
{
    public static void Run()=>Debug.Log(RunChecks());
    static Curve Rotation(string name,float from,float to)=>new Curve{
        animation=name,property="Rotation",animationStart=0,animationEnd=60000,
        times=new[]{0f,60000},values=new[]{from,to}};
    static Meter Meter(){
        var layers=new Layer[22];
        layers[0]=new Layer{name="CenterPin",curves=new[]{Rotation("Anim_CenterPin_INST",-149,90)}};
        layers[1]=new Layer{name="LeftPin",curves=new[]{Rotation("Anim_LeftPin_INST",-225,15)}};
        for(int i=0;i<10;++i){layers[2+i]=new Layer{name="CenterPinTrail"+(i+1).ToString("00"),angle=90,ownOpacity=.1f};
            layers[12+i]=new Layer{name="LeftPinTrail"+(i+1).ToString("00"),angle=-225,ownOpacity=.5f};}
        return new Meter{id=3,layers=layers};
    }
    static Idas3ArcadeHud.Telemetry Data(float seconds)=>new Idas3ArcadeHud.Telemetry{
        flags=1,revLimit=8000,rpm=2000+1200*seconds,speedKmh=60+24*seconds};
    static float Rpm(float value,int max=8000)=>-149+239*Math.Max(0,Math.Min(1,value/max));
    static float Speed(float value)=>-225+Math.Max(0,Math.Min(1,value/240))*240;
    public static string RunChecks(){
        int checks=0;
        void Check(bool ok,string reason){++checks;if(!ok)throw new InvalidOperationException("Needle trail: "+reason);}
        void Near(float actual,float expected,string reason)=>Check(Math.Abs(actual-expected)<.0001f,reason+" ("+actual+" vs "+expected+")");
        var meter=Meter();var state=new Idas3MeterNeedleTrails();
        Check(!state.TryRotation(meter.layers[2],out _),"uninitialized trail angle available");
        state.Update(meter,Data(0),0);
        for(int i=0;i<10;++i){
            Check(state.TryRotation(meter.layers[2+i],out var rpm),"initial RPM trail missing");Near(rpm,Rpm(2000),"initial RPM trail keeps serialized90 degrees");
            Check(state.TryRotation(meter.layers[12+i],out var speed),"initial speed trail missing");Near(speed,Speed(60),"initial speed trail keeps serialized angle");
        }
        foreach(int fps in new[]{30,60,144}){
            state.Reset();state.Update(meter,Data(0),0);
            for(int frame=1;frame<=fps;++frame){float now=frame/(float)fps;state.Update(meter,Data(now),now);}
            for(int i=0;i<10;++i){
                float prior=1-i/60f;
                Check(state.TryRotation(meter.layers[2+i],out var rpm),"RPM trail unavailable at "+fps+" FPS");Near(rpm,Rpm(Data(prior).rpm),"RPM history depends on "+fps+" FPS");
                Check(state.TryRotation(meter.layers[12+i],out var speed),"speed trail unavailable at "+fps+" FPS");Near(speed,Speed(Data(prior).speedKmh),"speed history depends on "+fps+" FPS");
            }
        }
        state.TryRotation(meter.layers[11],out var beforeFreeze);
        for(int i=0;i<20;++i)state.Update(meter,Data(1),1);
        state.TryRotation(meter.layers[11],out var afterFreeze);Near(afterFreeze,beforeFreeze,"same-time samples age history");
        var replacement=Data(1);replacement.rpm=4800;replacement.speedKmh=120;state.Update(meter,replacement,1);
        for(int i=0;i<10;++i){state.TryRotation(meter.layers[2+i],out var rpm);Near(rpm,Rpm(4800),"replacement sample retains stale RPM history");
            state.TryRotation(meter.layers[12+i],out var speed);Near(speed,Speed(120),"replacement sample retains stale speed history");}
        state.Update(meter,Data(.25f),.25f);
        state.TryRotation(meter.layers[11],out var rewind);Near(rewind,Rpm(Data(.25f).rpm),"rewind retains stale trail");
        state.Update(meter,Data(3),3);
        state.TryRotation(meter.layers[11],out var gap);Near(gap,Rpm(Data(3).rpm),"long sampling gap invents old history");
        var changedScale=Data(3);changedScale.revLimit=8500;state.Update(meter,changedScale,3.01f);
        state.TryRotation(meter.layers[11],out var scale);Near(scale,Rpm(changedScale.rpm,9000),"tach range change retains old scale history");
        var invalid=Data(4);invalid.flags=0;state.Update(meter,invalid,4);
        Check(!state.TryRotation(meter.layers[2],out _),"telemetry loss retains history");
        state.Update(meter,Data(4),4);
        state.TryRotation(meter.layers[11],out var restored);Near(restored,Rpm(Data(4).rpm),"telemetry recovery retains old history");
        var other=Meter();state.Update(other,Data(0),4.01f);
        state.TryRotation(other.layers[11],out var switched);Near(switched,Rpm(2000),"style switch retains another meter history");
        Check(!state.TryRotation(meter.layers[11],out _),"old meter layer retains history binding");
        Check(!state.TryRotation(other.layers[0],out _),"main needle mistaken for a trail");
        Check(!state.TryRotation(new Layer{name="CenterPinTrail"},out _),"unnumbered material trail rebound");
        Check(!state.TryRotation(new Layer{name="CenterPinTrail00"},out _),"zero suffix accepted");
        Check(!state.TryRotation(new Layer{name="CenterPinTrail11"},out _),"out-of-range suffix accepted");
        state.Update(other,Data(0),float.NaN);Check(!state.TryRotation(other.layers[2],out _),"nonfinite clock keeps history");
        state.Update(other,Data(0),0);state.Update(other,Data(0),float.PositiveInfinity);
        Check(!state.TryRotation(other.layers[2],out _),"infinite clock keeps history");
        state.Update(other,Data(0),0);state.Update(null,Data(0),0);Check(!state.TryRotation(other.layers[2],out _),"missing meter keeps history");
        for(int i=0;i<10;++i){Near(meter.layers[2+i].ownOpacity,.1f,"RPM source opacity changed");Near(meter.layers[12+i].ownOpacity,.5f,"speed source opacity changed");}
        return "Needle trail checks passed: "+checks;
    }
}
