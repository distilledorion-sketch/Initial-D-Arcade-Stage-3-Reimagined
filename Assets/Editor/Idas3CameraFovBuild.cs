using System;
using System.IO;
using UnityEditor;

public static class Idas3CameraFovBuild
{
    public static void Build()
    {
        Idas3ControllerMenuChecks.RunCameraFovChecks();
        Idas3JapaneseUiChecks.Run();
        Idas3CustomMenuLanguageChecks.Run();
        const string player = "Builds/Current/InitialDUnity.exe";
        if (!File.Exists(player)) throw new FileNotFoundException("A staged player is required", player);
        var result = BuildPipeline.BuildPlayer(new BuildPlayerOptions {
            scenes = new[] { "Assets/Scenes/InitialDUnityScene.unity" },
            locationPathName = player, target = BuildTarget.StandaloneWindows64,
            options = BuildOptions.CompressWithLz4HC
        });
        if (result.summary.result != UnityEditor.Build.Reporting.BuildResult.Succeeded)
            throw new Exception("Camera FOV build failed");
    }
}
