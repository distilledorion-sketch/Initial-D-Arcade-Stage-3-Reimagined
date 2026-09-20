using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;
using Idas3.Multiplayer;

// Optional observer of the real two-process LAN smoke. Fixture history and
// result packets exist only behind the explicit showcase flag/private marker.
public sealed class Idas3MultiplayerPresentationSmoke : MonoBehaviour
{
    private static string pendingRoot,pendingSaves,pendingPeer,pendingRole;
    private static bool expectReturn,expectDisconnect;
    private static Idas3MultiplayerPresentationSmoke active;
    private Idas3SceneGame host;
    private Idas3MultiplayerSession session;
    private int checks;
    private int auraCountdownSamples,auraDrivingSamples,auraCountdownMask,auraCameraMask;
    private bool lobbyVerified,winnerVerified,duplicateVerified,returnedVerified,drawVerified,disconnectVerified,capturing;
    private string error;
    private Idas3BattleRecord expectedLocal;
    private readonly HashSet<ulong> capturesStarted=new HashSet<ulong>();
    private readonly List<CaptureReport> captures=new List<CaptureReport>();
    [Serializable,StructLayout(LayoutKind.Sequential,Pack=8)] private struct AuraStatus
    {public uint size,version,level,streak,eligible,submittedRanges,colorArgb,sourceFrame;}
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Idas3MultiplayerGetAuraStatus(int side,ref AuraStatus status);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Idas3SceneGetPreRaceStatus(ref Idas3PreRaceSmoke.PreRaceStatus status);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Idas3MultiplayerDiagnosticFinish(ulong ticks60);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Idas3SceneCopyPreRaceName(int side,StringBuilder destination,int capacity);
    [Serializable] private sealed class CaptureReport
    {
        public string file,role,localText,remoteText;
        public string row0Text,row1Text,row0Name,row1Name,rowOrder="row0=host/upper-left; row1=guest/lower-right";
        public int localUiRow,remoteUiRow,row0Car,row1Car;
        public ulong raceId;
        public uint phase,sourceFrame;
        public int width,height,visiblePixels;
        public Idas3BattleRecord local,remote;
        public AuraStatus localAura,remoteAura;
    }
    [Serializable] private sealed class Report
    {
        public string schema="idas3-network-vs-records-v1",applicationVersion=Application.version,role,error;
        public string scope="Two actual LAN clients use isolated seeded history. Native battle-record/aura getters retain local/opponent semantics. Shared showcase UI rows instead follow fixed grid slots on both clients: host upper-left, guest lower-right. Captures identify each row's name, record, car and local/remote mapping. A separate explicitly marked result fixture uses ordinary Results packets to verify winner persistence and duplicate handling; it does not claim those fixture times were driven.";
        public bool passed,lobbyVerified,winnerVerified,duplicateVerified,returnedVerified,drawVerified,disconnectVerified;
        public int checks;
        public int auraCountdownSamples,auraDrivingSamples,auraCountdownMask,auraCameraMask;
        public Idas3BattleRecord persistedLocal;
        public CaptureReport[] captures;
    }
    private static string Arg(string[] args,string key)
    {int i=Array.IndexOf(args,key);return i>=0&&i+1<args.Length?args[i+1]:null;}
    private static bool SamePath(string a,string b)=>string.Equals(Path.GetFullPath(a).TrimEnd(Path.DirectorySeparatorChar),Path.GetFullPath(b).TrimEnd(Path.DirectorySeparatorChar),StringComparison.OrdinalIgnoreCase);
    private static Idas3BattleRecord Seed(string role)=>role=="host"?
        new Idas3BattleRecord{battles=400,wins=217,level=11,streak=2}:
        new Idas3BattleRecord{battles=99,wins=85,level=25,streak=3};
    private static int OwnCar=>pendingRole=="host"?0:8;
    private static int OtherCar=>pendingRole=="host"?8:0;
    public static bool Configure(ref string saves)
    {
        var args=Environment.GetCommandLineArgs();if(Array.IndexOf(args,"-idas3-multiplayer-showcase-check")<0)return false;
        string directory=Arg(args,"-idas3-multiplayer-smoke"),role=Arg(args,"-idas3-multiplayer-role"),peer=Arg(args,"-idas3-multiplayer-peer-output");
        if(string.IsNullOrWhiteSpace(directory)||string.IsNullOrWhiteSpace(peer)||(role!="host"&&role!="join"))
            throw new ArgumentException("Presentation check requires explicit isolated host/join output paths.");
        pendingRoot=Path.GetFullPath(directory);pendingSaves=Path.Combine(pendingRoot,"userdata");
        pendingPeer=Path.GetFullPath(peer);pendingRole=role;
        if(!SamePath(saves,pendingSaves)||SamePath(pendingRoot,pendingPeer)||!File.Exists(Path.Combine(pendingRoot,"ISOLATED_MULTIPLAYER_TEST.txt")))
            throw new IOException("Presentation fixtures require the newly configured private multiplayer save directory.");
        if(Array.IndexOf(args,"-idas3-multiplayer-steam-check")>=0||Array.IndexOf(args,"-idas3-multiplayer-quick-check")>=0)
            throw new ArgumentException("Presentation fixtures require the isolated two-peer LAN diagnostic.");
        expectReturn=Array.IndexOf(args,"-idas3-multiplayer-return-check")>=0||Array.IndexOf(args,"-idas3-multiplayer-finish-music-check")>=0;
        expectDisconnect=Array.IndexOf(args,"-idas3-multiplayer-disconnect-check")>=0;
        Idas3MultiplayerRecords.SeedDiagnostic(pendingSaves,OwnCar,Seed(role));
        File.WriteAllText(Path.Combine(pendingRoot,"ISOLATED_MULTIPLAYER_PRESENTATION_TEST.txt"),"Explicit private multiplayer showcase fixture. Seeded history and fixture finish times are synthetic and never installed in ordinary saves.\n");
        return true;
    }
    private static Idas3MultiplayerPresentationSmoke Ensure(Idas3SceneGame game,Idas3MultiplayerSession owner)
    {
        if(pendingRoot==null)return null;
        if(active==null){active=game.gameObject.AddComponent<Idas3MultiplayerPresentationSmoke>();active.host=game;active.session=owner;active.expectedLocal=Seed(pendingRole);}
        return active;
    }
    public static void AfterFrame(Idas3SceneGame game,Idas3MultiplayerSession owner)
    {
        var self=Ensure(game,owner);if(self==null||self.error!=null)return;
        try{self.Observe();}catch(Exception e){self.Fail(e);throw;}
    }
    private void Check(bool value,string why){++checks;if(!value)throw new InvalidOperationException(why+" ["+pendingRole+", "+session.StateName+"]");}
    private static string Format(Idas3BattleRecord value)
    {ulong tenths=value.battles==0?0:(ulong)Math.Min(value.wins,value.battles)*1000/value.battles;return value.battles+" BATTLE(S)  "+Math.Min(value.wins,value.battles)+" WIN(S)  "+tenths/10+"."+tenths%10+"%";}
    private Idas3BattleRecord ReadSaved()=>new Idas3MultiplayerRecords(pendingSaves).Read(OwnCar);
    private Idas3PreRaceSmoke.PreRaceStatus ReadPresentation()
    {var s=new Idas3PreRaceSmoke.PreRaceStatus{size=(uint)Marshal.SizeOf<Idas3PreRaceSmoke.PreRaceStatus>()};Check(Idas3SceneGetPreRaceStatus(ref s)==1&&s.version==1,"Pre-race status ABI failed");return s;}
    private AuraStatus ReadAura(int side)
    {var s=new AuraStatus{size=(uint)Marshal.SizeOf<AuraStatus>()};Check(s.size==32&&Idas3MultiplayerGetAuraStatus(side,ref s)==1&&s.version==1,"Aura status ABI failed");return s;}
    private string ReadRow(int side)
    {var text=new StringBuilder(256);int count=Idas3MultiplayerNative.Idas3SceneCopyPreRaceBattleRecord(side,text,text.Capacity);Check(count>=0&&count<text.Capacity,"Native VS record copy failed");return text.ToString();}
    private string ReadName(int row)
    {var text=new StringBuilder(256);int count=Idas3SceneCopyPreRaceName(row,text,text.Capacity);Check(count>=0&&count<text.Capacity,"Native VS name copy failed");return text.ToString().Normalize(NormalizationForm.FormKC);}
    private void CheckFrozenRecords()
    {
        int localResult=Idas3MultiplayerNative.Idas3MultiplayerGetBattleRecord(0,out var local);
        int remoteResult=Idas3MultiplayerNative.Idas3MultiplayerGetBattleRecord(1,out var remote);
        Check(localResult==1&&remoteResult==1,"Native race records are missing");
        Check(local.Equals(session.RaceLocalRecord)&&remote.Equals(session.RaceRemoteRecord),"Native records differ from frozen local/opponent wire records");
        int localRow=session.IsHost?0:1;
        Check(session.IsHost==(pendingRole=="host"),"Diagnostic role does not match admitted local grid slot");
        Check(ReadRow(localRow)==Format(local)&&ReadRow(1-localRow)==Format(remote),"VS rows do not associate frozen records with their actual car grid slots");
        Check(ReadName(0)=="SMOKE HOST"&&ReadName(1)=="SMOKE JOIN","Shared showcase names must label host/left and guest/right on both clients");
    }
    private void Observe()
    {
        if(session.DisconnectedFinish){Check(ReadSaved().Equals(expectedLocal),"Disconnect awarded online history");disconnectVerified=true;return;}
        if(!session.IsRacing)return;
        var source=ReadPresentation();
        if(source.phase==4||source.phase==5){
            var local=ReadAura(0);var remote=ReadAura(1);
            Check(local.eligible!=0&&remote.eligible!=0,"Aura timing fixture lost qualified drivers");
            Check(local.submittedRanges==0&&remote.submittedRanges==0,"Aura remained visible after the showcase");
            if(source.phase==4){++auraCountdownSamples;if(source.countdownDigit>=1&&source.countdownDigit<=3)auraCountdownMask|=1<<source.countdownDigit;}
            else {++auraDrivingSamples;auraCameraMask|=1<<(int)source.cameraView;}
        }
        if(source.phase>=1&&source.phase<=3){
            CheckFrozenRecords();
            if(source.reserved>=152&&!capturing&&capturesStarted.Add(session.CurrentRaceId))StartCoroutine(Guard(Capture()));
        }
        if(session.CanReturnToLobby&&(session.ResultText.StartsWith("DRAW",StringComparison.Ordinal)||session.ResultText.StartsWith("BOTH DRIVERS",StringComparison.Ordinal))){
            Check(ReadSaved().Equals(expectedLocal),"Draw/time-up awarded a battle, win or loss");drawVerified=true;
        }
    }
    private IEnumerator Frames(int count){for(int i=0;i<count;++i)yield return null;}
    private IEnumerator Until(Func<bool> condition,double seconds,string reason)
    {double end=Time.realtimeSinceStartupAsDouble+seconds;while(!condition()&&Time.realtimeSinceStartupAsDouble<end)yield return null;Check(condition(),reason);}
    private IEnumerator Barrier(string file)
    {File.WriteAllText(Path.Combine(pendingRoot,file),"{\"ready\":true}");yield return Until(()=>File.Exists(Path.Combine(pendingPeer,file)),20,"Peer missed presentation fixture barrier "+file);}
    private IEnumerator Guard(IEnumerator routine)
    {
        var stack=new Stack<IEnumerator>();stack.Push(routine);
        while(stack.Count>0&&error==null){object value=null;Exception failure=null;try{if(!stack.Peek().MoveNext()){stack.Pop();continue;}value=stack.Peek().Current;}catch(Exception e){failure=e;}
            if(failure!=null){Fail(failure);yield break;}if(value is IEnumerator child)stack.Push(child);else yield return value;}
    }
    private void Fail(Exception failure){error=failure.ToString();WriteReport(false);Debug.LogError(error);}
    private IEnumerator Capture()
    {
        capturing=true;int previousAA=QualitySettings.antiAliasing;Texture2D picture=null;
        try{
            QualitySettings.antiAliasing=0;yield return Frames(3);yield return new WaitForEndOfFrame();
            var source=ReadPresentation();Check(source.phase>=2&&source.phase<=3&&source.reserved>=152,"Capture missed settled original name animation");
            CheckFrozenRecords();var local=ReadAura(0);var remote=ReadAura(1);
            Check(local.level==session.RaceLocalRecord.level&&remote.level==session.RaceRemoteRecord.level&&local.streak==session.RaceLocalRecord.streak&&remote.streak==session.RaceRemoteRecord.streak,"Aura used another driver's level/streak");
            Check(local.eligible!=0&&remote.eligible!=0&&local.submittedRanges>0&&remote.submittedRanges>0,"Eligible cars did not submit actual aura geometry");
            var menu=FindFirstObjectByType<Idas3MultiplayerMenu>();Check(menu!=null&&!menu.IsOpen&&!host.PauseMenu.IsOpen,"VS capture is covered by a menu");
            picture=ScreenCapture.CaptureScreenshotAsTexture();Check(picture!=null&&picture.width>0,"Showcase window capture failed");
            int visible=0;foreach(var p in picture.GetPixels32())if(Mathf.Max(p.r,Mathf.Max(p.g,p.b))>24)++visible;
            Check(visible>picture.width*picture.height/100,"Actual showcase capture was blank");
            string file="network-vs-records-"+(captures.Count+1)+".png";File.WriteAllBytes(Path.Combine(pendingRoot,file),picture.EncodeToPNG());
            captures.Add(new CaptureReport{file=file,role=pendingRole,raceId=session.CurrentRaceId,phase=source.phase,sourceFrame=source.reserved,
                width=picture.width,height=picture.height,visiblePixels=visible,local=session.RaceLocalRecord,remote=session.RaceRemoteRecord,
                localText=ReadRow(session.IsHost?0:1),remoteText=ReadRow(session.IsHost?1:0),localAura=local,remoteAura=remote,
                localUiRow=session.IsHost?0:1,remoteUiRow=session.IsHost?1:0,row0Text=ReadRow(0),row1Text=ReadRow(1),row0Name=ReadName(0),row1Name=ReadName(1),
                row0Car=session.IsHost?session.LocalCar:session.RemoteCar,row1Car=session.IsHost?session.RemoteCar:session.LocalCar});WriteReport(false);
        }finally{if(picture!=null)Destroy(picture);QualitySettings.antiAliasing=previousAA;capturing=false;}
    }
    public static IEnumerator VerifyLobby(Idas3SceneGame game,Idas3MultiplayerSession owner,string directory,string role)
    {
        var self=Ensure(game,owner);if(self==null)yield break;
        self.Check(SamePath(directory,pendingRoot)&&role==pendingRole,"Presentation observer received a different private role/path");
        yield return self.Until(()=>owner.LocalRecord.Equals(Seed(role))&&owner.RemoteRecord.Equals(Seed(role=="host"?"join":"host")),20,"Initial handshake did not preserve asymmetric seeded records");
        yield return self.Until(()=>owner.CanReady,15,"Private car selection was not acknowledged");owner.SetReady(true);
        yield return self.Until(()=>BothReady(owner),15,"Both seeded players did not become ready");yield return self.Barrier("records-ready.json");
        owner.SetCar(OwnCar+1);
        yield return self.Until(()=>owner.LocalCar==OwnCar+1&&owner.RemoteCar==OtherCar+1&&owner.LocalRecord.Equals(Idas3BattleRecord.Fresh)&&owner.RemoteRecord.Equals(Idas3BattleRecord.Fresh)&&!AnyReady(owner),20,"Car edit retained stale record or readiness");
        yield return self.Barrier("records-fresh-car.json");owner.SetCar(OwnCar);
        yield return self.Until(()=>owner.LocalCar==OwnCar&&owner.RemoteCar==OtherCar&&owner.LocalRecord.Equals(Seed(role))&&owner.RemoteRecord.Equals(Seed(role=="host"?"join":"host"))&&owner.CanReady&&!AnyReady(owner),20,"Restoring cars did not re-advertise their actual records");
        yield return self.Barrier("records-restored.json");self.lobbyVerified=true;self.WriteReport(false);
    }
    private static bool BothReady(Idas3MultiplayerSession owner){int count=0;foreach(var p in owner.Players)if(p.Connected&&p.Ready)++count;return count==2;}
    private static bool AnyReady(Idas3MultiplayerSession owner){foreach(var p in owner.Players)if(p.Connected&&p.Ready)return true;return false;}
    private string RecordFingerprint()
    {
        string path=Path.Combine(pendingSaves,"online_records_v1");var files=Directory.GetFiles(path,"*",SearchOption.AllDirectories);Array.Sort(files,StringComparer.Ordinal);var text=new StringBuilder();
        using(var sha=SHA256.Create())foreach(string file in files){text.Append(file.Substring(path.Length));using(var stream=File.OpenRead(file))text.Append(Convert.ToBase64String(sha.ComputeHash(stream)));}return text.ToString();
    }
    private void SendWinnerFixture(bool first)
    {
        Check(session.IsHost&&session.IsRacing&&session.HandshakeComplete&&File.Exists(Path.Combine(pendingRoot,"ISOLATED_MULTIPLAYER_PRESENTATION_TEST.txt")),"Winner fixture requires this live private host session");
        var type=typeof(Idas3MultiplayerSession);const BindingFlags flags=BindingFlags.Instance|BindingFlags.NonPublic;
        var send=type.GetMethod("Send",flags);var show=type.GetMethod("ShowResult",flags);var packet=type.GetNestedType("Packet",BindingFlags.NonPublic);var sent=type.GetField("resultSent",flags);
        Check(send!=null&&show!=null&&packet!=null&&sent!=null,"Private result fixture no longer matches the ordinary protocol handler");
        sent.SetValue(session,true);ulong race=session.CurrentRaceId;
        Action<BinaryWriter> payload=w=>{w.Write(race);w.Write(0);w.Write(1000UL);w.Write(1100UL);};
        send.Invoke(session,new object[]{Enum.Parse(packet,"Results"),payload,true});show.Invoke(session,new object[]{0,1000UL,1100UL});
        if(first)File.WriteAllText(Path.Combine(pendingRoot,"WINNER_PACKET_FIXTURE.txt"),"Explicit synthetic result for private second-race persistence checks only. Host winner; fixture times1000/1100. Ordinary Results send/receive/ShowResult handlers are used.\n");
    }
    public static IEnumerator VerifyWinnerAndReturn(Idas3SceneGame game,Idas3MultiplayerSession owner,string directory,string role)
    {
        var self=Ensure(game,owner);if(self==null||!expectReturn)yield break;
        self.Check(self.error==null,self.error??"Presentation observer failed");self.Check(self.captures.Count>=2&&self.drawVerified,"Second race did not preserve/capture records after natural no-result finish");
        var before=owner.RaceLocalRecord;var opponent=owner.RaceRemoteRecord;ulong raceId=owner.CurrentRaceId;
        self.Check(before.Equals(Seed(role))&&opponent.Equals(Seed(role=="host"?"join":"host")),"Second-race frozen records differ from the first race's unchanged records");
        // Both private seeds have source experience0; the preceding natural
        // no-result race must leave that retained progression state unchanged.
        var expected=Idas3MultiplayerRecords.Next(before,role=="host",opponent,0,out _);self.expectedLocal=expected;
        yield return self.Barrier("winner-fixture-armed.json");
        if(role=="host"){
            var sent=typeof(Idas3MultiplayerSession).GetField("resultSent",BindingFlags.Instance|BindingFlags.NonPublic);
            self.Check(sent!=null,"Private host result guard is unavailable");sent.SetValue(owner,true);
        }
        ulong finishTicks=role=="host"?1000UL:1100UL;
        self.Check(Idas3MultiplayerDiagnosticFinish(finishTicks)==1,"Guarded private native finish was rejected: "+Idas3Native.Error());
        yield return self.Until(()=>owner.LocalSnapshot.Finished&&!owner.LocalSnapshot.TimeUp&&owner.LocalSnapshot.raceTicks==finishTicks,10,"Private native terminal snapshot did not preserve its exact finish time");
        self.Check(self.ReadSaved().Equals(before),"Pending native finish committed history before a resolved network result");
        yield return self.Barrier("winner-native-finished.json");if(role=="host")self.SendWinnerFixture(true);
        yield return self.Until(()=>owner.CanReturnToLobby&&owner.ResultText.StartsWith(role=="host"?"YOU WIN":"OPPONENT WINS",StringComparison.Ordinal),20,"Actual Results packet did not resolve the intended private winner on both peers");
        self.Check(self.ReadSaved().Equals(expected),"Winner/loser record did not persist through the ordinary result handler");
        self.Check(expected.battles==before.battles+1&&expected.wins==before.wins+(role=="host"?1u:0u),"Resolved result awarded the wrong battle/win counters");self.winnerVerified=true;
        string fingerprint=self.RecordFingerprint();yield return self.Barrier("winner-first-verified.json");
        if(role=="host")for(int i=0;i<3;++i){self.SendWinnerFixture(false);yield return self.Frames(2);}
        yield return self.Frames(30);
        self.Check(self.ReadSaved().Equals(expected)&&self.RecordFingerprint()==fingerprint,"Duplicate Results packets rewrote or incremented history");
        self.Check(new Idas3MultiplayerRecords(pendingSaves).Commit(OwnCar,raceId,role=="host",opponent).Equals(expected)&&self.RecordFingerprint()==fingerprint,"Reloaded race receipt did not reject duplicate persistence");
        self.duplicateVerified=true;yield return self.Barrier("winner-duplicates-verified.json");
        // The first natural race returns through the guest. Exercise the host
        // return path after the persisted winner, including ReturnAck records.
        if(role=="host")owner.ReturnToLobby();
        yield return self.Until(()=>owner.StateName=="Lobby"&&owner.HandshakeComplete&&!owner.IsRacing&&owner.LocalRecord.Equals(expected),30,"Winner return did not preserve the connected lobby and updated local record");
        var remoteExpected=Idas3MultiplayerRecords.Next(opponent,role!="host",before,0,out _);
        yield return self.Until(()=>owner.RemoteRecord.Equals(remoteExpected)&&owner.CanReady,20,"Returned lobby did not receive the opponent's updated record");
        self.Check(self.RecordFingerprint()==fingerprint,"Returning after a winner changed the persisted receipt");self.returnedVerified=true;yield return self.Barrier("winner-return-verified.json");self.WriteReport(false);
    }
    private void WriteReport(bool passed)
    {
        File.WriteAllText(Path.Combine(pendingRoot,"multiplayer-presentation-report.json"),JsonUtility.ToJson(new Report{passed=passed,role=pendingRole,error=error,
            auraCountdownSamples=auraCountdownSamples,auraDrivingSamples=auraDrivingSamples,auraCountdownMask=auraCountdownMask,auraCameraMask=auraCameraMask,
            checks=checks,lobbyVerified=lobbyVerified,winnerVerified=winnerVerified,duplicateVerified=duplicateVerified,returnedVerified=returnedVerified,
            drawVerified=drawVerified,disconnectVerified=disconnectVerified,persistedLocal=ReadSaved(),captures=captures.ToArray()},true));
    }
    public static void RequirePassed()
    {
        if(pendingRoot==null)return;
        if(active==null)throw new InvalidOperationException("Presentation diagnostic was configured but never observed a frame");
        try{
            active.Check(active.auraCountdownSamples>0&&active.auraDrivingSamples>0&&active.auraCountdownMask==14&&active.auraCameraMask==3,"Both auras must be absent during 3/2/1 and both driving cameras");
            active.Check(active.error==null,active.error??"Presentation observer failure");
            active.Check(active.lobbyVerified&&active.captures.Count>0&&!active.capturing,"Network records or settled VS/aura capture were not verified");
            active.Check(active.ReadSaved().Equals(active.expectedLocal),"Terminal leave/drop modified online records");
            if(expectReturn)active.Check(active.drawVerified&&active.winnerVerified&&active.duplicateVerified&&active.returnedVerified&&active.captures.Count>=2,"Return/winner persistence fixture did not finish");
            if(expectDisconnect)active.disconnectVerified=true; // Both roles' final saved history was checked above; guest also observes DisconnectedFinish.
            active.WriteReport(true);
        }catch(Exception e){active.Fail(e);throw;}
    }
}
