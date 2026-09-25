using System;
using System.Collections.Generic;
using UnityEngine;
using Meter=Idas3ArcadeMeterCatalog.Meter;
using Layer=Idas3ArcadeMeterCatalog.Layer;
using Curve=Idas3ArcadeMeterCatalog.Curve;

// Reconstructs material motion from the retained textures, scalar parameters
// and MovieScene keys. The stripped LED graph does not retain its atlas clock:
// the penguin uses one six-frame atlas cycle per second. LED programs play in
// serialized switcher order; each program keeps its authored playback range.
internal sealed class Idas3MeterMaterialAnimation
{
    sealed class RotationChannel {
        internal string parameter;
        internal double speed,completedBurst;
        internal Curve curve;
    }
    sealed class LedClip {
        internal Layer layer;
        internal Curve curve;
        internal int index;
        internal double duration;
    }
    readonly Dictionary<Layer,RotationChannel[]> rotations=new Dictionary<Layer,RotationChannel[]>();
    readonly List<LedClip> leds=new List<LedClip>();
    Meter meter;
    bool ready,gearEvent;
    int gear;
    double seconds,gearStarted,ledDuration;

    static bool Finite(double value)=>!double.IsNaN(value)&&!double.IsInfinity(value);
    static bool Contains(string value,string part)=>value!=null&&value.IndexOf(part,StringComparison.OrdinalIgnoreCase)>=0;
    static double Safe(double value,double fallback=0)=>Finite(value)?value:fallback;
    static double Wrap(double value,double period)=>value-Math.Floor(value/period)*period;
    static float Scalar(Layer layer,string parameter,float fallback){
        if(layer?.parameters!=null)foreach(var value in layer.parameters)
            if(value.name==parameter)return (float)Safe(value.value,fallback);
        return fallback;
    }
    static void Range(Curve curve,out double begin,out double end,out double rate){
        begin=curve.playbackStart;end=curve.playbackEnd;
        if(!Finite(begin)||!Finite(end)||end<=begin){begin=curve.animationStart;end=curve.animationEnd;}
        rate=Finite(curve.ticksPerSecond)&&curve.ticksPerSecond>0?curve.ticksPerSecond:24000;
    }
    static Curve ParameterCurve(Layer layer,string parameter,string animation){
        Curve result=null;
        if(layer.curves!=null)foreach(var curve in layer.curves)
            if(curve!=null&&curve.parameter==parameter&&Contains(curve.animation,animation)&&
                (curve.owner==null||string.IsNullOrEmpty(curve.owner.name)||curve.owner.name==layer.name))result=curve;
        return result;
    }
    internal static bool IsLed(Layer layer)=>layer!=null&&Contains(layer.materialParent,"M_Meter75_LedMotion");
    internal void Reset(){
        rotations.Clear();leds.Clear();meter=null;ready=false;gearEvent=false;
        gear=0;seconds=0;gearStarted=0;ledDuration=0;
    }
    void Initialize(Meter next,int nextGear,double now){
        Reset();meter=next;gear=nextGear;seconds=now;ready=true;
        if(meter.layers==null)return;
        foreach(var layer in meter.layers){
            if(layer==null)continue;
            if(Contains(layer.materialParent,"MeterRotation")||Contains(layer.materialParent,"EffRotation")){
                var channels=new RotationChannel[2];
                for(int i=0;i<channels.Length;++i){
                    string parameter=i==0?"frame01speed":"frame02speed";
                    channels[i]=new RotationChannel{parameter=parameter,speed=Scalar(layer,parameter,Scalar(layer,"frame01speed",0)),
                        curve=ParameterCurve(layer,parameter,"Gear_Change")};
                }
                rotations.Add(layer,channels);
            }
            if(IsLed(layer)){
                var curve=ParameterCurve(layer,"AnimationPhaseIndex","LED");
                double duration=Idas3MeterAnimationState.DurationSeconds(curve);
                if(curve==null||!Finite(duration)||duration<=0)continue;
                int index=leds.Count;
                if(layer.switchers!=null)foreach(var choice in layer.switchers)if(choice.name=="LED_Top"){index=choice.index;break;}
                leds.Add(new LedClip{layer=layer,curve=curve,index=index,duration=duration});
            }
        }
        leds.Sort((a,b)=>a.index.CompareTo(b.index));
        foreach(var clip in leds)ledDuration+=clip.duration;
    }
    internal void Update(Meter next,Idas3ArcadeHud.Telemetry telemetry,float now){
        bool valid=next!=null&&(telemetry.flags&1)!=0&&telemetry.gear>=0&&telemetry.gear<=6&&Finite(now);
        if(!valid){Reset();return;}
        if(!ready||!ReferenceEquals(meter,next)||now<seconds){Initialize(next,telemetry.gear,now);return;}
        if(gear!=telemetry.gear){
            // Match the event observer: gear changes at a frozen presentation
            // timestamp establish a baseline, not a deferred shift pulse.
            if(now>seconds){
                if(gearEvent)foreach(var entry in rotations)foreach(var channel in entry.Value)
                    channel.completedBurst+=BurstIntegral(channel.curve,channel.speed,now-gearStarted);
                gearStarted=now;gearEvent=true;
            }
            gear=telemetry.gear;
        }
        seconds=now;
    }
    // Integrate the piecewise-linear source speed exactly, then subtract its
    // baseline contribution. Sampling more display frames cannot change phase.
    static double BurstIntegral(Curve curve,double baseline,double elapsed){
        if(curve==null||!Finite(elapsed)||elapsed<=0)return 0;
        Range(curve,out var begin,out var end,out var rate);
        if(!Finite(begin)||!Finite(end)||end<=begin)return 0;
        end=Math.Min(end,begin+elapsed*rate);
        if(end<=begin)return 0;
        int count=Math.Min(curve.times?.Length??0,curve.values?.Length??0);
        if(count==0)return (Safe(curve.defaultValue,baseline)-baseline)*(end-begin)/rate;
        for(int i=0;i<count;++i)if(!Finite(curve.times[i])||!Finite(curve.values[i])||(i>0&&curve.times[i]<curve.times[i-1]))return 0;
        double integral=0,first=curve.times[0],last=curve.times[count-1];
        if(begin<first)integral+=(Math.Min(end,first)-begin)*curve.values[0];
        for(int i=0;i<count-1;++i){
            double a=curve.times[i],b=curve.times[i+1];
            double left=Math.Max(begin,a),right=Math.Min(end,b);
            if(right<=left||b<=a)continue;
            double slope=(curve.values[i+1]-curve.values[i])/(b-a);
            double leftValue=curve.values[i]+(left-a)*slope,rightValue=curve.values[i]+(right-a)*slope;
            integral+=(leftValue+rightValue)*.5*(right-left);
        }
        if(end>last)integral+=(end-Math.Max(begin,last))*curve.values[count-1];
        return (integral-baseline*(end-begin))/rate;
    }
    internal float Rotation(Layer layer,string parameter,float baseSpeed){
        if(!ready)return 0;
        double phase=seconds*Safe(baseSpeed);
        if(layer!=null&&rotations.TryGetValue(layer,out var channels))foreach(var channel in channels)
            if(channel.parameter==parameter){
                phase=seconds*channel.speed+channel.completedBurst;
                if(gearEvent)phase+=BurstIntegral(channel.curve,channel.speed,seconds-gearStarted);
                break;
            }
        return (float)Wrap(Safe(phase),1);
    }
    static double AtTick(Curve curve,double tick){
        int count=Math.Min(curve.times?.Length??0,curve.values?.Length??0);
        if(count==0)return Safe(curve.defaultValue);
        if(tick<=curve.times[0])return Safe(curve.values[0]);
        for(int i=1;i<count;++i)if(tick<=curve.times[i]){
            double a=curve.times[i-1],b=curve.times[i];
            double fraction=b>a?Math.Max(0,Math.Min(1,(tick-a)/(b-a))):1;
            return Safe(curve.values[i-1]+(curve.values[i]-curve.values[i-1])*fraction);
        }
        return Safe(curve.values[count-1]);
    }
    internal bool TryLed(Layer layer,out Rect uv){
        uv=new Rect(0,0,1,1);
        if(!ready||!IsLed(layer)||!Finite(ledDuration)||ledDuration<=0)return false;
        double elapsed=Wrap(seconds,ledDuration);
        LedClip selected=null;
        foreach(var clip in leds){
            if(elapsed<clip.duration){selected=clip;break;}
            elapsed-=clip.duration;
        }
        if(selected==null||!ReferenceEquals(selected.layer,layer))return false;
        Range(selected.curve,out var begin,out _,out var rate);
        double phase=AtTick(selected.curve,begin+elapsed*rate);
        int columns=Math.Max(1,Math.Min(256,(int)Scalar(layer,"Columns",1)));
        int rows=Math.Max(1,Math.Min(256,(int)Scalar(layer,"Rows",1)));
        double size=Math.Max(.0001,Scalar(layer,"SizeU_280x",1));
        double ledColumns=Math.Max(1,Scalar(layer,"NumberOfColumnsLEDAnime",35));
        int frame=(int)Math.Floor(Wrap(elapsed,1)*columns*rows);
        int column=frame%columns,row=frame/columns;
        uv=new Rect((float)((column+phase/ledColumns/size)/columns),1-(row+1)/(float)rows,
            (float)(1/(size*columns)),1f/rows);
        return true;
    }
}
