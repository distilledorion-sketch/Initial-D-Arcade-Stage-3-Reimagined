using System;
using System.IO;
using System.Linq;
using UnityEngine;

// Explicit offline test of native-generated revision3 files through the
// shipping managed codec, parser and time sampler. No game/account is opened.
public static class Idas3ReplayCarSmokeAudit {
    public static void RunAndBuild(){Run();Idas3Build.RebuildPrivateDiagnosticScripts();}
    [Serializable] sealed class Report { public bool passed;public int checks,scenarios,liveRaces;public string error; }
    static int checks;
    static void Check(bool ok,string why){++checks;if(!ok)throw new Exception(why);}
    static uint Word(byte[] bytes,int at)=>BitConverter.ToUInt32(bytes,at);
    static void Put(byte[] bytes,int at,uint value)=>BitConverter.GetBytes(value).CopyTo(bytes,at);
    static byte[] Legacy(byte[] raw){
        int count=(int)Word(raw,8);var old=new byte[96+160*count];Array.Copy(raw,old,96);
        Put(old,20,160);Put(old,24,1);Put(old,92,0);
        for(int i=0;i<count;i++)Array.Copy(raw,96+i*164,old,96+i*160,160);
        return old;
    }
    static Idas3ReplayData Parse(byte[] raw,Idas3ReplayData.Details meta,byte[] opponent=null)=>
        Idas3ReplayData.Parse(Idas3ReplayLibrary.Package(meta,raw,opponent??Array.Empty<byte>()));
    public static void Run(){
        string input=Environment.GetEnvironmentVariable("IDAS3_REPLAY_SMOKE_INPUT"),output=Environment.GetEnvironmentVariable("IDAS3_REPLAY_SMOKE_REPORT");
        if(string.IsNullOrEmpty(input)||string.IsNullOrEmpty(output)||File.Exists(output))throw new InvalidOperationException("New report and native fixtures required");
        var report=new Report();checks=0;
        try{
            Check(Idas3ReplayCodec.MaxRaw>=96L+108000L*164+109024L*136,"Longest valid revision3 exceeds codec limit");
            for(int course=0;course<9;course++)for(int weather=0;weather<2;weather++)for(int night=0;night<2;night++){
                var raw=File.ReadAllBytes(Path.Combine(input,$"scene-{course}-{weather}-{night}.idr2"));
                var meta=new Idas3ReplayData.Details{condition=course*2,weather=weather,night=night,car=0,ticks6000=(int)Word(raw,4),outcome=2};
                var replay=Parse(raw,meta);int count=(int)Word(raw,8),tail=96+164*count;
                var exact=raw.Skip(tail).ToArray();
                Check(replay.SmokeFrames.SequenceEqual(exact)&&replay.DustFrames.Length==0,"Parser altered smoke inputs/seed or selected legacy controller");
                var compressed=Idas3ReplayCodec.Encode(raw);
                Check(Idas3ReplayCodec.Decode(compressed).SequenceEqual(raw),"Compression changed source data");
                Check(Parse(compressed,meta).SmokeFrames.SequenceEqual(exact),"Compressed replay lost effect inputs");
                foreach(int fps in new[]{30,60,144,240})for(int paint=0;paint<fps*12;paint++){
                    var p=replay.Sample(paint/(double)fps);
                    Check(p.dustStep==p.tick+61,"Render sampling selected a different source tick");
                }
                foreach(double time in new[]{12d,0d,3.5,7.23,1d,12d,12d}){
                    var p=replay.Sample(time);Check(p.dustStep==p.tick+61,"Seek selected a different source tick");
                }
                var old=Legacy(raw);var legacy=Parse(old,meta);
                Check(legacy.SmokeFrames.Length==0&&legacy.DustFrames.Length==0,"Legacy pose replay acquired effects");
                var pairMeta=JsonUtility.FromJson<Idas3ReplayData.Details>(JsonUtility.ToJson(meta));
                pairMeta.mode=1;pairMeta.opponentCar=19;pairMeta.opponentBytes=old.Length;
                var pair=Parse(raw,pairMeta,old);
                Check(pair.SmokeFrames.SequenceEqual(exact)&&pair.Opponent.SmokeFrames.Length==0,"Paired replay duplicated world effects");
                Check(pair.Opponent.Frames.Length==count&&pair.Frames[^1].dustStep==781,"Paired timeline changed");
                foreach(Action<byte[]> edit in new Action<byte[]>[]{
                    b=>Put(b,24,4),b=>Put(b,20,160),b=>Put(b,92,0),b=>Put(b,92,109025),
                    b=>Put(b,96+160,782),b=>Put(b,96+164+160,0),b=>Put(b,96+164*(count-1)+160,780),
                    b=>Put(b,tail,4),b=>Put(b,tail+8,0xbf800000),b=>Put(b,tail+12,0x7fc00000),
                    b=>Put(b,tail+128,Word(b,tail+128)|0x1000),
                    b=>Put(b,tail+128,(Word(b,tail+128)&~0x3c0u)|(9u<<6)),
                    b=>{Put(b,tail,3);Put(b,tail+128,Word(b,tail+128)|0x400);},
                    b=>Put(b,tail+136+128,Word(b,tail+136+128)^0x800),
                    b=>Put(b,tail+136+128,Word(b,tail+136+128)^0x20)
                }){
                    var bad=(byte[])raw.Clone();edit(bad);bool rejected=false;
                    try{Parse(bad,meta);}catch(InvalidDataException){rejected=true;}
                    Check(rejected,"Malformed combined recording accepted");
                }
                foreach(int kind in new[]{0,1,2,3}){
                    var wrong=JsonUtility.FromJson<Idas3ReplayData.Details>(JsonUtility.ToJson(meta));
                    if(kind==0)wrong.condition=((course+1)%9)*2;else if(kind==1)wrong.weather^=1;else if(kind==2)wrong.night^=1;else wrong.condition=18;
                    bool rejected=false;try{Parse(raw,wrong);}catch(InvalidDataException){rejected=true;}
                    Check(rejected,"Smoke scene accepted with different race metadata");
                }
                bool truncated=false;try{Parse(raw.Take(raw.Length-1).ToArray(),meta);}catch(InvalidDataException){truncated=true;}
                Check(truncated,"Truncated smoke timeline accepted");
                pairMeta.opponentBytes=raw.Length;bool duplicate=false;
                try{Parse(raw,pairMeta,raw);}catch(InvalidDataException){duplicate=true;}
                Check(duplicate,"Opponent track accepted a second world timeline");
                report.scenarios++;
            }
            string live=Environment.GetEnvironmentVariable("IDAS3_REPLAY_SMOKE_LIVE_INPUT");
            if(!string.IsNullOrEmpty(live))for(int scenario=0;scenario<4;scenario++){
                var raw=File.ReadAllBytes(Path.Combine(live,$"scene-{scenario}.idr2"));
                var meta=new Idas3ReplayData.Details{condition=scenario==1?15:14,night=scenario==2?1:0,weather=scenario==3?1:0,car=0,ticks6000=(int)Word(raw,4),outcome=2};
                var parsed=Parse(raw,meta);Check(parsed.Frames.Length==720&&parsed.SmokeFrames.Length==780*136,"Actual race capture failed managed parsing");
                Check(Parse(Idas3ReplayCodec.Encode(raw),meta).SmokeFrames.SequenceEqual(parsed.SmokeFrames),"Actual race compression changed smoke");
                report.liveRaces++;
            }
            // Keep the actual revision2 fixtures under test as well.
            Idas3ReplayDustAudit.Run();
            report.passed=true;Debug.Log("PASS combined replay managed checks: "+checks);
        }catch(Exception e){report.error=e.ToString();throw;}
        finally{report.checks=checks;File.WriteAllText(output,JsonUtility.ToJson(report,true));}
    }
}
