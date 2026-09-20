using System;
using UnityEngine;
using UnityEngine.LowLevel;

internal static class Idas3FramePacingChecks
{
    internal static void Run(){
        const long frequency=10000000;
        foreach(int cap in new[]{30,60,90,120,144,165,240,360}){
            var schedule=new Idas3FramePacing.Schedule();long start=12345678;
            Check(schedule.Target(start,cap,frequency)==start,"First frame must not wait");
            schedule.Advance(start,cap,frequency);
            for(int i=1;i<=600;++i){
                double target=schedule.Target(start,cap,frequency);
                Check(Math.Abs(target-(start+i*(double)frequency/cap))<.01,"Fractional deadlines drifted");
                schedule.Advance((long)Math.Ceiling(target),cap,frequency);
            }
            long stalled=start+30*frequency;
            schedule.Advance(stalled,cap,frequency);
            Check(Math.Abs(schedule.Target(stalled,cap,frequency)-(stalled+(double)frequency/cap))<.01,"Stall caused catch-up frames");
            schedule.Reset();Check(schedule.Target(stalled,cap,frequency)==stalled,"Focus/cap reset retained an old deadline");
        }
    }
    internal static void RunPlatform(Idas3GameOptions options){
        foreach(int cap in new[]{30,60,90,120,144,165,240,360}){
            options.BeginEdit();options.Draft.vSync=false;options.Draft.frameRateLimit=cap;
            Check(options.ApplyDraft(),"Cap Apply failed");
            Check(Application.targetFrameRate==-1&&Idas3FramePacing.ActiveLimit==cap,"Cap did not replace Unity timer");
            Check(Count(PlayerLoop.GetCurrentPlayerLoop())==1,"Duplicate frame limiters installed");
        }
        var loaded=new Idas3GameOptions();loaded.Initialize(System.IO.Path.GetDirectoryName(options.FilePath));
        Check(loaded.Current.frameRateLimit==360&&Idas3FramePacing.ActiveLimit==360,"Saved cap did not reload");
        options.BeginEdit();options.Draft.vSync=true;Check(options.ApplyDraft(),"VSync Apply failed");
        Check(QualitySettings.vSyncCount==1&&Idas3FramePacing.ActiveLimit==0,"VSync stacked two limiters");
        options.BeginEdit();options.Draft.vSync=false;options.Draft.frameRateLimit=0;Check(options.ApplyDraft(),"Unlimited Apply failed");
        Check(QualitySettings.vSyncCount==0&&Application.targetFrameRate==-1&&Idas3FramePacing.ActiveLimit==0,"Unlimited still waits");
    }
    private static int Count(PlayerLoopSystem loop){
        int count=loop.type?.FullName=="Idas3FramePacing+FramePacingLoop"?1:0;
        if(loop.subSystemList!=null)foreach(var child in loop.subSystemList)count+=Count(child);
        return count;
    }
    private static void Check(bool ok,string error){if(!ok)throw new InvalidOperationException(error);}
}
