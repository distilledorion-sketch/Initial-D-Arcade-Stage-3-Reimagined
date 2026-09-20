using System;
using System.Runtime.InteropServices;
using System.Text;
using DiscordRPC;
using UnityEngine;
using Idas3.Multiplayer;

// Presence only: no Discord login, bot token, invites, or account data access.
// The RPC library owns its background IPC thread; native state is sampled once
// per second and only changed descriptions are queued, at most every 15 seconds.
public sealed class Idas3DiscordPresence : MonoBehaviour
{
    public const string ApplicationId="1551296157567819807";
    public const string GameTitle="Initial D Arcade Stage 3";
    public const string LogoUrl="https://raw.githubusercontent.com/distilledorion-sketch/Initial-D-Arcade-Stage-3-Reimagined/main/docs/discord/initial-d-stage-3.png";
    [Serializable] internal sealed class Snapshot
    {
        public int condition,night,weather,car,mode,ticks6000;
        public string opponentName;
    }
    internal struct Description
    {
        public string details,state,session;
        public bool timed;
        public double elapsedSeconds;
    }
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    static extern int Idas3SceneReadPresence([Out]byte[] output,int capacity);
    readonly byte[] snapshotBytes=new byte[2048];
    DiscordRpcClient client;
    Idas3SceneGame host;
    double nextSample,nextPublish,nextConnect;
    string lastDescription,lastSession;
    DateTime sessionStart;
    double lastRaceElapsed;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Bootstrap()
    {
        if(Application.isEditor||FindAnyObjectByType<Idas3DiscordPresence>()!=null)return;
        foreach(string arg in Environment.GetCommandLineArgs())
            if(arg.StartsWith("-idas3-",StringComparison.Ordinal)&&arg!="-idas3-replay-viewer"&&arg!="-idas3-replay-library"&&arg!="-idas3-skip-update-once")return;
        new GameObject("Discord Rich Presence").AddComponent<Idas3DiscordPresence>();
    }
    void Update()
    {
        double now=Time.realtimeSinceStartupAsDouble;
        if(now<nextSample)return;nextSample=now+1;
        try
        {
            Description description;
            var viewer=Idas3ReplayViewer.Instance;
            if(viewer!=null){if(!viewer.PresenceEnabled){Disconnect();return;}description=DescribeReplay(viewer.PresenceMetadata);}
            else
            {
                if(host==null)host=FindAnyObjectByType<Idas3SceneGame>();
                if(host==null||!host.PresenceAllowed||host.ReplayViewerOpen||!host.GameOptions.Current.discordPresence){Disconnect();return;}
                var snapshot=ReadSnapshot(snapshotBytes);
                if(snapshot==null){Disconnect();return;}
                description=Describe(snapshot,host.Status,host.MultiplayerSession?.InLobby==true,host.MultiplayerSession?.IsQuickMatching==true);
            }
            if(client==null)
            {
                if(now<nextConnect)return;nextConnect=now+30;
                client=new DiscordRpcClient(ApplicationId,autoEvents:false);
                // Disable URI registration / invitation handling entirely.
                client.Initialize();lastDescription=null;nextPublish=0;
            }
            client.Invoke();
            if(description.session!=lastSession||(description.timed&&description.elapsedSeconds+1<lastRaceElapsed)){
                lastSession=description.session;sessionStart=DateTime.UtcNow.AddSeconds(-description.elapsedSeconds);lastDescription=null;
            }
            lastRaceElapsed=description.elapsedSeconds;
            string key=description.details+"\n"+description.state+"\n"+description.session;
            if(key==lastDescription||now<nextPublish)return;
            client.SetPresence(Build(description,sessionStart));lastDescription=key;nextPublish=now+15;
        }
        catch(Exception)
        {
            // Optional desktop integration must never interrupt racing.
            Disconnect();nextConnect=now+30;
        }
    }
    internal static RichPresence Build(Description value,DateTime started)=>new RichPresence{
        Details=Limit(value.details),State=Limit(value.state),
        Assets=new Assets{LargeImageKey=LogoUrl,LargeImageText=GameTitle},
        Buttons=new[]{new DiscordRPC.Button{Label="View Leaderboard",Url=Idas3CommunityTimes.ServiceUrl}},
        Timestamps=value.timed?new Timestamps(started):null
    };
    internal static Snapshot ReadSnapshot(byte[] bytes){int count=Idas3SceneReadPresence(bytes,bytes.Length);return count>0&&count<bytes.Length?JsonUtility.FromJson<Snapshot>(Encoding.UTF8.GetString(bytes,0,count)):null;}
    static string Limit(string value){value=value??"";while(Encoding.UTF8.GetByteCount(value)>128){value=value.Substring(0,value.Length-1);if(value.Length>0&&char.IsHighSurrogate(value[value.Length-1]))value=value.Substring(0,value.Length-1);}return value;}
    internal static Description Describe(Snapshot s,Idas3Native.Status status,bool lobby,bool quickMatch)
    {
        bool menu=(status.flags&1)!=0,results=(status.flags&(4096u|262144u))!=0;
        string course=Course(s.condition),car=Car(s.car),conditions=Conditions(s.night,s.weather);
        if(quickMatch)return new Description{details="Online Battle · Finding a driver",state=car,session="matchmaking"};
        if(lobby&&(menu||(status.flags&256)!=0))return new Description{details="Online Battle · In the lobby",state=car,session="lobby"};
        if(menu)
        {
            string[] stages={"At the title screen","Selecting a save","Selecting a make","Selecting a car","Selecting transmission","Choosing a mode","Selecting a course","Selecting a route","Selecting weather","Selecting time of day","Selecting a rival","Entering a driver name","Choosing upgrades"};
            return new Description{details=stages[Math.Max(0,Math.Min(stages.Length-1,status.frontendStage))],state=status.frontendStage==0?GameTitle:Mode(s.mode)+" · "+car,session="menu"};
        }
        string versus=string.IsNullOrWhiteSpace(s.opponentName)?"":" · vs "+Clean(s.opponentName);
        if(results||status.racePhase>=3)return new Description{details=Mode(s.mode)+" · Race results",state=course+" · "+conditions,session="results"};
        string detail=s.mode==0?"Time Attack · "+course:Mode(s.mode)+versus;
        string state=s.mode==0?conditions+" · "+car:course+" · "+conditions;
        return new Description{details=detail,state=state,session="race:"+s.mode+":"+s.condition+":"+s.night+":"+s.weather+":"+s.opponentName,timed=true,elapsedSeconds=Math.Max(0,s.ticks6000/6000.0)};
    }
    internal static Description DescribeReplay(Idas3ReplayData.Details m)
    {
        if(m==null)return new Description{details="Browsing replays",state="Local replay library",session="replay-library"};
        string players=Clean(m.playerName);
        if(m.mode!=0&&!string.IsNullOrWhiteSpace(m.opponentName))players+=(players.Length>0?" vs ":"")+Clean(m.opponentName);
        return new Description{details="Watching a replay"+(players.Length>0?" · "+players:""),state=Course(m.condition)+" · "+Conditions(m.night,m.weather)+" · "+Mode(m.mode),session="replay"};
    }
    static string Clean(string text)
    {
        if(string.IsNullOrWhiteSpace(text))return "";
        var b=new StringBuilder();foreach(char c in text)if(!char.IsControl(c)&&b.Length<24)b.Append(c);
        return b.ToString().Trim();
    }
    static string Car(int car)=>car>=0&&car<Idas3MultiplayerSession.CarNames.Length?Idas3MultiplayerSession.CarNames[car]:"Choosing a car";
    static string Mode(int mode)=>mode==1?"Online Battle":mode==2?"Legend of the Streets":mode==3?"Bunta Challenge":"Time Attack";
    static string Conditions(int night,int wet)=>(night!=0?"Night":"Day")+" / "+(wet!=0?"Wet":"Dry");
    static string Course(int condition)
    {
        int course=condition/2;if(condition<0||course>=Idas3ReplayData.Courses.Length)return "Choosing a course";
        string[] forward={"Counterclockwise","Counterclockwise","Downhill","Downhill","Outbound","Downhill","Outbound","Outbound","Downhill","Downhill","Downhill"};
        string[] reverse={"Clockwise","Clockwise","Uphill","Uphill","Inbound","Reverse","Inbound","Inbound","Uphill","Uphill","Uphill"};
        return Idas3ReplayData.Courses[course]+" "+((condition&1)==0?forward[course]:reverse[course]);
    }
    void Disconnect()
    {
        var previous=client;client=null;lastDescription=null;lastSession=null;
        if(previous!=null)try{previous.Dispose();}catch{}
    }
    internal static void YieldToReplay(){var presence=FindAnyObjectByType<Idas3DiscordPresence>();if(presence!=null)presence.Disconnect();}
    void OnApplicationQuit()=>Disconnect();
    void OnDestroy()=>Disconnect();
}
