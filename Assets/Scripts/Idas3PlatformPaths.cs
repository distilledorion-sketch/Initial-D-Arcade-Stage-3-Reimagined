using System;
using System.IO;
using UnityEngine;

// Keep writable user data separate from the APK and from read-only packaged
// assets. Windows retains the existing beside-the-player layout; Android uses
// Unity's private persistent directory, which is available without storage
// permissions and survives application updates.
internal static class Idas3PlatformPaths
{
    internal static bool IsAndroid => Application.platform == RuntimePlatform.Android;

    internal static string GameRoot
    {
        get
        {
            if (IsAndroid) return Path.Combine(Application.persistentDataPath, "game");
            return Path.GetFullPath(Path.Combine(Application.dataPath, ".."));
        }
    }

    internal static string RomRoot => Path.Combine(GameRoot, "rom");

    internal static string CustomMusicRoot => IsAndroid
        ? Path.Combine(Application.persistentDataPath, "Custom Music")
        : Path.Combine(Path.GetDirectoryName(Application.dataPath), Idas3CustomRaceMusic.FolderName);

    // Set by the Android asset bootstrap once optional imported-course packs
    // have a real filesystem root. Windows keeps its StreamingAssets layout.
    internal static string RuntimeAssetsRoot { get; set; }
    internal static string RuntimePackPath(string pack)
    {
        if (!string.IsNullOrWhiteSpace(RuntimeAssetsRoot)) return Path.Combine(RuntimeAssetsRoot, pack);
        if (IsAndroid) return Path.Combine(Application.persistentDataPath, "RuntimeData", "RuntimeAssets", pack);
        return Path.Combine(Application.streamingAssetsPath, pack);
    }

    internal static string NativePluginFingerprintPath => IsAndroid
        ? Path.Combine(Application.persistentDataPath, "android-native-build-id.txt")
        : Path.Combine(Application.dataPath, "Plugins/x86_64/Idas3Unity.dll");
}
