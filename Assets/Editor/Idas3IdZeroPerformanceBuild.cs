using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build.Reporting;

// Matched release-configuration player; private smoke fixtures own all saves.
public static class Idas3IdZeroPerformanceBuild
{
    public static void Build()
    {
        string output=Path.GetFullPath("Verification/idzero-performance-20261008/player/InitialDUnity.exe");
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-performance-build-output");
        if(at>=0){if(at+1>=args.Length)throw new ArgumentException("Missing private performance build output");output=Path.GetFullPath(args[at+1]);}
        Directory.CreateDirectory(Path.GetDirectoryName(output));
        var result=BuildPipeline.BuildPlayer(new BuildPlayerOptions{
            scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},locationPathName=output,
            target=BuildTarget.StandaloneWindows64,options=BuildOptions.None});
        if(result.summary.result!=BuildResult.Succeeded)throw new InvalidOperationException("IDZero performance player failed");
        File.WriteAllText(Path.Combine(Path.GetDirectoryName(output),"steam_appid.txt"),"480\n");
    }
}
