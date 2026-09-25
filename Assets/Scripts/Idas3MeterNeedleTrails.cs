using System;
using System.Collections.Generic;
using Meter=Idas3ArcadeMeterCatalog.Meter;
using Layer=Idas3ArcadeMeterCatalog.Layer;
using Curve=Idas3ArcadeMeterCatalog.Curve;

// Future's numbered trail images are populated by the source RPM/speed angle
// history functions, not independent needles. Resample observed angles at a
// fixed 60 Hz so their ten history slots do not change duration with render FPS.
// This cadence is a reconstruction; the cooked source does not retain a rate.
internal sealed class Idas3MeterNeedleTrails
{
    const int Capacity=16,TrailCount=10;
    const double SamplesPerSecond=60;
    const double HistoryWindow=TrailCount/SamplesPerSecond;
    struct Trail {internal bool rpm;internal int index;}
    struct Sample {internal double time;internal float rpm,speed;}
    readonly Dictionary<Layer,Trail> trails=new Dictionary<Layer,Trail>();
    readonly Sample[] history=new Sample[Capacity];
    Meter meter;
    Curve rpmCurve,speedCurve;
    bool ready;
    int newest=-1,count,tachMaximum;
    double seconds,nextTick;
    float rpmAngle,speedAngle;

    static bool Finite(double value)=>!double.IsNaN(value)&&!double.IsInfinity(value);
    static float Safe(float value)=>float.IsNaN(value)||float.IsInfinity(value)?0:value;
    static float Clamp01(float value)=>Math.Max(0,Math.Min(1,value));
    static bool Contains(string value,string part)=>value!=null&&value.IndexOf(part,StringComparison.OrdinalIgnoreCase)>=0;
    static Curve Rotation(Layer layer,string animation){
        if(layer?.curves!=null)foreach(var curve in layer.curves)
            if(curve!=null&&curve.property=="Rotation"&&Contains(curve.animation,animation)&&
                (curve.owner==null||string.IsNullOrEmpty(curve.owner.name)||curve.owner.name==layer.name))return curve;
        return null;
    }
    static bool TrailIndex(string name,string prefix,out int index){
        index=0;
        if(name==null||name.Length!=prefix.Length+2||!name.StartsWith(prefix,StringComparison.Ordinal))return false;
        int a=name[prefix.Length]-'0',b=name[prefix.Length+1]-'0';
        if(a<0||a>9||b<0||b>9)return false;
        index=a*10+b-1;return index>=0&&index<TrailCount;
    }
    internal void Reset(){
        trails.Clear();meter=null;rpmCurve=null;speedCurve=null;ready=false;
        newest=-1;count=0;tachMaximum=0;seconds=0;nextTick=0;rpmAngle=0;speedAngle=0;
    }
    void Configure(Meter next){
        Reset();meter=next;
        if(next?.layers==null)return;
        foreach(var layer in next.layers){
            if(layer==null)continue;
            if(layer.name=="CenterPin")rpmCurve=Rotation(layer,"CenterPin");
            else if(layer.name=="LeftPin")speedCurve=Rotation(layer,"LeftPin");
        }
        foreach(var layer in next.layers){
            if(layer==null)continue;
            if(rpmCurve!=null&&TrailIndex(layer.name,"CenterPinTrail",out int rpmIndex))trails[layer]=new Trail{rpm=true,index=rpmIndex};
            else if(speedCurve!=null&&TrailIndex(layer.name,"LeftPinTrail",out int speedIndex))trails[layer]=new Trail{rpm=false,index=speedIndex};
        }
    }
    void Push(double time,float rpm,float speed){
        newest=(newest+1)%Capacity;history[newest]=new Sample{time=time,rpm=rpm,speed=speed};count=Math.Min(Capacity,count+1);
    }
    void Seed(double now,float rpm,float speed,int maximum){
        newest=-1;count=0;
        double tick=Math.Floor(now*SamplesPerSecond);
        for(int i=Capacity-1;i>=0;--i)Push((tick-i)/SamplesPerSecond,rpm,speed);
        seconds=now;nextTick=tick+1;rpmAngle=rpm;speedAngle=speed;tachMaximum=maximum;ready=true;
    }
    internal void Update(Meter next,Idas3ArcadeHud.Telemetry telemetry,float now){
        if(next==null||(telemetry.flags&1)==0||!Finite(now)){Reset();return;}
        if(!ReferenceEquals(meter,next))Configure(next);
        if(trails.Count==0)return;
        int maximum=Idas3ArcadeHud.TachMaximum(telemetry.revLimit);
        float rpm=rpmCurve==null?0:Idas3ImportedMeter.Evaluate(rpmCurve,Clamp01(Safe(telemetry.rpm)/maximum));
        float speed=speedCurve==null?0:Idas3ImportedMeter.Evaluate(speedCurve,Clamp01(Safe(telemetry.speedKmh)/240));
        // Missing an entire trail lifespan, changing scale, or seeking cannot
        // provide historical observations. Start every slot at the current pin.
        if(!ready||now<seconds||maximum!=tachMaximum||now-seconds>=HistoryWindow){Seed(now,rpm,speed,maximum);return;}
        if(now==seconds){
            // Paused clocks do not age trails. Changed same-time telemetry is
            // a replacement sample (also used by deterministic renderer QA).
            if(rpm!=rpmAngle||speed!=speedAngle)Seed(now,rpm,speed,maximum);
            return;
        }
        while(nextTick/SamplesPerSecond<=now){
            double sampleTime=nextTick/SamplesPerSecond;
            float fraction=(float)Math.Max(0,Math.Min(1,(sampleTime-seconds)/(now-seconds)));
            Push(sampleTime,rpmAngle+(rpm-rpmAngle)*fraction,speedAngle+(speed-speedAngle)*fraction);++nextTick;
        }
        seconds=now;rpmAngle=rpm;speedAngle=speed;
    }
    float Historical(double when,bool rpm){
        if(when>=seconds)return rpm?rpmAngle:speedAngle;
        Sample after=new Sample{time=seconds,rpm=rpmAngle,speed=speedAngle};
        for(int age=0;age<count;++age){
            var before=history[(newest-age+Capacity)%Capacity];
            if(before.time<=when){
                float a=rpm?before.rpm:before.speed,b=rpm?after.rpm:after.speed;
                float fraction=after.time>before.time?(float)Math.Max(0,Math.Min(1,(when-before.time)/(after.time-before.time))):0;
                return a+(b-a)*fraction;
            }
            after=before;
        }
        return rpm?after.rpm:after.speed;
    }
    internal bool TryRotation(Layer layer,out float angle){
        angle=0;
        if(!ready||layer==null||!trails.TryGetValue(layer,out var trail))return false;
        angle=Historical(seconds-trail.index/SamplesPerSecond,trail.rpm);return true;
    }
}
