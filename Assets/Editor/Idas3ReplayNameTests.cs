using System;
using System.IO;
using System.Linq;
using UnityEngine;
public static class Idas3ReplayNameTests
{
    public static void CheckDigitNames(){
        // Original glyph table uses 1..9,0, not 0..9.
        int[] glyphs={197,188,189,190,191,192,193,194,195,196};
        var m=new Idas3ReplayData.Details{condition=6,mode=0};
        for(int digit=0;digit<10;digit++){
            m.nameGlyphs=new[]{164,179,glyphs[digit],184,221};
            if(Idas3ReplayLibrary.FileStem(m)!="cr"+digit+"w_akina_dh_day_dry_tat")throw new Exception("Replay numeric glyph "+digit);
        }
        Directory.CreateDirectory("Verification/name-digits-20260922");
        File.WriteAllText("Verification/name-digits-20260922/replay-digit-checks.txt","PASS all 10 original numeric glyphs in mixed replay filenames\n");
    }
    public static void BuildDigitVerified(){CheckDigitNames();Idas3HudSizeBuild.Build();}
    public static void Run(){
        CheckDigitNames();
        int checks=0;void Check(bool ok,string why){if(!ok)throw new Exception(why);checks++;}
        var m=new Idas3ReplayData.Details{playerName="Chris",opponentName="Tak",condition=6,night=1,weather=1,mode=2};
        Check(Idas3ReplayLibrary.FileStem(m)=="chris_vs_tak_akina_dh_night_wet_lots","Requested Legend naming");
        m.mode=1;Check(Idas3ReplayLibrary.FileStem(m)=="chris_vs_tak_akina_dh_night_wet_ol","Online suffix");
        m.mode=0;Check(Idas3ReplayLibrary.FileStem(m)=="chris_akina_dh_night_wet_tat","Solo Time Attack omits opponent");
        string[] directions={"ccw","cw","ccw","cw","dh","uh","dh","uh","ob","ib","dh","rev","ob","ib","ob","ib","dh","uh","dh","uh","dh","uh"};
        for(int i=0;i<22;i++){m.condition=i;Check(Idas3ReplayLibrary.FileStem(m).Contains("_"+directions[i]+"_night_wet_tat"),"Original course direction "+i);}
        m.condition=6;m.night=0;m.weather=0;m.playerName="ＣＨＲＩＳ";Check(Idas3ReplayLibrary.FileStem(m)=="chris_akina_dh_day_dry_tat","Original full-width driver name normalizes");
        m.playerName="../Chris : \\ test?";Check(Idas3ReplayLibrary.FileStem(m)=="chris_test_akina_dh_day_dry_tat","Filename characters sanitized");
        m.playerName=null;m.nameGlyphs=new[]{164,169,179,170,180};Check(Idas3ReplayLibrary.FileStem(m)=="chris_akina_dh_day_dry_tat","Legacy glyph name fallback");
        string root=Path.GetFullPath("Verification/replay-names-20260919"),folder=Path.Combine(root,"files-"+Guid.NewGuid().ToString("N"));
        foreach(string mode in new[]{"legend","online"}){
            string fixture=Path.Combine(root,"capture",mode);
            var meta=JsonUtility.FromJson<Idas3ReplayData.Details>(File.ReadAllText(fixture+".json"));
            Check(!string.IsNullOrWhiteSpace(meta.playerName)&&!string.IsNullOrWhiteSpace(meta.opponentName),"Native names captured for "+mode);
            Check(mode!="legend"||meta.opponentName=="KENJI","Legend uses the game's localized rival label");
            var player=File.ReadAllBytes(fixture+"-player.idr");var opponent=File.ReadAllBytes(fixture+"-opponent.idr");
            string first=Idas3ReplayLibrary.Save(folder,meta,player,opponent);byte[] before=File.ReadAllBytes(first);
            string second=Idas3ReplayLibrary.Save(folder,meta,player,opponent);
            Check(Path.GetFileName(first)==Idas3ReplayLibrary.FileStem(meta)+".idreplay","Readable file name saved");
            Check(Path.GetFileName(second)==Idas3ReplayLibrary.FileStem(meta)+"_2.idreplay"&&before.SequenceEqual(File.ReadAllBytes(first)),"Repeat race does not overwrite");
            var replay=Idas3ReplayData.Load(second);Check(replay.Metadata.opponentName==meta.opponentName&&replay.Opponent!=null,"Renamed package still opens with both tracks");
        }
        Check(Directory.GetFiles(folder,"*.tmp").Length==0,"No temporary files remain");
        File.WriteAllText(Path.Combine(root,"report.json"),"{\"passed\":true,\"checks\":"+checks+"}");Debug.Log("Replay filename checks passed: "+checks);
    }
    public static void BuildVerified(){Run();Idas3Build.RebuildSadamineStagingScripts();}
}
