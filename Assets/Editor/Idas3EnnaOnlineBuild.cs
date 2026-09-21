using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEngine;
using Idas3.Multiplayer;
public static class Idas3EnnaOnlineBuild {
    public static void Build(){
        const string proof="Verification/enna-online-20260921";Directory.CreateDirectory(proof);int checks=0;
        void Check(bool ok,string why){++checks;if(!ok)throw new Exception(why);}
        foreach(bool reverse in new[]{false,true})foreach(bool wet in new[]{false,true}){
            var c=new Idas3RaceChoice(11,reverse,wet,false);Check(c.Course==11&&c.Reverse==reverse&&c.Wet==wet&&c.Night,"Enna options must force authored night");
            var method=typeof(Idas3MultiplayerSession).GetMethod("ReadChoice",System.Reflection.BindingFlags.Static|System.Reflection.BindingFlags.NonPublic);
            using(var stream=new MemoryStream())using(var writer=new BinaryWriter(stream)){
                writer.Write(11);writer.Write(reverse);writer.Write(wet);writer.Write(true);stream.Position=0;
                Check(((Idas3RaceChoice)method.Invoke(null,new object[]{new BinaryReader(stream)})).Equals(c),"Peer choice roundtrip changed conditions");
                stream.Position=6;writer.Write(false);stream.Position=0;bool rejected=false;try{method.Invoke(null,new object[]{new BinaryReader(stream)});}catch(System.Reflection.TargetInvocationException e){rejected=e.InnerException is InvalidDataException;}Check(rejected,"Peer daytime Enna accepted");
            }
        }
        bool unknown=false;try{new Idas3RaceChoice(12,false,false,false);}catch(ArgumentOutOfRangeException){unknown=true;}Check(unknown,"Unknown track accepted");
        var fingerprint=typeof(Idas3MultiplayerSession).GetMethod("EnnaFingerprint",System.Reflection.BindingFlags.Static|System.Reflection.BindingFlags.NonPublic);
        string folder=proof+"/fingerprint-fixture";Directory.CreateDirectory(folder);File.WriteAllText(folder+"/menu.idastex","fixture");
        var names=new[]{"course.id","enna_path.bin","enna_path_l.bin","enna_path_r.bin","race-markers.bin","collision-0.rcl","collision-1.rcl"};
        foreach(var name in names)File.WriteAllText(folder+"/"+name,name);
        string Get()=>(string)fingerprint.Invoke(null,new object[]{folder});string original=Get();Check(original==Get(),"Unstable pack fingerprint");
        foreach(var name in names){File.AppendAllText(folder+"/"+name,"changed");Check(Get()!=original,"Changed simulation asset retained fingerprint");File.WriteAllText(folder+"/"+name,name);}
        Check(Get()==original,"Restored assets changed fingerprint");
        File.WriteAllText(proof+"/checks.txt","PASS "+checks+" online Enna choice and fingerprint checks\n");
        var product=PlayerSettings.productName;var version=PlayerSettings.bundleVersion;
        try{
            PlayerSettings.bundleVersion="0.3.95-community-replays.13";
            string scene="Assets/Scenes/InitialDUnityScene.unity";
            if(File.Exists("Assets/Experimental/EnnaSkyline/EnnaRaceTest.unity")){scene="Assets/Experimental/EnnaSkyline/EnnaRaceTest.unity";PlayerSettings.productName="Initial D Unity Enna Test";}
            var result=BuildPipeline.BuildPlayer(new BuildPlayerOptions{scenes=new[]{scene},locationPathName=proof+"/Player/InitialDUnity.exe",target=BuildTarget.StandaloneWindows64,options=BuildOptions.None});
            if(result.summary.result!=BuildResult.Succeeded)throw new Exception("Enna online build failed");
        }finally{PlayerSettings.productName=product;PlayerSettings.bundleVersion=version;AssetDatabase.SaveAssets();}
    }
}
