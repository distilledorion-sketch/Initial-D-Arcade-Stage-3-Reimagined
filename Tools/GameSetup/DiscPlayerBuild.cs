using System;
using UnityEditor;
using UnityEditor.Build.Reporting;

public static class DiscPlayerBuild
{
    public static void Build()
    {
        PlayerSettings.productName = "Initial D Arcade Stage 3 Reimagined";
        var icon = AssetDatabase.LoadAssetAtPath<UnityEngine.Texture2D>("Assets/Branding/Reimagined.png");
        if (!icon) throw new InvalidOperationException("The Reimagined application icon is missing.");
        PlayerSettings.SetIconsForTargetGroup(BuildTargetGroup.Unknown, new[] { icon });
        if (!Idas3ArcadeHud.Available || Idas3ArcadeMeterCatalog.Count < 3 || Idas3OrnamentCatalog.Count < 2)
            throw new InvalidOperationException("The race meter or ornament catalog is missing from the player project.");
        var report = BuildPipeline.BuildPlayer(new BuildPlayerOptions
        {
            scenes = new[] { "Assets/Scenes/InitialDUnityScene.unity" },
            locationPathName = Environment.GetEnvironmentVariable("IDAS3_DISC_PLAYER"),
            target = BuildTarget.StandaloneWindows64,
            extraScriptingDefines = new[] { "IDAS3_PORTABLE_PLAYER" },
            options = BuildOptions.StrictMode | BuildOptions.CompressWithLz4HC
        });
        if (report.summary.result != BuildResult.Succeeded)
            throw new InvalidOperationException("Disc player build failed: " + report.summary.result);
    }
}

