using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;

public sealed partial class Idas3SceneSmoke
{
    [System.Runtime.InteropServices.DllImport("Idas3Unity",CallingConvention=System.Runtime.InteropServices.CallingConvention.Cdecl)]
    static extern int Idas3SharedReadFinish([System.Runtime.InteropServices.Out] byte[] buffer,int capacity);
    [Serializable] sealed class PendingPlayerReport {
        public string scope="Private full pending queue during real Akina simulation; main-step timings exclude background disk work and do not measure displayed FPS.";
        public CommunityRefreshSample[] samples;
        public bool retainedFinishOnFailure,acknowledgedDurableFinish,destroyedOwnerCancelled;
    }
    static Idas3CommunityTimes.Run CopyPendingRun(Idas3CommunityTimes.Run value)=>JsonUtility.FromJson<Idas3CommunityTimes.Run>(JsonUtility.ToJson(value));
    private IEnumerator CommunityPendingCheck()
    {
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-community-pending-fixture");
        Check(at>=0&&at+1<args.Length,"Private pending fixture required");
        var source=File.ReadAllText(Path.GetFullPath(args[at+1]));
        var fixture=JsonUtility.FromJson<Idas3CommunityTimes.Pending>(source);
        Check(fixture.runs.Count==2450,"Complete pending fixture required");
        Check(Idas3CommunityTimes.SubmissionBuild(Application.version),"Native collection test uses the accepted submission build");
        QualitySettings.vSyncCount=0;Application.targetFrameRate=-1;
        yield return Key(116);
        for(int n=0;host.Status.racePhase!=2&&n<1800;n++)yield return null;
        Check(host.Status.racePhase==2,"Akina countdown completed");held=87;yield return Frames(120);
        Check(host.Status.speedMetresPerSecond>1,"Car moving before queue test");
        File.WriteAllText(Path.Combine(root,"ISOLATED_MODE_FLOW_TEST.txt"),"Private pending-time persistence test.");
        string folder=Path.Combine(root,"community"),file=Path.Combine(folder,"pending.json");Directory.CreateDirectory(folder);File.WriteAllText(file,source);
        host.GameOptions.Current.communityTimes=true;
        var owner=host.gameObject.AddComponent<Idas3CommunityTimes>();
        owner.Initialize(host,host.GameOptions,host.PauseMenu,folder,true);
        Check(owner.PendingCount==2450,"Complete pending queue restored in player");
        var samples=new List<CommunityRefreshSample>();var current=CopyPendingRun(fixture.runs[fixture.runs.Count-1]);
        for(int pass=0;pass<9;pass++) {
            current=CopyPendingRun(current);current.id=Guid.NewGuid().ToString();current.ticks6000--;current.splits[3]--;
            var sample=new CommunityRefreshSample {firstTick=host.Status.simulationTicks,entries=owner.PendingCount};
            var all=Stopwatch.StartNew();var step=Stopwatch.StartNew();var task=owner.QueueFinish(current);step.Stop();
            sample.maxMainStepMs=step.Elapsed.TotalMilliseconds;
            do {sample.frames++;yield return null;Check(all.Elapsed.TotalSeconds<30,"Pending writer deadline");}while(!task.IsCompleted);
            step.Restart();bool published=owner.PublishPending(task);step.Stop();sample.maxMainStepMs=Math.Max(sample.maxMainStepMs,step.Elapsed.TotalMilliseconds);
            sample.elapsedMs=all.Elapsed.TotalMilliseconds;sample.lastTick=host.Status.simulationTicks;sample.speed=host.Status.speedMetresPerSecond;
            Check(published&&owner.PendingCount==2450,"Faster pending time committed without losing rows");
            Check(sample.lastTick>sample.firstTick,"Simulation advanced while queue persistence completed");
            var saved=JsonUtility.FromJson<Idas3CommunityTimes.Pending>(File.ReadAllText(file));
            Check(saved.runs.Count==2450&&saved.runs.Exists(r=>r.id==current.id&&r.ticks6000==current.ticks6000),"New pending time is durable");
            samples.Add(sample);yield return Frames(15);
        }
        // Real original gate crossings produce the native pending payload.
        // Lock its destination so production Collect must retain it for retry.
        held=0;Check(Idas3SceneModeFlowFixture(-5)==1,"Native completed-finish fixture");
        var buffer=new byte[4096];int length=Idas3SharedReadFinish(buffer,buffer.Length);
        Check(length>0,"Completed native finish awaits durable acknowledgement");
        string payload=Encoding.UTF8.GetString(buffer,0,length);var finished=JsonUtility.FromJson<Idas3CommunityTimes.Run>(payload);
        var field=typeof(Idas3CommunityTimes).GetField("finishWrite",BindingFlags.Instance|BindingFlags.NonPublic);
        string previousFile=File.ReadAllText(file);
        using(var locked=new FileStream(file,FileMode.Open,FileAccess.Read,FileShare.None)) {
            var wait=Stopwatch.StartNew();Task<Idas3CommunityTimes.PendingCommit> failed=null;
            while(failed==null){Check(wait.Elapsed.TotalSeconds<10,"Collect queued native finish");yield return null;failed=(Task<Idas3CommunityTimes.PendingCommit>)field.GetValue(owner);}
            while(!failed.IsCompleted){Check(wait.Elapsed.TotalSeconds<10,"Failed native write completed");yield return null;}
            Check(failed.Result.error!=null,"Locked queue rejects native write");
            length=Idas3SharedReadFinish(buffer,buffer.Length);
            Check(length>0&&Encoding.UTF8.GetString(buffer,0,length)==payload,"Failed persistence retains exact native finish");
        }
        Check(File.ReadAllText(file)==previousFile,"Failed native write retains prior file");
        var retry=Stopwatch.StartNew();
        while(Idas3SharedReadFinish(buffer,buffer.Length)>0){Check(retry.Elapsed.TotalSeconds<10,"Native finish retry deadline");yield return null;}
        var durable=JsonUtility.FromJson<Idas3CommunityTimes.Pending>(File.ReadAllText(file));
        Check(durable.runs.Exists(r=>r.condition==finished.condition&&r.weather==finished.weather&&r.car==finished.car&&r.ticks6000<=finished.ticks6000),"Native finish is acknowledged only after its time is durable");
        var closing=owner.ClearPending();Destroy(owner);yield return null;
        var shutdown=Stopwatch.StartNew();while(!closing.IsCompleted){Check(shutdown.Elapsed.TotalSeconds<10,"Shutdown writer deadline");yield return null;}
        Check(!closing.IsFaulted&&!owner.PublishPending(closing),"Actual destruction prevents late publication");
        Check(JsonUtility.FromJson<Idas3CommunityTimes.Pending>(File.ReadAllText(file)).runs!=null,"Shutdown leaves complete JSON");
        File.WriteAllText(Path.Combine(root,"community-pending.json"),JsonUtility.ToJson(new PendingPlayerReport {samples=samples.ToArray(),retainedFinishOnFailure=true,acknowledgedDurableFinish=true,destroyedOwnerCancelled=true},true));
        Finish(true,null);
    }
}
