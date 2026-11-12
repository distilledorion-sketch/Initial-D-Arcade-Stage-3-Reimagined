using System;
using System.IO;
using System.Linq;
using UnityEngine;

// Explicit offline codec audit. Uses a native capture driven by the original
// engine controller; older revisions must never acquire guessed flash timing.
public static class Idas3ReplayBackfireAudit {
    [Serializable] sealed class Report {public bool passed;public int checks,flashes;public string error;}
    static int checks;
    static void Check(bool value,string why){++checks;if(!value)throw new Exception(why);}
    static uint Word(byte[] data,int at)=>BitConverter.ToUInt32(data,at);
    static void Put(byte[] data,int at,uint value)=>BitConverter.GetBytes(value).CopyTo(data,at);
    public static void RunAndBuild(){Run();Idas3Build.RebuildPrivateDiagnosticScripts();}
    public static void Run(){
        string input=Environment.GetEnvironmentVariable("IDAS3_REPLAY_BACKFIRE_INPUT"),output=Environment.GetEnvironmentVariable("IDAS3_REPLAY_BACKFIRE_REPORT");
        if(string.IsNullOrEmpty(input)||string.IsNullOrEmpty(output)||File.Exists(output))throw new InvalidOperationException("Native input and NEW report required");
        var report=new Report();checks=0;
        try{
            var raw=File.ReadAllBytes(input);
            var meta=new Idas3ReplayData.Details{condition=6,night=1,car=19,manual=1,ticks6000=(int)Word(raw,4),outcome=2};
            Idas3ReplayData Parse(byte[] bytes,byte[] other=null){
                var m=JsonUtility.FromJson<Idas3ReplayData.Details>(JsonUtility.ToJson(meta));
                if(other!=null){m.mode=1;m.opponentCar=19;m.opponentBytes=other.Length;}
                return Idas3ReplayData.Parse(Idas3ReplayLibrary.Package(m,bytes,other??Array.Empty<byte>()));
            }
            var replay=Parse(raw);int count=(int)Word(raw,8);
            Check(Idas3ReplayCodec.MaxRaw>=100L+108000L*168+109024L*136,"Maximum revision4 exceeds codec limit");
            Check(replay.Frames.Length==count&&replay.SmokeFrames.Length==0&&replay.DustFrames.Length==0,"No-effect recording invented smoke");
            var compressed=Idas3ReplayCodec.Encode(raw);Check(Idas3ReplayCodec.Decode(compressed).SequenceEqual(raw),"Compression changed recorded states");
            var zipped=Parse(compressed);
            for(int i=0;i<count;i++){
                uint wanted=Word(raw,100+i*168+164);Check(replay.Frames[i].backfire==wanted&&zipped.Frames[i].backfire==wanted,"Recorded flash lost in parser");
                report.flashes+=wanted>0?1:0;
            }
            Check(report.flashes>0,"Fixture has no accepted flashes");
            foreach(int fps in new[]{30,60,144,240})for(int paint=0;paint<fps*3;paint++){
                var p=replay.Sample(paint/(double)fps);Check(p.backfire==replay.Frames[(int)p.tick-1].backfire,"Render interpolation changed discrete flash state");
            }
            foreach(double time in new[]{1.02,2.7,.5,1.02,1.02,3,0}){
                var p=replay.Sample(time);Check(p.backfire==replay.Frames[(int)p.tick-1].backfire,"Backward/repeated seek changed flash state");
            }
            // An independently timed opponent stream stays separate, including
            // compressed packaging. No second world-effects timeline is needed.
            var other=(byte[])raw.Clone();for(int i=0;i<count;i++)Put(other,100+i*168+164,(uint)((i+1)%3));
            var pair=Parse(compressed,Idas3ReplayCodec.Encode(other));
            for(int i=0;i<count;i++)Check(pair.Frames[i].backfire==replay.Frames[i].backfire&&pair.Opponent.Frames[i].backfire==(uint)((i+1)%3),"Paired flash clocks mixed");
            var old=new byte[96+160*count];Array.Copy(raw,old,96);Put(old,16,96);Put(old,20,160);Put(old,24,1);Put(old,92,0);
            for(int i=0;i<count;i++)Array.Copy(raw,100+i*168,old,96+i*160,160);
            Check(Parse(old).Frames.All(p=>p.backfire==0),"Old replay acquired fabricated flashes");
            foreach(Action<byte[]> mutate in new Action<byte[]>[]{
                b=>Put(b,16,96),b=>Put(b,20,164),b=>Put(b,24,5),b=>Put(b,92,1),
                b=>Put(b,96,1),b=>Put(b,96,uint.MaxValue),b=>Put(b,100+160,1),b=>Put(b,100+164,3),b=>Put(b,100+164,uint.MaxValue)
            }){var bad=(byte[])raw.Clone();mutate(bad);bool rejected=false;try{Parse(bad);}catch(InvalidDataException){rejected=true;}Check(rejected,"Malformed flash recording accepted");}
            bool truncated=false;try{Parse(raw.Take(raw.Length-1).ToArray());}catch(InvalidDataException){truncated=true;}Check(truncated,"Truncated flash state accepted");
            // Existing original revision2/revision3 source fixtures remain an
            // independent compatibility check of the revised parser.
            Idas3ReplayCarSmokeAudit.Run();
            report.passed=true;Debug.Log("PASS replay backfire checks="+checks);
        }catch(Exception e){report.error=e.ToString();throw;}
        finally{report.checks=checks;File.WriteAllText(output,JsonUtility.ToJson(report,true));}
    }
}
