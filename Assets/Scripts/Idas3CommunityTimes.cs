using System.Threading.Tasks;
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;
using UnityEngine.Networking;

// HTTP service is optional and independent of Steam, the simulation and saves.
// No network wait is ever made from the game's Update or native render lock.
public sealed class Idas3CommunityTimes : MonoBehaviour
{
    public const string ServiceUrl="https://initial-d-leaderboard.initial-d-community-leaderboard.workers.dev";
    public const string Ruleset="d3-community-v1";
    public const string MinimumClientBuild="0.3.95-community-replays.1";
    public const int FirstReplaySeason=2;
    [Serializable] public sealed class Run {
        public string id,ruleset,build;
        public int condition,weather,car,ticks6000,manual,night,points,epoch,imported;
        public int replayVersion;
        public bool replayAvailable;
        public int[] nameGlyphs,splits;
    }
    [Serializable] public sealed class Snapshot {public string ruleset;public int epoch;public long generatedAt;public Run[] entries;}
    [Serializable] private sealed class Pending {public List<Run> runs=new List<Run>();}
    [Serializable] private sealed class Identity {public string token;}
    private Idas3SceneGame host;private Idas3GameOptions options;private Idas3PauseMenu menu;
    private string folder,credential;private Pending pending=new Pending();private Snapshot snapshot;
    private bool enabledLast,cacheDirty,registered,applied;private double nextNetwork,confirmedUntil;
    private Task<byte[]> encodingJob;private Run encodingRun;private string encodingSource;
    private string status="Connecting to community times…";
    private long responseCode;private string responseText;
    internal int PendingCount=>pending.runs.Count;
    private readonly byte[] finishBuffer=new byte[4096];
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SharedReadFinish([Out] byte[] buffer,int capacity);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern void Idas3SharedAckFinish();
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SharedReadReplay([Out] byte[] buffer,int capacity);
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
            if(File.Exists(Path.Combine(folder,"identity.json")))credential=Read<Identity>("identity.json",1024)?.token;
            if(string.IsNullOrEmpty(credential)||credential.Length!=64){byte[] bytes=new byte[32];using(var rng=RandomNumberGenerator.Create())rng.GetBytes(bytes);credential=BitConverter.ToString(bytes).Replace("-","").ToLowerInvariant();Write("identity.json",new Identity{token=credential});}
            if(File.Exists(Path.Combine(folder,"pending.json")))pending=Read<Pending>("pending.json",2*1024*1024)??new Pending();
            pending.runs.RemoveAll(r=>!Uploadable(r));if(pending.runs.Count>1980)pending.runs.RemoveRange(1980,pending.runs.Count-1980);
            Write("pending.json",pending);CleanReplayFiles();
            if(File.Exists(Path.Combine(folder,"snapshot.json"))){try{snapshot=Read<Snapshot>("snapshot.json",2*1024*1024);if(!UsableCommunitySnapshot(snapshot))snapshot=null;}catch(Exception){snapshot=null;}}
            cacheDirty=true;StartCoroutine(Collect());if(!diagnosticOffline)StartCoroutine(Synchronize());
        }catch(Exception e){status="Community storage unavailable. Local records remain active.";Debug.LogWarning("Community times initialization: "+e.GetType().Name);menu.CommunityStatus=status;}
    }
    private T Read<T>(string name,long max){var p=Path.Combine(folder,name);if(new FileInfo(p).Length>max)throw new IOException("Community file too large.");return JsonUtility.FromJson<T>(File.ReadAllText(p));}
    private void Write(string name,object value){string dest=Path.Combine(folder,name),temp=dest+".tmp";File.WriteAllText(temp,JsonUtility.ToJson(value),new UTF8Encoding(false));if(File.Exists(dest))File.Replace(temp,dest,null);else File.Move(temp,dest);}
    private string ReplayPath(Run run){if(!Guid.TryParseExact(run.id,"D",out _))throw new InvalidDataException("Invalid replay ID.");return Path.Combine(folder,run.id+".idr");}
    private void CleanReplayFiles(){
        foreach(string path in Directory.GetFiles(folder,"*.idr")){
            string id=Path.GetFileNameWithoutExtension(path);
            if(Guid.TryParseExact(id,"D",out _)&&!pending.runs.Exists(r=>r.id==id&&r.replayVersion==2))File.Delete(path);
        }
    }
    public static byte[] ReplayEnvelope(Run run,byte[] replay){
        var raw=Idas3ReplayCodec.Decode(replay);
        if(raw.Length<96||BitConverter.ToUInt32(raw,0)!=0x32524449||BitConverter.ToUInt32(raw,4)!=run.ticks6000||BitConverter.ToUInt32(raw,12)!=60||BitConverter.ToUInt32(raw,16)!=96||BitConverter.ToUInt32(raw,20)!=160||96L+160L*BitConverter.ToUInt32(raw,8)!=raw.Length)throw new InvalidDataException("A matching detailed replay is required.");
        byte[] meta=Encoding.UTF8.GetBytes(JsonUtility.ToJson(run));if(meta.Length>8192)throw new InvalidDataException("Run metadata too large.");
        byte[] result=new byte[4+meta.Length+replay.Length];Array.Copy(BitConverter.GetBytes(meta.Length),result,4);Array.Copy(meta,0,result,4,meta.Length);Array.Copy(replay,0,result,4+meta.Length,replay.Length);return result;
    }
    private bool QueueFinish(Run run,string source){
        var existing=pending.runs.Find(r=>r.condition==run.condition&&r.weather==run.weather&&r.car==run.car);
        if(existing!=null&&existing.ticks6000<=run.ticks6000)return false;
        if(existing==null&&pending.runs.Count>=1980)throw new IOException("Upload queue is full.");
        int size=Idas3SharedReadReplay(null,0);if(size<96||size>Idas3ReplayCodec.MaxRaw)throw new InvalidDataException("Completed replay unavailable.");
        byte[] replay=new byte[size];if(Idas3SharedReadReplay(replay,size)!=size)throw new InvalidDataException("Replay changed while reading.");
        run.replayVersion=2;encodingRun=run;encodingSource=source;
        encodingJob=Task.Run(()=>{var stored=Idas3ReplayCodec.Encode(replay);ReplayEnvelope(run,stored);return stored;});
        return true;
    }
    private void CompleteEncoding(){
        var job=encodingJob;var run=encodingRun;encodingJob=null;
        byte[] replay=job.GetAwaiter().GetResult();
        if(!options.Current.communityTimes)return;
        var existing=pending.runs.Find(r=>r.condition==run.condition&&r.weather==run.weather&&r.car==run.car);
        if(existing!=null&&existing.ticks6000<=run.ticks6000)return;
        string dest=ReplayPath(run),temp=dest+".tmp";File.WriteAllBytes(temp,replay);File.Move(temp,dest);
        var next=new Pending{runs=new List<Run>(pending.runs)};if(existing!=null)next.runs.Remove(existing);next.runs.Add(run);
        Write("pending.json",next);pending=next;nextNetwork=0;CleanReplayFiles();
    }
    private void OnApplicationQuit(){
        if(encodingJob!=null)try{encodingJob.GetAwaiter().GetResult();CompleteEncoding();}catch(Exception){}
    }
    public static bool SupportedBuild(string build){
        const string pattern=@"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([a-z][a-z0-9-]*)\.(0|[1-9]\d*))?$";
        if(build==null||build.Length>64)return false;
        var a=System.Text.RegularExpressions.Regex.Match(build,pattern);
        var b=System.Text.RegularExpressions.Regex.Match(MinimumClientBuild,pattern);
        if(!a.Success)return false;
        var av=new long[6];var bv=new long[6];
        foreach(int i in new[]{1,2,3,5}){
            if(a.Groups[i].Success&&(!long.TryParse(a.Groups[i].Value,out av[i])||av[i]>9007199254740991L))return false;
            if(b.Groups[i].Success)long.TryParse(b.Groups[i].Value,out bv[i]);
        }
        for(int i=1;i<=3;i++)if(av[i]!=bv[i])return av[i]>bv[i];
        return !a.Groups[4].Success||(a.Groups[4].Value==b.Groups[4].Value&&av[5]>=bv[5]);
    }
    public static bool Uploadable(Run r)=>Valid(r)&&r.imported==0&&r.replayVersion==2&&r.ruleset==Ruleset&&r.epoch>=FirstReplaySeason&&SupportedBuild(r.build)&&Guid.TryParseExact(r.id,"D",out _);
    public static bool Valid(Run r){
        if(r==null||r.condition<0||r.condition>21||r.weather<0||r.weather>1||r.car<0||r.car>34||r.ticks6000<60000||r.ticks6000>=10800000||r.nameGlyphs==null||r.nameGlyphs.Length!=5||r.splits==null||r.splits.Length!=4||r.imported<0||r.imported>1||r.manual<(r.imported==1?-1:0)||r.manual>1||r.night<(r.imported==1?-1:0)||r.night>1||r.points<(r.imported==1?-1:0)||r.points>999999)return false;
        foreach(int n in r.nameGlyphs)if(n<0||n>221)return false;
        int prior=0,count=0;bool ended=false;foreach(int n in r.splits){if(n==0){ended=true;continue;}if(ended||n<=prior||n>r.ticks6000)return false;prior=n;count++;}
        return (r.imported==1&&count==0)||(count>=2&&prior==r.ticks6000);
    }
    private static bool ValidSnapshot(Snapshot s){if(s==null||s.ruleset!=Ruleset||s.entries==null||s.entries.Length>1980)return false;foreach(var r in s.entries)if(!Valid(r))return false;return true;}
    internal static bool UsableCommunitySnapshot(Snapshot s){
        if(!ValidSnapshot(s)||s.epoch<FirstReplaySeason)return false;
        foreach(var r in s.entries)if(r.epoch!=s.epoch||r.imported!=0||!r.replayAvailable||!SupportedBuild(r.build))return false;
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
                if(enabled!=enabledLast){enabledLast=enabled;cacheDirty=true;nextNetwork=0;confirmedUntil=0;if(!enabled){pending.runs.Clear();Write("pending.json",pending);CleanReplayFiles();status="Sharing is off. Personal records are active.";}else status="Connecting to community times…";}
                if(Application.internetReachability==NetworkReachability.NotReachable)confirmedUntil=0;
                bool use=UseCommunityRecords(enabled,snapshot,Time.realtimeSinceStartupAsDouble,confirmedUntil);
                if(use!=applied)cacheDirty=true;
                // Apply downloaded data only while the native frontend is open.
                // A race keeps the records captured at its start.
                if(cacheDirty&&Idas3SceneModeFlowValue(4)==1){
                    int[] data=use?Flatten(snapshot):Array.Empty<int>();
                    if(Idas3SharedSetRecords(data,data.Length/14,use?1:0)==1){cacheDirty=false;applied=use;}
                }
                if(encodingJob!=null&&encodingJob.IsCompleted){
                    CompleteEncoding();
                    int currentLength=Idas3SharedReadFinish(finishBuffer,finishBuffer.Length);
                    if(currentLength>0&&Encoding.UTF8.GetString(finishBuffer,0,currentLength)==encodingSource)Idas3SharedAckFinish();
                }
                int length=encodingJob==null?Idas3SharedReadFinish(finishBuffer,finishBuffer.Length):0;
                if(length>0){
                    if(enabled){var run=JsonUtility.FromJson<Run>(Encoding.UTF8.GetString(finishBuffer,0,length));if(Valid(run)){
                        run.id=Guid.NewGuid().ToString();run.ruleset=Ruleset;run.build=Application.version;run.epoch=snapshot?.epoch??FirstReplaySeason;
                        if(QueueFinish(run,Encoding.UTF8.GetString(finishBuffer,0,length)))length=0;
                    }else status="This run could not be shared: checkpoint validation failed.";}
                    if(length>0)Idas3SharedAckFinish();
                }
            }catch(Exception e){status="Community cache unavailable; racing continues.";Debug.LogWarning("Community times cache: "+e.GetType().Name);}
            menu.CommunityStatus=status+(pending.runs.Count>0?" Pending runs: "+pending.runs.Count+".":"")+(applied?" Rankings combine your personal bests and current community times.":"");
            yield return new WaitForSecondsRealtime(.5f);
        }
    }
    private IEnumerator Request(string path,string payload=null,bool authorize=false,byte[] binary=null){
        responseCode=0;responseText="";
        using(var request=new UnityWebRequest(ServiceUrl+path,payload==null&&binary==null?"GET":"POST")){
            request.downloadHandler=new DownloadHandlerBuffer();request.timeout=10;
            if(payload!=null){request.uploadHandler=new UploadHandlerRaw(Encoding.UTF8.GetBytes(payload));request.SetRequestHeader("Content-Type","application/json");}
            if(binary!=null){request.uploadHandler=new UploadHandlerRaw(binary);request.SetRequestHeader("Content-Type","application/octet-stream");request.timeout=30;}
            if(authorize)request.SetRequestHeader("Authorization","Bearer "+credential);
            UnityWebRequestAsyncOperation operation=null;try{operation=request.SendWebRequest();}catch(Exception){status="Community times offline; uploads will retry.";}
            if(operation!=null){yield return operation;responseCode=request.responseCode;responseText=request.downloadHandler.text;if(responseText.Length>2*1024*1024){responseCode=0;responseText="";}}
        }
    }
    private IEnumerator RefreshSnapshot(){
        yield return Request("/api/v1/snapshot?ruleset="+Ruleset+"&imports=1");
        confirmedUntil=0;cacheDirty=true;
        if(responseCode==200){try{
            var next=JsonUtility.FromJson<Snapshot>(responseText);
            if(!UsableCommunitySnapshot(next)||(snapshot!=null&&next.epoch<snapshot.epoch))throw new InvalidDataException();
            Write("snapshot.json",next);snapshot=next;confirmedUntil=Time.realtimeSinceStartupAsDouble+75;
            status="Community times updated. Personal records stay with your save.";
        }catch(Exception){status="Community response unavailable; using personal records.";}}
        else status="Community times offline; using personal records.";
    }
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
                var run=pending.runs[0];byte[] envelope=null;bool replayReady=true;
                if(Uploadable(run)){
                    string file=ReplayPath(run);
                    var prepare=Task.Run(()=>{if(new FileInfo(file).Length>Idas3ReplayCodec.MaxStored)throw new InvalidDataException();return ReplayEnvelope(run,File.ReadAllBytes(file));});
                    while(!prepare.IsCompleted)yield return null;
                    try{envelope=prepare.GetAwaiter().GetResult();}catch(Exception){replayReady=false;status="Queued replay unavailable; keeping the time pending.";}
                }
                if(!options.Current.communityTimes)break;
                if(!replayReady||envelope==null){status="A complete replay is required to share this time.";break;}
                yield return Request("/api/v2/runs",null,true,envelope);
                if(responseCode==200||responseCode==400||responseCode==409){pending.runs.Remove(run);try{Write("pending.json",pending);}catch(Exception){status="Could not save upload queue.";}if(responseCode!=200)status="A run was rejected by the leaderboard.";sent++;}
                else{status=responseCode==403?"This installation is blocked from sharing.":"Upload pending; the service will retry.";if(responseCode==401)registered=false;break;}
                try{CleanReplayFiles();}catch(Exception){Debug.LogWarning("Could not clean acknowledged replay file.");}
                yield return new WaitForSecondsRealtime(1);
            }
            if(sent>0&&options.Current.communityTimes)yield return RefreshSnapshot();
            nextNetwork=Time.realtimeSinceStartupAsDouble+60;
        }
    }
}
