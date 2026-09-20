using System;
using System.IO;
using System.Linq;
using UnityEngine;
public static class Idas3PlayerReplayTests
{
    static int checks;
    static void Check(bool ok,string why){if(!ok)throw new Exception(why);checks++;}
    public static void Run(){
        Idas3ReplayDetailTests.Run();
        string root=Path.GetFullPath("Verification/player-replays-20260919");Directory.CreateDirectory(root);
        var v=new Idas3GameOptions.Values{replayTimeAttack=false,replayOnline=false,replayLegend=false,communityTimes=true};
        Check(v.TimeAttackReplayRequired&&Idas3ReplayLibrary.RecordingFlags(v)==8,"Sharing must force capture");
        v.communityTimes=false;Check(!v.TimeAttackReplayRequired&&Idas3ReplayLibrary.RecordingFlags(v)==0,"All optional recording disabled");
        v.replayOnline=v.replayLegend=true;Check(Idas3ReplayLibrary.RecordingFlags(v)==6,"Personal flags separate from upload flag");
        var restored=JsonUtility.FromJson<Idas3GameOptions.Values>(JsonUtility.ToJson(v));Check(restored.replayOnline&&restored.replayLegend&&!restored.communityTimes,"Options round trip");
        foreach(string mode in new[]{"legend","online"}){
            string fixture=Path.Combine(root,"capture",mode);
            var m=JsonUtility.FromJson<Idas3ReplayData.Details>(File.ReadAllText(fixture+".json"));
            string saved=Idas3ReplayLibrary.Save(Path.Combine(root,"library"),m,File.ReadAllBytes(fixture+"-player.idr"),File.ReadAllBytes(fixture+"-opponent.idr"));
            var r=Idas3ReplayData.Load(saved);Check(r.Opponent!=null&&r.Detailed&&r.Opponent.Detailed,"Both detailed tracks persisted locally");
            Check(r.Frames.Length==r.Opponent.Frames.Length,"Synchronized tracks");
            foreach(int i in new[]{0,r.Frames.Length/2,r.Frames.Length-1}){
                Check(r.Sample(r.Frames[i].tick/60.0).state.SequenceEqual(r.Frames[i].state),"Player state exact");
                Check(r.Opponent.Sample(r.Opponent.Frames[i].tick/60.0).state.SequenceEqual(r.Opponent.Frames[i].state),"Opponent state exact");
            }
            Check(mode!="online"||r.Metadata.opponentTelemetry==1,"Online exact telemetry marker");
            File.Copy(saved,Path.Combine(root,mode+".idreplay"),true);
            var packet=File.ReadAllBytes(saved);Array.Resize(ref packet,packet.Length-1);
            bool rejected=false;try{Idas3ReplayData.Parse(packet);}catch(Exception){rejected=true;}Check(rejected,"Truncated opponent stream accepted");
        }
        Check(Idas3ReplayLibrary.Files(Path.Combine(root,"library")).Length>=2,"Local library discovery");
        File.WriteAllText(Path.Combine(root,"parser-tests.json"),"{\"passed\":true,\"checks\":"+checks+"}");Debug.Log("Player replay tests passed: "+checks);
    }
    public static void BuildVerified(){Run();Idas3Build.BuildSadamineStaging();}
    public static void RebuildVerified(){Run();Idas3Build.RebuildSadamineStagingScripts();}
}
