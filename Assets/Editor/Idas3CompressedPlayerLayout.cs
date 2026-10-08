using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;

public sealed class Idas3CompressedPlayerLayout : IPostprocessBuildWithReport
{
    // Existing updater versions only replace files and require the old manager
    // name. Explicit zero-byte replacements retire the loose Unity payloads
    // during that same transaction (including rollback). Omitting them would
    // leave almost a gigabyte behind on upgraded installs. Unity uses the
    // complete data.unity3d archive instead; never truncate files without it.
    internal static readonly string[] RetiredLooseFiles = {
        "globalgamemanagers", "globalgamemanagers.assets", "globalgamemanagers.assets.resS",
        "level0", "resources.assets", "resources.assets.resS",
        "sharedassets0.assets", "sharedassets0.assets.resS", "Resources/unity_builtin_extra"
    };

    public int callbackOrder => 90;
    public void OnPostprocessBuild(BuildReport report)
    {
        if ((report.summary.options & (BuildOptions.CompressWithLz4 | BuildOptions.CompressWithLz4HC)) != 0)
            StageCompatibility(report.summary.platform, report.summary.outputPath);
    }

    public static void StageCompatibility(BuildTarget target, string outputPath)
    {
        if (target != BuildTarget.StandaloneWindows64) return;
        string data = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(outputPath)),
            Path.GetFileNameWithoutExtension(outputPath) + "_Data");
        string bundle = Path.Combine(data, "data.unity3d");
        if (!File.Exists(bundle)) throw new FileNotFoundException("Compressed player archive missing.", bundle);
        Idas3UpdateStaging.NoLinks(bundle);
        using (var input = File.OpenRead(bundle)) {
            var header = new byte[8];
            if (input.Read(header, 0, header.Length) != 8 || System.Text.Encoding.ASCII.GetString(header) != "UnityFS\0")
                throw new IOException("Compressed player archive header is invalid.");
        }
        foreach (string name in RetiredLooseFiles) {
            string path = Path.Combine(data, name);
            Idas3UpdateStaging.NoLinks(path);
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllBytes(path, Array.Empty<byte>());
        }
    }
}
