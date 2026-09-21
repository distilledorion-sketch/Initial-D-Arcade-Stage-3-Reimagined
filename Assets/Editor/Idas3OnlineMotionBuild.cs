using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build.Reporting;

public static class Idas3OnlineMotionBuild
{
    public static void Build()
    {
        CheckCachedAdmission();
        const string output="Verification/online-motion-20260921/Player/InitialDUnity.exe";
        Directory.CreateDirectory(Path.GetDirectoryName(output));
        var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{
            scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},locationPathName=output,
            target=BuildTarget.StandaloneWindows64,options=BuildOptions.None});
        if(report.summary.result!=BuildResult.Succeeded)throw new InvalidOperationException("Online rules build failed");
        File.WriteAllText(Path.GetDirectoryName(output)+"/steam_appid.txt","480\n");
    }
    static void CheckCachedAdmission()
    {
        var transport=new Idas3.Multiplayer.Idas3SteamTransport();
        var type=transport.GetType();const System.Reflection.BindingFlags flags=System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic;
        void Set(string name,object value)=>type.GetField(name,flags).SetValue(transport,value);
        bool Allowed(ulong id)=>(bool)type.GetMethod("Allowed",flags).Invoke(transport,new object[]{id});
        int checks=0;void Check(bool ok,string why){++checks;if(!ok)throw new InvalidOperationException(why);}
        // No Steam initialization: any accidental synchronous query throws.
        Set("local",100UL);Set("peer",200UL);
        Check(!Allowed(200),"Closed lobby admitted a peer");
        Set("lobby",new Steamworks.CSteamID(300));
        Check(Allowed(200),"Previously validated peer not accepted without Steam queries");
        Check(!Allowed(0)&&!Allowed(100)&&!Allowed(201),"Unadmitted identity accepted");
        type.GetMethod("ClosePeer",flags).Invoke(transport,null);
        Check(!Allowed(200),"Closed peer still admitted");
        Set("peer",201UL);Check(Allowed(201)&&!Allowed(200),"Stale peer admitted after replacement");
        transport.Leave();Check(!Allowed(201),"Leave retained admission");
        Directory.CreateDirectory("Verification/online-motion-20260921");
        File.WriteAllText("Verification/online-motion-20260921/admission-checks.txt","PASS "+checks+" cached admission checks without Steam initialized\n");
    }
}
