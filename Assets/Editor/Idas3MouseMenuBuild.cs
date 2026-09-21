using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build.Reporting;

public static class Idas3MouseMenuBuild
{
    public static void Build()
    {
        Idas3ControllerMenuChecks.RunPointerChecks();
        const string output="Verification/mouse-menu-20260921/Player/InitialDUnity.exe";
        Directory.CreateDirectory(Path.GetDirectoryName(output));
        var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{
            scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},locationPathName=output,
            target=BuildTarget.StandaloneWindows64,options=BuildOptions.None});
        if(report.summary.result!=BuildResult.Succeeded)throw new InvalidOperationException("Mouse menu build failed");
        File.WriteAllText(Path.GetDirectoryName(output)+"/steam_appid.txt","480\n");
    }
}
