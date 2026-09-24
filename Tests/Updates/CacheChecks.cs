using System;
using System.Diagnostics;
using System.IO;
using System.Text;

// Exercises real Windows sharing modes, junctions and process identities in
// isolated fixtures. Never reads or deletes the player's actual update cache.
public static class CacheChecks {
    static string fixtures; static int checks;
    static readonly DateTime now = DateTime.UtcNow;
    static string Session(string name) {
        string path = Path.Combine(fixtures, name, "InitialDUpdates", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(path); return path;
    }
    static void Write(string session, string name, string text) {
        string path = Path.Combine(session, name); Directory.CreateDirectory(Path.GetDirectoryName(path)); File.WriteAllText(path, text);
    }
    static void Payload(string session) { Write(session, "game.zip", "archive"); Write(session, "stage/nested/game.dat", "new"); Write(session, "backup/game.dat", "old"); }
    static void Old(string session) {
        foreach(string f in Directory.GetFiles(session, "*", SearchOption.AllDirectories)) File.SetLastWriteTimeUtc(f, now.AddDays(-2));
        foreach(string d in Directory.GetDirectories(session, "*", SearchOption.AllDirectories)) Directory.SetLastWriteTimeUtc(d, now.AddDays(-2));
        Directory.SetLastWriteTimeUtc(session, now.AddDays(-2));
    }
    static void Clean(string session) { Idas3UpdateCache.Clean(Path.GetDirectoryName(session), now); }
    static void Check(bool condition, string name) { if(!condition)throw new Exception(name); ++checks; Console.WriteLine("PASS " + name); }
    static void Safe(string session) { Idas3UpdateCache.MarkSafe(session); }
    static void Plan(string session, int pid, long stamp) {
        using(var w = new BinaryWriter(File.Create(Path.Combine(session,"install.plan")),Encoding.Unicode)) {
            w.Write(Encoding.ASCII.GetBytes("IDUPD002")); string root=Path.GetDirectoryName(fixtures); w.Write(root.Length); w.Write(Encoding.Unicode.GetBytes(root)); w.Write(pid); w.Write(stamp); w.Write(0);
        }
    }
    static void Junction(string link,string target) {
        var start=new ProcessStartInfo("cmd.exe", "/d /c mklink /J \""+link+"\" \""+target+"\"") {UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};
        using(var p=Process.Start(start)){p.WaitForExit();if(p.ExitCode!=0)throw new Exception("Could not create junction fixture: "+p.StandardError.ReadToEnd());}
    }
    public static int Main(string[] args) {
        fixtures=Path.GetFullPath(args[0]); Directory.CreateDirectory(fixtures);
        string s=Session("safe");Payload(s);Safe(s);Clean(s);Check(!Directory.Exists(s),"confirmed safe payload removed immediately");
        s=Session("lease");Payload(s);using(Idas3UpdateCache.AcquireLease(s)){Safe(s);Clean(s);Check(File.Exists(Path.Combine(s,"game.zip")),"active lease protects even terminal status");}Clean(s);Check(!Directory.Exists(s),"released lease allows cleanup");
        s=Session("legacy-success");Payload(s);Plan(s,0,0);Write(s,"result.json","{\"passed\":true,\"changedFiles\":3}");Old(s);Clean(s);Check(!Directory.Exists(s),"old native success reclaimed");
        s=Session("legacy-restart-failure");Payload(s);Write(s,"result.json","{\"passed\":true}");Write(s,"error.txt","Could not restart the game.");Old(s);Clean(s);Check(!Directory.Exists(s),"legacy committed restart failure reclaimed");
        s=Session("pending");Payload(s);Write(s,"ready","Prepared");Old(s);Clean(s);Check(File.Exists(Path.Combine(s,"backup/game.dat")),"uncertain recovery preserved");
        s=Session("rollback-warning");Payload(s);Safe(s);Write(s,"error.txt","Rollback needs attention: game.dat");Old(s);Clean(s);Check(File.Exists(Path.Combine(s,"backup/game.dat")),"rollback warning overrides safe status");
        s=Session("rollback-needed");Payload(s);Write(s,"result.json","{\"schema\":1,\"cleanupSafe\":true,\"rollbackNeeded\":true}");Old(s);Clean(s);Check(File.Exists(Path.Combine(s,"backup/game.dat")),"rollback-needed flag preserves backups");
        s=Session("abandoned");Write(s,"game.zip","partial");Write(s,"stage/game.dat","staged");Old(s);Clean(s);Check(!Directory.Exists(s),"stale abandoned download reclaimed");
        s=Session("fresh-file");Write(s,"game.zip","partial");Old(s);File.SetLastWriteTimeUtc(Path.Combine(s,"game.zip"),now);Clean(s);Check(Directory.Exists(s),"recent payload protects old session");
        s=Session("unknown");Payload(s);Safe(s);Write(s,"personal.txt","keep");Clean(s);Check(File.ReadAllText(Path.Combine(s,"personal.txt"))=="keep","unknown files preserve whole session");
        s=Session("invalid-name");string invalid=Path.Combine(Path.GetDirectoryName(s),"my-backups");Directory.Move(s,invalid);Write(invalid,"game.zip","keep");SafeInvalid(invalid);Old(invalid);Clean(invalid);Check(Directory.Exists(invalid),"non-GUID folders preserved");
        s=Session("malformed-status");Payload(s);Write(s,"result.json","broken");Old(s);Clean(s);Check(File.Exists(Path.Combine(s,"game.zip")),"malformed status preserves payload");
        s=Session("oversized-status");Payload(s);Write(s,"result.json",new string(' ',33000)+"{\"passed\":true}");Old(s);Clean(s);Check(File.Exists(Path.Combine(s,"game.zip")),"oversized status preserved");
        s=Session("wrong-type");Write(s,"stage","keep");Safe(s);Clean(s);Check(File.ReadAllText(Path.Combine(s,"stage"))=="keep","payload directory names cannot be files");
        s=Session("locked-payload");Payload(s);Safe(s);using(var locked=new FileStream(Path.Combine(s,"game.zip"),FileMode.Open,FileAccess.Read,FileShare.None)){Clean(s);Check(File.Exists(Path.Combine(s,"result.json")),"failed cleanup retains terminal status");}Clean(s);Check(!Directory.Exists(s),"locked-file cleanup retries successfully");
        using(var current=Process.GetCurrentProcess()) {
            s=Session("live-parent");Write(s,"game.zip","partial");Plan(s,current.Id,current.StartTime.ToUniversalTime().ToFileTimeUtc());Old(s);Clean(s);Check(Directory.Exists(s),"live legacy parent protected");
            s=Session("reused-pid");Write(s,"game.zip","partial");Plan(s,current.Id,current.StartTime.ToUniversalTime().ToFileTimeUtc()-1);Old(s);Clean(s);Check(!Directory.Exists(s),"reused PID does not retain abandoned cache");
        }
        s=Session("legacy-powershell");Payload(s);Write(s,"install.ps1","# legacy");Write(s,"install.json","{\"parentId\":0,\"parentStartTicks\":\"0\"}");Write(s,"result.json","{\"passed\":true}");Old(s);
        Idas3UpdateCache.Clean(Path.GetDirectoryName(s),now,delegate(string path){return true;});Check(File.Exists(Path.Combine(s,"game.zip")),"active legacy helper prevents cleanup");
        Idas3UpdateCache.Clean(Path.GetDirectoryName(s),now.AddDays(2),delegate(string path){return false;});Check(!Directory.Exists(s),"inactive legacy PowerShell success reclaimed");
        string outside=Path.Combine(fixtures,"outside");Directory.CreateDirectory(outside);Write(outside,"sentinel.txt","untouched");
        s=Session("nested-junction");Safe(s);Directory.CreateDirectory(Path.Combine(s,"stage"));Junction(Path.Combine(s,"stage/link"),outside);Clean(s);Check(Directory.Exists(s)&&File.ReadAllText(Path.Combine(outside,"sentinel.txt"))=="untouched","nested junction never traversed");
        s=Session("session-junction");Directory.Delete(s);Junction(s,outside);Clean(s);Check(Directory.Exists(s)&&File.Exists(Path.Combine(outside,"sentinel.txt")),"session junction preserved");
        string linkedRoot=Path.Combine(fixtures,"root-junction","InitialDUpdates");Directory.CreateDirectory(Path.GetDirectoryName(linkedRoot));Junction(linkedRoot,outside);Idas3UpdateCache.Clean(linkedRoot,now);Check(File.Exists(Path.Combine(outside,"sentinel.txt")),"linked cache root refused");
        Idas3UpdateCache.Clean(fixtures,now);Check(File.Exists(Path.Combine(outside,"sentinel.txt")),"unexpected cache root refused");
        File.WriteAllText(Path.Combine(fixtures,"report.json"),"{\"passed\":true,\"checks\":"+checks+"}");Console.WriteLine("Cache checks passed: "+checks);return 0;
    }
    static void SafeInvalid(string path){Write(path,"result.json","{\"schema\":1,\"cleanupSafe\":true}");}
}
