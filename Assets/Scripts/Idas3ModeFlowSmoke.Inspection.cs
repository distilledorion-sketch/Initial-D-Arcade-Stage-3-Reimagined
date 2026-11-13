using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;

public sealed partial class Idas3ModeFlowSmoke {
    private static void ConfigureInspectionLanguage(string[] args,string saves){
        if(Array.IndexOf(args,"-idas3-car-inspection-check")<0)return;
        int index=Array.IndexOf(args,"-idas3-inspection-language"),language=0;
        if(index>=0&&(index+1>=args.Length||!int.TryParse(args[index+1],out language)||language<0||language>2))
            throw new ArgumentException("Inspection language must be 0, 1 or 2.");
        File.WriteAllText(Path.Combine(saves,"game-options.json"),"{\"version\":1,\"customMenuLanguage\":"+language+",\"arcadeTextLanguage\":"+(language==1?1:0)+"}");
        Idas3MenuLocalization.SetLanguage(language);
        if(Idas3Native.Idas3SceneSetCustomMenuLanguage(language)!=1)throw new InvalidOperationException(Idas3Native.Error());
    }
    private IEnumerator InspectionStage(int stage,string message){
        yield return Until(()=>Idas3SceneModeFlowValue(5)==stage&&Idas3SceneModeFlowValue(27)==1,400,message);
    }
    private IEnumerator InspectionOpen(bool actionsAlreadyOpen=false){
        if(!actionsAlreadyOpen)yield return SaveClick(150,130);
        Check(Idas3SceneModeFlowValue(42)==1,"Saved-car actions are open");
        yield return SaveClick(380,380);
        yield return InspectionStage(4,"Zero-play Continue opens inspection transmission choice");
        Check(Idas3SceneModeFlowValue(61)==1,"Renewal owns the draft selectors");
    }
    private void InspectionBytes(byte[] actual,byte[] expected,string message){
        Check(actual.Length==expected.Length,message+" size");
        for(int i=0;i<expected.Length;++i)Check(actual[i]==expected[i],message+" byte "+i);
    }
    private Dictionary<string,Tuple<byte[],DateTime>> InspectionFiles(string directory){
        var files=new Dictionary<string,Tuple<byte[],DateTime>>();
        foreach(string file in Directory.GetFiles(directory,"*",SearchOption.AllDirectories))
            if(!file.EndsWith(".lock",StringComparison.OrdinalIgnoreCase))files.Add(file,Tuple.Create(File.ReadAllBytes(file),File.GetLastWriteTimeUtc(file)));
        return files;
    }
    private void InspectionUnchanged(string directory,Dictionary<string,Tuple<byte[],DateTime>> before,string label){
        var after=InspectionFiles(directory);Check(after.Count==before.Count,label+" file count");
        foreach(var item in before){
            Check(after.ContainsKey(item.Key),label+" file exists");
            InspectionBytes(after[item.Key].Item1,item.Value.Item1,label+" "+Path.GetFileName(item.Key));
            Check(after[item.Key].Item2==item.Value.Item2,label+" file timestamp");
        }
    }
    private IEnumerator InspectionFinish(){
        yield return Frames(80);yield return Pad(0x1000);
        yield return InspectionStage(5,"Inspection returns to Mode after its presentation hold");
        Check(Idas3SceneModeFlowValue(61)==0&&Idas3SceneModeFlowValue(59)==0,"Playback finished without a save-error modal");
    }
    private void InspectionSetPlays(string file,uint plays){
        var data=File.ReadAllBytes(file);LegendProfileWord(data,1140,plays);SealLegendProfile(data);File.WriteAllBytes(file,data);
    }
    private IEnumerator InspectionChoose(){
        yield return Pad(0x1000);
        yield return InspectionStage(12,"Transmission proceeds to package selection");
        yield return Capture("renewal-package");yield return Pad(0x1000);
        yield return InspectionStage(13,"Saved renewal proceeds to completion");
    }
    private IEnumerator CarInspection(){
        yield return Until(()=>host.Ready,600,"Scene initialized");yield return Fixture(350);
        string saves=Path.Combine(root,"userdata","saves"),profile=Path.Combine(saves,"slot_1","driver_profiles_v1","car_00.profile");
        foreach(uint plays in new[]{1u,2u,50u}){
            yield return Fixture(351);InspectionSetPlays(profile,plays);
            yield return SaveClick(150,130);yield return SaveClick(520,412);
            Check(Idas3SceneModeFlowValue(5)==1&&Idas3SceneModeFlowValue(59)==0,"No manual renewal action");
            if(plays==1)yield return Capture("save-actions");
            yield return SaveClick(380,380);yield return InspectionStage(13,"Positive count opens Saved Data");
            Check(Idas3SceneModeFlowValue(61)==0&&Idas3SceneModeFlowValue(63)==0,"Positive count skips renewal");
            Check(LegendProfileWord(File.ReadAllBytes(profile),1140)==plays-1,"Card acceptance consumes one use");
        }
        yield return Fixture(351);InspectionSetPlays(profile,0);
        byte[] original=File.ReadAllBytes(profile),expected=(byte[])original.Clone();
        LegendProfileWord(expected,1140,50);SealLegendProfile(expected);
        yield return InspectionOpen();yield return Capture("renewal-transmission");
        InspectionBytes(File.ReadAllBytes(profile),original,"Opening inspection does not write draft");
        var before=InspectionFiles(saves);yield return Pad(0x2000);yield return InspectionStage(1,"Cancel returns to saves");
        InspectionUnchanged(saves,before,"Cancelled renewal preserves expired card");
        yield return InspectionOpen(true);yield return InspectionChoose();yield return Capture("renewal-complete");
        InspectionBytes(File.ReadAllBytes(profile),expected,"Keeping transmission/package preserves all progress");
        before=InspectionFiles(saves);yield return Frames(160);yield return Capture("renewal-summary");
        yield return InspectionFinish();yield return Capture("renewal-mode");
        InspectionUnchanged(saves,before,"Completion does not charge or reset again");
        yield return Fixture(351);yield return SaveClick(150,130);yield return SaveClick(380,380);yield return InspectionStage(13,"Next entry opens ordinary review");
        Check(Idas3SceneModeFlowValue(63)==0&&LegendProfileWord(File.ReadAllBytes(profile),1140)==49,"Next card entry consumes one renewed use");
        yield return Fixture(351);InspectionSetPlays(profile,0);yield return InspectionOpen();before=InspectionFiles(saves);
        using(var writeLock=new FileStream(profile+".lock",FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){
            yield return Pad(0x1000);yield return InspectionStage(12,"Locked writer still allows draft selection");
            yield return Pad(0x1000);yield return InspectionStage(1,"Failed commit returns to save selection");
            InspectionUnchanged(saves,before,"Failed commit preserves expired card and all progress");
        }
        yield return InspectionOpen(true);yield return InspectionChoose();yield return InspectionFinish();
        Check(LegendProfileWord(File.ReadAllBytes(profile),1140)==50,"Retry renews exactly once");
        Finish(true,null);
    }
}
