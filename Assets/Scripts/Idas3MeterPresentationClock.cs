using System;

// Cosmetic time comes from the native 60 Hz presentation samples, not Unity's
// wall clock. Subtract integer ticks before converting so a large tick origin
// cannot discard subframe precision. Pauses and authority waits hold exactly.
internal sealed class Idas3MeterPresentationClock
{
    bool ready,frozen,replayClock;
    ulong ticks;
    double alpha,seconds;
    double replayPosition;
    uint replayRevision;
    internal float Seconds=>(float)seconds;
    internal void Reset(){ready=false;frozen=false;replayClock=false;ticks=0;alpha=0;seconds=0;replayPosition=0;replayRevision=0;}
    internal float Update(ulong nextTicks,float nextAlpha,uint flags,out bool rebased){
        bool valid=(flags&1)!=0&&!float.IsNaN(nextAlpha)&&!float.IsInfinity(nextAlpha);
        if(!valid){rebased=ready;Reset();return 0;}
        double phase=Math.Max(0,Math.Min(1,nextAlpha));
        bool nextFrozen=(flags&2)!=0;
        // A rewind or a large unobserved jump is a different presentation
        // interval (restart/seek/return to HUD), not time to catch up visually.
        bool discontinuity=!ready||replayClock||nextTicks<ticks||nextTicks-ticks>600;
        double delta=discontinuity?0:(double)(nextTicks-ticks)+phase-alpha;
        if(!frozen&&!nextFrozen&&delta<-.000001)discontinuity=true;
        rebased=discontinuity;
        if(discontinuity)seconds=0;
        else if(!frozen&&!nextFrozen&&delta>0)seconds+=delta/60;
        ticks=nextTicks;alpha=phase;frozen=nextFrozen;ready=true;replayClock=false;
        return Seconds;
    }
    // Replay ornaments deliberately rest because rounded source poses cannot
    // supply physical acceleration. HUD effects instead follow the viewer's
    // exact playback position: pause holds it, playback rate advances it, and
    // an explicit seek/driver change invalidates transient animation history.
    internal float UpdateReplay(double position,uint revision,out bool rebased){
        if(double.IsNaN(position)||double.IsInfinity(position)||position<0){rebased=ready;Reset();return 0;}
        rebased=!ready||!replayClock||revision!=replayRevision||position<replayPosition;
        if(rebased)seconds=0;
        else seconds+=position-replayPosition;
        replayPosition=position;replayRevision=revision;ready=true;replayClock=true;frozen=false;
        return Seconds;
    }
}
