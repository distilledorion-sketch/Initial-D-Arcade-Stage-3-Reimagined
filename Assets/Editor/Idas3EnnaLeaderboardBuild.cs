using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEngine;

public static class Idas3EnnaLeaderboardBuild
{
    public static void Build()
    {
        const string proof="Verification/enna-leaderboard-20260921";
        Directory.CreateDirectory(proof);
        int checks=0;
        void Check(bool ok,string why){++checks;if(!ok)throw new Exception(why);}
        foreach(int condition in new[]{22,23})foreach(int weather in new[]{0,1}){
            var r=new Idas3CommunityTimes.Run{id=Guid.NewGuid().ToString(),ruleset=Idas3CommunityTimes.Ruleset,build="0.3.95-community-replays.12",epoch=2,replayVersion=2,replayAvailable=true,condition=condition,weather=weather,night=1,car=0,ticks6000=1200000,nameGlyphs=new[]{162,163,164,221,221},splits=new[]{300000,600000,900000,1200000}};
            Check(Idas3CommunityTimes.Uploadable(r),"Enna replay finish rejected");
            var s=new Idas3CommunityTimes.Snapshot{ruleset=r.ruleset,epoch=2,entries=new[]{r}};
            bool usable=(bool)typeof(Idas3CommunityTimes).GetMethod("UsableCommunitySnapshot",System.Reflection.BindingFlags.Static|System.Reflection.BindingFlags.NonPublic).Invoke(null,new object[]{s});
            Check(usable&&Idas3CommunityTimes.Flatten(s)[0]==condition,"Enna snapshot lost");
            r.condition=24;Check(!Idas3CommunityTimes.Valid(r),"Unknown course accepted");r.condition=condition;
            r.imported=1;Check(!Idas3CommunityTimes.Uploadable(r),"Historical times accepted");r.imported=0;
            r.replayVersion=0;Check(!Idas3CommunityTimes.Uploadable(r),"Replayless run accepted");r.replayVersion=2;
            r.build="0.3.95-enna-preview.1";Check(!Idas3CommunityTimes.Uploadable(r),"Old Enna preview accepted");
        }
        File.WriteAllText(proof+"/checks.txt","PASS "+checks+" Enna client submission checks\n");
        var scene="Assets/Scenes/InitialDUnityScene.unity";
        bool enna=File.Exists("Assets/Experimental/EnnaSkyline/EnnaRaceTest.unity");
        if(enna)scene="Assets/Experimental/EnnaSkyline/EnnaRaceTest.unity";
        string product=PlayerSettings.productName,version=PlayerSettings.bundleVersion;
        try{
            if(enna){PlayerSettings.productName="Initial D Unity Enna Test";PlayerSettings.bundleVersion="0.3.95-community-replays.12";}
            var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{scenes=new[]{scene},locationPathName=proof+"/Player/InitialDUnity.exe",target=BuildTarget.StandaloneWindows64,options=BuildOptions.None});
            if(report.summary.result!=BuildResult.Succeeded)throw new Exception("Enna leaderboard build failed");
        }finally{PlayerSettings.productName=product;PlayerSettings.bundleVersion=version;AssetDatabase.SaveAssets();}
    }
}
