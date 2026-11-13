using System;
using System.IO;
using System.Linq;
using System.Text;
using UnityEngine;

// Explicit private check of actual App-produced battle recordings through the
// same compression, atomic library save, parser and sampler used by the game.
public static class Idas3OnlineReplayAudit {
    [Serializable] sealed class Report { public bool passed;public int checks,races,tracks;public string error; }
    static int checks;
    static void Check(bool value,string message){++checks;if(!value)throw new InvalidOperationException(message);}
    static (Idas3ReplayData.Details,byte[],byte[]) Split(byte[] bytes){
        int n=BitConverter.ToInt32(bytes,0);
        var meta=JsonUtility.FromJson<Idas3ReplayData.Details>(Encoding.UTF8.GetString(bytes,4,n));
        return (meta,bytes.Skip(4+n).Take(bytes.Length-4-n-meta.opponentBytes).ToArray(),bytes.Skip(bytes.Length-meta.opponentBytes).ToArray());
    }
    public static void Run(){
        string input=Environment.GetEnvironmentVariable("IDAS3_ONLINE_REPLAY_INPUT"),output=Environment.GetEnvironmentVariable("IDAS3_ONLINE_REPLAY_OUTPUT");
        if(string.IsNullOrEmpty(input)||string.IsNullOrEmpty(output)||Directory.Exists(output)||File.Exists(output))
            throw new InvalidOperationException("Native archives and a NEW private evidence directory required");
        string allowed=Path.GetFullPath(Path.Combine(Application.dataPath,"../Verification"))+Path.DirectorySeparatorChar;
        if(!Path.GetFullPath(output).StartsWith(allowed,StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("Private output must remain under Verification");
        Directory.CreateDirectory(output);var report=new Report();checks=0;
        try{
            var files=Directory.GetFiles(input,"*.idreplay");Check(files.Length==12,"Expected both slots for six actual race scenarios");
            foreach(string file in files){
                byte[] package=File.ReadAllBytes(file);var (metadata,player,opponent)=Split(package);
                var before=Idas3ReplayData.Parse(package);
                string saved=Idas3ReplayLibrary.Save(Path.Combine(output,Path.GetFileNameWithoutExtension(file)),metadata,player,opponent);
                var after=Idas3ReplayData.Load(saved);var (_,a,b)=Split(File.ReadAllBytes(saved));
                Check(Idas3ReplayCodec.Decode(a).SequenceEqual(player)&&Idas3ReplayCodec.Decode(b).SequenceEqual(opponent),"Library compression changed original recorded bytes");
                Check(after.Opponent!=null&&after.Metadata.mode==1&&after.Metadata.condition==before.Metadata.condition,"Saved battle identity changed");
                foreach(var pair in new[]{(before,after),(before.Opponent,after.Opponent)}){
                    var (source,restored)=pair;Check(source.Frames.Length==restored.Frames.Length,"Saved track length changed");
                    Check(source.SmokeFrames.SequenceEqual(restored.SmokeFrames)&&source.DustFrames.SequenceEqual(restored.DustFrames),"Saved effects timeline changed");
                    for(int i=0;i<source.Frames.Length;i++){
                        var x=source.Frames[i];var y=restored.Frames[i];
                        Check(x.tick==y.tick&&x.position==y.position&&x.yaw==y.yaw&&x.speed==y.speed&&x.gear==y.gear&&x.dustStep==y.dustStep&&x.state.SequenceEqual(y.state),"Saved vehicle/HUD/light state changed");
                    }
                    foreach(int hz in new[]{30,60,144,240})for(int i=0;i<=Math.Ceiling(source.Duration*hz);i++){
                        double at=(double)i/hz;var x=source.Sample(at);var y=restored.Sample(at);
                        Check(x.tick==y.tick&&x.position==y.position&&x.yaw==y.yaw&&x.speed==y.speed&&x.gear==y.gear&&x.dustStep==y.dustStep&&x.state.SequenceEqual(y.state),"Compressed sampling diverged");
                    }
                    ++report.tracks;
                }
                Check(after.Opponent.SmokeFrames.Length==0&&after.Opponent.DustFrames.Length==0,"Opponent duplicated the root world timeline");
                Check((after.Metadata.condition>=18)==(after.SmokeFrames.Length==0),"Imported/original effect format changed");
                ++report.races;
            }
            report.passed=true;Debug.Log("PASS actual online replay library checks: "+checks);
        }catch(Exception e){report.error=e.ToString();throw;}
        finally{report.checks=checks;File.WriteAllText(Path.Combine(output,"report.json"),JsonUtility.ToJson(report,true));}
    }
}
