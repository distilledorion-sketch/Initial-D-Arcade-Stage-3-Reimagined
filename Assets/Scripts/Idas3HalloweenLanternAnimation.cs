using System;
using UnityEngine;
using Meter=Idas3ArcadeMeterCatalog.Meter;
using Curve=Idas3ArcadeMeterCatalog.Curve;

// Halloween's source lantern swings on drift entry/exit. The source plays its
// InOut animation forwards/reversed; its separate graded Blink state has no
// equivalent telemetry here. Use the native active bit, never the fading alpha.
internal sealed class Idas3HalloweenLanternAnimation
{
    Meter meter;
    bool initialized,drifting,playing,reverse;
    float seconds,started;
    internal void Reset(){meter=null;initialized=false;drifting=false;playing=false;reverse=false;seconds=started=0;}
    internal void Update(Meter next,Idas3ArcadeHud.Telemetry data,float now){
        if(next==null||next.id!=83||data.version<2||(data.flags&1)==0||float.IsNaN(now)||float.IsInfinity(now)){
            Reset();return;
        }
        bool active=(data.flags&8)!=0;
        if(!initialized||meter!=next||now<seconds){
            meter=next;initialized=true;drifting=active;playing=false;seconds=now;return;
        }
        if(active!=drifting){
            // Replaced telemetry at an unchanged presentation time is not a
            // newly observed event (e.g. a paused preview/verification sample).
            playing=now>seconds;started=now;reverse=!active;drifting=active;
        }
        seconds=now;
    }
    internal bool TryProgress(Curve curve,out float progress){
        progress=0;
        if(!initialized||!playing||curve==null||curve.owner?.name!="Cantera"||curve.property!="Rotation"||
            curve.animation==null||curve.animation.IndexOf("DriftLamp_InOut",StringComparison.Ordinal)<0)return false;
        float duration=Idas3MeterAnimationState.DurationSeconds(curve);
        float elapsed=seconds-started;
        if(duration<=0||elapsed<0||elapsed>=duration)return false;
        float phase=Mathf.Clamp01(elapsed/duration);
        progress=reverse?1-phase:phase;return true;
    }
}
