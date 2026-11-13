using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using UnityEngine;

public sealed partial class Idas3SceneSmoke
{
    [System.Runtime.InteropServices.DllImport("Idas3Unity",CallingConvention=System.Runtime.InteropServices.CallingConvention.Cdecl)]
    static extern int Idas3SceneModeFlowFixture(int scene);
    private IEnumerator CommunityDisplayCheck()
    {
        File.WriteAllText(Path.Combine(root,"ISOLATED_MODE_FLOW_TEST.txt"),"Private combined ranking cache check.");
        Check(Idas3SceneModeFlowFixture(-18)==1,"Full personal/community display cache matches prior rankings");
        Check(Idas3SceneModeFlowFixture(-9)==1,"Ranking ownership, real save selection and race targets remain correct");
        yield return Frames(2);Finish(true,null);
    }
    [System.Runtime.InteropServices.DllImport("Idas3Unity",CallingConvention=System.Runtime.InteropServices.CallingConvention.Cdecl)]
    static extern int Idas3SharedSetRecords(int[] values,int count,int enabled);
    [System.Runtime.InteropServices.DllImport("Idas3Unity",CallingConvention=System.Runtime.InteropServices.CallingConvention.Cdecl)]
    static extern int Idas3SceneSharedRecordsDiagnostic(int[] values,int capacity);
    [Serializable] sealed class CapacityObservation {public int rows,result;public double milliseconds;}
    [Serializable] sealed class CapacityPlayerReport {public CapacityObservation[] applications;public int comparedFields;}
    private IEnumerator CommunityCapacityCheck()
    {
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-community-refresh-fixture");
        Check(at>=0&&at+1<args.Length,"Capacity fixture required");
        var expected=JsonUtility.FromJson<Idas3CommunityTimes.Snapshot>(File.ReadAllText(Path.GetFullPath(args[at+1])));
        Check(expected.entries.Length==3080,"Server-shaped fixture includes all 70 valid boards");
        // Bypass managed flatten validation once to isolate the native bridge's
        // independent capacity failure against the previous accepted plugin.
        var data=new int[expected.entries.Length*14];int cursor=0;
        foreach(var e in expected.entries){data[cursor++]=e.condition;data[cursor++]=e.weather;data[cursor++]=e.car;data[cursor++]=e.ticks6000;
            foreach(int n in e.nameGlyphs)data[cursor++]=n;data[cursor++]=e.manual;data[cursor++]=e.night;
            for(int i=0;i<3;i++)data[cursor++]=e.splits[i];}
        var observations=new List<CapacityObservation>();
        int passes=Array.IndexOf(args,"-idas3-community-apply-timing")>=0?9:1;
        for(int pass=0;pass<passes;pass++)foreach(int count in new[]{2000,3080}) {
            var watch=Stopwatch.StartNew();int result=Idas3SharedSetRecords(data,count,1);watch.Stop();
            observations.Add(new CapacityObservation {rows=count,result=result,milliseconds=watch.Elapsed.TotalMilliseconds});
            File.WriteAllText(Path.Combine(root,"capacity.json"),JsonUtility.ToJson(new CapacityPlayerReport {applications=observations.ToArray()},true));
            Check(result==1,"Native bridge accepts "+count+" valid rows");
        }
        Check(Idas3CommunityTimes.UsableCommunitySnapshot(expected),"Managed validation accepts the complete board");
        var flattened=Idas3CommunityTimes.Flatten(expected);Check(flattened.Length==data.Length,"Complete flatten size");
        for(int i=0;i<data.Length;i++)Check(data[i]==flattened[i],"Flatten field "+i);
        var actual=new int[data.Length];Check(Idas3SceneSharedRecordsDiagnostic(actual,actual.Length)==expected.entries.Length,"Native retained all rows");
        for(int i=0;i<data.Length;i++)Check(data[i]==actual[i],"Native ranking field "+i);
        foreach(var invalid in new[]{-1,Idas3CourseCatalog.ConditionCount*2*44+1})
            Check(Idas3SharedSetRecords(data,invalid,1)==0,"Out-of-bounds count rejected before reading memory");
        Check(Idas3SharedSetRecords(null,1,1)==0,"Missing shared rows rejected");
        var bad=(int[])data.Clone();bad[bad.Length-14]=Idas3CourseCatalog.ConditionCount;
        Check(Idas3SharedSetRecords(bad,expected.entries.Length,1)==0,"Malformed final row rejected atomically");
        Check(Idas3SceneSharedRecordsDiagnostic(actual,actual.Length)==expected.entries.Length,"Failure retained prior board");
        for(int i=0;i<data.Length;i++)Check(data[i]==actual[i],"Failure preserved ranking field "+i);
        Check(Idas3SharedSetRecords(Array.Empty<int>(),0,1)==1&&Idas3SceneSharedRecordsDiagnostic(Array.Empty<int>(),0)==0,"Empty current board clears old rows");
        File.WriteAllText(Path.Combine(root,"capacity.json"),JsonUtility.ToJson(new CapacityPlayerReport {applications=observations.ToArray(),comparedFields=data.Length*3},true));
        yield return Frames(2);Finish(true,null);
    }
    [Serializable] sealed class CommunityRefreshSample {
        public int frames,entries;
        public ulong firstTick,lastTick;
        public double maxMainStepMs,elapsedMs;
        public float speed;
    }
    [Serializable] sealed class CommunityRefreshReport {
        public string scope="Private synthetic responses during real native Akina simulation. Hidden muted player; no HTTP or personal saves. Main-step timing excludes worker cost and does not measure displayed FPS.";
        public CommunityRefreshSample[] samples;
        public bool destroyedOwnerCancelled;
    }
    private IEnumerator CommunityRefreshCheck()
    {
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-community-refresh-fixture");
        Check(at>=0&&at+1<args.Length,"Community refresh fixture required");
        var path=Path.GetFullPath(args[at+1]);Check(new FileInfo(path).Length<=Idas3CommunityTimes.MaximumResponseBytes,"Private fixture size");
        byte[] body=File.ReadAllBytes(path);
        var expected=JsonUtility.FromJson<Idas3CommunityTimes.Snapshot>(System.Text.Encoding.UTF8.GetString(body));
        Check(expected.entries.Length==2000&&Idas3CommunityTimes.UsableCommunitySnapshot(expected),"Valid 2,000-entry response fixture");
        QualitySettings.vSyncCount=0;Application.targetFrameRate=-1;
        yield return Key(116);
        for(int n=0;host.Status.racePhase!=2&&n<1800;n++)yield return null;
        Check(host.Status.racePhase==2,"Akina countdown completed");held=87;yield return Frames(120);
        Check(host.Status.speedMetresPerSecond>1,"Car accelerated before response processing");
        File.WriteAllText(Path.Combine(root,"ISOLATED_MODE_FLOW_TEST.txt"),"Private community response diagnostic.");
        var owner=host.gameObject.AddComponent<Idas3CommunityTimes>();
        owner.Initialize(host,host.GameOptions,host.PauseMenu,Path.Combine(root,"community"),true);
        var snapshotField=typeof(Idas3CommunityTimes).GetField("snapshot",BindingFlags.Instance|BindingFlags.NonPublic);
        var confirmedField=typeof(Idas3CommunityTimes).GetField("confirmedUntil",BindingFlags.Instance|BindingFlags.NonPublic);
        var samples=new List<CommunityRefreshSample>();
        for(int run=0;run<8;run++) {
            var sample=new CommunityRefreshSample {firstTick=host.Status.simulationTicks};
            var all=Stopwatch.StartNew();var iterator=owner.ProcessSnapshot(200,body);
            while(true) {
                var step=Stopwatch.StartNew();bool more=iterator.MoveNext();step.Stop();
                sample.maxMainStepMs=Math.Max(sample.maxMainStepMs,step.Elapsed.TotalMilliseconds);
                if(!more)break;
                Check(all.Elapsed.TotalSeconds<30,"Background response deadline");sample.frames++;yield return null;
            }
            sample.elapsedMs=all.Elapsed.TotalMilliseconds;sample.lastTick=host.Status.simulationTicks;
            sample.speed=host.Status.speedMetresPerSecond;
            var current=(Idas3CommunityTimes.Snapshot)snapshotField.GetValue(owner);sample.entries=current?.entries?.Length??0;
            Check(sample.entries==2000&&sample.lastTick>sample.firstTick,"Race advances while valid records are processed");
            var packed=Idas3CommunityTimes.Flatten(current);var reference=Idas3CommunityTimes.Flatten(expected);
            Check(packed.Length==reference.Length,"Complete ranking payload retained");
            for(int i=0;i<packed.Length;i++)Check(packed[i]==reference[i],"Ranking field preserved "+i);
            samples.Add(sample);yield return Frames(30);
        }
        var pending=owner.ProcessSnapshot(200,body);Check(pending.MoveNext(),"Response yields before destruction");
        Destroy(owner);yield return null;
        var shutdown=Stopwatch.StartNew();while(pending.MoveNext()){Check(shutdown.Elapsed.TotalSeconds<30,"Cancelled response deadline");yield return null;}
        Check((double)confirmedField.GetValue(owner)==0,"Actual player destruction prevents late response publication");
        File.WriteAllText(Path.Combine(root,"community-refresh.json"),JsonUtility.ToJson(new CommunityRefreshReport {samples=samples.ToArray(),destroyedOwnerCancelled=true},true));
        held=0;Finish(true,null);
    }
}
