using System;
using System.IO;
using System.Reflection;
using UnityEngine;

public static class Idas3TimesOnlyChecks {
    const BindingFlags Private=BindingFlags.Instance|BindingFlags.NonPublic;
    static int checks;
    static int PendingCount(Idas3CommunityTimes uploader)=>(int)typeof(Idas3CommunityTimes).GetProperty("PendingCount",Private).GetValue(uploader);
    static void Check(bool condition,string message){if(!condition)throw new Exception(message);checks++;}
    static Idas3CommunityTimes.Run RunData()=>new Idas3CommunityTimes.Run{
        id=Guid.NewGuid().ToString(),ruleset=Idas3CommunityTimes.Ruleset,build=Idas3CommunityTimes.RequiredSubmissionBuild,
        epoch=2,condition=35,weather=1,car=34,ticks6000=1200001,manual=1,night=1,points=100,
        nameGlyphs=new[]{162,163,164,221,221},splits=new[]{300000,600000,900000,1200001}
    };
    public static void Run(){
        var output=Path.GetFullPath("Verification/leaderboard-times-only-20261018");Directory.CreateDirectory(output);
        var folder=Path.Combine(output,"client-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(folder);
        var go=new GameObject("Time-only isolated checks");
        try{
            var settings=new Idas3GameOptions.Values{communityTimes=true,replayTimeAttack=false,replayOnline=false,replayLegend=false};
            Check(!settings.TimeAttackReplayRequired&&Idas3ReplayLibrary.RecordingFlags(settings)==0,"Sharing does not force personal recording");
            settings.replayTimeAttack=true;Check(settings.TimeAttackReplayRequired&&Idas3ReplayLibrary.RecordingFlags(settings)==1,"Optional personal Time Attack recording still works");
            settings.replayOnline=settings.replayLegend=true;Check(Idas3ReplayLibrary.RecordingFlags(settings)==7,"Personal recordings keep independent mode flags");
            var uploader=go.AddComponent<Idas3CommunityTimes>();
            typeof(Idas3CommunityTimes).GetField("folder",Private).SetValue(uploader,folder);
            var queue=typeof(Idas3CommunityTimes).GetMethod("QueueFinish",Private);
            var run=RunData();
            Check(Idas3CommunityTimes.Uploadable(run),"Fresh result must not need a recording");
            var json=Idas3CommunityTimes.TimeSubmissionJson(run);
            Check(json.Length<1024,"Small JSON result only");
            Check(JsonUtility.FromJson<Idas3CommunityTimes.Run>(json).ticks6000==1200001,"Exact finish precision");
            Check((bool)queue.Invoke(uploader,new object[]{run}),"Commit result without native replay access");
            Check(PendingCount(uploader)==1&&File.Exists(Path.Combine(folder,"pending.json")),"Durable pending time");
            Check(Directory.GetFiles(folder,"*.idr").Length==0,"No replay upload cache created");
            var slower=RunData();slower.ticks6000++;slower.splits[3]++;
            Check(!(bool)queue.Invoke(uploader,new object[]{slower})&&PendingCount(uploader)==1,"Slower pending result does not replace best");
            var faster=RunData();faster.ticks6000--;faster.splits[3]--;
            Check((bool)queue.Invoke(uploader,new object[]{faster})&&PendingCount(uploader)==1,"Faster pending result replaces best");
            Check(File.ReadAllText(Path.Combine(folder,"pending.json")).Contains(faster.id),"Replacement saved atomically");
            var oldReplay=Path.Combine(folder,faster.id+".idr");File.WriteAllText(oldReplay,"obsolete cache");
            var personal=Path.Combine(folder,"personal.idreplay");File.WriteAllText(personal,"keep personal replay");
            typeof(Idas3CommunityTimes).GetMethod("CleanReplayFiles",Private).Invoke(uploader,null);
            Check(!File.Exists(oldReplay)&&File.Exists(personal)&&PendingCount(uploader)==1,"Cleanup removes only leaderboard replay cache");
            run.replayVersion=2;Check(Idas3CommunityTimes.Uploadable(run),"Legacy pending time needs no replay file");
            var snapshot=new Idas3CommunityTimes.Snapshot{ruleset=run.ruleset,epoch=2,entries=new[]{run}};
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
}
