using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build.Reporting;

// Isolated full-scene player for storage comparisons. Native data is staged
// separately, using the same lossless packer as release builds.
public static class Idas3StorageBuild
{
    public static void CheckBuildCallbacks()
    {
        const string output="Builds/StorageLayoutCheck/InitialDUnity.exe";
        foreach(var options in new[]{BuildOptions.CompressWithLz4HC,BuildOptions.CompressWithLz4HC|BuildOptions.BuildScriptsOnly}) {
            var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions {
                scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},locationPathName=output,
                target=BuildTarget.StandaloneWindows64,options=options
            });
            if(report.summary.result!=BuildResult.Succeeded)throw new Exception("Storage callback build failed: "+options);
            string data="Builds/StorageLayoutCheck/InitialDUnity_Data";
            foreach(string name in Idas3CompressedPlayerLayout.RetiredLooseFiles)
                if(new FileInfo(Path.Combine(data,name)).Length!=0)throw new Exception("Legacy file was not retired: "+name);
            if(new FileInfo(Path.Combine(data,"data.unity3d")).Length<1024)throw new Exception("Packed player is missing");
        }
        File.WriteAllText("Verification/storage-optimization/build-callbacks.txt","PASS full and scripts-only compressed builds; legacy replacements staged.\n");
    }

    public static void BuildComparison()
    {
        var baseline = BuildPipeline.BuildPlayer(new BuildPlayerOptions {
            scenes = new[] { "Assets/Scenes/InitialDUnityScene.unity" },
            locationPathName = "Builds/StorageBaseline/InitialDUnity.exe",
            target = BuildTarget.StandaloneWindows64, options = BuildOptions.None
        });
        if (baseline.summary.result != BuildResult.Succeeded)
            throw new Exception("Baseline storage build failed: " + baseline.summary.result);
        BuildCompressedResources();
    }
    public static void BuildCompressedResources()
    {
        var report = BuildPipeline.BuildPlayer(new BuildPlayerOptions {
            scenes = new[] { "Assets/Scenes/InitialDUnityScene.unity" },
            locationPathName = "Builds/StorageOptimized/InitialDUnity.exe",
            target = BuildTarget.StandaloneWindows64,
            options = BuildOptions.CompressWithLz4HC
        });
        if (report.summary.result != BuildResult.Succeeded)
            throw new Exception("Storage build failed: " + report.summary.result);
        Directory.CreateDirectory("Verification/storage-optimization");
        File.WriteAllText("Verification/storage-optimization/unity-size.txt",
            report.summary.totalSize.ToString());
    }
}
