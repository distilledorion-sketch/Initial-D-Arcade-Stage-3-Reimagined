using System;
using UnityEngine;
using Meter=Idas3ArcadeMeterCatalog.Meter;
using Layer=Idas3ArcadeMeterCatalog.Layer;
using Curve=Idas3ArcadeMeterCatalog.Curve;
using Parameter=Idas3ArcadeMeterCatalog.Parameter;
using Switcher=Idas3ArcadeMeterCatalog.Switcher;

public static class Idas3MeterMaterialAnimationChecks
{
    public static void Run()=>Debug.Log(RunChecks());
    static Parameter P(string name,float value)=>new Parameter{name=name,value=value};
    static Curve Curve(string animation,string parameter,float[] times,float[] values,float playbackEnd)=>new Curve{
        animation=animation,parameter=parameter,property="Parameter",times=times,values=values,
        animationStart=times[0],animationEnd=times[times.Length-1],playbackStart=0,playbackEnd=playbackEnd,ticksPerSecond=24000};
    static Layer Led(int index,float rows,float size,float duration)=>new Layer{
        name="LED_"+index,materialParent="/Game/Material/M_Meter75_LedMotion.M_Meter75_LedMotion",
        parameters=new[]{P("Rows",rows),P("Columns",1),P("NumberOfColumnsLEDAnime",35),P("SizeU_280x",size)},
        switchers=new[]{new Switcher{name="LED_Top",index=index,activeIndex=0}},
        curves=new[]{Curve("LEDAnimation0"+index,"AnimationPhaseIndex",new[]{0f,duration*24000},new[]{-35f,35f},duration*24000)}};
    public static string RunChecks(){
        int checks=0;
        void Check(bool ok,string reason){++checks;if(!ok)throw new InvalidOperationException("Meter material animation: "+reason);}
        void Near(float actual,float expected,string reason)=>Check(Math.Abs(actual-expected)<.00001f,reason+" ("+actual+" vs "+expected+")");
        var rotation=new Layer{name="Baseframe",materialParent="/Game/M_MeterRotation.M_MeterRotation",
            parameters=new[]{P("frame01speed",.1f),P("frame02speed",-.04f)},
            curves=new[]{Curve("Anim_Gear_Change_INST","frame01speed",new[]{0f,12000,24000},new[]{.1f,.3f,.1f},24000),
                Curve("Anim_Gear_Change_INST","frame02speed",new[]{0f,12000,24000},new[]{-.04f,-.08f,-.04f},24000)}};
        var led0=Led(0,6,1,2);var led1=Led(1,1,4,3);var led2=Led(2,1,2,4);var led3=Led(3,1,2,5);
        // Deliberately shuffled so sequence must follow switcher indices.
        var meter=new Meter{id=75,layers=new[]{rotation,led2,led0,led3,led1}};
        var telemetry=new Idas3ArcadeHud.Telemetry{flags=1,gear=3};
        var state=new Idas3MeterMaterialAnimation();
        Near(state.Rotation(rotation,"frame01speed",.1f),0,"uninitialized phase");
        Check(!state.TryLed(led0,out _),"uninitialized LED visible");
        state.Update(meter,telemetry,4);
        Near(state.Rotation(rotation,"frame01speed",.1f),.4f,"startup manufactured burst");
        Near(state.Rotation(rotation,"frame02speed",-.04f),.84f,"negative baseline rotation");
        state.Reset();state.Update(meter,telemetry,0);telemetry.gear=4;state.Update(meter,telemetry,1);
        Near(state.Rotation(rotation,"frame01speed",.1f),.1f,"phase jumps at shift start");
        state.Update(meter,telemetry,1.25f);
        Near(state.Rotation(rotation,"frame01speed",.1f),.1375f,"partial speed curve integral");
        state.Update(meter,telemetry,1.5f);
        Near(state.Rotation(rotation,"frame01speed",.1f),.2f,"half speed curve integral");
        state.Update(meter,telemetry,2);
        Near(state.Rotation(rotation,"frame01speed",.1f),.3f,"full speed curve integral");
        Near(state.Rotation(rotation,"frame02speed",-.04f),.9f,"negative speed curve integral");
        state.Update(meter,telemetry,2.0001f);
        Near(state.Rotation(rotation,"frame01speed",.1f),.30001f,"phase resets after shift");
        telemetry.gear=5;state.Update(meter,telemetry,2.5f);
        Near(state.Rotation(rotation,"frame01speed",.1f),.35f,"second shift loses prior phase");
        state.Update(meter,telemetry,2.75f);
        Near(state.Rotation(rotation,"frame01speed",.1f),.3875f,"second partial burst");
        telemetry.gear=4;state.Update(meter,telemetry,3);
        Near(state.Rotation(rotation,"frame01speed",.1f),.45f,"overlapping shift loses partial integral");
        state.Update(meter,telemetry,4);
        Near(state.Rotation(rotation,"frame01speed",.1f),.65f,"successive shifts phase");
        foreach(int fps in new[]{30,60,144}){
            var sampled=new Idas3MeterMaterialAnimation();telemetry.gear=3;sampled.Update(meter,telemetry,0);
            for(int frame=1;frame<=4*fps;++frame){
                telemetry.gear=frame>=3*fps?4:frame>=5*fps/2?5:frame>=fps?4:3;
                sampled.Update(meter,telemetry,frame/(float)fps);
            }
            Near(sampled.Rotation(rotation,"frame01speed",.1f),.65f,"rotation depends on "+fps+" FPS");
            Check(sampled.TryLed(led1,out var sampledUv),"LED selection differs at "+fps+" FPS");
            Near(sampledUv.x,1f/12,"LED scroll differs at "+fps+" FPS");
        }
        telemetry.gear=3;state.Reset();state.Update(meter,telemetry,1);
        telemetry.gear=4;state.Update(meter,telemetry,1);state.Update(meter,telemetry,1.5f);
        Near(state.Rotation(rotation,"frame01speed",.1f),.15f,"frozen-time gear change starts burst");
        telemetry.gear=5;state.Update(meter,telemetry,2);state.Update(meter,telemetry,1.5f);
        Near(state.Rotation(rotation,"frame01speed",.1f),.15f,"rewind retains burst history");
        state.Update(new Meter{id=75,layers=meter.layers},telemetry,2);
        Near(state.Rotation(rotation,"frame01speed",.1f),.2f,"meter change retains burst history");
        telemetry.flags=0;state.Update(meter,telemetry,2.5f);
        Near(state.Rotation(rotation,"frame01speed",.1f),0,"invalid telemetry retains phase");
        telemetry.flags=1;state.Update(meter,telemetry,3);
        Near(state.Rotation(rotation,"frame01speed",.1f),.3f,"recovered telemetry manufactures burst");
        state.Update(meter,telemetry,float.NaN);
        Check(!state.TryLed(led0,out _),"nonfinite timestamp keeps LED active");

        state.Update(meter,telemetry,0);
        Check(state.TryLed(led0,out var uv),"playlist does not start with switcher0");
        Near(uv.x,-1,"penguin entry offset");Near(uv.width,1,"penguin frame width");Near(uv.height,1f/6,"penguin atlas cropped incorrectly");
        Near(uv.y,5f/6,"penguin starts on first atlas row");
        foreach(var inactive in new[]{led1,led2,led3})Check(!state.TryLed(inactive,out _),"multiple LED programs visible");
        state.Update(meter,telemetry,.25f);Check(state.TryLed(led0,out uv),"penguin early frame missing");Near(uv.y,4f/6,"penguin does not advance atlas frames");
        state.Update(meter,telemetry,.9f);state.TryLed(led0,out uv);Near(uv.y,0,"penguin final atlas row unavailable");
        state.Update(meter,telemetry,1.01f);state.TryLed(led0,out uv);Near(uv.y,5f/6,"penguin atlas fails to wrap");
        state.Update(meter,telemetry,2);Check(state.TryLed(led1,out uv),"second program not selected");Near(uv.width,.25f,"CHUNITHM strip squeezed instead of viewport crop");Near(uv.x,-.25f,"CHUNITHM entry offset");
        state.Update(meter,telemetry,5);Check(state.TryLed(led2,out uv),"third program not selected");Near(uv.width,.5f,"NEW strip crop");
        state.Update(meter,telemetry,9);Check(state.TryLed(led3,out uv),"fourth program not selected");Near(uv.width,.5f,"SEGA strip crop");
        state.Update(meter,telemetry,14);Check(state.TryLed(led0,out uv),"playlist fails to repeat");Near(uv.x,-1,"playlist repeat does not reset source phase");
        var fresh=new Idas3MeterMaterialAnimation();fresh.Update(meter,telemetry,14);
        Check(fresh.TryLed(led0,out var freshUv),"fresh renderer has different playlist");Near(freshUv.x,uv.x,"playlist depends on renderer lifetime");
        Check(!Idas3MeterMaterialAnimation.IsLed(rotation),"rotation layer mistaken for LED");
        Check(!state.TryLed(rotation,out _),"non-LED layer claims LED UV");
        return "Meter material animation checks passed: "+checks;
    }
}
