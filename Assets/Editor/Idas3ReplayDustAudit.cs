using System;
using System.IO;
using System.Linq;
using UnityEngine;

// Explicit offline audit of the actual managed reader/codec. No player, user
// save, service or native game session is opened by this editor command.
public static class Idas3ReplayDustAudit {
    public static void RunAndBuild(){Run();Idas3Build.RebuildPrivateDiagnosticScripts();}
    [Serializable] sealed class Report { public bool passed;public int checks;public string error; }
    static int checks;
    static void Check(bool ok,string why){++checks;if(!ok)throw new Exception(why);}
    static uint Word(byte[] bytes,int at)=>BitConverter.ToUInt32(bytes,at);
    static void Put(byte[] bytes,int at,uint value)=>BitConverter.GetBytes(value).CopyTo(bytes,at);
    static Idas3ReplayData.Details Metadata(byte[] raw,int direction=0)=>new Idas3ReplayData.Details{
        condition=14+direction,car=0,ticks6000=(int)Word(raw,4),outcome=2
    };
    static Idas3ReplayData Parse(byte[] raw,Idas3ReplayData.Details meta=null)=>
        Idas3ReplayData.Parse(Idas3ReplayLibrary.Package(meta??Metadata(raw),raw,Array.Empty<byte>()));
    static byte[] Legacy(byte[] raw){
        int count=(int)Word(raw,8);var old=new byte[96+160*count];Array.Copy(raw,old,96);
        Put(old,20,160);Put(old,24,1);Put(old,92,0);
        for(int i=0;i<count;i++)Array.Copy(raw,96+i*164,old,96+i*160,160);
        return old;
    }
    public static void Run(){
        string input=Environment.GetEnvironmentVariable("IDAS3_REPLAY_DUST_INPUT");
        string output=Environment.GetEnvironmentVariable("IDAS3_REPLAY_DUST_REPORT");
        if(string.IsNullOrEmpty(input)||string.IsNullOrEmpty(output)||File.Exists(output))throw new InvalidOperationException("New audit report and native fixture directory required");
        var report=new Report();
        try{
            foreach(int direction in new[]{0,1}){
                var raw=File.ReadAllBytes(Path.Combine(input,"direction-"+direction+".idr2"));
                var replay=Parse(raw,Metadata(raw,direction));
                int tail=96+164*(int)Word(raw,8);var exact=raw.Skip(tail).ToArray();
                Check(replay.DustFrames.SequenceEqual(exact),"Managed reader changed source float bits");
                Check(Idas3ReplayCodec.Decode(Idas3ReplayCodec.Encode(raw)).SequenceEqual(raw),"Codec changed extended recording");
                var compressed=Parse(Idas3ReplayCodec.Encode(raw),Metadata(raw,direction));
                Check(compressed.DustFrames.SequenceEqual(exact),"Compressed replay lost dust inputs");
                foreach(int fps in new[]{30,60,144,240})for(int frame=0;frame<fps*12;frame++){
                    double seconds=frame/(double)fps;var sample=replay.Sample(seconds);
                    Check(sample.dustStep==replay.Frames[sample.tick-1].dustStep,"Render interpolation changed source dust clock");
                }
                foreach(double seconds in new[]{12d,0d,4.125,11.9,3,3,0,12}){
                    var p=replay.Sample(seconds);Check(p.dustStep<=Word(raw,92),"Seek selected invalid source input");
                }
                var old=Legacy(raw);var legacy=Parse(old,Metadata(raw,direction));
                Check(legacy.Detailed&&legacy.DustFrames.Length==0&&legacy.Frames.Length==replay.Frames.Length,"Revision1 compatibility failed");
                Check(Idas3ReplayCodec.Decode(Idas3ReplayCodec.Encode(old)).SequenceEqual(old),"Legacy codec regression");
                var battle=Metadata(raw,direction);battle.mode=2;battle.opponentCar=0;battle.opponentEnemy=-1;battle.opponentBytes=old.Length;
                var pair=Idas3ReplayData.Parse(Idas3ReplayLibrary.Package(battle,raw,old));
                Check(pair.Opponent!=null&&pair.Opponent.DustFrames.Length==0&&pair.DustFrames.SequenceEqual(exact),"Battle replay world timeline lost");
                string folder=Path.GetDirectoryName(output);
                File.WriteAllBytes(Path.Combine(folder,"direction-"+direction+".idreplay"),Idas3ReplayLibrary.Package(Metadata(raw,direction),Idas3ReplayCodec.Encode(raw),Array.Empty<byte>()));
                File.WriteAllBytes(Path.Combine(folder,"legacy-"+direction+".idreplay"),Idas3ReplayLibrary.Package(Metadata(raw,direction),old,Array.Empty<byte>()));
                File.WriteAllBytes(Path.Combine(folder,"paired-"+direction+".idreplay"),Idas3ReplayLibrary.Package(battle,raw,old));
                foreach(var edit in new Action<byte[]>[]{
                    b=>Put(b,20,160),b=>Put(b,24,3),b=>Put(b,92,0),b=>Put(b,92,109025),
                    b=>Put(b,tail,4),b=>Put(b,tail+8,0x7fc00000),b=>Put(b,tail+8,0xbf800000),
                    b=>Put(b,tail+12,0x7f800000),b=>Put(b,96+160,Word(raw,92)+1),
                    b=>Put(b,96+164+160,0),b=>Put(b,96+164*((int)Word(raw,8)-1)+160,0)
                }){
                    var bad=(byte[])raw.Clone();edit(bad);bool rejected=false;
                    try{Parse(bad,Metadata(raw,direction));}catch(InvalidDataException){rejected=true;}
                    Check(rejected,"Malformed dust recording accepted");
                }
                foreach(int mode in new[]{0,1,2}){
                    var wrong=Metadata(raw,direction);if(mode==0)wrong.condition=6;else if(mode==1)wrong.night=1;else wrong.weather=1;
                    bool rejected=false;try{Parse(raw,wrong);}catch(InvalidDataException){rejected=true;}
                    Check(rejected,"Dust accepted outside source course/weather gate");
                }
                bool truncated=false;try{Parse(raw.Take(raw.Length-1).ToArray(),Metadata(raw,direction));}catch(InvalidDataException){truncated=true;}
                Check(truncated,"Truncated dust timeline accepted");
            }
            report.passed=true;Debug.Log("PASS replay dust managed checks: "+checks);
        }catch(Exception e){report.error=e.ToString();throw;}
        finally{report.checks=checks;File.WriteAllText(output,JsonUtility.ToJson(report,true));}
    }
}
