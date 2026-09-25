using System;
using UnityEngine;

public static class Idas3MeterPresentationClockChecks
{
    public static void Run()=>Debug.Log(RunChecks());
    public static string RunChecks(){
        int checks=0;
        void Check(bool value,string message){++checks;if(!value)throw new InvalidOperationException("Meter presentation clock: "+message);}
        void Near(float actual,float expected,string message)=>Check(Math.Abs(actual-expected)<.000002f,message);
        var clock=new Idas3MeterPresentationClock();
        Near(clock.Update(9000,.25f,1,out var reset),0,"initial observation did not rebase");Check(reset,"initial observation retained an old event");
        Near(clock.Update(9000,.75f,1,out reset),.5f/60,"subtick progress missing");Check(!reset,"subtick progress reset clock");
        Near(clock.Update(9001,.25f,1,out reset),1f/60,"adjacent tick progress changed phase");
        float held=clock.Seconds;
        for(int i=0;i<100;++i)Near(clock.Update((ulong)(9001+i),1,3,out reset),held,"frozen native clock advanced");
        Near(clock.Update(9101,.1f,1,out reset),held,"resume jumped through frozen ticks");Check(!reset,"resume reset event");
        Near(clock.Update(9101,.6f,1,out reset),held+.5f/60,"resumed subframe progress missing");
        Near(clock.Update(3,.5f,1,out reset),0,"restart did not rebase");Check(reset,"restart did not invalidate event history");
        Near(clock.Update(10000,.5f,1,out reset),0,"large seek caught up old event");Check(reset,"large seek did not invalidate event history");
        clock.Update(10001,.5f,1,out reset);
        Near(clock.Update(10001,.5f,0,out reset),0,"inactive session retained clock");Check(reset,"inactive session did not invalidate history");
        Near(clock.Update(70000,.5f,1,out reset),0,"new session did not start fresh");Check(reset,"new session did not request fresh renderer");
        Near(clock.Update(70001,float.NaN,1,out reset),0,"NaN clock was accepted");Check(reset,"NaN clock kept event history");
        Near(clock.Update(70001,float.PositiveInfinity,1,out reset),0,"infinite clock was accepted");
        clock.Update(70001,.8f,1,out reset);
        Near(clock.Update(70001,.2f,1,out reset),0,"unfrozen phase rewind was accepted");Check(reset,"phase rewind retained old event");
        clock.Reset();Near(clock.Seconds,0,"explicit reset retained preview time");
        Near(clock.Update(3,1,3,out reset),0,"initial frozen sample advanced");
        Near(clock.Update(4,0,1,out reset),0,"initial frozen resume jumped");
        foreach(int rate in new[]{30,60,144,240}){
            clock.Reset();const ulong origin=ulong.MaxValue-10000;
            clock.Update(origin,0,1,out reset);
            for(int frame=1;frame<=rate*3;++frame){
                double tickPosition=frame*60d/rate;ulong whole=(ulong)Math.Floor(tickPosition);
                float value=clock.Update(origin+whole,(float)(tickPosition-whole),1,out reset);
                Near(value,frame/(float)rate,"large origin/frame-rate drift at "+rate+" Hz");Check(!reset,"continuous samples reset at "+rate+" Hz");
            }
            Near(clock.Seconds,3,"final phase depends on render FPS");
        }
        // The replay's ornament timing remains frozen for physics safety.
        // The HUD must use the viewer position instead, including sub-ticks.
        clock.Reset();
        Near(clock.UpdateReplay(120.25,1,out reset),0,"first replay sample did not prime");Check(reset,"replay retained live events");
        Near(clock.UpdateReplay(120.255,1,out reset),.005f,"replay sub-tick progress was rounded away");Check(!reset,"continuous replay reset");
        Near(clock.UpdateReplay(121.25,1,out reset),1,"replay did not advance");
        held=clock.Seconds;
        for(int i=0;i<144;++i){Near(clock.UpdateReplay(121.25,1,out reset),held,"paused replay advanced");Check(!reset,"paused replay lost held effect");}
        Near(clock.UpdateReplay(121.25+1d/60,1,out reset),held+1f/60,"resume discarded the first playback interval");
        Near(clock.UpdateReplay(126.25,2,out reset),0,"forward seek advanced through an unobserved interval");Check(reset,"forward seek retained gear event");
        Near(clock.UpdateReplay(126.251,3,out reset),0,"short scrub was mistaken for playback");Check(reset,"short scrub retained trails");
        Near(clock.UpdateReplay(126.251,4,out reset),0,"driver change changed elapsed phase");Check(reset,"driver change retained previous car events");
        Near(clock.UpdateReplay(126.351,4,out reset),.1f,"playback did not resume after seek");
        Near(clock.UpdateReplay(125,4,out reset),0,"unmarked rewind was accepted");Check(reset,"rewind retained events");
        Near(clock.UpdateReplay(double.NaN,4,out reset),0,"invalid replay position accepted");Check(reset,"invalid replay position retained history");
        Near(clock.UpdateReplay(double.PositiveInfinity,4,out reset),0,"infinite replay position accepted");
        Near(clock.UpdateReplay(-1,4,out reset),0,"negative replay position accepted");
        clock.Update(9000,.5f,1,out reset);
        Near(clock.UpdateReplay(10,5,out reset),0,"live-to-replay switch retained live clock");Check(reset,"live-to-replay did not invalidate events");
        Near(clock.Update(9001,.5f,1,out reset),0,"replay-to-live switch retained replay clock");Check(reset,"replay-to-live did not invalidate events");
        foreach(int fps in new[]{30,60,144})foreach(double speed in new[]{.25,.5,1,2,4}){
            clock.Reset();const double origin=120.125;
            clock.UpdateReplay(origin,7,out reset);
            for(int frame=1;frame<=fps*3;++frame){
                float value=clock.UpdateReplay(origin+frame*speed/fps,7,out reset);
                Near(value,(float)(frame*speed/fps),"replay phase changed at "+fps+" Hz / "+speed+"x");
                Check(!reset,"continuous replay reset at "+fps+" Hz / "+speed+"x");
            }
            Near(clock.Seconds,(float)(3*speed),"replay rate did not scale effects");
        }
        return "Meter presentation clock checks passed: "+checks;
    }
}
