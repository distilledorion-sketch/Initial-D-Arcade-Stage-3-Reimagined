using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build.Reporting;

public static class Idas3OnlineRulesBuild
{
    public static void Build()
    {
        const string output="Verification/online-rules-20260921/Player/InitialDUnity.exe";
        Directory.CreateDirectory(Path.GetDirectoryName(output));
        var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{
            scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},locationPathName=output,
            target=BuildTarget.StandaloneWindows64,options=BuildOptions.None});
        if(report.summary.result!=BuildResult.Succeeded)throw new InvalidOperationException("Online rules build failed");
        File.WriteAllText(Path.GetDirectoryName(output)+"/steam_appid.txt","480\n");
    }
}
