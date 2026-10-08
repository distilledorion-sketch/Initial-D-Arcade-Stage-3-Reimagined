using System;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Text;
using Idas3.Multiplayer;
using UnityEngine;

// Release-only checks use private fixtures, never a player's installation.
public static class Idas3Release46Checks {
    const BindingFlags Hidden=BindingFlags.Static|BindingFlags.NonPublic;
    static readonly string Output=Path.GetFullPath("Verification/release-46-20261008");
    static int checks;
    static void Check(bool ok,string why){if(!ok)throw new Exception(why);++checks;}
    static string Value(object result,string field)=>result.GetType().GetField(field).GetValue(result)?.ToString();
    static void Reject(Action action,string why){bool failed=false;try{action();}catch(IOException){failed=true;}Check(failed,why);}
    static void Zip(string path,params string[] names){using(var zip=ZipFile.Open(path,ZipArchiveMode.Create))foreach(string name in names)using(var writer=new StreamWriter(zip.CreateEntry(name).Open()))writer.Write("fixture:"+name);}
    public static void Run(){
        Directory.CreateDirectory(Output);
        var assembly=typeof(Idas3Updates).Assembly;
        assembly.GetType("Idas3UpdateChecks").GetMethod("Run",Hidden).Invoke(null,new object[]{(Action<bool,string>)Check});
        var read=typeof(Idas3OnlineCar).GetMethod("Read",Hidden);
        foreach(int car in new[]{0,17,34})foreach(uint paint in new uint[]{0,7,8,46,92,93,uint.MaxValue}){
            using var memory=new MemoryStream();using var writer=new BinaryWriter(memory);
            writer.Write(40+car);writer.Write(true);writer.Write(false);
            foreach(int offset in new[]{16,24,44,48,52,56,60,64,76,152,156,160,164})writer.Write(offset==16?(uint)car:offset==64?paint:0u);
            writer.Flush();memory.Position=0;using var reader=new BinaryReader(memory);
            try{var loaded=(Idas3OnlineCar)read.Invoke(null,new object[]{reader,car});Check(paint<93&&loaded.Car==car,"Online paint validation accepted invalid paint");}
            catch(TargetInvocationException e)when(e.InnerException is InvalidDataException){Check(paint>=93,"Online paint validation rejected available paint");}
        }
        const string tag="v0.3.95-community-replays.46";
        string root=Idas3Updates.RepositoryUrl, core="Initial-D-Arcade-Stage-3-Reimagined-0.3.95.46-Windows-x64.zip", content="Initial-D-Additional-Courses-0.3.95-community-replays.46.zip";
        Func<string,string,string> asset=(name,url)=>"{\"name\":\""+name+"\",\"state\":\"uploaded\",\"size\":10000,\"digest\":\"sha256:"+new string('a',64)+"\",\"browser_download_url\":\""+url+"\"}";
        string url=root+"/releases/download/"+tag+"/";
        Func<string,object> evaluate=extra=>typeof(Idas3Updates).GetMethod("Evaluate",Hidden).Invoke(null,new object[]{"0.3.95-community-replays.45",200L,"{\"tag_name\":\""+tag+"\",\"html_url\":\""+root+"/releases/tag/"+tag+"\",\"assets\":["+asset(core,url+core)+","+extra+"]}",false});
        Check(Value(evaluate(asset(content,url+content)),"contentUrl")==url+content,"Additional course download was not selected");
        Check(Value(evaluate(asset(content,"https://example.test/evil.zip")),"state")=="Unavailable","External content URL was accepted");
        Check(Value(evaluate(asset(content,url+content)+","+asset(content,url+content)),"state")=="Unavailable","Duplicate content archives were accepted");
        foreach(string scenario in new[]{"valid","bad-hash","duplicate","traversal"}){
            string folder=Path.Combine(Output,"staging-"+scenario),game=Path.Combine(folder,"game"),session=Path.Combine(folder,"session");
            if(Directory.Exists(folder))throw new IOException("Use fresh release fixtures");
            Directory.CreateDirectory(game);Directory.CreateDirectory(session);File.WriteAllText(Path.Combine(game,"InitialDUnity.exe"),"unchanged-game");
            string a=Path.Combine(session,"game.zip"),b=Path.Combine(session,"content.zip");
            Zip(a,"InitialDUnity.exe","UnityPlayer.dll","InitialDUnity_Data/globalgamemanagers","InitialDUnity_Data/Managed/Assembly-CSharp.dll");
            string name=scenario=="duplicate"?"InitialDUnity.exe":scenario=="traversal"?"../outside.txt":"InitialDUnity_Data/StreamingAssets/GUNSAI/menu.idastex";
            Zip(b,name);string digest=scenario=="bad-hash"?new string('0',64):Idas3UpdateStaging.Hash(b);
            Func<string> prepare=()=>Idas3UpdateStaging.Prepare(game,session,a,Idas3UpdateStaging.Hash(a),false,"old","new",123,456,json=>null,null,b,digest);
            if(scenario=="valid"){
                Check(File.Exists(prepare()),"Combined install plan missing");
                Check(File.ReadAllText(Path.Combine(session,"stage",name))=="fixture:"+name,"Additional course was not staged");
            }else Reject(()=>prepare(),"Invalid additional archive was accepted: "+scenario);
            Check(File.ReadAllText(Path.Combine(game,"InitialDUnity.exe"))=="unchanged-game","Staging changed the live game");
        }
        File.WriteAllText(Path.Combine(Output,"release46-PASS.txt"),"PASS "+checks+" updater, multipart staging and online paint checks.\n");
        Debug.Log("RELEASE46_PASS "+checks);
    }
    public static void Build(){
        Run();Idas3StorageUpdateChecks.Run();Idas3Build.Configure();
        typeof(Idas3Build).GetMethod("BuildPlayer",Hidden).Invoke(null,new object[]{"Verification/release-46-20261008/player/InitialDUnity.exe","Assets/Scenes/InitialDUnityScene.unity"});
    }
    public static void RefreshScripts(){
        string path=Path.Combine(Output,"player/InitialDUnity.exe");
        if(!File.Exists(path))throw new FileNotFoundException("Build the complete release player first.",path);
        var report=UnityEditor.BuildPipeline.BuildPlayer(new UnityEditor.BuildPlayerOptions{
            scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},locationPathName=path,
            target=UnityEditor.BuildTarget.StandaloneWindows64,
            options=UnityEditor.BuildOptions.BuildScriptsOnly|UnityEditor.BuildOptions.CompressWithLz4HC});
        if(report.summary.result!=UnityEditor.Build.Reporting.BuildResult.Succeeded)throw new Exception("Release scripts build failed.");
    }
    public static void CheckCompressedLayout(){
        Idas3StorageUpdateChecks.Run();
        Idas3CompressedPlayerLayout.StageCompatibility(UnityEditor.BuildTarget.StandaloneWindows64,Path.Combine(Output,"player/InitialDUnity.exe"));
    }
}
