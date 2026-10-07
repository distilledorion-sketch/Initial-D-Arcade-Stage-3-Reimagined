using System;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using UnityEditor.Android;
using UnityEngine;

// Keep this in source instead of patching Library/Bee after every export.
public sealed class Idas3AndroidGradle : IPostGenerateGradleAndroidProject
{
    public int callbackOrder => 1000;

    public void OnPostGenerateGradleAndroidProject(string path)
    {
        string root = Path.GetFullPath(path);
        if (!File.Exists(Path.Combine(root, "gradle.properties"))) root = Path.GetDirectoryName(root);
        string properties = Path.Combine(root, "gradle.properties");
        if (!File.Exists(properties)) throw new FileNotFoundException("Generated Gradle properties missing", properties);
        string content = File.ReadAllText(properties);
        // Unity normally exempts ALL StreamingAssets extensions from ZIP
        // compression. Our 4.48 GB raw data would then exceed a ZIP32 APK.
        // RuntimeData uses UnityWebRequest and extracts to private storage, so
        // the custom binary/texture/mesh formats can safely be ZIP-compressed.
        // Keep media/ROM already using compressed formats stored verbatim.
        content = SetProperty(content, "unityStreamingAssets", ".png, .chd, .mp3, .ogg, .adx");
        content = SetProperty(content, "org.gradle.workers.max", "4");
        content = SetProperty(content, "org.gradle.logging.level", "info");
        content = SetProperty(content, "systemProp.org.gradle.internal.http.connectionTimeout", "30000");
        content = SetProperty(content, "systemProp.org.gradle.internal.http.socketTimeout", "60000");
        File.WriteAllText(properties, content, new UTF8Encoding(false));
        // AGP's local lint AAR includes all raw assets, even when the final APK
        // compresses them. Allow ZIP64 only for this diagnostic intermediary;
        // Android APKs must remain ordinary ZIP32 archives.
        string libraryBuild = Path.Combine(root, "unityLibrary", "build.gradle");
        string libraryContent = File.ReadAllText(libraryBuild);
        const string marker = "// IDAS3: large local lint archive";
        if (!libraryContent.Contains(marker))
        {
            File.AppendAllText(libraryBuild, "\n" + marker + "\n" +
                "tasks.withType(org.gradle.api.tasks.bundling.Zip).configureEach {\n" +
                "    if (name.endsWith('LocalLintAar')) {\n" +
                "        zip64 = true\n" +
                "        entryCompression = org.gradle.api.tasks.bundling.ZipEntryCompression.DEFLATED\n" +
                "    }\n" +
                "}\n", new UTF8Encoding(false));
        }
        Debug.Log("Android: enabled lossless ZIP compression for runtime data; Gradle workers limited to 4.");
    }

    private static string SetProperty(string content, string name, string value)
    {
        string pattern = @"(?m)^" + Regex.Escape(name) + @"=[^\r\n]*";
        return Regex.IsMatch(content, pattern)
            ? Regex.Replace(content, pattern, name + "=" + value)
            : content.TrimEnd() + Environment.NewLine + name + "=" + value + Environment.NewLine;
    }
}
