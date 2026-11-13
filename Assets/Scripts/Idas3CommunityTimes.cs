using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.Networking;

// HTTP service is optional and independent of multiplayer, the simulation and saves.
// No network wait is ever made from the game's Update or native render lock.
public sealed partial class Idas3CommunityTimes : MonoBehaviour
{
    public const string ServiceUrl="https://initial-d-leaderboard.initial-d-community-leaderboard.workers.dev";
    public const string Ruleset="d3-community-v1";
    public const string MinimumClientBuild="0.3.95-community-replays.1";
    public const string RequiredSubmissionBuild="0.3.95-community-replays.55";
    public const int FirstReplaySeason=2;
    // Public Season 2 uses database epoch 3; epoch 2 already held earlier runs.
    public const int CurrentSeasonEpoch=3;
    public const int LocalRecordSeason=2;
    public const string CurrentSeasonMinimumBuild="0.3.95-community-replays.55";
    [Serializable] public sealed class Run {
        public string id,ruleset,build;
        public int condition,weather,car,ticks6000,manual,night,points,epoch,imported;
        public int replayVersion;
        public bool replayAvailable;
        public int[] nameGlyphs,splits;
    }
    [Serializable] public sealed class Snapshot {public string ruleset;public int epoch;public long generatedAt;public Run[] entries;}
    [Serializable] internal sealed class Pending {public List<Run> runs=new List<Run>();}
    [Serializable] private sealed class Identity {public string token;}
    private Idas3SceneGame host;private Idas3GameOptions options;private Idas3PauseMenu menu;
    private string folder,credential;private Pending pending=new Pending();private Snapshot snapshot;
    private bool enabledLast,cacheDirty,registered,applied;private double nextNetwork,confirmedUntil;
    private string status="Connecting to community times…";
    private long responseCode;private byte[] responseBody;
    internal const int MaximumResponseBytes=2*1024*1024;
    // Per direction/weather: course top ten plus other models' bests, or one
    // pending personal best per model. Keep both tied to the current catalog.
    internal static int MaximumSnapshotEntries=>Idas3CourseCatalog.ConditionCount*2*(10+35-1);
    internal static int MaximumPendingRuns=>Idas3CourseCatalog.ConditionCount*2*35;
    private readonly CancellationTokenSource lifetime=new CancellationTokenSource();
    internal int PendingCount=>pending.runs.Count;
    private readonly byte[] finishBuffer=new byte[4096];
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SharedReadFinish([Out] byte[] buffer,int capacity);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern void Idas3SharedAckFinish();
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SharedSetRecords(int[] values,int count,int enabled);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneModeFlowValue(int field);
    public void Initialize(Idas3SceneGame owner,Idas3GameOptions settings,Idas3PauseMenu view,string diagnosticStorage=null,bool diagnosticOffline=false){
        host=owner;options=settings;menu=view;folder=Path.Combine(Application.persistentDataPath,"community-times");
        if(diagnosticOffline&&diagnosticStorage==null)throw new InvalidOperationException("Offline diagnostic requires isolated storage.");
        if(diagnosticStorage!=null){
            var p=Path.GetFullPath(diagnosticStorage);
            if(!File.Exists(Path.Combine(Path.GetDirectoryName(p),"ISOLATED_MODE_FLOW_TEST.txt")))throw new InvalidOperationException("Private community storage requires a marked diagnostic.");
            folder=p;
        }
        try{
            Directory.CreateDirectory(folder);
            MigrateSeasonCache(folder);
            if(File.Exists(Path.Combine(folder,"identity.json")))credential=Read<Identity>("identity.json",1024)?.token;
            if(string.IsNullOrEmpty(credential)||credential.Length!=64){byte[] bytes=new byte[32];using(var rng=RandomNumberGenerator.Create())rng.GetBytes(bytes);credential=BitConverter.ToString(bytes).Replace("-","").ToLowerInvariant();Write("identity.json",new Identity{token=credential});}
            if(File.Exists(Path.Combine(folder,"pending.json")))pending=Read<Pending>("pending.json",2*1024*1024)??new Pending();
            if(pending.runs==null)pending=new Pending();
            pending.runs.RemoveAll(r=>!Uploadable(r));if(pending.runs.Count>MaximumPendingRuns)pending.runs.RemoveRange(MaximumPendingRuns,pending.runs.Count-MaximumPendingRuns);
            Write("pending.json",pending);CleanReplayFiles();
            if(File.Exists(Path.Combine(folder,"snapshot.json"))){try{snapshot=Read<Snapshot>("snapshot.json",2*1024*1024);if(!UsableCommunitySnapshot(snapshot))snapshot=null;}catch(Exception){snapshot=null;}}
            cacheDirty=true;StartCoroutine(Collect());if(!diagnosticOffline)StartCoroutine(Synchronize());
            if(diagnosticStorage==null&&!Application.isEditor)StartCoroutine(ShareActivity());
        }catch(Exception e){status="Community storage unavailable. Local records remain active.";Debug.LogWarning("Community times initialization: "+e.GetType().Name);menu.CommunityStatus=status;}
    }
    private T Read<T>(string name,long max){var p=Path.Combine(folder,name);if(new FileInfo(p).Length>max)throw new IOException("Community file too large.");return JsonUtility.FromJson<T>(File.ReadAllText(p));}
    private void Write(string name,object value)=>WriteCache(folder,name,value,CancellationToken.None);
    private static void WriteCache(string directory,string name,object value,CancellationToken stop){
        string json=JsonUtility.ToJson(value),dest=Path.Combine(directory,name),temp=dest+".tmp";
        stop.ThrowIfCancellationRequested();
        File.WriteAllText(temp,json,new UTF8Encoding(false));
        stop.ThrowIfCancellationRequested();
        if(File.Exists(dest))File.Replace(temp,dest,null);else File.Move(temp,dest);
    }
    private void CleanReplayFiles()=>CleanReplayFiles(folder);
    private static void CleanReplayFiles(string directory){
        // Only the obsolete leaderboard upload cache, never personal replays.
        foreach(string path in Directory.GetFiles(directory,"*.idr")){
            string id=Path.GetFileNameWithoutExtension(path);
            if(Guid.TryParseExact(id,"D",out _))File.Delete(path);
        }
    }
    public static byte[] ReplayEnvelope(Run run,byte[] replay){
        var raw=Idas3ReplayCodec.Decode(replay);
        if(raw.Length<96||BitConverter.ToUInt32(raw,0)!=0x32524449||BitConverter.ToUInt32(raw,4)!=run.ticks6000||BitConverter.ToUInt32(raw,12)!=60||BitConverter.ToUInt32(raw,16)!=96||BitConverter.ToUInt32(raw,20)!=160||96L+160L*BitConverter.ToUInt32(raw,8)!=raw.Length)throw new InvalidDataException("A matching detailed replay is required.");
        byte[] meta=Encoding.UTF8.GetBytes(JsonUtility.ToJson(run));if(meta.Length>8192)throw new InvalidDataException("Run metadata too large.");
        byte[] result=new byte[4+meta.Length+replay.Length];Array.Copy(BitConverter.GetBytes(meta.Length),result,4);Array.Copy(meta,0,result,4,meta.Length);Array.Copy(replay,0,result,4+meta.Length,replay.Length);return result;
    }
    public static string TimeSubmissionJson(Run run){
        if(!Uploadable(run))throw new InvalidDataException("Invalid Time Attack submission.");
        return JsonUtility.ToJson(run);
    }
    public static bool SupportedBuild(string build)=>SupportedBuild(build,MinimumClientBuild);
    private static bool SupportedBuild(string build,string minimum){
        const string pattern=@"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([a-z][a-z0-9-]*)\.(0|[1-9]\d*))?$";
        if(build==null||build.Length>64)return false;
        var a=System.Text.RegularExpressions.Regex.Match(build,pattern);
        var b=System.Text.RegularExpressions.Regex.Match(minimum,pattern);
        if(!a.Success)return false;
        var av=new long[6];var bv=new long[6];
        foreach(int i in new[]{1,2,3,5}){
            if(a.Groups[i].Success&&(!long.TryParse(a.Groups[i].Value,out av[i])||av[i]>9007199254740991L))return false;
            if(b.Groups[i].Success)long.TryParse(b.Groups[i].Value,out bv[i]);
        }
        for(int i=1;i<=3;i++)if(av[i]!=bv[i])return av[i]>bv[i];
        return !a.Groups[4].Success||(a.Groups[4].Value==b.Groups[4].Value&&av[5]>=bv[5]);
    }
    // Historical posted results keep the compatibility floor above. Only newly
    // submitted runs use this exact release policy, including restored queues.
    public static bool SubmissionBuild(string build)=>string.Equals(build,RequiredSubmissionBuild,StringComparison.Ordinal);
    // This is a declared-client compatibility gate, not proof that a client is unmodified.
    public static bool Uploadable(Run r)=>Valid(r)&&r.imported==0&&r.ruleset==Ruleset&&r.epoch==CurrentSeasonEpoch&&SubmissionBuild(r.build)&&Guid.TryParseExact(r.id,"D",out _);
    internal static bool PrepareFreshFinish(Run run,string build){
        // Only an unstamped native finish may acquire this season/build. Never
        // relabel restored queues, snapshots or historical records as new runs.
        if(!Valid(run)||run.imported!=0||!SubmissionBuild(build)||run.epoch!=0||
           !string.IsNullOrEmpty(run.id)||!string.IsNullOrEmpty(run.ruleset)||!string.IsNullOrEmpty(run.build))return false;
        run.id=Guid.NewGuid().ToString();run.ruleset=Ruleset;run.build=build;run.epoch=CurrentSeasonEpoch;
        run.replayVersion=0;run.replayAvailable=false;
        return true;
    }
    public static bool Valid(Run r){
        if(r==null||r.condition<0||r.condition>=Idas3CourseCatalog.ConditionCount||r.weather<0||r.weather>1||r.car<0||r.car>34||r.ticks6000<60000||r.ticks6000>=10800000||r.nameGlyphs==null||r.nameGlyphs.Length!=5||r.splits==null||r.splits.Length!=4||r.imported<0||r.imported>1||r.manual<(r.imported==1?-1:0)||r.manual>1||r.night<(r.imported==1?-1:0)||r.night>1||r.points<(r.imported==1?-1:0)||r.points>999999)return false;
        foreach(int n in r.nameGlyphs)if(n<0||n>221)return false;
        int prior=0,count=0;bool ended=false;foreach(int n in r.splits){if(n==0){ended=true;continue;}if(ended||n<=prior||n>r.ticks6000)return false;prior=n;count++;}
        return (r.imported==1&&count==0)||(count>=2&&prior==r.ticks6000);
    }
    private static bool ValidSnapshot(Snapshot s){if(s==null||s.ruleset!=Ruleset||s.entries==null||s.entries.Length>MaximumSnapshotEntries)return false;foreach(var r in s.entries)if(!Valid(r))return false;return true;}
    internal static bool UsableCommunitySnapshot(Snapshot s){
        if(!ValidSnapshot(s)||s.epoch!=CurrentSeasonEpoch)return false;
        foreach(var r in s.entries)if(r.epoch!=s.epoch||r.imported!=0||!SupportedBuild(r.build,CurrentSeasonMinimumBuild))return false;
        return true;
    }
    public static int[] Flatten(Snapshot s){
        if(!ValidSnapshot(s))throw new InvalidDataException("Invalid community snapshot.");
        var values=new int[s.entries.Length*14];int at=0;
        foreach(var r in s.entries){values[at++]=r.condition;values[at++]=r.weather;values[at++]=r.car;values[at++]=r.ticks6000;foreach(int n in r.nameGlyphs)values[at++]=n;values[at++]=r.manual;values[at++]=r.night;bool four=r.splits[2]>0&&r.splits[2]<r.ticks6000;for(int i=0;i<3;i++)values[at++]=four?r.splits[i]:0;}
        return values;
    }
    internal static bool UseCommunityRecords(bool enabled,Snapshot current,double now,double liveUntil){
        // Snapshots are validated before assignment, never on the frame loop.
        return enabled&&current!=null&&now<liveUntil;
    }
    private IEnumerator Collect(){
        while(host!=null&&host.Ready){
            bool enabled=options.Current.communityTimes;
            try{
                if(enabled!=enabledLast){enabledLast=enabled;cacheDirty=true;nextNetwork=0;confirmedUntil=0;if(!enabled){clearWrite=ClearPending();status="Sharing is off. Personal records are active.";}else status="Connecting to community times…";}
                if(clearWrite!=null&&clearWrite.IsCompleted){if(!PublishPending(clearWrite))status="Could not save upload queue. Retrying.";clearWrite=null;}
                if(finishWrite!=null&&finishWrite.IsCompleted){
                    if(PublishPending(finishWrite)){
                        // A slow disk can outlast a race. Never acknowledge a
                        // newer native finish while committing the earlier one.
                        int latest=Idas3SharedReadFinish(finishBuffer,finishBuffer.Length);
                        if(latest>0&&Encoding.UTF8.GetString(finishBuffer,0,latest)==finishPayload)Idas3SharedAckFinish();
                        if(finishWrite.Result.changed)nextNetwork=0;
                    }else status="Could not save upload queue. Retrying.";
                    finishWrite=null;finishPayload=null;
                }
                if(!enabled&&pending.runs.Count>0&&clearWrite==null)clearWrite=ClearPending();
                if(Application.internetReachability==NetworkReachability.NotReachable)confirmedUntil=0;
                bool use=UseCommunityRecords(enabled,snapshot,Time.realtimeSinceStartupAsDouble,confirmedUntil);
                if(use!=applied)cacheDirty=true;
                // Apply downloaded data only while the native frontend is open.
                // A race keeps the records captured at its start.
                if(cacheDirty&&Idas3SceneModeFlowValue(4)==1){
                    int[] data=use?Flatten(snapshot):Array.Empty<int>();
                    if(Idas3SharedSetRecords(data,data.Length/14,use?1:0)==1){cacheDirty=false;applied=use;}
                }
                int length=Idas3SharedReadFinish(finishBuffer,finishBuffer.Length);
                if(length>0&&finishWrite==null){
                    bool acknowledge=true;
                    if(enabled){var run=JsonUtility.FromJson<Run>(Encoding.UTF8.GetString(finishBuffer,0,length));if(Valid(run)){
                        if(!SubmissionBuild(Application.version))status="Only game build "+RequiredSubmissionBuild+" can share Time Attack runs.";
                        else if(PrepareFreshFinish(run,Application.version)){finishPayload=Encoding.UTF8.GetString(finishBuffer,0,length);finishWrite=QueueFinish(run);acknowledge=false;}
                        else status="This result belongs to an earlier run. Complete a new Time Attack to share it.";
                    }else status="This run could not be shared: checkpoint validation failed.";}
                    if(acknowledge)Idas3SharedAckFinish();
                }
            }catch(Exception e){status="Community cache unavailable; racing continues.";Debug.LogWarning("Community times cache: "+e.GetType().Name);}
            menu.CommunityStatus=status+(pending.runs.Count>0?" Pending runs: "+pending.runs.Count+".":"")+(applied?" Rankings combine your personal bests and current community times.":"");
            yield return new WaitForSecondsRealtime(.5f);
        }
    }
    private IEnumerator Request(string path,string payload=null,bool authorize=false){
        responseCode=0;responseBody=null;
        using(var request=new UnityWebRequest(ServiceUrl+path,payload==null?"GET":"POST")){
            var response=new LimitedResponse();request.downloadHandler=response;request.timeout=10;
            if(payload!=null){request.uploadHandler=new UploadHandlerRaw(Encoding.UTF8.GetBytes(payload));request.SetRequestHeader("Content-Type","application/json");}
            if(authorize)request.SetRequestHeader("Authorization","Bearer "+credential);
            UnityWebRequestAsyncOperation operation=null;try{operation=request.SendWebRequest();}catch(Exception){status="Community times offline; uploads will retry.";}
            if(operation!=null){
                yield return operation;
                if(!response.Exceeded){
                    responseCode=request.responseCode;
                    if(responseCode==200&&request.result==UnityWebRequest.Result.Success)responseBody=response.Body;
                    else if(responseCode==200)responseCode=0;
                }
            }
        }
    }
    private IEnumerator RefreshSnapshot(){
        yield return Request("/api/v2/snapshot?ruleset="+Ruleset+"&imports=1");
        yield return ProcessSnapshot(responseCode,responseBody);
    }
    internal static Task<Snapshot> PrepareSnapshot(byte[] body,string directory,int minimumEpoch,CancellationToken stop){
        // Only detached plain data crosses this boundary. JsonUtility's string
        // overloads are thread-safe; no scene, options, native API or Unity time
        // is accessed here. A slow disk must not block the game's frame loop.
        return Task.Run(()=>{
            try {
                stop.ThrowIfCancellationRequested();
                if(body==null||body.Length>MaximumResponseBytes)return null;
                var next=JsonUtility.FromJson<Snapshot>(Encoding.UTF8.GetString(body));
                if(!UsableCommunitySnapshot(next)||next.epoch<minimumEpoch)return null;
                WriteCache(directory,"snapshot.json",next,stop);
                return next;
            }catch(Exception){return null;}
        });
    }
    internal IEnumerator ProcessSnapshot(long code,byte[] body){
        confirmedUntil=0;cacheDirty=true;
        if(code==200){
            var work=PrepareSnapshot(body,folder,CurrentSeasonEpoch,lifetime.Token);
            while(!work.IsCompleted)yield return null;
            if(lifetime.IsCancellationRequested||!options.Current.communityTimes)yield break;
            var next=work.GetAwaiter().GetResult();
            if(next!=null){snapshot=next;confirmedUntil=Time.realtimeSinceStartupAsDouble+75;
                status="Community times updated. Personal records stay with your save.";}
            else status="Community response unavailable; using personal records.";
        }
        else status="Community times offline; using personal records.";
    }
    internal sealed class LimitedResponse : DownloadHandlerScript {
        private readonly MemoryStream body=new MemoryStream();
        internal bool Exceeded {get;private set;}
        internal byte[] Body=>Exceeded?null:body.ToArray();
        internal LimitedResponse():base(new byte[8192]){}
        protected override bool ReceiveData(byte[] data,int length){
            if(Exceeded||data==null||length<0||length>data.Length||body.Length+length>MaximumResponseBytes){Exceeded=true;return false;}
            body.Write(data,0,length);return true;
        }
        public override void Dispose(){body.Dispose();base.Dispose();}
    }
    private void OnDestroy(){if(stopping)return;stopping=true;lifetime.Cancel();lifetime.Dispose();}
    private IEnumerator Synchronize(){
        yield return new WaitForSecondsRealtime(5);
        while(host!=null&&host.Ready){
            if(!options.Current.communityTimes||Time.realtimeSinceStartupAsDouble<nextNetwork){yield return new WaitForSecondsRealtime(1);continue;}
            nextNetwork=Time.realtimeSinceStartupAsDouble+60;
            // Reading current rankings is public: an upload/authentication
            // failure must not keep deleted remote records on screen.
            yield return RefreshSnapshot();
            if(!registered){yield return Request("/api/v1/register",JsonUtility.ToJson(new Identity{token=credential}));if(responseCode==200)registered=true;else{status=responseCode==403?"This installation is blocked from sharing.":"Community times offline; uploads will retry.";yield return null;continue;}}
            // Maximum one upload per second, preserving the server's rate limit.
            int sent=0;while(options.Current.communityTimes&&pending.runs.Count>0&&sent<5){
                var run=pending.runs[0];
                if(!Uploadable(run)){bool removed=false;yield return CommitPending(RemovePending(run.id),ok=>removed=ok);if(!removed)break;continue;}
                if(!options.Current.communityTimes)break;
                yield return Request("/api/v3/runs",TimeSubmissionJson(run),true);
                if(responseCode==200||responseCode==400||responseCode==409){
                    bool removed=false;yield return CommitPending(RemovePending(run.id),ok=>removed=ok);
                    if(!removed){status="Could not save upload queue. Uploads will retry.";break;}
                    if(responseCode!=200)status="A run was rejected by the leaderboard.";sent++;
                }
                else{status=responseCode==403?"This installation is blocked from sharing.":"Upload pending; the service will retry.";if(responseCode==401)registered=false;break;}
                yield return new WaitForSecondsRealtime(1);
            }
            if(sent>0&&options.Current.communityTimes)yield return RefreshSnapshot();
            nextNetwork=Time.realtimeSinceStartupAsDouble+60;
        }
    }
    [Serializable] internal sealed class ActivityReport {public int online,queuing,racing,age;public bool limited;}
    internal static ActivityReport PrepareActivity(Idas3.Multiplayer.Idas3OnlineActivity activity,double now,double last){
        double age=now-activity.ObservedAt;
        if(!activity.Available||activity.Online<0||activity.Online>200||activity.Queuing<0||activity.Queuing>activity.Online||activity.Racing<0||activity.Racing>activity.Online||activity.Queuing+activity.Racing>activity.Online||activity.ObservedAt==last||double.IsNaN(age)||age<0||age>30)return null;
        return new ActivityReport{online=activity.Online,queuing=activity.Queuing,racing=activity.Racing,limited=activity.Limited,age=(int)Math.Ceiling(age)};
    }
    private IEnumerator ShareActivity(){
        double last=-1;
        while(host!=null){
            var session=host.MultiplayerSession;
            bool share=host.Ready&&options.Current.communityTimes&&registered;
            var activity=session?.Activity??default;
            var report=PrepareActivity(activity,Time.realtimeSinceStartupAsDouble,last);
            if(share&&report!=null){
                last=activity.ObservedAt;
                // Own request state: uploads and ranking refreshes may run concurrently.
                using(var request=new UnityWebRequest(ServiceUrl+"/api/v2/activity","POST")){
                    request.uploadHandler=new UploadHandlerRaw(Encoding.UTF8.GetBytes(JsonUtility.ToJson(report)));
                    request.downloadHandler=new DownloadHandlerBuffer();request.timeout=8;
                    request.SetRequestHeader("Content-Type","application/json");request.SetRequestHeader("Authorization","Bearer "+credential);
                    UnityWebRequestAsyncOperation operation=null;try{operation=request.SendWebRequest();}catch(Exception){}
                    if(operation!=null)yield return operation;
                }
            }
            yield return new WaitForSecondsRealtime(15);
        }
    }
}
