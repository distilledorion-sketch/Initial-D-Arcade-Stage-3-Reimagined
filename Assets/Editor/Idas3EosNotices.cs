using System;
using System.IO;
using UnityEditor;
using UnityEditor.PackageManager;

// A global post-build hook covers both normal releases and the online probe.
// Keep the official license files byte-for-byte intact in the player package.
public static class Idas3EosNotices
{
    [UnityEditor.Callbacks.PostProcessBuild(102)]
    public static void Stage(BuildTarget target, string outputPath)
    {
        string platform;
        if (target == BuildTarget.StandaloneWindows64) platform = "Windows/x64";
        else if (target == BuildTarget.StandaloneLinux64) platform = "Linux";
        else return;

        var package = UnityEditor.PackageManager.PackageInfo.FindForAssetPath("Packages/com.initiald.eos-sdk");
        if (package == null || string.IsNullOrEmpty(package.resolvedPath))
            throw new InvalidOperationException("The EOS SDK package is required to stage its player notices.");

        string player = Path.GetFullPath(outputPath);
        string data = Path.Combine(Path.GetDirectoryName(player),Path.GetFileNameWithoutExtension(player) + "_Data");
        string managed = Path.Combine(data,"Managed","com.Epic.OnlineServices.dll");
        string native = Path.Combine(data,"Plugins","x86_64",target == BuildTarget.StandaloneWindows64 ? "EOSSDK-Win64-Shipping.dll" : "libEOSSDK-Linux-Shipping.so");
        foreach(string runtime in new[]{managed,native})
            if(!File.Exists(runtime)||new FileInfo(runtime).Length==0)
                throw new FileNotFoundException("A required EOS player runtime is missing or empty.",runtime);
        string destination = Path.Combine(data,"Plugins","EOS-Notices");
        StageFromPackage(package.resolvedPath, platform, destination);
    }

    internal static void StageFromPackage(string packagePath, string platform, string destination)
    {
        var sources = new[] {
            Path.Combine(packagePath, "Runtime", "EOS_SDK", "license.txt"),
            Path.Combine(packagePath, "Runtime", platform, "license.txt"),
            Path.Combine(packagePath, "Runtime", platform, "ThirdPartySoftwareNotice.txt"),
            Path.Combine(packagePath, "LICENSE-PLUGIN.md")
        };
        var names = new[] {
            "EOS-SDK-license.txt", "EOS-native-license.txt",
            "EOS-native-third-party-notices.txt", "EOS-plugin-MIT-license.txt"
        };
        // Validate every source before writing any notices into the player.
        foreach (string source in sources)
            if (!File.Exists(source)) throw new FileNotFoundException("A required EOS player notice is missing.", source);
        Directory.CreateDirectory(destination);
        for (int i = 0; i < sources.Length; ++i)
            File.Copy(sources[i], Path.Combine(destination, names[i]), true);
    }
}
