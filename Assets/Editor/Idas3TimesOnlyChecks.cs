using System;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;

public static class Idas3TimesOnlyChecks {
    const BindingFlags Private=BindingFlags.Instance|BindingFlags.NonPublic;
    static int checks;
    static int PendingCount(Idas3CommunityTimes uploader)=>(int)typeof(Idas3CommunityTimes).GetProperty("PendingCount",Private).GetValue(uploader);
    internal static bool Commit(Idas3CommunityTimes uploader,Idas3CommunityTimes.Run run){
        var task=(Task)typeof(Idas3CommunityTimes).GetMethod("QueueFinish",Private).Invoke(uploader,new object[]{run});
        var watch=System.Diagnostics.Stopwatch.StartNew();while(!task.IsCompleted){if(watch.Elapsed.TotalSeconds>30)throw new TimeoutException("Pending queue write");Thread.Sleep(1);}
        Check((bool)typeof(Idas3CommunityTimes).GetMethod("PublishPending",Private).Invoke(uploader,new object[]{task}),"Queue write committed");
        var result=task.GetType().GetProperty("Result").GetValue(task);return (bool)result.GetType().GetField("changed").GetValue(result);
    }
    static void Check(bool condition,string message){if(!condition)throw new Exception(message);checks++;}
    static Idas3CommunityTimes.Run RunData()=>new Idas3CommunityTimes.Run{
        id=Guid.NewGuid().ToString(),ruleset=Idas3CommunityTimes.Ruleset,build=Idas3CommunityTimes.RequiredSubmissionBuild,
        epoch=Idas3CommunityTimes.CurrentSeasonEpoch,condition=35,weather=1,car=34,ticks6000=1200001,manual=1,night=1,points=100,
        nameGlyphs=new[]{162,163,164,221,221},splits=new[]{300000,600000,900000,1200001}
    };
    public static void Run(){
        checks=0;var output=Path.GetFullPath(Environment.GetEnvironmentVariable("IDAS3_TIMES_ONLY_AUDIT")??"Verification/leaderboard-times-only-20261018");Directory.CreateDirectory(output);
        var folder=Path.Combine(output,"client-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(folder);
        var go=new GameObject("Time-only isolated checks");
        try{
            var settings=new Idas3GameOptions.Values{communityTimes=true,replayTimeAttack=false,replayOnline=false,replayLegend=false};
            Check(!settings.TimeAttackReplayRequired&&Idas3ReplayLibrary.RecordingFlags(settings)==0,"Sharing does not force personal recording");
            settings.replayTimeAttack=true;Check(settings.TimeAttackReplayRequired&&Idas3ReplayLibrary.RecordingFlags(settings)==1,"Optional personal Time Attack recording still works");
            settings.replayOnline=settings.replayLegend=true;Check(Idas3ReplayLibrary.RecordingFlags(settings)==7,"Personal recordings keep independent mode flags");
            var uploader=go.AddComponent<Idas3CommunityTimes>();
            typeof(Idas3CommunityTimes).GetField("folder",Private).SetValue(uploader,folder);
            var run=RunData();
            Check(Idas3CommunityTimes.Uploadable(run),"Fresh result must not need a recording");
            var json=Idas3CommunityTimes.TimeSubmissionJson(run);
            Check(json.Length<1024,"Small JSON result only");
            Check(JsonUtility.FromJson<Idas3CommunityTimes.Run>(json).ticks6000==1200001,"Exact finish precision");
            Check(Commit(uploader,run),"Commit result without native replay access");
            Check(PendingCount(uploader)==1&&File.Exists(Path.Combine(folder,"pending.json")),"Durable pending time");
            Check(Directory.GetFiles(folder,"*.idr").Length==0,"No replay upload cache created");
            var slower=RunData();slower.ticks6000++;slower.splits[3]++;
            Check(!Commit(uploader,slower)&&PendingCount(uploader)==1,"Slower pending result does not replace best");
            var faster=RunData();faster.ticks6000--;faster.splits[3]--;
            Check(Commit(uploader,faster)&&PendingCount(uploader)==1,"Faster pending result replaces best");
            Check(File.ReadAllText(Path.Combine(folder,"pending.json")).Contains(faster.id),"Replacement saved atomically");
            var oldReplay=Path.Combine(folder,faster.id+".idr");File.WriteAllText(oldReplay,"obsolete cache");
            var personal=Path.Combine(folder,"personal.idreplay");File.WriteAllText(personal,"keep personal replay");
            typeof(Idas3CommunityTimes).GetMethod("CleanReplayFiles",Private).Invoke(uploader,null);
            Check(!File.Exists(oldReplay)&&File.Exists(personal)&&PendingCount(uploader)==1,"Cleanup removes only leaderboard replay cache");
            run.replayVersion=2;Check(Idas3CommunityTimes.Uploadable(run),"Legacy pending time needs no replay file");
            var snapshot=new Idas3CommunityTimes.Snapshot{ruleset=run.ruleset,epoch=Idas3CommunityTimes.CurrentSeasonEpoch,entries=new[]{run}};
            var valid=typeof(Idas3CommunityTimes).GetMethod("UsableCommunitySnapshot",BindingFlags.Static|BindingFlags.NonPublic);
            Check((bool)valid.Invoke(null,new object[]{snapshot}),"Time-only snapshot remains visible");
            Check(Idas3CommunityTimes.Flatten(snapshot)[3]==run.ticks6000,"Native ranking precision retained");
            run.imported=1;Check(!Idas3CommunityTimes.Uploadable(run),"Historical import rejected");run.imported=0;
            run.build="0.3.95-community-replays.49";Check(!Idas3CommunityTimes.Uploadable(run),"Exact build gate retained");run.build=Idas3CommunityTimes.RequiredSubmissionBuild;
            run.splits[0]=0;Check(!Idas3CommunityTimes.Uploadable(run),"Incomplete checkpoints rejected");
            File.WriteAllText(Path.Combine(output,"client-PASS.txt"),"PASS "+checks+" time-only client checks\n");
            Debug.Log("TIMES_ONLY_PASS "+checks);
        }finally{UnityEngine.Object.DestroyImmediate(go);}
    }
    [Serializable] sealed class QueueFixture {public Idas3CommunityTimes.Run[] runs;}
    [Serializable] sealed class SeasonReport {public bool passed;public int checks;public string build;public int epoch,localSeason;public string scope;}
    [Serializable] sealed class JournalHashFixture {public string sha256;}
    public static void RunSeason2Verification(){
        RunSeason2();Run();RunSeason2JournalHash();
        string suite=Environment.GetEnvironmentVariable("IDAS3_COMMUNITY_SEASON_SUITE");
        if(!string.IsNullOrEmpty(suite)){
            Environment.SetEnvironmentVariable("IDAS3_COMMUNITY_REFRESH_AUDIT",Path.Combine(suite,"refresh"));Idas3CommunityRefreshChecks.RunCandidate();
            Environment.SetEnvironmentVariable("IDAS3_COMMUNITY_REFRESH_AUDIT",Path.Combine(suite,"capacity"));Idas3CommunityRefreshChecks.RunCapacity();
            Environment.SetEnvironmentVariable("IDAS3_COMMUNITY_REFRESH_AUDIT",Path.Combine(suite,"queue"));Idas3PendingQueueChecks.RunCandidate();
        }
    }
    public static void RunSeason2JournalHash(){
        string fixture=Path.GetFullPath(Environment.GetEnvironmentVariable("IDAS3_LEGACY_JOURNAL_FIXTURE")??throw new InvalidOperationException("Private legacy fixture required."));
        string output=Path.GetFullPath(Environment.GetEnvironmentVariable("IDAS3_SEASON2_COMMUNITY_AUDIT")??throw new InvalidOperationException("Private evidence output required."));
        var assembly=typeof(Idas3CommunityTimes).Assembly;
        var recordsType=assembly.GetType("Idas3.Multiplayer.Idas3MultiplayerRecords",true);
        var journalType=assembly.GetType("Idas3.Multiplayer.Idas3ResultJournal",true);
        var records=Activator.CreateInstance(recordsType,new object[]{Path.GetDirectoryName(fixture)});
        var journal=Activator.CreateInstance(journalType,BindingFlags.Instance|BindingFlags.NonPublic,null,new[]{records},null);
        var read=journalType.GetMethod("Read",BindingFlags.Instance|BindingFlags.NonPublic);
        var hash=journalType.GetMethod("Hash",BindingFlags.Static|BindingFlags.NonPublic);
        int verified=0;
        foreach(string path in new[]{fixture,Path.Combine(Path.GetDirectoryName(fixture),"online_result_v1.json")}){
            object state=read.Invoke(journal,new object[]{path});
            string expected=JsonUtility.FromJson<JournalHashFixture>(File.ReadAllText(path)).sha256;
            if((string)hash.Invoke(null,new[]{state})!=expected)throw new Exception("Unity journal hash mismatch: "+Path.GetFileName(path));
            int version=(int)state.GetType().GetField("version").GetValue(state);
            if(version!=(verified==0?1:2))throw new Exception("Expected original v1 and upgraded v2 fixtures.");
            verified++;
        }
        Directory.CreateDirectory(output);File.WriteAllText(Path.Combine(output,"unity-journal-hash-PASS.txt"),"PASS "+verified+" original/upgraded journal hashes using actual Unity JsonUtility; read-only fixture validation.\n");
        Debug.Log("SEASON2_JOURNAL_HASH_PASS "+verified);
    }
    public static void RunSeason2(){
        checks=0;
        var output=Path.GetFullPath(Environment.GetEnvironmentVariable("IDAS3_SEASON2_COMMUNITY_AUDIT")??"Verification/season2-release-20261113/client");
        Directory.CreateDirectory(output);
        var root=Path.Combine(output,"cache-"+Guid.NewGuid().ToString("N"));var folder=Path.Combine(root,"community");Directory.CreateDirectory(folder);
        File.WriteAllText(Path.Combine(root,"ISOLATED_MODE_FLOW_TEST.txt"),"Private offline season checks.");
        var type=typeof(Idas3CommunityTimes);
        var migrate=type.GetMethod("MigrateSeasonCache",BindingFlags.Static|BindingFlags.NonPublic);
        var stamp=type.GetMethod("PrepareFreshFinish",BindingFlags.Static|BindingFlags.NonPublic);
        var usable=type.GetMethod("UsableCommunitySnapshot",BindingFlags.Static|BindingFlags.NonPublic);
        var prepare=type.GetMethod("PrepareSnapshot",BindingFlags.Static|BindingFlags.NonPublic);
        Func<string,bool> reset=p=>(bool)migrate.Invoke(null,new object[]{p});
        Func<Idas3CommunityTimes.Run,string,bool> fresh=(r,b)=>(bool)stamp.Invoke(null,new object[]{r,b});
        Func<Idas3CommunityTimes.Snapshot,bool> valid=s=>(bool)usable.Invoke(null,new object[]{s});
        var stale=RunData();stale.build="0.3.95-community-replays.54";stale.epoch=2;
        string oldRun=JsonUtility.ToJson(stale);
        File.WriteAllText(Path.Combine(folder,"pending.json"),JsonUtility.ToJson(new QueueFixture{runs=new[]{stale}}));
        File.WriteAllText(Path.Combine(folder,"snapshot.json"),JsonUtility.ToJson(new Idas3CommunityTimes.Snapshot{ruleset=stale.ruleset,epoch=2,entries=new[]{stale}}));
        string identity="{\"token\":\""+new string('a',64)+"\"}";
        File.WriteAllText(Path.Combine(folder,"identity.json"),identity);
        string obsolete=Path.Combine(folder,stale.id+".idr"),personal=Path.Combine(folder,"personal.idreplay");
        File.WriteAllText(obsolete,"old upload");File.WriteAllText(personal,"personal recording");
        Check(reset(folder),"First Season 2 startup migrates old cache");
        Check(JsonUtility.FromJson<QueueFixture>(File.ReadAllText(Path.Combine(folder,"pending.json"))).runs.Length==0,"Old queued runs cleared");
        var empty=JsonUtility.FromJson<Idas3CommunityTimes.Snapshot>(File.ReadAllText(Path.Combine(folder,"snapshot.json")));
        Check(empty.epoch==Idas3CommunityTimes.CurrentSeasonEpoch&&empty.entries.Length==0&&valid(empty),"Old snapshot replaced with empty Season 2 namespace");
        Check(!File.Exists(obsolete)&&File.ReadAllText(personal)=="personal recording","Only obsolete leaderboard replay upload cache removed");
        Check(File.ReadAllText(Path.Combine(folder,"identity.json"))==identity,"Installation identity preserved");
        Check(!fresh(stale,Idas3CommunityTimes.RequiredSubmissionBuild)&&JsonUtility.ToJson(stale)==oldRun,"Old run cannot be relabeled or mutated");
        foreach(int epoch in new[]{0,1,2,4,int.MaxValue}){var r=RunData();r.epoch=epoch;Check(!Idas3CommunityTimes.Uploadable(r),"Wrong epoch rejected: "+epoch);}
        foreach(string build in new[]{null,"0.3.95-community-replays.54","0.3.95-community-replays.56","0.3.95-community-replays.055","0.3.95-community-replays.55 ","0.3.95-Community-replays.55"}){
            var r=RunData();r.build=build;Check(!Idas3CommunityTimes.Uploadable(r),"Submission requires exact declared build: "+build);
        }
        var native=RunData();native.id=native.ruleset=native.build=null;native.epoch=0;
        Check(!fresh(native,"0.3.95-community-replays.54")&&native.id==null&&native.epoch==0,"Old running build cannot stamp a fresh result");
        Check(fresh(native,Idas3CommunityTimes.RequiredSubmissionBuild)&&Idas3CommunityTimes.Uploadable(native)&&native.epoch==3,"Fresh native finish receives fixed new epoch without a downloaded snapshot");
        Check(!fresh(native,Idas3CommunityTimes.RequiredSubmissionBuild),"Already stamped result cannot be restamped");
        var board=new Idas3CommunityTimes.Snapshot{ruleset=native.ruleset,epoch=3,entries=new[]{native}};
        Check(valid(board),"Current Season 2 snapshot accepted");
        foreach(int epoch in new[]{1,2,4}){board.epoch=epoch;Check(!valid(board),"Old or future snapshot namespace rejected: "+epoch);}board.epoch=3;
        foreach(string build in new[]{"0.3.95-community-replays.54","0.3.95-community-replays.055","not-a-build"}){native.build=build;Check(!valid(board),"Old or malformed Season 2 snapshot build rejected: "+build);}
        native.build="0.3.95-community-replays.56";Check(valid(board)&&!Idas3CommunityTimes.Uploadable(native),"Future supported Season 2 results readable without relaxing exact upload gate");native.build=Idas3CommunityTimes.RequiredSubmissionBuild;
        native.epoch=2;Check(!valid(board),"Mixed-epoch row rejected");native.epoch=3;
        File.WriteAllText(Path.Combine(folder,"pending.json"),JsonUtility.ToJson(new QueueFixture{runs=new[]{native}}));
        File.WriteAllText(Path.Combine(folder,"snapshot.json"),JsonUtility.ToJson(board));
        string queued=File.ReadAllText(Path.Combine(folder,"pending.json")),cached=File.ReadAllText(Path.Combine(folder,"snapshot.json"));
        Check(!reset(folder)&&File.ReadAllText(Path.Combine(folder,"pending.json"))==queued&&File.ReadAllText(Path.Combine(folder,"snapshot.json"))==cached,"Repeated startup preserves this season's queued times and cache byte for byte");
        var oldEpoch=RunData();oldEpoch.epoch=2;
        var oldBuild=RunData();oldBuild.build="0.3.95-community-replays.54";
        File.WriteAllText(Path.Combine(folder,"pending.json"),JsonUtility.ToJson(new QueueFixture{runs=new[]{native,oldEpoch,oldBuild}}));
        var go=new GameObject("Offline Season 2 checks");
        try{
            var uploader=go.AddComponent<Idas3CommunityTimes>();
            uploader.Initialize(null,new Idas3GameOptions(),go.AddComponent<Idas3PauseMenu>(),folder,true);
            Check(PendingCount(uploader)==1,"Restore drops old build and epoch without relabeling them");
            var restored=JsonUtility.FromJson<QueueFixture>(File.ReadAllText(Path.Combine(folder,"pending.json")));
            Check(restored.runs[0].id==native.id&&restored.runs[0].build==native.build&&restored.runs[0].epoch==native.epoch,"Current queued run identity and provenance preserved");
            bool rejected=false;try{type.GetMethod("QueueFinish",Private).Invoke(uploader,new object[]{stale});}catch(TargetInvocationException e){rejected=e.InnerException is InvalidDataException;}
            Check(rejected&&PendingCount(uploader)==1,"Old queued result cannot enter live writer");
            byte[] oldBytes=System.Text.Encoding.UTF8.GetBytes(JsonUtility.ToJson(new Idas3CommunityTimes.Snapshot{ruleset=stale.ruleset,epoch=2,entries=new[]{stale}}));
            var work=(Task<Idas3CommunityTimes.Snapshot>)prepare.Invoke(null,new object[]{oldBytes,folder,3,CancellationToken.None});
            Check(work.Wait(30000)&&work.Result==null&&File.ReadAllText(Path.Combine(folder,"snapshot.json"))==cached,"Asynchronous old snapshot cannot overwrite current cache");
            type.GetMethod("OnDestroy",Private).Invoke(uploader,null);
        }finally{UnityEngine.Object.DestroyImmediate(go);}
        string failed=Path.Combine(root,"interrupted");Directory.CreateDirectory(failed);
        string lockedFile=Path.Combine(failed,"snapshot.json");File.WriteAllText(lockedFile,"old snapshot");
        using(var locked=new FileStream(lockedFile,FileMode.Open,FileAccess.Read,FileShare.None)){
            bool rejected=false;try{reset(failed);}catch(TargetInvocationException e){rejected=e.InnerException is IOException;}
            Check(rejected&&!File.Exists(Path.Combine(failed,"season.json")),"Interrupted migration never marks incomplete reset as complete");
        }
        Check(reset(failed)&&!reset(failed),"Interrupted migration retries once then remains idempotent");
        File.WriteAllText(Path.Combine(failed,"season.json"),"{bad json}");
        Check(reset(failed),"Malformed migration marker safely retries reset");
        Check(File.ReadAllText(Path.Combine(folder,"identity.json"))==identity&&File.ReadAllText(personal)=="personal recording","Migration and restore leave identity and personal recordings untouched");
        File.WriteAllText(Path.Combine(output,"season2-client-results.json"),JsonUtility.ToJson(new SeasonReport{
            passed=true,checks=checks,build=Idas3CommunityTimes.RequiredSubmissionBuild,epoch=Idas3CommunityTimes.CurrentSeasonEpoch,localSeason=Idas3CommunityTimes.LocalRecordSeason,
            scope="Offline production client migration, queue restore, exact upload gate and snapshot tests; no HTTP, live data or hardware."},true));
        Debug.Log("SEASON2_COMMUNITY_PASS "+checks);
    }
}
