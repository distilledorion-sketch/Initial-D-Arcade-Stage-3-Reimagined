using System;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;

public static class Idas3UpdateCacheChecks {
    public static void RunAndBuild(){Run();Idas3Build.RebuildWindowsPlayer();}
    private static int checks;
    private static void Check(bool ok,string description){if(!ok)throw new Exception(description);++checks;Debug.Log("Update cache: "+description);}
    private static void Set(Idas3Updates updates,string field,object value){typeof(Idas3Updates).GetField(field,BindingFlags.Instance|BindingFlags.NonPublic).SetValue(updates,value);}
    private static void Shutdown(Idas3Updates updates){
        // Edit-mode objects never enter the play-mode lifecycle automatically.
        typeof(Idas3Updates).GetMethod("OnDestroy",BindingFlags.Instance|BindingFlags.NonPublic).Invoke(updates,null);
        UnityEngine.Object.DestroyImmediate(updates.gameObject);
    }
    private static void Wait(Func<bool> predicate,string description){
        var end=DateTime.UtcNow.AddSeconds(10);
        while(!predicate()&&DateTime.UtcNow<end)Thread.Sleep(10);
        Check(predicate(),description);
    }
    private sealed class Lease : IDisposable {
        private readonly IDisposable inner;
        internal readonly ManualResetEvent closed=new ManualResetEvent(false);
        internal Lease(IDisposable inner){this.inner=inner;}
        public void Dispose(){inner.Dispose();closed.Set();}
    }
    private static string Attach(Idas3Updates updates,string root,out Lease lease){
        string session=Path.Combine(root,Guid.NewGuid().ToString("N"));
        lease=new Lease(Idas3UpdateCache.AcquireLease(session));File.WriteAllText(Path.Combine(session,"game.zip"),"isolated fixture");
        Set(updates,"sessionFolder",session);Set(updates,"sessionLease",lease);return session;
    }
    private static bool Locked(string session){
        try{using(var stream=new FileStream(Path.Combine(session,".cleanup-lock"),FileMode.Open,FileAccess.ReadWrite,FileShare.None))return false;}
        catch(IOException){return true;}
    }
    public static void Run(){
        string proof=Path.GetFullPath("Verification/updater-cache-20260924/unity-lifecycle-"+Guid.NewGuid().ToString("N"));
        string root=Path.Combine(proof,"InitialDUpdates");Directory.CreateDirectory(root);
        var go=new GameObject("Isolated updater lifecycle checks");var updates=go.AddComponent<Idas3Updates>();
        Lease lease;string session=Attach(updates,root,out lease);
        var worker=new TaskCompletionSource<bool>();Set(updates,"sessionWorker",worker.Task);
        Shutdown(updates);
        Check(!lease.closed.WaitOne(100)&&Locked(session),"shutdown retains lease while staging worker is active");
        Check(File.Exists(Path.Combine(session,"game.zip")),"shutdown cannot remove an active worker's payload");
        worker.SetResult(true);
        Wait(()=>!Directory.Exists(session),"shutdown cleans preparation after worker completes");
        go=new GameObject("Isolated helper handoff checks");updates=go.AddComponent<Idas3Updates>();session=Attach(updates,root,out lease);
        using(var nativeLease=new FileStream(Path.Combine(session,".cleanup-lock"),FileMode.Open,FileAccess.ReadWrite,FileShare.ReadWrite)){
            Set(updates,"installerLaunched",true);Idas3UpdateCache.MarkSafe(session);
            Shutdown(updates);
            Wait(()=>lease.closed.WaitOne(0),"managed lease releases after helper launch");
            Idas3UpdateCache.Clean(root,DateTime.UtcNow);
            Check(File.Exists(Path.Combine(session,"game.zip"))&&Locked(session),"helper lease protects handoff until installer exits");
        }
        Idas3UpdateCache.Clean(root,DateTime.UtcNow);Check(!Directory.Exists(session),"post-helper cleanup removes completed session");
        go=new GameObject("Isolated failed worker checks");updates=go.AddComponent<Idas3Updates>();session=Attach(updates,root,out lease);
        worker=new TaskCompletionSource<bool>();Set(updates,"sessionWorker",worker.Task);Shutdown(updates);
        worker.SetException(new IOException("Controlled staging failure"));Wait(()=>!Directory.Exists(session),"failed staging worker also cleans abandoned session");
        // Existing update-choice checks cover patch fallback and full-repair selection.
        var type=typeof(Idas3Updates).Assembly.GetType("Idas3UpdateChecks");
        type.GetMethod("Run",BindingFlags.Static|BindingFlags.NonPublic).Invoke(null,new object[]{(Action<bool,string>)Check});
        string report="{\"passed\":true,\"checks\":"+checks+",\"scope\":\"managed lifecycle and update selection\"}";
        File.WriteAllText(Path.Combine(proof,"report.json"),report);Debug.Log("UPDATE CACHE CHECKS PASSED "+report);
    }
}
