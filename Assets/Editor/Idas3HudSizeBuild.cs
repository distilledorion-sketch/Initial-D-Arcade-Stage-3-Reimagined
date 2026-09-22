using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build.Reporting;
public static class Idas3HudSizeBuild {
    public static void Build(){
        const string output="Verification/hud-size-20260921/Player/InitialDUnity.exe";
        Directory.CreateDirectory(Path.GetDirectoryName(output));
        var result=BuildPipeline.BuildPlayer(new BuildPlayerOptions{scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},locationPathName=output,target=BuildTarget.StandaloneWindows64,options=BuildOptions.None});
        if(result.summary.result!=BuildResult.Succeeded)throw new Exception("HUD size player build failed");
        File.WriteAllText(Path.GetDirectoryName(output)+"/steam_appid.txt","480\n");
    }
}
