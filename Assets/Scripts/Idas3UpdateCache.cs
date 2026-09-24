using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Text;
using System.Text.RegularExpressions;
using Microsoft.Win32.SafeHandles;

// Only updater-owned GUID sessions below InitialDUpdates are eligible. A
// shared lease spans managed preparation and native installation; the cleaner
// needs exclusive access. Unknown recovery state is deliberately retained.
public static class Idas3UpdateCache
{
    public static string Root { get { return Path.Combine(Path.GetTempPath(), "InitialDUpdates"); } }
    private static readonly object gate = new object();
    private const int MaximumEntries = 100000, MaximumDepth = 48, MaximumStatusBytes = 32768;
    private static readonly HashSet<string> files = new HashSet<string>(StringComparer.OrdinalIgnoreCase) {
        "game.zip", "install.exe", "install.ps1", "install.json", "install.plan", "ready", "result.json", "result.json.tmp",
        "error.txt", "patch-invalid", "owner.json", ".cleanup-lock", ".install-lock"
    };
    public sealed class CleanupSummary { public int removed, kept, failed; }
    [DataContract] private sealed class Result {
        [DataMember] public int schema;
        [DataMember] public bool passed;
        [DataMember] public bool cleanupSafe;
        [DataMember] public bool rollbackNeeded;
    }
    [DataContract] private sealed class Owner {
        [DataMember] public int parentId;
        [DataMember] public long parentFileTime;
    }
    [DataContract] private sealed class LegacyPlan {
        [DataMember] public int parentId;
        [DataMember] public string parentStartTicks;
    }
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern SafeFileHandle CreateFile(string path, uint access, uint share, IntPtr security, uint mode, uint flags, IntPtr template);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern uint GetLongPathName(string shortPath, StringBuilder longPath, uint size);

    private static void NoLinks(string path) {
        for (string p = Path.GetFullPath(path); !string.IsNullOrEmpty(p); p = Path.GetDirectoryName(p)) {
            if (string.Equals(p.TrimEnd(Path.DirectorySeparatorChar), Path.GetPathRoot(p).TrimEnd(Path.DirectorySeparatorChar), StringComparison.OrdinalIgnoreCase)) break;
            try { if ((File.GetAttributes(p) & FileAttributes.ReparsePoint) != 0) throw new IOException("Linked update cache path."); }
            catch (FileNotFoundException) { }
            catch (DirectoryNotFoundException) { }
        }
    }
    private static SafeFileHandle Pin(string directory) {
        NoLinks(directory);
        var handle = CreateFile(directory, 0x80, 3, IntPtr.Zero, 3, 0x02000000, IntPtr.Zero);
        if (handle.IsInvalid) { handle.Dispose(); throw new IOException("Cannot protect update cache directory."); }
        try { NoLinks(directory); return handle; } catch { handle.Dispose(); throw; }
    }
    private static string CheckedRoot(string root) {
        root = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar);
        if (!string.Equals(Path.GetFileName(root), "InitialDUpdates", StringComparison.OrdinalIgnoreCase)) throw new IOException("Unexpected update cache root.");
        NoLinks(root); return root;
    }
    private static string CheckedSession(string session) {
        session = Path.GetFullPath(session).TrimEnd(Path.DirectorySeparatorChar);
        CheckedRoot(Path.GetDirectoryName(session));
        if (!Regex.IsMatch(Path.GetFileName(session), "\\A[0-9a-fA-F]{32}\\z")) throw new IOException("Unexpected update session name.");
        NoLinks(session); return session;
    }
    private static T ReadJson<T>(string path) {
        NoLinks(path);
        using (var input = File.OpenRead(path)) {
            if (input.Length > MaximumStatusBytes) throw new IOException("Oversized update status.");
            return (T)new DataContractJsonSerializer(typeof(T)).ReadObject(input);
        }
    }
    private static string ReadText(string path) {
        NoLinks(path);
        using (var input = File.OpenRead(path)) {
            if (input.Length > MaximumStatusBytes) throw new IOException("Oversized update status.");
            using (var reader = new StreamReader(input)) return reader.ReadToEnd();
        }
    }
    public static IDisposable AcquireLease(string session) {
        session = CheckedSession(session); Directory.CreateDirectory(session);
        using (Pin(session)) {
            string path = Path.Combine(session, ".cleanup-lock"); NoLinks(path);
            var lease = new FileStream(path, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.ReadWrite);
            try {
                string ownerPath = Path.Combine(session, "owner.json"); NoLinks(ownerPath);
                using (var process = Process.GetCurrentProcess())
                using (var output = new FileStream(ownerPath, FileMode.CreateNew, FileAccess.Write, FileShare.Read))
                    new DataContractJsonSerializer(typeof(Owner)).WriteObject(output, new Owner { parentId = process.Id, parentFileTime = process.StartTime.ToUniversalTime().ToFileTimeUtc() });
                return lease;
            } catch { lease.Dispose(); throw; }
        }
    }
    // Call only after all managed I/O has stopped and no helper was launched.
    public static void MarkSafe(string session) {
        session = CheckedSession(session);
        using (Pin(session)) {
            string path = Path.Combine(session, "result.json"); NoLinks(path);
            using (var output = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read))
                new DataContractJsonSerializer(typeof(Result)).WriteObject(output, new Result { schema = 1, cleanupSafe = true });
        }
    }
    private static bool ProcessAlive(int id, long time, bool fileTime) {
        if (id <= 0) return false;
        try { using (var p = Process.GetProcessById(id)) return (fileTime ? p.StartTime.ToUniversalTime().ToFileTimeUtc() : p.StartTime.ToUniversalTime().Ticks) == time; }
        catch (ArgumentException) { return false; }
        catch (InvalidOperationException) { return false; }
        catch { return true; } // Cannot prove inactivity.
    }
    private static string LongPath(string path) {
        var value = new StringBuilder(32768);
        uint length = GetLongPathName(path, value, (uint)value.Capacity);
        if (length == 0 || length >= value.Capacity) throw new IOException("Cannot resolve installer image name.");
        return Path.GetFullPath(value.ToString());
    }
    private static bool LegacyActive(string session) {
        string owner = Path.Combine(session, "owner.json");
        if (File.Exists(owner)) { var p = ReadJson<Owner>(owner); if (p == null || ProcessAlive(p.parentId, p.parentFileTime, true)) return true; }
        string binary = Path.Combine(session, "install.plan");
        if (File.Exists(binary)) {
            NoLinks(binary);
            using (var reader = new BinaryReader(File.OpenRead(binary), Encoding.Unicode)) {
                if (Encoding.ASCII.GetString(reader.ReadBytes(8)) != "IDUPD002") return true;
                int length = reader.ReadInt32(); if (length <= 0 || length >= 32768) return true;
                if (reader.ReadBytes(length * 2).Length != length * 2) return true;
                if (ProcessAlive(reader.ReadInt32(), reader.ReadInt64(), true)) return true;
            }
        }
        string json = Path.Combine(session, "install.json");
        bool powershell = File.Exists(json) || File.Exists(Path.Combine(session, "install.ps1"));
        if (File.Exists(json)) {
            var p = ReadJson<LegacyPlan>(json); long stamp;
            if (p == null || !long.TryParse(p.parentStartTicks, out stamp) || ProcessAlive(p.parentId, stamp, false)) return true;
        }
        foreach (var process in Process.GetProcesses()) using (process) {
            string name;
            try { name = process.ProcessName; } catch { return true; }
            if (powershell && (name.Equals("powershell", StringComparison.OrdinalIgnoreCase) || name.Equals("pwsh", StringComparison.OrdinalIgnoreCase))) return true;
            if (!name.Equals("install", StringComparison.OrdinalIgnoreCase)) continue;
            try {
                if (string.Equals(LongPath(process.MainModule.FileName), LongPath(Path.Combine(session, "install.exe")), StringComparison.OrdinalIgnoreCase)) return true;
            } catch (InvalidOperationException) { } catch { return true; }
        }
        return false;
    }
    private static void Scan(string directory, int depth, ref int count, ref DateTime latest, ref bool hasBackup, bool backup) {
        if (depth > MaximumDepth) throw new IOException("Update cache depth limit.");
        using (Pin(directory)) foreach (string entry in Directory.EnumerateFileSystemEntries(directory)) {
            if (++count > MaximumEntries) throw new IOException("Update cache entry limit.");
            NoLinks(entry); var attributes = File.GetAttributes(entry);
            if (depth == 0 && !files.Contains(Path.GetFileName(entry)) && Path.GetFileName(entry) != "stage" && Path.GetFileName(entry) != "backup") throw new IOException("Unknown update cache entry.");
            bool childBackup = backup || depth == 0 && Path.GetFileName(entry) == "backup";
            if ((attributes & FileAttributes.Directory) != 0) {
                if (depth == 0 && Path.GetFileName(entry) != "stage" && Path.GetFileName(entry) != "backup") throw new IOException("Unexpected cache directory.");
                Scan(entry, depth + 1, ref count, ref latest, ref hasBackup, childBackup);
            } else {
                if (depth == 0 && (Path.GetFileName(entry) == "stage" || Path.GetFileName(entry) == "backup")) throw new IOException("Unexpected cache payload file.");
                if (childBackup) hasBackup = true;
                if (Path.GetFileName(entry) != ".cleanup-lock") latest = Later(latest, File.GetLastWriteTimeUtc(entry));
            }
        }
    }
    private static DateTime Later(DateTime a, DateTime b) { return a > b ? a : b; }
    private static void DeleteTree(string path, int depth, ref int count) {
        if (depth > MaximumDepth) throw new IOException("Update cache depth limit.");
        using (Pin(path)) foreach (string entry in Directory.EnumerateFileSystemEntries(path)) {
            if (++count > MaximumEntries) throw new IOException("Update cache entry limit.");
            NoLinks(entry);
            if ((File.GetAttributes(entry) & FileAttributes.Directory) != 0) DeleteTree(entry, depth + 1, ref count);
            else File.Delete(entry);
        }
        NoLinks(path); Directory.Delete(path, false);
    }
    private static bool CleanSession(string session, DateTime now, Func<string, bool> active) {
        session = CheckedSession(session);
        using (Pin(session)) {
            DateTime latest = Directory.GetLastWriteTimeUtc(session);
            string lockPath = Path.Combine(session, ".cleanup-lock"); NoLinks(lockPath);
            using (var lease = new FileStream(lockPath, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None)) {
                int count = 0; bool backup = false; Scan(session, 0, ref count, ref latest, ref backup, false);
                string resultPath = Path.Combine(session, "result.json"), errorPath = Path.Combine(session, "error.txt");
                Result result = File.Exists(resultPath) ? ReadJson<Result>(resultPath) : null;
                if (File.Exists(errorPath) && ReadText(errorPath).IndexOf("Rollback needs attention", StringComparison.OrdinalIgnoreCase) >= 0) return false;
                if (result != null && result.rollbackNeeded) return false;
                bool explicitSafe = result != null && result.schema == 1 && result.cleanupSafe;
                if (!explicitSafe) {
                    if (latest > now.AddDays(-1) || active(session)) return false;
                    if (result != null) { if (result.schema != 0 || !result.passed) return false; }
                    else if (backup || File.Exists(Path.Combine(session, "ready"))) return false;
                }
                // Hold legacy native images against execution while deleting
                // the plan/payload. A running mapped image refuses this open.
                string installer = Path.Combine(session, "install.exe");
                using (var executable = File.Exists(installer) ? new FileStream(installer, FileMode.Open, FileAccess.ReadWrite, FileShare.None) : null) {
                    count = 0;
                    foreach (string directory in new[] { "stage", "backup" }) {
                        string path = Path.Combine(session, directory);
                        if (Directory.Exists(path)) DeleteTree(path, 1, ref count);
                    }
                    // Keep ownership/status until payload deletion succeeds.
                    foreach (string name in new[] { "game.zip", "install.plan", "install.json", "install.ps1", "ready", "patch-invalid", "result.json.tmp", ".install-lock", "error.txt" }) {
                        string path = Path.Combine(session, name); NoLinks(path); File.Delete(path);
                    }
                }
                NoLinks(installer); File.Delete(installer);
                foreach (string name in new[] { "owner.json", "result.json" }) { string path = Path.Combine(session, name); NoLinks(path); File.Delete(path); }
            }
            NoLinks(lockPath); File.Delete(lockPath);
        }
        NoLinks(session); Directory.Delete(session, false); return true;
    }
    public static CleanupSummary Clean(string root, DateTime utcNow) { return Clean(root, utcNow, LegacyActive); }
    internal static CleanupSummary Clean(string root, DateTime utcNow, Func<string, bool> legacyActive) {
        var summary = new CleanupSummary();
        lock (gate) try {
            root = CheckedRoot(root); if (!Directory.Exists(root)) return summary;
            using (Pin(root)) foreach (string session in Directory.EnumerateDirectories(root)) {
                if (!Regex.IsMatch(Path.GetFileName(session), "\\A[0-9a-fA-F]{32}\\z")) { ++summary.kept; continue; }
                try { if (CleanSession(session, utcNow, legacyActive)) ++summary.removed; else ++summary.kept; }
                catch { ++summary.failed; }
            }
        } catch { ++summary.failed; }
        return summary;
    }
}
