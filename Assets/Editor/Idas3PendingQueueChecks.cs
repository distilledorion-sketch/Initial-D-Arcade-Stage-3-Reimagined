using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;

public static class Idas3PendingQueueChecks
{
    const BindingFlags Private=BindingFlags.Instance|BindingFlags.NonPublic;
    [Serializable] sealed class Fixture {public List<Idas3CommunityTimes.Run> runs;}
    [Serializable] sealed class Sample {public int rows;public double[] milliseconds;}
    [Serializable] sealed class Report {public string scope;public Sample[] samples;public string[] checks;}
    static string Output(){
        string path=Path.GetFullPath(Environment.GetEnvironmentVariable("IDAS3_COMMUNITY_REFRESH_AUDIT")??"");
        if(!File.Exists(Path.Combine(path,"PRIVATE_TEST_ONLY.txt")))throw new InvalidOperationException("Private output required");
        return path;
    }
    public static void RunBaseline(){
        string output=Output(),folder=Path.Combine(output,"queue");Directory.CreateDirectory(folder);
        var source=JsonUtility.FromJson<Fixture>(File.ReadAllText(Path.Combine(Path.GetDirectoryName(output),"pending.json")));
        var go=new GameObject("Private pending queue baseline");
        try {
            var owner=go.AddComponent<Idas3CommunityTimes>();
            typeof(Idas3CommunityTimes).GetField("folder",Private).SetValue(owner,folder);
            var write=typeof(Idas3CommunityTimes).GetMethod("Write",Private);
            var samples=new List<Sample>();
            foreach(int count in new[]{5,100,2450}){
                var value=new Fixture {runs=source.runs.GetRange(0,count)};
                var sample=new Sample {rows=count,milliseconds=new double[9]};
                for(int pass=0;pass<9;pass++){
                    var watch=Stopwatch.StartNew();write.Invoke(owner,new object[]{"pending.json",value});watch.Stop();
                    sample.milliseconds[pass]=watch.Elapsed.TotalMilliseconds;
                }
                samples.Add(sample);
            }
            File.WriteAllText(Path.Combine(output,"report.json"),JsonUtility.ToJson(new Report {
                scope="Production synchronous pending-file serialization/atomic replacement invoked on the Editor main thread. Private synthetic data; no HTTP, VPN or FPS measurement.",samples=samples.ToArray()},true));
        } finally {UnityEngine.Object.DestroyImmediate(go);}
    }
    static Idas3CommunityTimes.Run Copy(Idas3CommunityTimes.Run r)=>JsonUtility.FromJson<Idas3CommunityTimes.Run>(JsonUtility.ToJson(r));
    static Task Begin(Idas3CommunityTimes owner,string method,params object[] arguments)=>(Task)typeof(Idas3CommunityTimes).GetMethod(method,Private).Invoke(owner,arguments);
    static void Wait(Task task){var watch=Stopwatch.StartNew();while(!task.IsCompleted){if(watch.Elapsed.TotalSeconds>30)throw new TimeoutException("Pending write");Thread.Sleep(1);}if(task.IsFaulted)throw task.Exception;}
    static bool Publish(Idas3CommunityTimes owner,Task task)=>(bool)typeof(Idas3CommunityTimes).GetMethod("PublishPending",Private).Invoke(owner,new object[]{task});
    static int Count(Idas3CommunityTimes owner)=>(int)typeof(Idas3CommunityTimes).GetProperty("PendingCount",Private).GetValue(owner);
    public static void RunCandidate(){
        string output=Output(),folder=Path.Combine(output,"queue"),file=Path.Combine(folder,"pending.json");Directory.CreateDirectory(folder);
        var fixture=JsonUtility.FromJson<Fixture>(File.ReadAllText(Path.Combine(Path.GetDirectoryName(output),"pending.json")));
        // These are synthetic current-season queue stress fixtures, not restored
        // player files. The separate Season2 checks verify rejecting old queues.
        foreach(var row in fixture.runs){row.epoch=Idas3CommunityTimes.CurrentSeasonEpoch;row.build=Idas3CommunityTimes.RequiredSubmissionBuild;}
        typeof(Idas3CommunityTimes).GetMethod("MigrateSeasonCache",BindingFlags.Static|BindingFlags.NonPublic).Invoke(null,new object[]{folder});
        string source=JsonUtility.ToJson(fixture);File.WriteAllText(file,source);
        File.WriteAllText(Path.Combine(output,"ISOLATED_MODE_FLOW_TEST.txt"),"Offline pending writer checks.");
        var go=new GameObject("Private asynchronous queue checks");var checks=new List<string>();
        Action<bool,string> check=(ok,why)=>{if(!ok)throw new Exception(why);checks.Add(why);};
        try {
            var settings=new Idas3GameOptions();typeof(Idas3GameOptions).GetField("current",Private).SetValue(settings,new Idas3GameOptions.Values {communityTimes=true});
            var owner=go.AddComponent<Idas3CommunityTimes>();owner.Initialize(null,settings,go.AddComponent<Idas3PauseMenu>(),folder,true);
            var original=JsonUtility.FromJson<Fixture>(source);check(Count(owner)==2450,"Full pending queue restored");
            var current=Copy(original.runs[original.runs.Count-1]);var sample=new Sample {rows=2450,milliseconds=new double[9]};
            for(int pass=0;pass<9;pass++){
                current=Copy(current);current.id=Guid.NewGuid().ToString();current.ticks6000--;current.splits[3]--;
                var step=Stopwatch.StartNew();var task=Begin(owner,"QueueFinish",current);step.Stop();double launch=step.Elapsed.TotalMilliseconds;
                Wait(task);step.Restart();bool ok=Publish(owner,task);step.Stop();sample.milliseconds[pass]=Math.Max(launch,step.Elapsed.TotalMilliseconds);
                check(ok&&Count(owner)==2450,"Asynchronous replacement committed: "+pass);
                check(File.ReadAllText(file).Contains(current.id),"Replacement is durable: "+pass);
            }
            var next=Copy(current);next.id=Guid.NewGuid().ToString();next.ticks6000--;next.splits[3]--;
            var add=Begin(owner,"QueueFinish",next);var staleAck=Begin(owner,"RemovePending",current.id);
            Wait(staleAck);check(Publish(owner,staleAck)&&Publish(owner,add)&&Count(owner)==2450,"Late old upload acknowledgement preserves faster pending result");
            check(File.ReadAllText(file).Contains(next.id)&&!File.ReadAllText(file).Contains(current.id),"Late acknowledgement leaves latest result on disk");
            current=next;next=Copy(current);next.id=Guid.NewGuid().ToString();next.ticks6000--;next.splits[3]--;
            var ackFirst=Begin(owner,"RemovePending",current.id);var addAfter=Begin(owner,"QueueFinish",next);
            Wait(addAfter);check(Publish(owner,addAfter)&&Publish(owner,ackFirst)&&Count(owner)==2450,"Out-of-order coroutine publication cannot rewind the queue");
            current=next;next=Copy(current);next.id=Guid.NewGuid().ToString();next.ticks6000--;next.splits[3]--;
            var beforeClear=Begin(owner,"QueueFinish",next);var clear=Begin(owner,"ClearPending");
            var after=Copy(next);after.id=Guid.NewGuid().ToString();after.ticks6000--;after.splits[3]--;
            var afterClear=Begin(owner,"QueueFinish",after);Wait(afterClear);
            check(Publish(owner,afterClear)&&Publish(owner,beforeClear)&&Publish(owner,clear)&&Count(owner)==1,"Sharing clear is ordered between prior and later finishes");
            var persisted=JsonUtility.FromJson<Fixture>(File.ReadAllText(file));check(persisted.runs.Count==1&&persisted.runs[0].id==after.id,"Ordered clear has the correct durable contents");
            var frozen=Copy(after);frozen.id=Guid.NewGuid().ToString();frozen.ticks6000--;frozen.splits[3]--;frozen.replayAvailable=true;frozen.replayVersion=2;
            var expected=Copy(frozen);var immutable=Begin(owner,"QueueFinish",frozen);frozen.nameGlyphs[0]=0;frozen.splits[0]=1;frozen.ticks6000=2;
            Wait(immutable);check(Publish(owner,immutable),"Caller mutation does not alter pending write");
            persisted=JsonUtility.FromJson<Fixture>(File.ReadAllText(file));
            check(persisted.runs[0].ticks6000==expected.ticks6000&&persisted.runs[0].nameGlyphs[0]==expected.nameGlyphs[0]&&persisted.runs[0].splits[0]==expected.splits[0]&&!persisted.runs[0].replayAvailable&&persisted.runs[0].replayVersion==0,"Queue freezes values and contains times only");
            string before=File.ReadAllText(file);var retry=Copy(expected);retry.id=Guid.NewGuid().ToString();retry.ticks6000--;retry.splits[3]--;
            using(var locked=new FileStream(file,FileMode.Open,FileAccess.Read,FileShare.None)){
                var failed=Begin(owner,"QueueFinish",retry);Wait(failed);check(!Publish(owner,failed)&&Count(owner)==1,"Failed replacement preserves live queue");
                var failedAck=Begin(owner,"RemovePending",expected.id);Wait(failedAck);check(!Publish(owner,failedAck)&&Count(owner)==1,"Failed acknowledgement is retryable without dropping memory state");
            }
            check(File.ReadAllText(file)==before,"Failed writes preserve prior disk bytes");
            var retried=Begin(owner,"QueueFinish",retry);Wait(retried);check(Publish(owner,retried)&&File.ReadAllText(file).Contains(retry.id),"Writer recovers after failed writes");
            // Start work then invoke the Editor lifecycle callback explicitly.
            // A commit that won the race with cancellation may exist on disk;
            // neither completed nor cancelled work can publish after shutdown.
            var closing=Begin(owner,"RemovePending",retry.id);
            typeof(Idas3CommunityTimes).GetMethod("OnDestroy",Private).Invoke(owner,null);Wait(closing);
            check(!Publish(owner,closing)&&!closing.IsFaulted,"Shutdown observes worker completion without late publication");
            check(JsonUtility.FromJson<Fixture>(File.ReadAllText(file)).runs!=null,"Shutdown leaves a complete queue file");
            File.WriteAllText(Path.Combine(output,"report.json"),JsonUtility.ToJson(new Report {
                scope="Asynchronous queue scheduling/publication timings exclude worker serialization and disk cost. Offline serialized mutations, late acknowledgements, out-of-order publication, sharing clear, failed writes/retry, immutable runs and cancellation.",samples=new[]{sample},checks=checks.ToArray()},true));
        } finally {UnityEngine.Object.DestroyImmediate(go);}
    }
}
