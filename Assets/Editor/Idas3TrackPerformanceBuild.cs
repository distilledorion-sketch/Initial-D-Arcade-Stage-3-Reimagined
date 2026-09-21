using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build.Reporting;

// Isolated release-configuration player; runtime data is attached separately.
public static class Idas3TrackPerformanceBuild
{
    public static void Build()
    {
        const string output="Verification/track-fps-20260921/Player/InitialDUnity.exe";
        Directory.CreateDirectory(Path.GetDirectoryName(output));
        var result=BuildPipeline.BuildPlayer(new BuildPlayerOptions{
            scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},locationPathName=output,
            target=BuildTarget.StandaloneWindows64,options=BuildOptions.None});
        if(result.summary.result!=BuildResult.Succeeded)throw new InvalidOperationException("Track performance player failed");
        File.WriteAllText(Path.GetDirectoryName(output)+"/steam_appid.txt","480\n");
    }
}
