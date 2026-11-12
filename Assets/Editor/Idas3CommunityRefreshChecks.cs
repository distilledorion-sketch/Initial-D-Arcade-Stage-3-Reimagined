using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Collections;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;

// Private, offline measurements of the real community response-processing path.
public static class Idas3CommunityRefreshChecks
{
    [Serializable] sealed class Sample { public int entries,bytes,yields;public bool accepted;public double[] milliseconds,mainStepMilliseconds; }
    [Serializable] sealed class Report { public string scope;public Sample[] samples;public string[] checks; }
    const BindingFlags Private=BindingFlags.Instance|BindingFlags.NonPublic;
    public static void Run()
    {
        string output=Environment.GetEnvironmentVariable("IDAS3_COMMUNITY_REFRESH_AUDIT");
        if(string.IsNullOrEmpty(output))throw new InvalidOperationException("Private output required");
        output=Path.GetFullPath(output);
        if(!File.Exists(Path.Combine(output,"PRIVATE_TEST_ONLY.txt")))throw new InvalidOperationException("Private marker required");
        string storage=Path.Combine(output,"cache");Directory.CreateDirectory(storage);
        var go=new GameObject("Offline leaderboard response audit");
        try {
            var owner=go.AddComponent<Idas3CommunityTimes>();
            typeof(Idas3CommunityTimes).GetField("folder",Private).SetValue(owner,storage);
            var validate=typeof(Idas3CommunityTimes).GetMethod("UsableCommunitySnapshot",BindingFlags.Static|BindingFlags.NonPublic);
            var write=typeof(Idas3CommunityTimes).GetMethod("Write",Private);
            var samples=new List<Sample>();
            foreach(int count in new[]{200,2000,3080}) {
                var entries=new List<Idas3CommunityTimes.Run>();
                for(int condition=0;condition<Idas3CourseCatalog.ConditionCount;++condition)
                    for(int weather=0;weather<2;++weather) {
                        if((condition==16||condition==17)&&weather==0)continue;
                        // Server selects the top ten plus the best of each other car (44 maximum).
                        for(int row=0;row<44&&entries.Count<count;++row)entries.Add(new Idas3CommunityTimes.Run {
                            id=Guid.NewGuid().ToString(),ruleset=Idas3CommunityTimes.Ruleset,
                            build=Idas3CommunityTimes.RequiredSubmissionBuild,epoch=2,condition=condition,
                            weather=weather,car=row<10?0:row-9,ticks6000=1000000+row,manual=1,night=1,
                            nameGlyphs=new[]{162,163,164,221,221},splits=new[]{250000,500000,750000,1000000+row}
                        });
                    }
                var fixture=new Idas3CommunityTimes.Snapshot {ruleset=Idas3CommunityTimes.Ruleset,epoch=2,entries=entries.ToArray()};
                string json=JsonUtility.ToJson(fixture);
                var sample=new Sample {entries=entries.Count,bytes=System.Text.Encoding.UTF8.GetByteCount(json),milliseconds=new double[12]};
                for(int i=-1;i<sample.milliseconds.Length;++i) {
                    var watch=Stopwatch.StartNew();
                    var next=JsonUtility.FromJson<Idas3CommunityTimes.Snapshot>(json);
                    sample.accepted=(bool)validate.Invoke(null,new object[]{next});
                    if(sample.accepted)write.Invoke(owner,new object[]{"snapshot.json",next});
                    watch.Stop();if(i>=0)sample.milliseconds[i]=watch.Elapsed.TotalMilliseconds;
                }
                samples.Add(sample);
            }
            File.WriteAllText(Path.Combine(output,"report.json"),JsonUtility.ToJson(new Report {
                scope="Offline Editor main-thread parse, validation and atomic cache write. Synthetic server-shaped snapshots; not live VPN traffic or displayed FPS.",samples=samples.ToArray()},true));
            UnityEngine.Debug.Log("COMMUNITY_REFRESH_AUDIT_COMPLETE");
        } finally {UnityEngine.Object.DestroyImmediate(go);}
    }

    public static void RunCandidate()
    {
        string output=Path.GetFullPath(Environment.GetEnvironmentVariable("IDAS3_COMMUNITY_REFRESH_AUDIT")??"");
        if(!File.Exists(Path.Combine(output,"PRIVATE_TEST_ONLY.txt")))throw new InvalidOperationException("Private marker required");
        string storage=Path.Combine(output,"cache");Directory.CreateDirectory(storage);
        var checks=new List<string>();var samples=new List<Sample>();
        Action<bool,string> check=(pass,label)=>{if(!pass)throw new Exception(label);checks.Add(label);};
        var go=new GameObject("Offline asynchronous leaderboard checks");
        try {
            var owner=go.AddComponent<Idas3CommunityTimes>();
            var settings=new Idas3GameOptions();
            typeof(Idas3GameOptions).GetField("current",Private).SetValue(settings,new Idas3GameOptions.Values {communityTimes=true});
            typeof(Idas3CommunityTimes).GetField("folder",Private).SetValue(owner,storage);
            typeof(Idas3CommunityTimes).GetField("options",Private).SetValue(owner,settings);
            var process=typeof(Idas3CommunityTimes).GetMethod("ProcessSnapshot",Private);
            var snapshot=typeof(Idas3CommunityTimes).GetField("snapshot",Private);
            var confirmed=typeof(Idas3CommunityTimes).GetField("confirmedUntil",Private);
            Func<long,byte[],IEnumerator> begin=(code,body)=>(IEnumerator)process.Invoke(owner,new object[]{code,body});
            Func<IEnumerator,Sample,double> drain=(iterator,sample)=>{
                var all=Stopwatch.StartNew();double max=0;
                while(true) {
                    var step=Stopwatch.StartNew();bool more=iterator.MoveNext();step.Stop();max=Math.Max(max,step.Elapsed.TotalMilliseconds);
                    if(!more)break;
                    if(sample!=null)sample.yields++;
                    if(all.Elapsed.TotalSeconds>30)throw new TimeoutException("Offline response processing");
                    Thread.Sleep(1);
                }
                return max;
            };
            var small=new Idas3CommunityTimes.Snapshot {ruleset=Idas3CommunityTimes.Ruleset,epoch=2,entries=new[]{ValidRun()}};
            var baselineCache=JsonUtility.FromJson<Idas3CommunityTimes.Snapshot>(File.ReadAllText(Path.Combine(Path.GetDirectoryName(output),"baseline/cache/snapshot.json")));
            foreach(int count in new[]{200,2000}) {
                var fixture=new Idas3CommunityTimes.Snapshot {ruleset=small.ruleset,epoch=2,entries=new Idas3CommunityTimes.Run[count]};
                for(int n=0;n<count;n++)fixture.entries[n]=baselineCache.entries[n];
                var bytes=Encoding.UTF8.GetBytes(JsonUtility.ToJson(fixture));
                var sample=new Sample {entries=count,bytes=bytes.Length,milliseconds=new double[12],mainStepMilliseconds=new double[12]};
                for(int i=-1;i<12;i++) {
                    snapshot.SetValue(owner,null);
                    var watch=Stopwatch.StartNew();double max=drain(begin(200,bytes),sample);watch.Stop();
                    sample.accepted=snapshot.GetValue(owner)!=null;
                    check(sample.accepted,"Asynchronous snapshot accepted: "+count+" / "+i);
                    if(i>=0){sample.milliseconds[i]=watch.Elapsed.TotalMilliseconds;sample.mainStepMilliseconds[i]=max;}
                }
                samples.Add(sample);
            }
            string dest=Path.Combine(storage,"snapshot.json");
            var validBytes=Encoding.UTF8.GetBytes(JsonUtility.ToJson(small));
            drain(begin(200,validBytes),null);
            var saved=File.ReadAllText(dest);var previous=snapshot.GetValue(owner);
            foreach(var invalid in new[]{Encoding.UTF8.GetBytes("{not json}"),Encoding.UTF8.GetBytes("{}"),new byte[2*1024*1024+1]}) {
                drain(begin(200,invalid),null);
                check(ReferenceEquals(snapshot.GetValue(owner),previous)&&(double)confirmed.GetValue(owner)==0&&File.ReadAllText(dest)==saved,"Invalid response retains cache and disables remote records: "+invalid.Length);
            }
            small.epoch=1;small.entries[0].epoch=1;
            drain(begin(200,Encoding.UTF8.GetBytes(JsonUtility.ToJson(small))),null);
            check(File.ReadAllText(dest)==saved&&(double)confirmed.GetValue(owner)==0,"Old season cannot replace current records");
            small.epoch=2;small.entries[0].epoch=2;
            small.entries[0].build="0.3.94-player-replays.4";
            drain(begin(200,Encoding.UTF8.GetBytes(JsonUtility.ToJson(small))),null);
            check(File.ReadAllText(dest)==saved,"Unsupported build cannot replace current records");
            small.entries[0].build=Idas3CommunityTimes.RequiredSubmissionBuild;
            using(var locked=new FileStream(dest,FileMode.Open,FileAccess.Read,FileShare.Read)) {
                drain(begin(200,validBytes),null);
                check((double)confirmed.GetValue(owner)==0&&ReferenceEquals(snapshot.GetValue(owner),previous),"Cache replacement failure does not publish uncommitted records");
            }
            check(File.ReadAllText(dest)==saved,"Failed atomic write leaves previous cache intact");
            drain(begin(503,null),null);check((double)confirmed.GetValue(owner)==0,"Failed HTTP response uses personal records");
            var disabled=begin(200,validBytes);disabled.MoveNext();settings.Current.communityTimes=false;drain(disabled,null);
            check((double)confirmed.GetValue(owner)==0,"Disabling sharing during preparation cannot re-enable remote records");settings.Current.communityTimes=true;
            small.entries=Array.Empty<Idas3CommunityTimes.Run>();
            drain(begin(200,Encoding.UTF8.GetBytes(JsonUtility.ToJson(small))),null);
            check(((Idas3CommunityTimes.Snapshot)snapshot.GetValue(owner)).entries.Length==0&&(double)confirmed.GetValue(owner)>0,"Fresh empty board removes old community records");
            var prepare=typeof(Idas3CommunityTimes).GetMethod("PrepareSnapshot",BindingFlags.Static|BindingFlags.NonPublic);
            saved=File.ReadAllText(dest);
            using(var stop=new CancellationTokenSource()) {
                stop.Cancel();
                var task=(Task<Idas3CommunityTimes.Snapshot>)prepare.Invoke(null,new object[]{validBytes,storage,2,stop.Token});
                check(task.Wait(30000)&&task.Result==null&&File.ReadAllText(dest)==saved,"Cancelled preparation never commits a cache");
            }
            var responseType=typeof(Idas3CommunityTimes).GetNestedType("LimitedResponse",BindingFlags.NonPublic);
            using(var response=(IDisposable)Activator.CreateInstance(responseType,true)) {
                var append=responseType.GetMethod("ReceiveData",Private);var block=new byte[8192];
                for(int n=0;n<256;n++)check((bool)append.Invoke(response,new object[]{block,block.Length}),"Response chunk within byte limit: "+n);
                check(!(bool)append.Invoke(response,new object[]{block,1}),"Over-limit response aborts before further buffering");
                check(!(bool)append.Invoke(response,new object[]{block,0})&&responseType.GetProperty("Body",Private).GetValue(response)==null,"Exceeded response cannot later become successful");
            }
            var pending=begin(200,validBytes);pending.MoveNext();
            // Edit-mode objects do not receive MonoBehaviour destruction events.
            typeof(Idas3CommunityTimes).GetMethod("OnDestroy",Private).Invoke(owner,null);drain(pending,null);
            check((double)confirmed.GetValue(owner)==0,"Shutdown callback prevents late response publication");
            File.WriteAllText(Path.Combine(output,"report.json"),JsonUtility.ToJson(new Report {
                scope="Production ProcessSnapshot coroutine and background preparation in an offline Unity Editor. Includes main-thread step timing, malformed/old data, atomic-write failure, cancellation, settings change and bounded response buffering. Not live VPN traffic or displayed FPS.",samples=samples.ToArray(),checks=checks.ToArray()},true));
            UnityEngine.Debug.Log("COMMUNITY_REFRESH_PASS "+checks.Count);
        } finally {if(go!=null)UnityEngine.Object.DestroyImmediate(go);}
    }
    static Idas3CommunityTimes.Run ValidRun()=>new Idas3CommunityTimes.Run {
        id=Guid.NewGuid().ToString(),ruleset=Idas3CommunityTimes.Ruleset,build=Idas3CommunityTimes.RequiredSubmissionBuild,
        epoch=2,condition=35,weather=1,car=34,ticks6000=1000000,manual=1,night=1,
        nameGlyphs=new[]{162,163,164,221,221},splits=new[]{250000,500000,750000,1000000}
    };
    [Serializable] sealed class CapacityReport {public bool snapshotAccepted;public int snapshotRows,restoredPendingRows;public string[] checks;public string scope="Offline full server-shaped board and pending-queue restore; isolated cache, no HTTP/native game.";}
    [Serializable] sealed class PendingFixture {public Idas3CommunityTimes.Run[] runs;}
    public static void RunCapacity()
    {
        string output=Path.GetFullPath(Environment.GetEnvironmentVariable("IDAS3_COMMUNITY_REFRESH_AUDIT")??"");
        if(!File.Exists(Path.Combine(output,"PRIVATE_TEST_ONLY.txt")))throw new InvalidOperationException("Private marker required");
        string proof=Path.GetDirectoryName(output),storage=Path.Combine(output,"community");Directory.CreateDirectory(storage);
        File.WriteAllText(Path.Combine(output,"ISOLATED_MODE_FLOW_TEST.txt"),"Private pending-queue recovery check.");
        File.Copy(Path.Combine(proof,"pending.json"),Path.Combine(storage,"pending.json"));
        var data=JsonUtility.FromJson<Idas3CommunityTimes.Snapshot>(File.ReadAllText(Path.Combine(proof,"snapshot.json")));
        var usable=typeof(Idas3CommunityTimes).GetMethod("UsableCommunitySnapshot",BindingFlags.Static|BindingFlags.NonPublic);
        var settings=new Idas3GameOptions();typeof(Idas3GameOptions).GetField("current",Private).SetValue(settings,new Idas3GameOptions.Values {communityTimes=true});
        var go=new GameObject("Offline capacity check");
        try {
            var owner=go.AddComponent<Idas3CommunityTimes>();
            owner.Initialize(null,settings,go.AddComponent<Idas3PauseMenu>(),storage,true);
            var count=typeof(Idas3CommunityTimes).GetProperty("PendingCount",Private);
            var result=new CapacityReport {snapshotRows=data.entries.Length,snapshotAccepted=(bool)usable.Invoke(null,new object[]{data}),restoredPendingRows=(int)count.GetValue(owner)};
            File.WriteAllText(Path.Combine(output,"report.json"),JsonUtility.ToJson(result,true));
            if(!result.snapshotAccepted||result.restoredPendingRows!=2450)throw new Exception("Full community board/queue rejected or truncated");
            var checks=new List<string>();Action<bool,string> check=(pass,label)=>{if(!pass)throw new Exception(label);checks.Add(label);};
            var original=JsonUtility.FromJson<PendingFixture>(File.ReadAllText(Path.Combine(proof,"pending.json")));
            var restored=JsonUtility.FromJson<PendingFixture>(File.ReadAllText(Path.Combine(storage,"pending.json")));
            for(int i=0;i<original.runs.Length;i++)check(JsonUtility.ToJson(original.runs[i])==JsonUtility.ToJson(restored.runs[i]),"Restored pending row intact: "+i);
            var faster=JsonUtility.FromJson<Idas3CommunityTimes.Run>(JsonUtility.ToJson(original.runs[original.runs.Length-1]));
            faster.id=Guid.NewGuid().ToString();faster.ticks6000--;faster.splits[3]--;
            check(Idas3TimesOnlyChecks.Commit(owner,faster)&&(int)count.GetValue(owner)==2450,"Faster replacement remains possible in a large restored queue");
            check(!Idas3TimesOnlyChecks.Commit(owner,original.runs[original.runs.Length-1])&&(int)count.GetValue(owner)==2450,"Slower result cannot overwrite the pending best");
            // Use a separately restored queue; never mutate a live writer's
            // state through reflection while its immutable snapshots are in use.
            string partialRoot=Path.Combine(output,"partial"),partialStorage=Path.Combine(partialRoot,"community");Directory.CreateDirectory(partialStorage);
            File.WriteAllText(Path.Combine(partialRoot,"ISOLATED_MODE_FLOW_TEST.txt"),"Private smaller queue.");
            var partialRows=new Idas3CommunityTimes.Run[2112];Array.Copy(original.runs,partialRows,partialRows.Length);
            File.WriteAllText(Path.Combine(partialStorage,"pending.json"),JsonUtility.ToJson(new PendingFixture {runs=partialRows}));
            var smaller=go.AddComponent<Idas3CommunityTimes>();smaller.Initialize(null,settings,go.GetComponent<Idas3PauseMenu>(),partialStorage,true);
            check(Idas3TimesOnlyChecks.Commit(smaller,faster)&&(int)count.GetValue(smaller)==2113,"New key can extend a queue past the obsolete limit");
            restored=JsonUtility.FromJson<PendingFixture>(File.ReadAllText(Path.Combine(partialStorage,"pending.json")));
            check(restored.runs.Length==2113&&restored.runs[2112].id==faster.id,"New queued result survives disk serialization");
            var cap=(int)typeof(Idas3CommunityTimes).GetProperty("MaximumSnapshotEntries",BindingFlags.Static|BindingFlags.NonPublic).GetValue(null);
            check(cap==Idas3CourseCatalog.ConditionCount*2*44,"Snapshot capacity follows the catalog and server retention");
            data.entries=new Idas3CommunityTimes.Run[cap+1];
            for(int i=0;i<data.entries.Length;i++)data.entries[i]=original.runs[0];
            check(!(bool)usable.Invoke(null,new object[]{data}),"Oversized snapshot remains rejected");
            Array.Resize(ref data.entries,cap);check((bool)usable.Invoke(null,new object[]{data}),"Exact snapshot capacity accepted");
            result.checks=checks.ToArray();File.WriteAllText(Path.Combine(output,"report.json"),JsonUtility.ToJson(result,true));
            UnityEngine.Debug.Log("COMMUNITY_CAPACITY_PASS");
        }finally{UnityEngine.Object.DestroyImmediate(go);}
    }
}
