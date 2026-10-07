using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;
using UnityEngine.Networking;

// Android packages StreamingAssets inside the APK. Native C++ needs an actual
// filesystem root, so copy the verified runtime data to app-private storage on
// first launch. The same manifest is emitted by Stage-GameData.ps1 and keeps
// the copy resumable at file granularity without trusting filenames alone.
internal static class Idas3AndroidRuntimeData
{
    [Serializable] private sealed class Manifest
    {
        public string schema;
        public Entry[] files;
    }
    [Serializable] private sealed class Entry
    {
        public string path, sha256;
        public long bytes;
    }

    internal static bool Ready { get; private set; }
    internal static string Root { get; private set; }
    internal static string Error { get; private set; }
    internal static int CompletedFiles { get; private set; }
    internal static int TotalFiles { get; private set; }
    internal static long CompletedBytes { get; private set; }
    internal static long TotalBytes { get; private set; }
    internal static string CurrentFile { get; private set; }
    internal static string Phase { get; private set; } = "Reading resource manifest";
    private static long currentBytes;
    internal static float Progress => TotalBytes > 0
        ? Mathf.Clamp01((float)((CompletedBytes + (double)currentBytes) / TotalBytes)) : 0;

    internal static IEnumerator Prepare(string packagedRoot)
    {
        if (!Idas3PlatformPaths.IsAndroid) { Ready = true; yield break; }
        if (Ready) yield break;
        Error = null;
        Root = null;
        CompletedFiles = TotalFiles = 0;
        CompletedBytes = TotalBytes = currentBytes = 0;
        CurrentFile = null;
        Phase = "Reading resource manifest";
        Debug.Log("IDAS3 Android data: reading manifest.");
        var preparation = PrepareFiles(packagedRoot);
        try
        {
            // Catch failures raised by MoveNext, not around a yield return:
            // otherwise IO failures escape Unity's coroutine and strand startup.
            while (true)
            {
                bool more;
                object current;
                try { more = preparation.MoveNext(); current = more ? preparation.Current : null; }
                catch (Exception error)
                {
                    Error = "Could not prepare Android game data: " + error.Message;
                    break;
                }
                if (!more) break;
                yield return current;
            }
        }
        finally { (preparation as IDisposable)?.Dispose(); }
    }

    private static IEnumerator PrepareFiles(string packagedRoot)
    {
        if (string.IsNullOrWhiteSpace(packagedRoot)) { Error = "Android runtime data path is empty."; yield break; }
        string manifestUrl = packagedRoot.TrimEnd('/') + "/data.manifest.json";
        using (var request = UnityWebRequest.Get(manifestUrl))
        {
            request.timeout = 60;
            yield return request.SendWebRequest();
            if (request.result != UnityWebRequest.Result.Success) { Error = "Could not read the Android game-data manifest: " + request.error; yield break; }
            Manifest manifest;
            try { manifest = JsonUtility.FromJson<Manifest>(request.downloadHandler.text); }
            catch (Exception error) { Error = "Android game-data manifest is invalid: " + error.Message; yield break; }
            if (manifest == null || manifest.schema != "idas3-unity-runtime-data-v1" || manifest.files == null || manifest.files.Length == 0)
            { Error = "Android game-data manifest is missing or incompatible."; yield break; }
            TotalFiles = manifest.files.Length;
            foreach (var entry in manifest.files)
            {
                if (!ValidEntry(entry)) { Error = "Android game-data manifest contains an invalid file."; yield break; }
                TotalBytes = checked(TotalBytes + entry.bytes);
            }

            string manifestId;
            using (var sha = SHA256.Create())
                manifestId = Convert.ToBase64String(sha.ComputeHash(Encoding.UTF8.GetBytes(request.downloadHandler.text)));
            string root = Path.Combine(Application.persistentDataPath, "RuntimeData", "IDAS3");
            string marker = Path.Combine(root, ".manifest-id");
            if (File.Exists(marker) && File.Exists(Path.Combine(root, "data", "original_physics", "fsca_table.bin")) &&
                string.Equals(File.ReadAllText(marker), manifestId, StringComparison.Ordinal))
            {
                CompletedFiles = TotalFiles; CompletedBytes = TotalBytes;
                Phase = "Starting game";
                Root = root; Ready = true;
                Debug.Log("IDAS3 Android data: verified installation marker found.");
                yield break;
            }

            double lastYield = Time.realtimeSinceStartupAsDouble;
            foreach (var entry in manifest.files)
            {
                CurrentFile = entry.path;
                Phase = "Checking existing resources";
                currentBytes = 0;
                string relative = Path.Combine("data", entry.path.Replace('/', Path.DirectorySeparatorChar));
                string destination = Path.GetFullPath(Path.Combine(root, relative));
                if (!Inside(root, destination)) { Error = "Android game-data manifest escaped its root."; yield break; }
                if (File.Exists(destination) && new FileInfo(destination).Length == entry.bytes &&
                    string.Equals(Hash(destination), entry.sha256, StringComparison.OrdinalIgnoreCase))
                {
                    CompleteEntry(entry);
                    // Resuming must keep the loading screen responsive too.
                    if (Time.realtimeSinceStartupAsDouble - lastYield >= 0.05)
                    { yield return null; lastYield = Time.realtimeSinceStartupAsDouble; }
                    continue;
                }
                // StreamingAssets is a URL on Android; '#'/'%' in actual asset
                // names must not turn into a fragment or an escape sequence.
                string source = packagedRoot.TrimEnd('/') + "/data/" + EscapeRelativeUrl(entry.path);
                string temporary = destination + ".download-" + Guid.NewGuid().ToString("N");
                try
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(destination));
                    using (var download = new UnityWebRequest(source, UnityWebRequest.kHttpVerbGET))
                    {
                        Phase = "Extracting resources";
                        download.downloadHandler = new DownloadHandlerFile(temporary, false) { removeFileOnAbort = true };
                        download.timeout = 300;
                        var operation = download.SendWebRequest();
                        while (!operation.isDone)
                        {
                            currentBytes = (long)Math.Min((ulong)entry.bytes, download.downloadedBytes);
                            yield return null;
                        }
                        if (download.result != UnityWebRequest.Result.Success)
                        { Error = "Could not unpack Android game data: " + entry.path + ": " + download.error; yield break; }
                    }
                    Phase = "Verifying resources";
                    currentBytes = entry.bytes;
                    if (!File.Exists(temporary) || new FileInfo(temporary).Length != entry.bytes ||
                        !string.Equals(Hash(temporary), entry.sha256, StringComparison.OrdinalIgnoreCase))
                        throw new InvalidDataException("Downloaded file failed its SHA-256 check.");
                    if (File.Exists(destination)) File.Delete(destination);
                    File.Move(temporary, destination);
                    CompleteEntry(entry);
                }
                finally { TryDelete(temporary); }
            }
            Directory.CreateDirectory(root);
            File.WriteAllText(marker, manifestId, new UTF8Encoding(false));
            Phase = "Starting game";
            CurrentFile = null;
            Root = root; Ready = true;
            Debug.Log("IDAS3 Android data: ready, " + CompletedFiles + " files / " + CompletedBytes + " bytes verified.");
        }
    }

    private static void CompleteEntry(Entry entry)
    {
        CompletedFiles++;
        CompletedBytes += entry.bytes;
        currentBytes = 0;
        if (CompletedFiles % 1000 == 0)
            Debug.Log("IDAS3 Android data: " + CompletedFiles + "/" + TotalFiles + " files verified.");
    }

    private static bool ValidEntry(Entry entry)
    {
        if (entry == null || string.IsNullOrWhiteSpace(entry.path) || entry.path.StartsWith("/", StringComparison.Ordinal) || entry.path.IndexOf('\0') >= 0 || entry.bytes < 0) return false;
        if (entry.path.IndexOf('\\') >= 0 || entry.path.IndexOf(':') >= 0) return false;
        foreach (var segment in entry.path.Split('/')) if (segment.Length == 0 || segment == "." || segment == "..") return false;
        return entry.sha256 != null && System.Text.RegularExpressions.Regex.IsMatch(entry.sha256, "\\A[0-9a-fA-F]{64}\\z");
    }

    private static bool Inside(string root, string path)
    {
        string basePath = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        return path.StartsWith(basePath, StringComparison.Ordinal);
    }
    private static string Hash(string path)
    { using (var stream = File.OpenRead(path)) using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant(); }
    private static string EscapeRelativeUrl(string path)
    {
        string[] segments = path.Split('/');
        for (int i = 0; i < segments.Length; ++i) segments[i] = Uri.EscapeDataString(segments[i]);
        return string.Join("/", segments);
    }
    private static void TryDelete(string path) { try { if (File.Exists(path)) File.Delete(path); } catch (Exception) { } }
}
