using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;

namespace Idas3.Multiplayer
{
    // Opt-in two-process integration diagnostic. It uses the normal LAN
    // transport/session and separate native profiles, never ordinary saves.
    public sealed partial class Idas3MultiplayerSmoke : MonoBehaviour
    {
        private static Idas3MultiplayerSmoke active;
        private static string pendingRoot,pendingRole,pendingAddress,pendingPeerRoot;
        private static int pendingPort,pendingCourse;
        private static bool pendingSteamCheck,pendingQuickCheck,pendingPauseCheck,pendingShowcaseCheck,pendingCourseDrawCheck,pendingMusicCheck,pendingDisconnectCheck,pendingReturnCheck,pendingFinishMusicCheck;
        private Idas3SceneGame host;
        private Idas3MultiplayerSession session;
        private Idas3MultiplayerMenu menu;
        private Idas3PreRaceSmoke showcaseObserver;
        private string root,role,address,peerRoot,phase="initializing";
        private int port,course,checks,pulse,maxPeerRanges,maxPeerMainRanges;
        private bool finished,frozen,driving,raceObserved,steamCheck,quickCheck,pauseCheck,showcaseCheck,courseDrawCheck,musicCheck,disconnectCheck,returnCheck,finishMusicCheck;
        private bool returnCheckInProgress,acceleratedReturnWait;
        private bool lobbyAutoClosed,liveMenuReopened,secondLobbyAutoClosed;
        private bool? priorDiagnosticFocus;
        private double began,releaseObservedAt=-1,firstPoseObservedAt=-1,lastPoseObservedAt=-1;
        private string releaseObservedUtc,firstPoseObservedUtc;
        private long releaseObservedUnixMilliseconds;
        private ulong releaseLocalTick,releaseRemoteTick;
        private double minObservedRemoteOffset=double.PositiveInfinity,maxObservedRemoteOffset=double.NegativeInfinity;
        private ulong firstLocalTick,firstRemoteTick;
        private Vector3 firstLocalPosition,firstRemotePosition;
        private float localTravel,remoteTravel,peakLocalSpeed,peakRemoteSpeed;
        private long greatestReceived,greatestSent;
        private Idas3CarSnapshot lastLocal,lastRemote;
        private readonly List<Shot> shots=new List<Shot>();
        private readonly List<Progress> progress=new List<Progress>();
        private ulong lastProgressTick;
        private Idas3AuthorityStatus lastAuthority;
        private int authorityStallFrames,maxObservedPing;


        [Serializable] private class ChoiceRecord {
            public int course;public bool reverse,wet,night;
            public static ChoiceRecord From(Idas3RaceChoice choice)=>new ChoiceRecord{course=choice.Course,reverse=choice.Reverse,wet=choice.Wet,night=choice.Night};
        }
        [Serializable] private class CourseDrawReport {
            public string schema="idas3-independent-course-player-v1",role,selectedText;
            public bool passed,readinessInvalidated,locked;
            public int winnerSlot,nativeCourse;
            public uint nativeCondition;
            public ChoiceRecord hostChoice,guestChoice,selected;
        }
        private bool courseReadinessInvalidated;
        [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
        private static extern int Idas3SceneGetPreRaceStatus(ref Idas3PreRaceSmoke.PreRaceStatus status);
        [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
        private static extern int Idas3SceneGetRaceAudioStatus(ref Idas3PreRaceSmoke.RaceAudioStatus status);
        private static Idas3RaceChoice HostCoursePick=>new Idas3RaceChoice(0,false,false,false);
        private static Idas3RaceChoice GuestCoursePick=>new Idas3RaceChoice(3,true,true,true);

        [Serializable] private class RoomNotice {public string role,code,phase,utc;public int port,course;}
        [Serializable] private class Progress {
            public ulong localTicks,remoteTicks;
            public long received,sent;
            public Vector3 localPosition,remotePosition;
            public float localSpeed,remoteSpeed;
            public int peerRanges,peerMainRanges;
            public double wallSinceReleaseSeconds,localSourceMinusWallSeconds,remoteSnapshotMinusLocalSeconds;
        }
        [Serializable] private class Shot {
            public string name,kind;
            public int width,height,visiblePixels,manualCameras,peerRanges,peerMainRanges,menuRepaints;
            public bool menuOpen,screenCaptureVisible,menuHeaderVisible;
            public long snapshotsReceived,snapshotsSent;
            public Idas3CarSnapshot local,remote;
        }
        [Serializable] private class Report {
            public string schema="idas3-two-client-smoke-v1",applicationVersion=Application.version,role,phase,error,unityVersion,device,roomCode;
            public string scope="Two real Unity processes using normal LAN loopback/session APIs and isolated original-handling profiles; host car0/join car8, ready/start/countdown, real unscaled frame delta, at least600 source race ticks and10 observed wall seconds, peer geometry and coordinated leave. World captures use diagnostic AA1; the actual menu uses a temporary AA1 screen or its real OnGUI Repaint in an AA1 target. Remote snapshot clock offset includes transport delay. This does not establish internet/Steam relay quality or arbitrary-network reliability.";
            public bool passed,shutdownComplete,peerLeft,menuInputBlocked,steamRuntimeCheck,quickMatchRuntimeCheck;
            public bool lobbyAutoClosed,liveMenuReopened,secondLobbyAutoClosed;
            public int checks,course,maxPeerRanges,maxPeerMainRanges;
            public double elapsedSeconds;
            public bool authorityEnabled,menuPixelsChecked,mirrorInputFixture,boostEnabled,collisionsEnabled;
            public Idas3AuthorityStatus authority;
            public int authorityStallFrames,maxObservedPing;
            public double configuredRttMs,configuredJitterMs,configuredLossPercent;
            public long droppedPackets;

            public string frameDeltaMode="real-unscaled-clamped-0.25",releaseObservedUtc,firstPoseObservedUtc;
            public long releaseObservedUnixMilliseconds;
            public ulong releaseLocalTick,releaseRemoteTick,firstLocalTick,firstRemoteTick;
            public double releaseObservedRealtime,firstPoseObservedRealtime,observedRaceWallSeconds,localSourceSeconds,remoteSourceSeconds;
            public double localSourceMinusWallSeconds,minRemoteSnapshotMinusLocalSeconds,maxRemoteSnapshotMinusLocalSeconds;
            public ulong localRaceTicksObserved,remoteRaceTicksObserved;
            public long snapshotsReceived,snapshotsSent;
            public float localTravel,remoteTravel,peakLocalSpeed,peakRemoteSpeed;
            public Idas3CarSnapshot finalLocal,finalRemote;
            public Shot[] captures;
            public Progress[] trajectory;
        }
        private static bool WorldOnly=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-world-only")>=0;
        private static bool ObserveStartOnly=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-observe-start")>=0;
        private static bool ChallengerCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-challenger-check")>=0;
        private string roomCode;
        private bool peerLeft,menuInputBlocked;

        public static bool Configure(ref string saves)
        {
            var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-multiplayer-smoke");
            if(at<0)return false;
            if(Array.IndexOf(args,"-idas3-scene-smoke")>=0)throw new ArgumentException("Use one scene diagnostic per process.");
            if(at+1>=args.Length)throw new ArgumentException("Multiplayer smoke needs a NEW output directory.");
            pendingRoot=Path.GetFullPath(args[at+1]);
            if(Directory.Exists(pendingRoot)||File.Exists(pendingRoot))throw new IOException("Use a NEW multiplayer output directory: "+pendingRoot);
            pendingRole=Argument(args,"-idas3-multiplayer-role","").ToLowerInvariant();
            if(pendingRole!="host"&&pendingRole!="join")throw new ArgumentException("Multiplayer role must be host or join.");
            pendingQuickCheck=Array.IndexOf(args,"-idas3-multiplayer-quick-check")>=0;
            pendingPauseCheck=Array.IndexOf(args,"-idas3-multiplayer-pause-check")>=0;
            pendingShowcaseCheck=Array.IndexOf(args,"-idas3-multiplayer-showcase-check")>=0;
            pendingCourseDrawCheck=Array.IndexOf(args,"-idas3-multiplayer-course-draw-check")>=0;
            pendingMusicCheck=Array.IndexOf(args,"-idas3-multiplayer-music-check")>=0;
            pendingDisconnectCheck=Array.IndexOf(args,"-idas3-multiplayer-disconnect-check")>=0;
            pendingFinishMusicCheck=Array.IndexOf(args,"-idas3-multiplayer-finish-music-check")>=0;
            pendingReturnCheck=pendingFinishMusicCheck||Array.IndexOf(args,"-idas3-multiplayer-return-check")>=0;
            pendingSteamCheck=pendingQuickCheck||Array.IndexOf(args,"-idas3-multiplayer-steam-check")>=0;
            if(pendingSteamCheck&&pendingRole!="host")throw new ArgumentException("Steam runtime smoke uses only its own host room.");
            pendingAddress=Argument(args,"-idas3-multiplayer-address","");
            if(pendingRole=="join"&&string.IsNullOrWhiteSpace(pendingAddress))throw new ArgumentException("Join diagnostic needs -idas3-multiplayer-address.");
            pendingPeerRoot=Argument(args,"-idas3-multiplayer-peer-output","");
            if(pendingPeerRoot.Length>0)pendingPeerRoot=Path.GetFullPath(pendingPeerRoot);
            if(pendingDisconnectCheck&&(pendingPeerRoot.Length==0||pendingSteamCheck))throw new ArgumentException("Disconnect check requires two isolated LAN peers and their output directories.");
            if(pendingReturnCheck&&(pendingPeerRoot.Length==0||pendingSteamCheck||pendingDisconnectCheck))throw new ArgumentException("Return-to-lobby check requires two isolated LAN peers and must run separately from disconnect/Steam checks.");
            pendingPort=IntegerArgument(args,"-idas3-multiplayer-port",27830,1024,65535);
            pendingCourse=IntegerArgument(args,"-idas3-multiplayer-course",0,0,11);
            Directory.CreateDirectory(pendingRoot);saves=Path.Combine(pendingRoot,"userdata");Directory.CreateDirectory(saves);
            if(Array.IndexOf(args,"-idas3-multiplayer-saved-cars-check")>=0){
                var fixtures=Path.GetFullPath(Argument(args,"-idas3-multiplayer-saved-cars-fixture",""));
                foreach(var source in Directory.GetFiles(fixtures,"*",SearchOption.AllDirectories)){
                    var target=Path.Combine(saves,Path.GetRelativePath(fixtures,source));
                    Directory.CreateDirectory(Path.GetDirectoryName(target));File.Copy(source,target,false);
                }
            }
            File.WriteAllText(Path.Combine(saves,"settings.txt"),"0 0 0 0 0 1 1 0\n");
            File.WriteAllText(Path.Combine(saves,"native_selection.txt"),"0 0\n");
            File.WriteAllText(Path.Combine(pendingRoot,"ISOLATED_MULTIPLAYER_TEST.txt"),"Local diagnostic profile; no ordinary save loaded.\n");
            Screen.SetResolution(1200,720,FullScreenMode.Windowed);AudioListener.volume=0;
            return true;
        }
        public static void Attach(Idas3SceneGame game,Idas3MultiplayerSession owner)
        {
            if(pendingRoot==null)return;
            active=game.gameObject.AddComponent<Idas3MultiplayerSmoke>();
            active.host=game;active.session=owner;active.root=pendingRoot;active.role=pendingRole;
            active.address=pendingAddress;active.peerRoot=pendingPeerRoot;active.port=pendingPort;active.course=pendingCourse;
            active.steamCheck=pendingSteamCheck;
            active.quickCheck=pendingQuickCheck;
            active.pauseCheck=pendingPauseCheck;
            active.showcaseCheck=pendingShowcaseCheck;
            active.courseDrawCheck=pendingCourseDrawCheck;
            active.musicCheck=pendingMusicCheck;
            active.disconnectCheck=pendingDisconnectCheck;
            active.returnCheck=pendingReturnCheck;
            active.finishMusicCheck=pendingFinishMusicCheck;
            active.priorDiagnosticFocus=game.DiagnosticFocusOverride;game.DiagnosticFocusOverride=true;
            active.began=Time.realtimeSinceStartupAsDouble;
        }
        internal static bool PreparePhysicalInput(ref Func<KeyCode,bool> key,ref Idas3ControlBindings.PadState pad)
        {
            if(active==null)return false;
            // A focused neutral poll clears the ordinary device-change release
            // latch before the private run starts driving. Later samples travel
            // through the same bindings as physical input, without using hardware.
            key=active.KeyHeld;pad=active.ManualCheck?new Idas3ControlBindings.PadState{connected=true,buttons=active.manualPad}:default;return true;
        }
        private bool KeyHeld(KeyCode key)
        {
            if(finished||frozen||!driving||!session.RaceReleased||!session.IsRacing)return false;
            if(key==manualKey&&manualKey!=KeyCode.None)return true;
            if(ContactMotionCheck)return ContactMotionKey(key);
            // Private mirror fixture: let the host pull ahead on the opening
            // straight so its rear-view actually contains the other car.
            // Inputs still pass through normal bindings and shared simulation.
            if(MirrorCheck)return key==KeyCode.W&&(role=="host"||session.LocalSnapshot.raceTicks>=60);
            if(key==KeyCode.W)return true;
            ulong tick=session.LocalSnapshot.raceTicks;
            return tick%180>=70&&tick%180<76&&key==(role=="host"?KeyCode.D:KeyCode.A);
        }
        private static bool HasKey(Idas3Native.FrameInput frame,int key)
        {
            uint word=key<32?frame.key0:key<64?frame.key1:key<96?frame.key2:frame.key3;
            return (word&(1u<<(key&31)))!=0;
        }
        private static readonly int[] DrivingDiagnosticKeys={87,83,65,68,69,81,67};
        internal static bool PrepareFrame(ref Idas3Native.FrameInput frame)
        {
            if(active==null)return true;
            if(active.finished||active.frozen)return false;
            var mapped=frame;
            frame=new Idas3Native.FrameInput{size=(uint)Marshal.SizeOf<Idas3Native.FrameInput>(),flags=1,deltaSeconds=active.acceleratedReturnWait&&!active.session.ExperimentalAuthority?.25:Math.Min(Time.unscaledDeltaTime,.25)};
            if(active.driving&&active.session.RaceReleased&&active.session.IsRacing){
                // Keep only the action keys produced by ApplyDriving; unrelated
                // real keyboard shortcuts must not enter the private fixture.
                foreach(int key in DrivingDiagnosticKeys)if(HasKey(mapped,key))frame.SetKey(key);
                if(active.ManualCheck){frame.padConnected=mapped.padConnected;frame.padButtons=mapped.padButtons;}
            }
            if(active.pulse!=0){frame.SetKey(active.pulse);active.pulse=0;}
            return true;
        }
        private static string Argument(string[] args,string key,string fallback)
        {
            int at=Array.IndexOf(args,key);if(at<0)return fallback;
            if(at+1>=args.Length)throw new ArgumentException(key+" needs a value.");return args[at+1];
        }
        private static int IntegerArgument(string[] args,string key,int fallback,int min,int max)
        {
            string value=Argument(args,key,fallback.ToString());
            if(!int.TryParse(value,out int result)||result<min||result>max)throw new ArgumentException("Invalid "+key);return result;
        }
        private void Start(){StartCoroutine(Guard(Run()));if(MotionCheck)StartCoroutine(RecordRenderedMotion());}
        private void Update()
        {
            if(finished)return;
            if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-profile")>=0){QualitySettings.vSyncCount=0;Application.targetFrameRate=240;}
            if(host.Failure!=null){Finish(false,host.Failure);return;}
            double timeout=returnCheck?360:240;
            if(Time.realtimeSinceStartupAsDouble-began>timeout){Finish(false,"Multiplayer diagnostic exceeded "+timeout+" seconds.");return;}
            if(session.IsRacing&&session.RaceReleased&&!returnCheckInProgress)ObserveRace();
        }
        private void Check(bool condition,string reason)
        {
            ++checks;if(condition)return;
            throw new InvalidOperationException(reason+" [role="+role+", phase="+phase+", session="+session.StateName+
                ", localTicks="+session.LocalSnapshot.raceTicks+", remoteTicks="+session.RemoteSnapshot.raceTicks+
                ", received="+session.RemoteSnapshotsReceived+", error="+session.ErrorText+"]");
        }
        private IEnumerator Guard(IEnumerator routine)
        {
            var stack=new Stack<IEnumerator>();stack.Push(routine);
            while(stack.Count>0&&!finished){
                object value=null;Exception failure=null;
                try{if(!stack.Peek().MoveNext()){stack.Pop();continue;}value=stack.Peek().Current;}catch(Exception e){failure=e;}
                if(failure!=null){Finish(false,failure.ToString());yield break;}
                if(value is IEnumerator child)stack.Push(child);else yield return value;
            }
        }
        private IEnumerator Frames(int count){for(int i=0;i<count;++i)yield return null;}
        private IEnumerator Until(Func<bool> ready,double seconds,string failure)
        {
            double deadline=Time.realtimeSinceStartupAsDouble+seconds;
            while(!ready()&&Time.realtimeSinceStartupAsDouble<deadline)yield return null;
            Check(ready(),failure);
        }
        private void Phase(string value)
        {
            phase=value;File.WriteAllText(Path.Combine(root,"status.json"),JsonUtility.ToJson(new RoomNotice{
                role=role,code=roomCode,phase=phase,port=port,course=course,utc=DateTime.UtcNow.ToString("O")},true));
        }
        private bool PeerConnected()
        {
            var players=session.Players;if(players==null)return false;
            foreach(var player in players)if(player.Connected&&!player.IsLocal)return true;return false;
        }
        private bool CarSelectionAgrees()
        {
            int connected=0;var cars=Idas3MultiplayerSession.CarNames;
            foreach(var player in session.Players){
                if(!player.Connected)continue;++connected;
                int expected=player.IsLocal?(role=="host"?0:8):(role=="host"?8:0);
                if(player.CarName!=cars[expected])return false;
            }
            return connected==2;
        }
        private bool BothReady(){int ready=0;foreach(var player in session.Players)if(player.Connected&&player.Ready)++ready;return ready==2;}
        private bool CoursePicksAgree(Idas3RaceChoice guest){return session.LocalChoice.Equals(role=="host"?HostCoursePick:guest)&&session.RemoteChoice.Equals(role=="host"?guest:HostCoursePick);}
        private IEnumerator CheckIndependentCoursePicks(){
            Phase("independent-course-picks");var initialGuest=new Idas3RaceChoice(3,false,false,false);var own=role=="host"?HostCoursePick:initialGuest;
            session.SetRaceOptions(own.Course,own.Reverse,own.Wet,own.Night);
            yield return Until(()=>CoursePicksAgree(initialGuest)&&session.CanReady,15,"Independent initial course picks were not acknowledged.");
            session.SetReady(true);yield return Until(BothReady,15,"Both players could not ready their independent picks.");
            Check(peerRoot.Length>0,"Course-draw diagnostic requires both private peer output paths.");
            File.WriteAllText(Path.Combine(root,"choice-ready-observed.json"),"{\"observed\":true}");
            if(role=="join")yield return Until(()=>File.Exists(Path.Combine(peerRoot,"choice-ready-observed.json")),15,"Host did not observe the initial ready barrier.");
            if(role=="join")session.SetRaceOptions(GuestCoursePick.Course,GuestCoursePick.Reverse,GuestCoursePick.Wet,GuestCoursePick.Night);
            yield return Until(()=>CoursePicksAgree(GuestCoursePick)&&!session.LocalReady&&!BothReady(),15,"Guest's changed course/conditions did not invalidate both players' readiness.");
            foreach(var player in session.Players)Check(!player.Ready,"A course edit retained stale readiness.");
            courseReadinessInvalidated=true;Check(!session.HasCourseDraw,"Merely editing choices selected a winner.");
            File.WriteAllText(Path.Combine(root,"choice-reset-observed.json"),"{\"observed\":true}");
            yield return Until(()=>File.Exists(Path.Combine(peerRoot,"choice-reset-observed.json")),15,"The other driver did not observe both readiness states reset.");
        }
        private void CheckLockedCourseDraw(){
            Check(courseReadinessInvalidated&&session.HasCourseDraw,"Race started without the synchronized choice check.");
            int winner=session.CourseWinnerSlot;Check(winner==0||winner==1,"Selected course winner is not a player slot.");
            var selected=winner==0?HostCoursePick:GuestCoursePick;
            Check(session.SelectedChoice.Equals(selected)&&CoursePicksAgree(GuestCoursePick),"Selected complete course does not match the winning player's agreed pick.");
            string text=session.CourseDrawText;Check(text.Contains(winner==0?"SMOKE HOST":"SMOKE JOIN"),"Selected-player result uses the wrong identity.");
            var source=new Idas3PreRaceSmoke.PreRaceStatus{size=(uint)Marshal.SizeOf<Idas3PreRaceSmoke.PreRaceStatus>()};
            Check(Idas3SceneGetPreRaceStatus(ref source)==1&&host.Status.course==selected.Course&&source.condition==(uint)(selected.Course*2+(selected.Reverse?1:0)),"Actual native course/route differs from selected choice.");
            int car=session.LocalCar;session.SetRaceOptions(7,false,false,false);session.SetCar(34);session.SetReady(false);session.StartRace();
            Check(session.SelectedChoice.Equals(selected)&&session.CourseWinnerSlot==winner&&session.LocalCar==car&&CoursePicksAgree(GuestCoursePick),"A locked course draw was edited or rerolled.");
            course=selected.Course;
            File.WriteAllText(Path.Combine(root,"course-draw-report.json"),JsonUtility.ToJson(new CourseDrawReport{passed=true,role=role,readinessInvalidated=courseReadinessInvalidated,locked=true,winnerSlot=winner,selectedText=text,
                hostChoice=ChoiceRecord.From(HostCoursePick),guestChoice=ChoiceRecord.From(GuestCoursePick),selected=ChoiceRecord.From(selected),nativeCourse=host.Status.course,nativeCondition=source.condition},true));
        }
        private IEnumerator Run()
        {
            yield return Frames(5);Check(host.Ready,"Native game did not initialize.");
            menu=FindFirstObjectByType<Idas3MultiplayerMenu>();Check(menu!=null,"Multiplayer menu component is missing.");
            if(quickCheck){yield return QuickMatchRuntimeCheck();yield break;}
            if(steamCheck){yield return SteamRuntimeCheck();yield break;}
            session.ConfigureLocalTest(port,role=="host"?"SMOKE HOST":"SMOKE JOIN");
            if(course==11){host.GameOptions.BeginEdit();host.GameOptions.Draft.replayOnline=true;Check(host.GameOptions.ApplyDraft(),"Enable online replay recording");}
            Check(session.Available,"LAN diagnostic transport is unavailable.");
            session.SetCar(role=="host"?0:8);
            if(ChallengerCheck)session.SetRaceOptions(course,false,course==8,course==4||course==8);
            if(role=="host"){
                Phase("hosting");if(ChallengerCheck)session.QuickMatch();else session.HostRoom();
                yield return Until(()=>session.InLobby&&!string.IsNullOrEmpty(session.RoomCode),20,"Host room did not open.");
                roomCode=session.RoomCode;
                if(ChallengerCheck){
                    var overlay=host.GetComponent<Idas3ChallengerOverlay>();
                    Check(!overlay.SearchingVisible,"Search badge visible in attract/menu");
                    pulse=13;yield return Until(()=>host.Status.frontendStage!=0,5,"Waiting room swallowed frontend Start");
                    pulse=116;yield return Until(()=>host.Status.racePhase==2&&(host.Status.flags&1)==0,35,"Cannot start offline race while searching");
                    var tick=host.Status.simulationTicks;
                    yield return Until(()=>host.Status.simulationTicks>tick&&overlay.SearchingVisible,5,"Waiting search stopped race or lost badge");
                    yield return CaptureChallenger("accepting-challengers");
                    foreach(var size in new[]{new Vector2Int(640,480),new Vector2Int(1280,720),new Vector2Int(1920,1080),new Vector2Int(2560,1080)})
                        yield return CaptureChallenger("accepting-"+size.x+"x"+size.y,size.x,size.y);
                    session.CancelQuickMatch();yield return Frames(3);Check(!overlay.SearchingVisible,"Cancelled search retained badge");
                    session.QuickMatch();yield return Until(()=>session.InLobby&&overlay.SearchingVisible,15,"Cannot restart background search");
                }
                session.SetRaceOptions(course,false,course==8,course==4||course==8);
                File.WriteAllText(Path.Combine(root,"room.json"),JsonUtility.ToJson(new RoomNotice{
                    role=role,code=roomCode,phase="room-open",port=port,course=course,utc=DateTime.UtcNow.ToString("O")},true));
            }else{Phase("joining");if(ChallengerCheck)session.QuickMatch();else session.JoinRoom(address);}
            yield return Until(()=>session.HandshakeComplete&&PeerConnected(),90,"Two-client handshake did not complete.");
            if(ChallengerCheck){
                var overlay=host.GetComponent<Idas3ChallengerOverlay>();
                yield return Until(()=>overlay.Active,5,"Admitted peer did not announce challenger");
                Check(!overlay.SearchingVisible&&!session.CanReady&&!menu.IsOpen,"Found match left search badge/Ready/lobby visible");
                var tick=host.Status.simulationTicks;yield return Frames(8);
                Check(host.Status.simulationTicks==tick,"Offline race advanced under challenger banner");
                yield return new WaitForSecondsRealtime(.45f);
                yield return CaptureChallenger("challenge-received");
                if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-challenger-drop-check")>=0){
                    if(role=="join")session.LeaveRoom();
                    yield return Until(()=>!session.HandshakeComplete&&!overlay.Active,10,"Disconnected challenger retained input/modal hold");
                    if(role=="host"){
                        tick=host.Status.simulationTicks;yield return Frames(15);
                        Check((host.Status.flags&1u)==0&&host.Status.simulationTicks>tick,"Cancelled challenger did not resume offline race");
                    }
                    Check(!overlay.SearchingVisible&&!session.ChallengerPending,"Disconnected challenger retained search badge");
                    Finish(true,null);yield break;
                }
                yield return Until(()=>!overlay.Active&&session.CanReady,15,"Challenger did not enter ready lobby");
                Check(host.Status.frontendStage==6&&(host.Status.flags&1)!=0&&menu.IsOpen,"Challenger did not open course select and lobby");
                Check(overlay.Announcements==1&&!overlay.SearchingVisible,"Duplicate announcement or stale search badge");
                File.WriteAllText(Path.Combine(root,"challenger-report.json"),JsonUtility.ToJson(new ChallengerReport{passed=true,announcements=overlay.Announcements,role=role},true));
            }
            yield return Until(()=>session.CanReady,15,"Connected room did not finish challenger presentation");
            if(ChallengerCheck&&role=="host")session.SetRaceOptions(course,false,course==8,course==4||course==8);
            roomCode=session.RoomCode;Phase("car-agreement");
            session.SetCar(role=="host"?0:8);
            yield return Until(CarSelectionAgrees,20,"Both player slots did not agree on car0/car8.");
            if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-online-boost-check")>=0){
                session.SetReady(true);
                yield return Until(()=>session.Players[0].Ready&&session.Players[1].Ready,15,"Boost test readiness did not settle");
                File.WriteAllText(Path.Combine(root,"boost-ready"),"ready");
                yield return Until(()=>File.Exists(Path.Combine(peerRoot,"boost-ready")),15,"Peer boost readiness missing");
                if(role=="host"){
                    session.SetBoost(false);
                    Check(!session.LocalReady&&!session.Players[1].Ready,"Changing boost did not clear readiness");
                    yield return Until(()=>File.Exists(Path.Combine(peerRoot,"boost-off")),15,"Boost OFF not acknowledged");
                    session.SetBoost(true);
                    yield return Until(()=>File.Exists(Path.Combine(peerRoot,"boost-on")),15,"Boost ON not acknowledged");
                }else{
                    yield return Until(()=>!session.BoostEnabled&&!session.LocalReady,15,"Host boost OFF/readiness reset not received");
                    session.SetBoost(true);Check(!session.BoostEnabled,"Guest changed host boost rule");
                    File.WriteAllText(Path.Combine(root,"boost-off"),"off");
                    yield return Until(()=>session.BoostEnabled,15,"Host boost ON not received");
                    File.WriteAllText(Path.Combine(root,"boost-on"),"on");
                }
                Check(session.BoostEnabled,"Boost final rule differs");
                Check(session.ExperimentalAuthority,"Normal online rooms must use shared simulation without a test switch");
                session.SetReady(true);
                yield return Until(()=>session.Players[0].Ready&&session.Players[1].Ready,15,"Collision test readiness did not settle");
                File.WriteAllText(Path.Combine(root,"collisions-ready"),"ready");
                yield return Until(()=>File.Exists(Path.Combine(peerRoot,"collisions-ready")),15,"Peer collision readiness missing");
                bool finalCollisions=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-online-collisions-off")<0;
                if(role=="host"){
                    session.SetCollisions(false);
                    Check(!session.LocalReady&&!session.Players[1].Ready,"Changing collisions did not clear readiness");
                    yield return Until(()=>File.Exists(Path.Combine(peerRoot,"collisions-off")),15,"Collision OFF not acknowledged");
                    if(finalCollisions)session.SetCollisions(true);
                    if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-online-boost-off")>=0)session.SetBoost(false);
                }else{
                    yield return Until(()=>!session.CollisionsEnabled&&!session.LocalReady,15,"Host collision OFF/readiness reset not received");
                    session.SetCollisions(true);Check(!session.CollisionsEnabled,"Guest changed host collision rule");
                    File.WriteAllText(Path.Combine(root,"collisions-off"),"off");
                    yield return Until(()=>session.CollisionsEnabled==finalCollisions,15,"Final collision rule not received");
                    if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-online-boost-off")>=0)
                        yield return Until(()=>!session.BoostEnabled,15,"Final boost OFF not received");
                }
                File.WriteAllText(Path.Combine(root,"rules-final"),"agreed");
                yield return Until(()=>File.Exists(Path.Combine(peerRoot,"rules-final")),15,"Peer rules acknowledgement missing");
                yield return CaptureMenu("room-rules");
                if(!WorldOnly){
                    if(role=="host"){
                        yield return VerifyRuleNavigation("BOOST",()=>session.BoostEnabled);
                        yield return VerifyRuleNavigation("CAR COLLISIONS",()=>session.CollisionsEnabled);
                        File.WriteAllText(Path.Combine(root,"controller-rules-passed.txt"),"Both rules toggled twice through menu navigation and retained focus.\n");
                    }else yield return Until(()=>File.Exists(Path.Combine(peerRoot,"controller-rules-passed.txt")),20,"Host rule navigation test did not finish");
                }
            }
            if(SavedCarCheck){
                Check(session.Garage.Count>=2&&session.LocalSavedCar.Saved,"Saved garage was not loaded");
                int model=session.LocalCar;
                bool secondary=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-secondary-car-check")>=0;
                if(secondary){
                    int selection=40+35+model;
                    Idas3OnlineCar selected=null;
                    foreach(var item in session.Garage)if(item.Selection==selection)selected=item;
                    Check(selected!=null&&selected.Saved&&selected.SaveSlot==1,"Secondary slot profile missing from online garage");
                    session.SelectSavedCar(selected);
                    int peerSelection=40+35+(role=="host"?8:0);
                    yield return Until(()=>session.RemoteSavedCar!=null&&session.RemoteSavedCar.Selection==peerSelection&&session.CanReady,20,"Peer lost the secondary saved-car selection");
                }else{
                    session.CycleCar(1);
                    Check(session.LocalCar==model&&session.LocalSavedCar.Selection==1,"Same-model save files were collapsed or selection did not cycle");
                    yield return Until(()=>session.RemoteSavedCar!=null&&session.RemoteSavedCar.Selection==1&&session.CanReady,20,"Peer did not receive the saved-car change");
                }
            }
            if(ManualCheck){
                session.SetAutomatic(true);session.SetAutomatic(false);
                session.RefreshGarage();Check(!session.LocalSavedCar.Automatic,"Garage refresh lost chosen manual transmission");
                yield return Until(()=>session.RemoteSavedCar!=null&&!session.RemoteSavedCar.Automatic&&session.CanReady,20,"Transmission choice was not synchronized");
            }
            if(courseDrawCheck)yield return CheckIndependentCoursePicks();
            if(course>=9){
                var args=Environment.GetCommandLineArgs();
                var choice=new Idas3RaceChoice(course,Array.IndexOf(args,"-hakone-uphill")>=0,Array.IndexOf(args,"-hakone-wet")>=0,Array.IndexOf(args,"-hakone-night")>=0);
                session.SetRaceOptions(choice.Course,choice.Reverse,choice.Wet,choice.Night);
                yield return Until(()=>session.CanReady&&session.RemoteChoice.Equals(choice),20,"Hakone choices did not synchronize");
            }
            if(musicCheck)yield return Idas3RaceMusicSmoke.VerifyLobby(host,session,root,role);
            if(showcaseCheck)yield return Idas3MultiplayerPresentationSmoke.VerifyLobby(host,session,root,role);
            menu.SetOpen(true);yield return Frames(3);
            menuInputBlocked=menu.BlocksGameInput;Check(menuInputBlocked,"Open multiplayer menu did not capture game input.");
            // Saved-car tests run hidden and validate the normal session plus
            // rendered race cars. Hidden windows do not issue IMGUI Repaint.
            if(!SavedCarCheck)yield return CaptureMenu("lobby");yield return Frames(3);
            if(ChallengerCheck){
                File.WriteAllText(Path.Combine(root,"choices-settled.txt"),"ready for normal Ready input");
                yield return Until(()=>File.Exists(Path.Combine(peerRoot,"choices-settled.txt")),15,"Peer did not finish selecting car/course");
                yield return new WaitForSecondsRealtime(1f);
                yield return Until(()=>session.CanReady,15,"Course/car changes were not acknowledged");
            }
            if(showcaseCheck||ObserveStartOnly)showcaseObserver=Idas3PreRaceSmoke.ObserveOnline(host,root,role);
            Phase("ready");if(courseDrawCheck)yield return Until(()=>session.CanReady,15,"Final independent picks were not acknowledged before Ready.");session.SetReady(true);
            Check(menu.IsOpen,"Lobby must remain open before starting the auto-close regression.");
            if(role=="host"){
                yield return Until(()=>session.CanStart,30,"Both drivers did not become ready.");
                session.StartRace();
            }
            yield return Until(()=>session.IsRacing,30,"Native multiplayer loading did not start.");
            if(ChallengerCheck){
                var overlay=host.GetComponent<Idas3ChallengerOverlay>();
                Check(!overlay.SearchingVisible&&!overlay.Active&&overlay.Announcements==1,"Online race retained challenger art or re-announced match");
            }
            if(SavedCarCheck)VerifySavedRaceCars();
            yield return Frames(2);
            var renderedStart=ReadSourceStatus();
            lobbyAutoClosed=!menu.IsOpen&&renderedStart.phase<=1&&renderedStart.rivalRanges>0;
            Check(lobbyAutoClosed,"The first rendered native race did not automatically close the lobby before its showcase.");
            yield return Until(()=>session.IsRacing&&session.RaceReleased,40,"Synchronized race countdown did not release.");
            if(course>=9){
                var choice=session.SelectedChoice;
                uint expected=(course==11?1048576u:course==10?524288u:0u)|16384u|(choice.Reverse?32768u:0u)|(choice.Night?65536u:0u)|(choice.Wet?131072u:0u);
                Check((host.Status.flags&1818624u)==expected,"Hakone native course/conditions differ from lobby");
                if(course==11){var enna=FindAnyObjectByType<IdasSpecialStageEnnaCourse>();Check(enna!=null&&enna.Loaded,"Enna online scenery missing");enna.VerifyPresentation();}
                else {Check(FindAnyObjectByType<Idas8HakoneCourse>().LoadedCourse==Idas8HakoneCourse.CourseName(expected),"Imported online course identity mismatch");
                Check(FindAnyObjectByType<Idas8HakoneCourse>().LoadedVariant==Idas8HakoneCourse.Variant(expected),"Hakone online scenery variant missing");}
            }
            Check(!menu.IsOpen&&ReadSourceStatus().phase==1,"Lobby reopened when the first showcase shot began.");
            if(showcaseCheck)yield return CaptureUnobstructedShowcase();
            if(courseDrawCheck)CheckLockedCourseDraw();
            if(musicCheck)yield return Idas3RaceMusicSmoke.VerifyOnlineRace(host,root,role);
            Phase("racing");driving=true;
            yield return Until(()=>raceObserved&&session.RemoteSnapshotsReceived>=10&&lastLocal.raceTicks-firstLocalTick>=90,25,"Race snapshots did not begin moving.");
            Check(!host.ControlBindings.SuppressInput,"Private neutral input did not clear the controller-change release latch.");
            Check((host.DiagnosticSubmittedInput.flags&2u)==0&&HasKey(host.DiagnosticSubmittedInput,87),"Mapped private throttle did not reach the native driving packet.");
            if(ManualCheck)yield return VerifyManualShifts();
            if(showcaseCheck||ObserveStartOnly)Check(showcaseObserver.OnlineCheckPassed,showcaseObserver.OnlineCheckError??"Online presentation check failed.");
            Check((host.Status.flags&16)!=0,"Multiplayer race did not use original handling.");
            pulse=67;yield return Frames(8);
            yield return CaptureWorld("race-chase-early");
            yield return Idas3MultiplayerHudSmoke.VerifyView(host,session,root,role,1);
            pulse=67;yield return Frames(8);
            yield return CaptureWorld("race-bumper-early");
            yield return Idas3MultiplayerHudSmoke.VerifyView(host,session,root,role,0);
            if(pauseCheck){Phase("pause-options-check");yield return Idas3PauseSmoke.VerifyOnline(host,session,root,role);Phase("racing");}
            yield return Until(()=>raceObserved&&lastPoseObservedAt-firstPoseObservedAt>=10&&lastLocal.raceTicks-firstLocalTick>=600&&lastRemote.raceTicks-firstRemoteTick>=600&&greatestReceived>=100,70,
                "Both native cars did not complete600 source race ticks with100 remote snapshots.");
            if(ContactMotionCheck){
                yield return Until(()=>lastLocal.raceTicks-firstLocalTick>=1800&&lastRemote.raceTicks-firstRemoteTick>=1800,70,"Contact fixture did not run for 30 source seconds");
                Check(session.CollisionsEnabled?lastAuthority.contactFrames>0:lastAuthority.contactFrames==0,
                    "Native car contact did not match the agreed collision rule");
            }
            Check(lastLocal.Valid((uint)(role=="host"?0:8))&&lastRemote.Valid((uint)(role=="host"?8:0)),"Final snapshots contain the wrong car model or invalid state.");
            Check(localTravel>10&&remoteTravel>10&&peakLocalSpeed>3&&peakRemoteSpeed>3,"Both drivers did not move through the course.");
            double wall=lastPoseObservedAt-firstPoseObservedAt,sourceSeconds=(lastLocal.raceTicks-firstLocalTick)/60.0;
            Check(sourceSeconds/wall>.85&&sourceSeconds/wall<1.15,"Native race clock did not track wall time.");
            Check(maxPeerRanges>0&&maxPeerMainRanges>0,"Received opponent was never submitted as main-view car geometry.");
            Check(greatestSent>=100,"Local client sent fewer than100 snapshots.");
            // Use the public action invoked by F1; this checks lifecycle
            // behavior without claiming physical keyboard focus in two windows.
            menu.Toggle();yield return Frames(3);liveMenuReopened=menu.IsOpen;
            Check(liveMenuReopened,"The same live race could not reopen its F1 menu after automatic closure.");
            menu.Toggle();yield return Frames(3);Check(!menu.IsOpen,"Live race menu could not close again.");
            yield return CaptureWorld("race-bumper-late");
            pulse=67;yield return Frames(8);yield return CaptureWorld("race-chase-late");
            if(courseDrawCheck){menu.SetOpen(true);yield return Frames(3);yield return CaptureMenu("selected-course");menu.SetOpen(false);yield return Frames(3);}
            if(session.ExperimentalAuthority){
                if(session.Night){
                    Check((session.RemoteSnapshot.flags&16u)!=0,"Authority dropped opponent headlights-on flag");
                    Check(session.RemoteSnapshot.headlightVisible!=0,"Opponent lamp geometry did not open/illuminate");
                }
                Check(lastAuthority.frame>600&&lastAuthority.verified>500,"Authority inputs were not mutually verified.");
                Check(lastAuthority.frame-lastAuthority.confirmed<64,"Authority exhausted its prediction window.");
                Check(authorityStallFrames==0,"Authority stalled during ordinary network conditions.");
            }
            Phase("race-verified");
            File.WriteAllText(Path.Combine(root,"race-verified.json"),JsonUtility.ToJson(BuildReport(true,null,false),true));
            if(disconnectCheck){yield return CheckConnectionDrop();Finish(true,null);yield break;}
            if(returnCheck){yield return CheckReturnToLobby();Finish(true,null);yield break;}
            if(role=="host"){
                if(peerRoot.Length>0)yield return Until(()=>File.Exists(Path.Combine(peerRoot,"race-verified.json")),35,"Joiner did not finish its own race validation.");
                else yield return Frames(120);
                Phase("leaving");driving=false;session.LeaveRoom();
                yield return Frames(10);Check(!session.InLobby&&!session.IsRacing,"Host did not leave its race and room.");
            }else{
                Phase("awaiting-host-leave");
                yield return Until(()=>!PeerConnected(),45,"Joiner did not observe the host leaving.");
                peerLeft=true;driving=false;session.LeaveRoom();yield return Frames(10);
                Check(!session.InLobby&&!session.IsRacing,"Joiner did not return from the disconnected race.");
            }
            if(course==11)yield return VerifyEnnaReplay();
            Finish(true,null);
        }
        private KeyCode manualKey;
        private ushort manualPad;
        private bool ManualCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-manual-check")>=0;
        private bool MirrorCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-mirror-check")>=0;
        private int Gear()=>Idas3MultiplayerNative.Idas3MultiplayerCurrentGear();
        private IEnumerator VerifyManualShifts(){
            var words=new uint[307];Check(Idas3MultiplayerNative.Idas3MultiplayerReadRaceCar(0,words,307)==1&&words[17]==1,"Online race was not manual");
            Check(Gear()==1,"Manual gearbox shifted without player input");
            manualKey=KeyCode.E;yield return Until(()=>Gear()==2,3,"Keyboard upshift failed online");
            yield return Frames(10);Check(Gear()==2,"Held upshift repeated");manualKey=KeyCode.None;yield return Frames(6);
            manualKey=KeyCode.Q;yield return Until(()=>Gear()==1,3,"Keyboard downshift failed online");manualKey=KeyCode.None;yield return Frames(6);
            driving=false;var bindings=host.ControlBindings;bindings.BeginEdit();
            Check(bindings.TrySetDraftPad(Idas3ControlBindings.ActionId.ShiftUp,Idas3ControlBindings.PadInput.RightShoulder)&&
                bindings.TrySetDraftPad(Idas3ControlBindings.ActionId.ShiftDown,Idas3ControlBindings.PadInput.LeftShoulder)&&bindings.ApplyDraft(),"Controller shift rebinding failed");
            yield return Frames(8);driving=true;manualPad=0x200;yield return Until(()=>Gear()==2,3,"Rebound controller upshift failed online");
            Check((host.DiagnosticSubmittedInput.padButtons&0x2000)!=0,"Controller upshift did not pass through binding translation");
            yield return Frames(10);Check(Gear()==2,"Held controller shift repeated");manualPad=0;yield return Frames(6);
            manualPad=0x100;yield return Until(()=>Gear()==1,3,"Rebound controller downshift failed online");manualPad=0;yield return Frames(6);
            File.WriteAllText(Path.Combine(root,"manual-shifts-passed.txt"),"Manual override synchronized; keyboard E/Q and remapped controller shoulder buttons changed actual native gears 1 -> 2 -> 1; held buttons did not repeat.\n");
        }
        private bool SavedCarCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-saved-cars-check")>=0;
        private void VerifySavedRaceCars(){
            var local=new uint[307];var remote=new uint[307];
            Check(Idas3MultiplayerNative.Idas3MultiplayerReadRaceCar(0,local,307)==1,"Local race profile unavailable");
            Check(Idas3MultiplayerNative.Idas3MultiplayerReadRaceCar(1,remote,307)==1,"Remote race profile unavailable");
            for(int i=0;i<307;++i){
                if(i==0||i==1||i==2||i==3||i==8||(ManualCheck&&i==17))continue; // Race course/mode/weather.
                Check(local[i]==session.LocalSavedCar.Words[i],"Saved local tuning/profile lost at word "+i);
            }
            foreach(int offset in Idas3OnlineCar.AppearanceOffsets)
                Check(remote[offset/4]==session.RemoteSavedCar.Words[offset/4],"Remote saved appearance lost at byte "+offset);
            Check((local[41]&255)>4&&local[16]!=0,"Diagnostic did not exercise upgraded painted cars");
            File.WriteAllText(Path.Combine(root,"saved-cars-passed.txt"),"Local saved profile retained, including tuning/transmission/paint/parts/name; remote appearance matched the agreed peer save; duplicate-model save selection synchronized.\n");
        }
        [Serializable] private class DisconnectReport
        {
            public string schema="idas3-real-tcp-disconnect-v1",role,phase,saveFingerprint,scope="Two actual LAN clients. Host closes the real TCP transport without an application Leave packet; production disconnect UI and native freeze are verified, followed by a normal Enter acknowledgement.";
            public bool passed,neutralResult,unhideable,saveUnchanged,acknowledged,windowCardVisible;
            public int checks;public ulong bridgeTicks,sourceTicks,raceTicks;public double frozenSeconds;public string capture;
        }
        private string SaveFingerprint()
        {
            string directory=Path.Combine(root,"userdata");var files=Directory.GetFiles(directory,"*",SearchOption.AllDirectories);Array.Sort(files,StringComparer.Ordinal);
            var text=new System.Text.StringBuilder();using(var sha=System.Security.Cryptography.SHA256.Create())foreach(string file in files){
                text.Append(file.Substring(directory.Length+1).Replace('\\','/')).Append('=');
                using(var stream=File.OpenRead(file))text.Append(BitConverter.ToString(sha.ComputeHash(stream)).Replace("-",""));text.Append('\n');
            }
            return text.ToString();
        }
        private Idas3CarSnapshot ReadNativePose()
        {
            var pose=new Idas3CarSnapshot{size=128,version=1};Check(Idas3MultiplayerNative.Idas3MultiplayerGetLocalSnapshot(ref pose)==1,"Native terminal pose unavailable.");return pose;
        }
        private static string PoseIdentity(Idas3CarSnapshot pose){pose.sequence=0;return JsonUtility.ToJson(pose);}
        private Idas3PreRaceSmoke.PreRaceStatus ReadSourceStatus()
        {
            var source=new Idas3PreRaceSmoke.PreRaceStatus{size=(uint)Marshal.SizeOf<Idas3PreRaceSmoke.PreRaceStatus>()};
            Check(Idas3SceneGetPreRaceStatus(ref source)==1,"Native source clock unavailable.");return source;
        }
        [Serializable] private class ReturnLobbyReport
        {
            public string schema="idas3-connected-lobby-return-v1",role,phase,scope="Two actual isolated LAN clients. First race reaches native source timeout using neutral input and diagnostic 0.25-second frame steps; no result/rule state is forced. Guest uses the normal Enter result action. Both preserve transport/nonces/picks/cars, reset readiness, then re-ready and start a second race at normal wall-clock delta.";
            public bool passed,naturalFinish,connectedLobby,savesUnchanged,secondCountdown,secondDrive,musicSelectionPreserved;
            public int checks,selectedMusic;public string roomCode,saveFingerprint;public double acceleratedWaitSeconds;
            public ulong firstRaceId,secondRaceId,firstFinishTicks,secondLocalTicks,secondRemoteTicks;
            public long secondReceived,secondSent;public uint[] secondPhases;public int[] secondDigits;
        }
        private T SessionField<T>(string name)
        {
            var field=typeof(Idas3MultiplayerSession).GetField(name,System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
            Check(field!=null,"Session field missing from isolated return fixture: "+name);return (T)field.GetValue(session);
        }
        [Serializable] private class FinishMusicReport
        {
            public string schema="idas3-connected-finish-music-v1",applicationVersion=Application.version,role,resultText;
            public string scope="Two real isolated LAN clients reach source time-up with neutral input. Host finishes first to observe pending silence, then the guest finishes and the ordinary Results packet selects TIMEUP on both. Repeated native and managed result delivery must retain the existing PCM cursor. Return-to-lobby/second-race checks follow.";
            public bool passed,pendingObserved,peerPendingConfirmed,naturalTimeUp,duplicateNativePreserved,duplicateManagedPreserved;
            public int checks;
            public Idas3PreRaceSmoke.RaceAudioStatus pending,firstResult,beforeDuplicate,afterDuplicate,settled;
        }
        private Idas3PreRaceSmoke.RaceAudioStatus ReadFinishAudio()
        {
            var status=new Idas3PreRaceSmoke.RaceAudioStatus{size=(uint)Marshal.SizeOf<Idas3PreRaceSmoke.RaceAudioStatus>()};
            Check(status.size==56&&Idas3SceneGetRaceAudioStatus(ref status)==1&&status.version==1,"Finish audio status ABI");return status;
        }
        private IEnumerator CheckNaturalFinishMusic()
        {
            int firstCheck=checks;var report=new FinishMusicReport{role=role};
            try{
                // Make the first driver's unresolved result observable while
                // both transports continue normally. No result/rule is forced.
                acceleratedReturnWait=role=="host";
                if(role=="host"){
                    yield return Until(()=>session.LocalSnapshot.Finished||session.LocalSnapshot.TimeUp,180,"Host did not reach source time-up first.");
                    Check(!session.CanReturnToLobby,"Host result resolved before pending music could be checked.");
                    report.pending=ReadFinishAudio();report.pendingObserved=report.pending.scene==6&&report.pending.streamFrame==0;
                    Check(report.pendingObserved,"Waiting for peer must hold a silent pending finish scene.");
                    File.WriteAllText(Path.Combine(root,"finish-music-pending.json"),JsonUtility.ToJson(report.pending,true));
                }else{
                    yield return Until(()=>File.Exists(Path.Combine(peerRoot,"finish-music-pending.json")),180,"Host did not verify pending finish silence.");
                    report.pending=JsonUtility.FromJson<Idas3PreRaceSmoke.RaceAudioStatus>(File.ReadAllText(Path.Combine(peerRoot,"finish-music-pending.json")));
                    report.peerPendingConfirmed=report.pending.scene==6&&report.pending.streamFrame==0;
                    Check(report.peerPendingConfirmed,"Host pending-silence evidence is invalid.");acceleratedReturnWait=true;
                }
                yield return Until(()=>session.CanReturnToLobby,180,"Natural time-up did not reach connected results.");
                acceleratedReturnWait=false;
                report.naturalTimeUp=session.LocalSnapshot.TimeUp&&session.ResultText.StartsWith("BOTH DRIVERS",StringComparison.Ordinal);
                Check(report.naturalTimeUp,"Natural finish did not produce both-driver time-up.");
                report.firstResult=ReadFinishAudio();Check(report.firstResult.scene==3,"Connected both-driver time-up selected the wrong music scene.");
                yield return Frames(6);report.beforeDuplicate=ReadFinishAudio();
                Check(report.beforeDuplicate.scene==3&&report.beforeDuplicate.streamFrame>0,"TIMEUP stream did not advance after the result packet.");
                Check(Idas3MultiplayerNative.Idas3MultiplayerSetResult(-1)==1,"Repeated native time-up result was rejected.");
                var afterNative=ReadFinishAudio();
                report.duplicateNativePreserved=afterNative.scene==3&&afterNative.streamFrame>=report.beforeDuplicate.streamFrame;
                Check(report.duplicateNativePreserved,"Repeated native result restarted finish music.");
                // Exercise the same managed handoff a duplicate Results packet
                // invokes. The actual initial delivery above used the network.
                var show=typeof(Idas3MultiplayerSession).GetMethod("ShowResult",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
                Check(show!=null,"Managed Results handler is absent from private duplicate fixture.");
                ulong local=session.LocalSnapshot.raceTicks,remote=SessionField<ulong>("remoteFinishTicks");
                show.Invoke(session,new object[]{-1,session.IsHost?local:remote,session.IsHost?remote:local});
                report.afterDuplicate=ReadFinishAudio();
                report.duplicateManagedPreserved=report.afterDuplicate.scene==3&&report.afterDuplicate.streamFrame>=afterNative.streamFrame;
                Check(report.duplicateManagedPreserved,"Repeated managed result restarted finish music.");
                yield return Frames(6);report.settled=ReadFinishAudio();
                Check(report.settled.scene==3&&report.settled.streamFrame>=report.afterDuplicate.streamFrame,"Finish music changed after repeated results.");
                report.resultText=session.ResultText;report.passed=true;
            }finally{
                acceleratedReturnWait=false;report.checks=checks-firstCheck;
                File.WriteAllText(Path.Combine(root,"finish-music-report.json"),JsonUtility.ToJson(report,true));
            }
        }
        private IEnumerator CheckReturnToLobby()
        {
            int firstCheck=checks;var report=new ReturnLobbyReport{role=role,roomCode=session.RoomCode};
            if(session.ExperimentalAuthority)report.scope="Two real authority clients reach source timeout at normal wall-clock speed; no rule state is forced. Guest uses the normal Enter action. Both preserve the connected lobby, picks, music, and cars, then start and drive a second race. acceleratedWaitSeconds is the legacy field name for the measured wait; acceleration is disabled in authority mode.";
            bool? priorFocus=host.DiagnosticFocusOverride;host.DiagnosticFocusOverride=true;
            returnCheckInProgress=true;driving=false;
            try{
                yield return Until(()=>File.Exists(Path.Combine(peerRoot,"race-verified.json")),35,"Peer did not finish its first-race validation.");
                object transport=SessionField<object>("transport");ulong localNonce=SessionField<ulong>("localNonce"),remoteNonce=SessionField<ulong>("remoteNonce");
                report.firstRaceId=SessionField<ulong>("raceId");report.saveFingerprint=SaveFingerprint();
                host.RaceMusic.Refresh();report.selectedMusic=host.RaceMusic.State.selectedIndex;
                if(finishMusicCheck)Check(report.selectedMusic==-1,"Private finish-music fixture must retain the game-default song choice.");
                var localPick=session.LocalChoice;var remotePick=session.RemoteChoice;int localCar=session.LocalCar;
                Phase("waiting-natural-results");double waitStarted=Time.realtimeSinceStartupAsDouble;acceleratedReturnWait=true;
                // Private diagnostic acceleration only: run the unchanged
                // source rules until their real timer ends with no car input.
                if(finishMusicCheck)yield return CheckNaturalFinishMusic();
                else yield return Until(()=>session.CanReturnToLobby,180,"Source race did not naturally reach connected results.");
                acceleratedReturnWait=false;report.acceleratedWaitSeconds=Time.realtimeSinceStartupAsDouble-waitStarted;
                report.naturalFinish=host.Status.racePhase==3&&(session.LocalSnapshot.Finished||session.LocalSnapshot.TimeUp);
                report.firstFinishTicks=session.LocalSnapshot.raceTicks;
                Check(report.naturalFinish&&!session.DisconnectedFinish&&session.HandshakeComplete,"Connected result was not a natural native finish.");
                Check(SaveFingerprint()==report.saveFingerprint,"Natural multiplayer result modified private profile/settings/records.");
                yield return Until(()=>menu.ResultsVisible&&menu.IsOpen&&menu.CanAcknowledgeReturnToLobby,5,"Finished race did not expose the controller-accessible Return to Lobby action.");
                yield return CaptureMenu("return-results");
                File.WriteAllText(Path.Combine(root,"return-results-ready.json"),"{\"ready\":true}");
                yield return Until(()=>File.Exists(Path.Combine(peerRoot,"return-results-ready.json")),20,"Both peers did not reach results before return.");
                Phase("requesting-connected-lobby");
                if(role=="join")pulse=13; // Same fixed Enter/A action as the visible button.
                yield return Until(()=>session.StateName=="Lobby"&&!session.IsRacing&&session.HandshakeComplete&&session.InLobby,30,"Return did not preserve the connected lobby on both clients.");
                yield return Frames(6);
                report.connectedLobby=menu.IsOpen&&session.RoomCode==report.roomCode&&PeerConnected()&&!session.DisconnectedFinish&&(host.Status.flags&(128u|2048u))==0&&(host.Status.flags&1u)!=0;
                Check(report.connectedLobby,"Return closed the room, hid lobby, or retained native race state.");
                Check(ReferenceEquals(transport,SessionField<object>("transport"))&&SessionField<ulong>("localNonce")==localNonce&&SessionField<ulong>("remoteNonce")==remoteNonce,"Return replaced the transport or started a new peer handshake.");
                Check(session.LocalChoice.Equals(localPick)&&session.RemoteChoice.Equals(remotePick)&&session.LocalCar==localCar&&CarSelectionAgrees(),"Return lost car or course picks.");
                Check(!session.HasCourseDraw&&!session.LocalReady&&!session.RaceReleased&&string.IsNullOrEmpty(session.ResultText),"Return retained readiness, draw, result or GO state.");
                foreach(var player in session.Players)Check(!player.Ready,"Return retained another player's Ready state.");
                Check(session.LocalSnapshot.sequence==0&&session.RemoteSnapshot.sequence==0&&session.SnapshotsSent==0&&session.RemoteSnapshotsReceived==0,"Return retained a prior race snapshot stream.");
                Check(!host.PauseMenu.IsOpen&&!host.RaceMusicMenu.IsOpen,"Return left another input overlay active.");
                Check(SaveFingerprint()==report.saveFingerprint,"Returning to connected lobby modified private saves.");
                yield return CaptureMenu("returned-connected-lobby");
                File.WriteAllText(Path.Combine(root,"returned-lobby-verified.json"),"{\"ready\":true}");
                yield return Until(()=>File.Exists(Path.Combine(peerRoot,"returned-lobby-verified.json")),20,"Peer did not verify its reset lobby before re-ready.");
                Phase("ready-second-race");yield return Until(()=>session.CanReady,15,"Returned lobby would not accept fresh readiness.");
                Check(menu.IsOpen,"Returned lobby must be open before the second automatic closure.");yield return Frames(3);session.SetReady(true);
                if(role=="host"){yield return Until(()=>session.CanStart,20,"Both returned drivers could not become Ready again.");session.StartRace();}
                yield return Until(()=>session.IsRacing,30,"Second native race did not load.");
                yield return Frames(2);var secondRenderedStart=ReadSourceStatus();
                secondLobbyAutoClosed=!menu.IsOpen&&secondRenderedStart.phase<=1&&secondRenderedStart.rivalRanges>0;
                Check(secondLobbyAutoClosed,"Returning and rendering another race did not automatically close the lobby before its showcase.");
                report.secondRaceId=SessionField<ulong>("raceId");Check(report.secondRaceId!=0&&report.secondRaceId!=report.firstRaceId,"Second race reused the prior race identity.");
                Check(ReferenceEquals(transport,SessionField<object>("transport"))&&session.HandshakeComplete,"Second race replaced the connected transport.");
                Phase("second-showcase-countdown");
                var phases=new List<uint>();var digits=new List<int>();uint previousPhase=0;int previousDigit=-1;
                ulong heldOwner=0,heldSolver=0;double startDeadline=Time.realtimeSinceStartupAsDouble+45;
                while(Time.realtimeSinceStartupAsDouble<startDeadline){
                    var source=ReadSourceStatus();
                    if(source.phase==0){yield return null;continue;}
                    if(previousPhase==0){Check(source.phase==1,"Second start did not begin at the first original showcase shot.");heldOwner=source.ownerTicks;heldSolver=source.simulationTicks;}
                    // Names animate within the two showcase shots; the current
                    // presentation has no separate phase 3 hold before countdown.
                    if(source.phase!=previousPhase){Check(source.phase==(previousPhase==2?4:previousPhase+1),"Second start skipped showcase/countdown order.");previousPhase=source.phase;phases.Add(source.phase);}
                    if(source.phase<=3)Check(source.ownerTicks==heldOwner&&source.simulationTicks==heldSolver&&source.countdownRemaining==240,"Second showcase advanced source physics or countdown early.");
                    if(source.phase==4&&source.countdownDigit>=1&&source.countdownDigit<=3&&source.countdownDigit!=previousDigit){
                        Check(previousDigit<0?source.countdownDigit==3:source.countdownDigit==previousDigit-1,"Second source countdown digit order.");previousDigit=source.countdownDigit;digits.Add(previousDigit);
                    }
                    if(source.phase==5){ulong elapsed=source.ownerTicks-heldOwner;Check(elapsed>=180&&elapsed<=195,"Second GO did not follow the original 180-tick countdown.");break;}
                    yield return null;
                }
                report.secondPhases=phases.ToArray();report.secondDigits=digits.ToArray();
                report.secondCountdown=previousPhase==5&&digits.Count==3&&digits[0]==3&&digits[1]==2&&digits[2]==1;
                Check(report.secondCountdown&&session.RaceReleased,"Second race did not complete source 3,2,1 and synchronized GO.");
                Phase("driving-second-race");driving=true;
                yield return Until(()=>session.LocalSnapshot.raceTicks>=300&&session.RemoteSnapshot.raceTicks>=300&&session.RemoteSnapshotsReceived>=50&&session.LocalSnapshot.speed>3&&session.RemoteSnapshot.speed>3,35,"Second race did not resume local/remote driving and snapshots.");
                PeerGeometry(out int peerRanges,out int mainRanges);report.secondLocalTicks=session.LocalSnapshot.raceTicks;report.secondRemoteTicks=session.RemoteSnapshot.raceTicks;report.secondReceived=session.RemoteSnapshotsReceived;report.secondSent=session.SnapshotsSent;
                report.secondDrive=peerRanges>0&&mainRanges>0&&(host.Status.flags&16u)!=0;
                Check(report.secondDrive,"Second race lost opponent geometry or original handling.");
                host.RaceMusic.Refresh();report.musicSelectionPreserved=host.RaceMusic.State.selectedIndex==report.selectedMusic;
                Check(report.musicSelectionPreserved,"Result/return flow changed the saved race music choice.");
                yield return CaptureWorld("returned-lobby-second-race");
                report.savesUnchanged=SaveFingerprint()==report.saveFingerprint;Check(report.savesUnchanged,"Second multiplayer race changed private saves.");
                File.WriteAllText(Path.Combine(root,"second-race-verified.json"),"{\"verified\":true}");
                yield return Until(()=>File.Exists(Path.Combine(peerRoot,"second-race-verified.json")),20,"Peer did not verify its second race.");
                if(showcaseCheck)yield return Idas3MultiplayerPresentationSmoke.VerifyWinnerAndReturn(host,session,root,role);
                report.passed=true;Phase("connected-return-verified");
            }finally{
                acceleratedReturnWait=false;driving=false;host.DiagnosticFocusOverride=priorFocus;
                report.phase=phase;report.checks=checks-firstCheck;File.WriteAllText(Path.Combine(root,"return-lobby-report.json"),JsonUtility.ToJson(report,true));
            }
        }
        private IEnumerator CheckConnectionDrop()
        {
            int firstCheck=checks;var report=new DisconnectReport{role=role};bool? previousFocus=host.DiagnosticFocusOverride;
            host.DiagnosticFocusOverride=true;driving=false;
            try{
                yield return Until(()=>File.Exists(Path.Combine(peerRoot,"race-verified.json")),35,"Peer did not finish its race checks before dropping connection.");
                Phase("disconnect-armed");report.saveFingerprint=SaveFingerprint();
                File.WriteAllText(Path.Combine(root,"disconnect-armed.json"),"{\"armed\":true}");
                yield return Until(()=>File.Exists(Path.Combine(peerRoot,"disconnect-armed.json")),20,"Peer did not arm disconnect save verification.");
                if(role=="host"){
                    var field=typeof(Idas3MultiplayerSession).GetField("transport",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
                    var transport=field?.GetValue(session) as IIdas3Transport;
                    Check(transport is Idas3TcpTransport,"Disconnect fixture must close an actual TCP transport.");
                    transport.Leave(); // Deliberately no session.LeaveRoom/application Leave packet.
                }
                Phase("awaiting-neutral-finish");
                yield return Until(()=>session.DisconnectedFinish&&(host.Status.flags&2048u)!=0,20,"Socket loss did not enter neutral FINISH.");
                peerLeft=true;Check(host.Status.racePhase==3&&(host.Status.flags&128u)!=0,"Native FINISH lost its multiplayer save barrier.");
                Check(menu.IsOpen&&menu.DisconnectedFinishVisible,"Neutral finish menu was not shown automatically.");menu.SetOpen(false);
                report.unhideable=menu.IsOpen&&menu.DisconnectedFinishVisible;Check(report.unhideable,"Neutral finish could be hidden without acknowledgement.");
                string result=session.ResultText.ToUpperInvariant();report.neutralResult=result.Contains("NO RESULT RECORDED")&&result.Contains("0 POINTS")&&!result.Contains("WIN")&&!result.Contains("LOSS")&&!result.Contains("DRAW")&&!result.Contains("TIME UP");
                Check(report.neutralResult,"Disconnect created a scored or win/loss result: "+result);
                var pose=ReadNativePose();var source=ReadSourceStatus();string identity=PoseIdentity(pose);ulong bridgeTicks=host.Status.simulationTicks;
                Check(pose.Finished&&!pose.TimeUp,"Native terminal pose contains a time-up or missing Finish.");
                double beganFreeze=Time.realtimeSinceStartupAsDouble;
                while(Time.realtimeSinceStartupAsDouble-beganFreeze<1.2){
                    yield return null;var current=ReadSourceStatus();
                    Check(host.Status.simulationTicks==bridgeTicks&&current.ownerTicks==source.ownerTicks&&current.simulationTicks==source.simulationTicks,"Terminal source/bridge clock advanced.");
                    Check(PoseIdentity(ReadNativePose())==identity,"Terminal car pose or wheels advanced.");
                    Check(session.DisconnectedFinish&&menu.IsOpen,"Terminal disappeared without acknowledgement.");
                }
                report.frozenSeconds=Time.realtimeSinceStartupAsDouble-beganFreeze;report.bridgeTicks=bridgeTicks;report.sourceTicks=source.ownerTicks;report.raceTicks=pose.raceTicks;
                Check(SaveFingerprint()==report.saveFingerprint,"Drop or terminal wait changed points/profiles/records.");
                Phase("capturing-neutral-finish");yield return CaptureDisconnectedFinish(report);
                yield return Until(()=>menu.CanAcknowledgeDisconnectedFinish,5,"Neutral input did not arm explicit acknowledgement.");
                Phase("acknowledging-neutral-finish");pulse=13;
                yield return Until(()=>!session.DisconnectedFinish&&(host.Status.flags&1u)!=0,10,"Normal Enter did not acknowledge FINISH.");yield return Frames(6);
                report.acknowledged=!session.IsRacing&&!session.InLobby&&!menu.IsOpen&&(host.Status.flags&(128u|2048u))==0;
                Check(report.acknowledged,"Acknowledgement left stale native/session finish state.");
                report.saveUnchanged=SaveFingerprint()==report.saveFingerprint;Check(report.saveUnchanged,"Acknowledgement changed private saves.");report.passed=true;
            }finally{
                report.phase=phase;report.checks=checks-firstCheck;File.WriteAllText(Path.Combine(root,"disconnect-report.json"),JsonUtility.ToJson(report,true));
                host.DiagnosticFocusOverride=previousFocus;
            }
        }
        private static bool DisconnectCardVisible(Texture2D image)
        {
            if(image==null)return false;var safe=Screen.safeArea;if(safe.width<=0||safe.height<=0)safe=new Rect(0,0,image.width,image.height);
            float scale=Mathf.Min(1.5f,Mathf.Min(safe.width/740f,safe.height/720f));float left=safe.x+(safe.width-700*scale)*.5f;
            float top=image.height-safe.y-168*scale;var pixels=image.GetPixels32();
            for(int offset=1;offset<=3;++offset)for(int flip=0;flip<2;++flip){
                int y=Mathf.RoundToInt(top+offset*scale);if(flip==0)y=image.height-1-y;y=Mathf.Clamp(y,0,image.height-1);int red=0,total=0;
                for(int logicalX=10;logicalX<690;logicalX+=5){int x=Mathf.Clamp(Mathf.RoundToInt(left+logicalX*scale),0,image.width-1);var p=pixels[y*image.width+x];++total;if(p.r>120&&p.g<100&&p.b<100&&p.r>2*p.g&&p.r>2*p.b)++red;}
                if(total>100&&red>total*.8f)return true;
            }
            return false;
        }
        private IEnumerator CaptureDisconnectedFinish(DisconnectReport report)
        {
            if(WorldOnly)yield break;
            int oldAa=QualitySettings.antiAliasing;Texture2D picture=null;
            try{
                QualitySettings.antiAliasing=0;yield return Frames(5);yield return new WaitForEndOfFrame();picture=ScreenCapture.CaptureScreenshotAsTexture();
                SaveCapture("disconnect-finish-window","AA1-normal-window-neutral-FINISH",picture,0);
                report.capture="disconnect-finish-window.png";report.windowCardVisible=DisconnectCardVisible(picture);
                Check(report.windowCardVisible&&menu.DiagnosticRepaints>0,"Actual window does not contain the neutral disconnect card.");
            }finally{if(picture!=null)Destroy(picture);QualitySettings.antiAliasing=oldAa;}
        }
        private IEnumerator QuickMatchRuntimeCheck()
        {
            session.ConfigureQuickMatchSmokeScope();session.OpenMenu();
            Check(session.Available,"Steam is unavailable for the Quick Match runtime check.");
            menu.SetOpen(true);yield return Frames(3);yield return CaptureMenu("quick-match-browser");
            session.QuickMatch();Check(session.IsQuickMatching,"Quick Match did not begin searching.");
            session.CancelQuickMatch();yield return new WaitForSecondsRealtime(1);
            Check(!session.IsQuickMatching&&!session.InLobby,"A cancelled search created or joined a room.");
            session.QuickMatch();Phase("quick-match-search");
            yield return Until(()=>session.IsQuickMatching&&session.InLobby&&session.IsHost,35,"Quick Match did not host after finding no compatible rooms.");
            roomCode=session.RoomCode;Phase("quick-match-waiting");
            if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-online-activity-check")>=0){
                yield return Until(()=>session.Activity.Available&&session.Activity.Queuing==1,45,"Isolated Steam presence did not count the queuing player.");
                Check(session.Activity.Online==1&&session.Activity.Racing==0,"Presence counted unrelated Spacewar players or duplicate memberships.");
                File.WriteAllText(Path.Combine(root,"activity-queuing.json"),JsonUtility.ToJson(session.Activity,true));
            }
            yield return new WaitForSecondsRealtime(6);
            Check(session.IsQuickMatching&&session.InLobby&&session.RoomCode==roomCode,"Waiting search lost or replaced its own room.");
            Check(!session.HandshakeComplete&&!session.IsRacing,"Isolated matchmaking unexpectedly admitted another player.");
            menuInputBlocked=menu.BlocksGameInput;Check(menuInputBlocked,"Quick Match menu did not capture input.");
            yield return CaptureMenu("quick-match-waiting");
            session.CancelQuickMatch();yield return Frames(12);
            Check(!session.IsQuickMatching&&!session.InLobby&&!session.IsRacing,"Cancel Search did not leave the hosted lobby.");
            if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-online-activity-check")>=0){
                yield return Until(()=>session.Activity.Available&&session.Activity.Online==1&&session.Activity.Queuing==0,45,"Leaving matchmaking did not update Steam presence.");
                File.WriteAllText(Path.Combine(root,"activity-online.json"),JsonUtility.ToJson(session.Activity,true));
            }
            Phase("quick-match-cancelled");yield return CaptureMenu("quick-match-cancelled");
            if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-online-activity-check")>=0){
                // The existing unique quick-smoke build key also isolates this
                // manual host room from every ordinary player's browser.
                session.HostRoom();yield return Until(()=>session.InLobby&&session.IsHost,30,"Manual lobby after Quick Match cancel failed.");
                yield return CaptureMenu("manual-lobby");session.LeaveRoom();yield return Frames(5);
                Check(!session.InLobby,"Manual test lobby did not close");
            }
            Finish(true,null);
        }
        private IEnumerator SteamRuntimeCheck()
        {
            Phase("steam-initialization");session.OpenMenu();
            yield return Until(()=>session.Available,20,"Official Steam runtime did not initialize in the Unity player.");
            Check(session.TransportIndex==0,"Steam check unexpectedly selected another transport.");
            session.HostRoom();
            yield return Until(()=>session.InLobby&&session.IsHost&&!string.IsNullOrEmpty(session.RoomCode),30,"Steam did not create this diagnostic's host lobby.");
            roomCode=session.RoomCode;
            if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-profile")>=0){
                var transport=(Idas3SteamTransport)typeof(Idas3MultiplayerSession).GetField("transport",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic).GetValue(session);
                var costs=transport.DiagnosticMembershipCost();
                File.WriteAllText(Path.Combine(root,"steam-membership-cost.txt"),string.Format(System.Globalization.CultureInfo.InvariantCulture,
                    "Own-room live Steam read-only query pair: {0:F6} ms/check; cached identity guard: {1:F6} ms/check; iterations: {2}. No second Steam account or Internet race.\n",costs[0],costs[1],costs[2]));
            }
            File.WriteAllText(Path.Combine(root,"room.json"),JsonUtility.ToJson(new RoomNotice{
                role=role,code=roomCode,phase="steam-own-room",port=0,course=session.Course,utc=DateTime.UtcNow.ToString("O")},true));
            Phase("steam-menu");menu.SetOpen(true);yield return Frames(4);
            menuInputBlocked=menu.BlocksGameInput;Check(menuInputBlocked,"Steam menu did not capture game input.");
            yield return CaptureMenu("lobby");yield return Frames(60);
            menu.SetOpen(false);session.LeaveRoom();yield return Frames(8);
            Check(!session.InLobby&&!session.IsRacing,"Steam diagnostic did not leave its own lobby.");
            Phase("steam-complete");Finish(true,null);
        }
        private void PeerGeometry(out int ranges,out int mainRanges)
        {
            ranges=mainRanges=0;var source=host.GetComponent<Idas3SceneRenderer>().CurrentFrame;
            for(int i=0;i<source.rangeCount;++i){
                int offset=checked(i*64);uint count=unchecked((uint)Marshal.ReadInt32(source.ranges,offset+4));
                uint flags=unchecked((uint)Marshal.ReadInt32(source.ranges,offset+28));
                uint scope=unchecked((uint)Marshal.ReadInt32(source.ranges,offset+36));
                uint mask=unchecked((uint)Marshal.ReadInt32(source.ranges,offset+40));
                if(count>0&&scope==3&&(flags&32)!=0){++ranges;if((mask&1)!=0)++mainRanges;}
            }
        }
        private bool MotionCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-motion-check")>=0;
        [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
        private static extern int Idas3MultiplayerMotionSample([Out] double[] values,uint count);
        private readonly List<double[]> motionSamples=new List<double[]>();
        private void RecordMotion(){
            if(!MotionCheck||!session.ExperimentalAuthority)return;
            var native=new double[26];
            if(Idas3MultiplayerMotionSample(native,26)!=1)return;
            var row=new double[47];row[0]=Time.realtimeSinceStartupAsDouble;Array.Copy(native,0,row,1,26);
            var camera=host.GetComponent<Camera>();var eye=camera.transform.position;var target=camera.transform.forward;
            var screen=camera.WorldToViewportPoint(new Vector3((float)native[7],(float)native[8],(float)native[9]));
            row[27]=eye.x;row[28]=eye.y;row[29]=eye.z;row[30]=target.x;row[31]=target.y;row[32]=target.z;
            row[33]=screen.x;row[34]=screen.y;row[35]=screen.z;row[36]=session.AuthorityStatus.contactFrames;
            row[37]=session.LocalSnapshot.yaw;row[38]=session.RemoteSnapshot.yaw;
            row[39]=host.NativeStepMilliseconds;row[40]=host.RendererMilliseconds;row[41]=host.UiMilliseconds;row[42]=host.NetworkMilliseconds;
            var renderer=host.GetComponent<Idas3SceneRenderer>();row[43]=renderer.GeometryUploadCount;row[44]=renderer.UploadedVertexCount;row[45]=renderer.ActiveMeshCount;row[46]=Time.unscaledDeltaTime*1000;
            motionSamples.Add(row);
        }
        private void WriteMotion(){
            if(!MotionCheck)return;
            using(var writer=new StreamWriter(Path.Combine(root,"motion.csv"))){
                writer.WriteLine("wall,frame,alpha,rollbacks,replayed,visual_x,visual_y,visual_z,body_x,body_y,body_z,raw_x,raw_y,raw_z,offset_x,offset_y,offset_z,previous_x,previous_y,previous_z,current_x,current_y,current_z,local_x,local_y,local_z,render_frame,eye_x,eye_y,eye_z,forward_x,forward_y,forward_z,viewport_x,viewport_y,viewport_z,contacts,local_yaw,remote_yaw,native_ms,renderer_ms,ui_ms,network_ms,uploads,vertices,meshes,frame_ms");
                foreach(var row in motionSamples){for(int i=0;i<row.Length;++i){if(i>0)writer.Write(',');writer.Write(row[i].ToString("R",System.Globalization.CultureInfo.InvariantCulture));}writer.WriteLine();}
            }
        }
        private void ObserveRace()
        {
            var local=session.LocalSnapshot;var remote=session.RemoteSnapshot;
            if(session.ExperimentalAuthority){lastAuthority=session.AuthorityStatus;if(lastAuthority.stalled!=0)++authorityStallFrames;}
            maxObservedPing=Math.Max(maxObservedPing,session.PingMilliseconds);
            if(releaseObservedAt<0){
                releaseObservedAt=Time.realtimeSinceStartupAsDouble;releaseObservedUtc=DateTime.UtcNow.ToString("O");
                releaseObservedUnixMilliseconds=DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                releaseLocalTick=local.raceTicks;releaseRemoteTick=remote.raceTicks;
            }
            if(!local.Valid((uint)(role=="host"?0:8))||!remote.Valid((uint)(role=="host"?8:0)))return;
            // The source countdown can hold raceTicks at zero after network
            // release. Measure the active race separately from that owner.
            if(local.raceTicks==0||remote.raceTicks==0)return;
            if(!raceObserved){raceObserved=true;firstLocalTick=local.raceTicks;firstRemoteTick=remote.raceTicks;
                firstPoseObservedAt=Time.realtimeSinceStartupAsDouble;firstPoseObservedUtc=DateTime.UtcNow.ToString("O");
                firstLocalPosition=local.actor;firstRemotePosition=remote.actor;lastProgressTick=local.raceTicks;}
            lastPoseObservedAt=Time.realtimeSinceStartupAsDouble;
            lastLocal=local;lastRemote=remote;
            localTravel=Mathf.Max(localTravel,Vector3.Distance(firstLocalPosition,local.actor));
            remoteTravel=Mathf.Max(remoteTravel,Vector3.Distance(firstRemotePosition,remote.actor));
            peakLocalSpeed=Mathf.Max(peakLocalSpeed,local.speed);peakRemoteSpeed=Mathf.Max(peakRemoteSpeed,remote.speed);
            greatestReceived=Math.Max(greatestReceived,session.RemoteSnapshotsReceived);greatestSent=Math.Max(greatestSent,session.SnapshotsSent);
            PeerGeometry(out int peerRanges,out int peerMainRanges);
            maxPeerRanges=Math.Max(maxPeerRanges,peerRanges);maxPeerMainRanges=Math.Max(maxPeerMainRanges,peerMainRanges);
            double remoteOffset=((long)remote.raceTicks-(long)local.raceTicks)/60.0;
            minObservedRemoteOffset=Math.Min(minObservedRemoteOffset,remoteOffset);maxObservedRemoteOffset=Math.Max(maxObservedRemoteOffset,remoteOffset);
            if(local.raceTicks>=lastProgressTick+30){
                lastProgressTick=local.raceTicks;progress.Add(new Progress{localTicks=local.raceTicks,remoteTicks=remote.raceTicks,
                    localPosition=local.actor,remotePosition=remote.actor,localSpeed=local.speed,remoteSpeed=remote.speed,
                    received=session.RemoteSnapshotsReceived,sent=session.SnapshotsSent,peerRanges=peerRanges,peerMainRanges=peerMainRanges,
                    wallSinceReleaseSeconds=lastPoseObservedAt-releaseObservedAt,
                    localSourceMinusWallSeconds=(local.raceTicks-releaseLocalTick)/60.0-(lastPoseObservedAt-releaseObservedAt),
                    remoteSnapshotMinusLocalSeconds=remoteOffset});
            }
        }
        [Serializable] private class ChallengerReport {public bool passed;public int announcements;public string role;public string scope="Private two-process matchmaking discovery over real TCP. Original challenge banner, background search/offline controls, cancellation, one notification and safe course/lobby transition. Steam internet discovery is not claimed.";}
        private IEnumerator CaptureChallenger(string name,int width=0,int height=0)
        {
            var overlay=host.GetComponent<Idas3ChallengerOverlay>();
            var camera=host.GetComponent<Camera>();var scene=host.GetComponent<Idas3SceneRenderer>();var ui=host.GetComponent<Idas3UnityUi>();
            var overlayCanvas=overlay.GetComponentInChildren<Canvas>();
            Check(overlayCanvas!=null&&overlayCanvas.worldCamera!=null,"Challenger canvas camera missing");
            Check((overlayCanvas.worldCamera.cullingMask&scene.MainCamera.cullingMask)==0,
                "Challenger camera redraws world geometry over the HUD");
            foreach(var graphic in overlay.GetComponentsInChildren<UnityEngine.UI.RawImage>(true))
                Check((scene.MainCamera.cullingMask&(1<<graphic.gameObject.layer))==0&&
                    (overlayCanvas.worldCamera.cullingMask&(1<<graphic.gameObject.layer))!=0,
                    "Challenger graphic is not isolated from world cameras");
            var previous=camera.targetTexture;
            var target=new RenderTexture(width>0?width:Screen.width,height>0?height:Screen.height,24,RenderTextureFormat.ARGB32){antiAliasing=1};
            Check(target.Create(),"Challenger capture target failed");camera.targetTexture=target;yield return Frames(2);
            Check(scene.CurrentFrame.width==target.width&&scene.CurrentFrame.height==target.height,"Native HUD matches challenger capture resolution");scene.ApplyFrame();ui.ApplyFrame();overlay.ApplyFrame();Canvas.ForceUpdateCanvases();
            if(overlay.SearchingVisible){
                var badge=Array.Find(overlay.GetComponentsInChildren<UnityEngine.UI.RawImage>(true),g=>g.name=="Accepting challengers");
                var corners=new Vector3[4];badge.rectTransform.GetWorldCorners(corners);
                Vector2 center=RectTransformUtility.WorldToScreenPoint(overlayCanvas.worldCamera,(corners[0]+corners[2])*.5f);
                float fit=Mathf.Min(target.width/640f,target.height/480f);
                float belowPanel=target.height-center.y-194f*fit;
                float aboveDial=center.y-161f*fit;
                Check(Mathf.Abs(belowPanel-aboveDial)<2,"Badge centered vertically between HUD groups at "+target.width+"x"+target.height);
                Check(Mathf.Abs(center.x-(target.width-80f*fit))<2,"Badge centered over right HUD column");
            }
            var cameras=new List<Camera>();
            foreach(var item in Resources.FindObjectsOfTypeAll<Camera>())if(item!=null&&item.enabled&&item.gameObject.activeInHierarchy&&item.targetTexture==target)cameras.Add(item);
            cameras.Sort((a,b)=>a.depth.CompareTo(b.depth));foreach(var item in cameras)item.Render();
            camera.targetTexture=previous;scene.ApplyFrame();ui.ApplyFrame();overlay.ApplyFrame();
            var old=RenderTexture.active;RenderTexture.active=target;
            var image=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);image.ReadPixels(new Rect(0,0,target.width,target.height),0,0);image.Apply();RenderTexture.active=old;
            File.WriteAllBytes(Path.Combine(root,name+".png"),image.EncodeToPNG());
            Destroy(image);target.Release();Destroy(target);yield return null;
        }
        private IEnumerator VerifyRuleNavigation(string name,Func<bool> value)
        {
            var field=typeof(Idas3MultiplayerMenu).GetField("controllerFocus",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
            var focus=(Idas3MenuFocus)field.GetValue(menu);string id="course-option:"+name;
            menu.ProcessMenuNavigation(0,0,false,false,false,Time.realtimeSinceStartupAsDouble);
            for(int i=0;i<40&&focus.Selected!=id;++i){
                menu.ProcessMenuNavigation(0,1,false,false,false,Time.realtimeSinceStartupAsDouble);
                menu.ProcessMenuNavigation(0,0,false,false,false,Time.realtimeSinceStartupAsDouble);
                yield return null;
            }
            Check(focus.Selected==id,"Controller cannot reach "+name);
            for(int i=0;i<2;++i){
                bool before=value();
                menu.ProcessMenuNavigation(0,0,true,false,false,Time.realtimeSinceStartupAsDouble);
                yield return Until(()=>value()!=before,3,"Controller did not toggle "+name);
                menu.ProcessMenuNavigation(0,0,false,false,false,Time.realtimeSinceStartupAsDouble);
                yield return Frames(2);Check(focus.Selected==id,"Changing "+name+" lost controller focus");
            }
        }
        private IEnumerator CaptureMenu(string name)
        {
            // Hidden-window transport matrix does not claim an IMGUI pixel check.
            // Game cameras are still rendered and validated below.
            if(WorldOnly)yield break;
            int oldAa=QualitySettings.antiAliasing;QualitySettings.antiAliasing=0;
            yield return Frames(5);yield return new WaitForEndOfFrame();
            var picture=ScreenCapture.CaptureScreenshotAsTexture();
            if(picture!=null&&MenuHeaderVisible(picture)){
                SaveCapture(name,"AA1-normal-window-menu",picture,0);Destroy(picture);QualitySettings.antiAliasing=oldAa;
                Check(menu.DiagnosticRepaints>0,"No actual menu Repaint occurred.");yield break;
            }
            if(picture!=null){SaveCapture(name+"-screen-aa1","AA1-screen-attempt",picture,0);Destroy(picture);}
            // Build the actual background with the already verified manual
            // camera path, then let the unchanged menu draw into this target
            // during its own real OnGUI Repaint. Readback happens afterward.
            var camera=host.GetComponent<Camera>();var scene=host.GetComponent<Idas3SceneRenderer>();var ui=host.GetComponent<Idas3UnityUi>();
            var previous=camera.targetTexture;
            var target=new RenderTexture(Screen.width,Screen.height,24,RenderTextureFormat.ARGB32){name="Actual multiplayer OnGUI capture",antiAliasing=1};
            Check(target.Create(),"Menu diagnostic target failed.");camera.targetTexture=target;scene.ApplyFrame();ui.ApplyFrame();
            var cameras=new List<Camera>();
            foreach(var item in Resources.FindObjectsOfTypeAll<Camera>())
                if(item!=null&&item.enabled&&item.gameObject.activeInHierarchy&&item.targetTexture==target)cameras.Add(item);
            cameras.Sort((a,b)=>{int order=a.depth.CompareTo(b.depth);return order!=0?order:string.CompareOrdinal(a.name,b.name);});
            foreach(var item in cameras)item.Render();
            camera.targetTexture=previous;scene.ApplyFrame();ui.ApplyFrame();
            menu.RequestDiagnosticCapture(target);
            yield return Until(()=>menu.DiagnosticCaptureReady,5,"No actual menu Repaint reached the diagnostic target; use a visible game window.");
            yield return new WaitForEndOfFrame();
            var oldActive=RenderTexture.active;RenderTexture.active=target;
            picture=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);
            picture.ReadPixels(new Rect(0,0,target.width,target.height),0,0);picture.Apply();RenderTexture.active=oldActive;
            bool visible=MenuHeaderVisible(picture);
            SaveCapture(name,"AA1-real-OnGUI-repaint",picture,cameras.Count);Destroy(picture);
            menu.CancelDiagnosticCapture();target.Release();Destroy(target);QualitySettings.antiAliasing=oldAa;
            Check(visible,"Captured image does not contain the actual F1 menu header.");
        }
        private IEnumerator CaptureUnobstructedShowcase()
        {
            int oldAa=QualitySettings.antiAliasing;Texture2D picture=null;
            try{
                QualitySettings.antiAliasing=0;yield return Frames(5);yield return new WaitForEndOfFrame();
                var source=ReadSourceStatus();
                Check(!menu.IsOpen&&source.phase==1&&source.playerRanges>0&&source.rivalRanges>0,"Showcase capture lost its first shot or still has the lobby open.");
                picture=ScreenCapture.CaptureScreenshotAsTexture();
                SaveCapture("lobby-autoclose-showcase","AA1-normal-window-showcase-no-lobby",picture,0);
                Check(shots[shots.Count-1].screenCaptureVisible&&!MenuHeaderVisible(picture),"Actual showcase window is blank or covered by the lobby header.");
            }finally{if(picture!=null)Destroy(picture);QualitySettings.antiAliasing=oldAa;}
        }
        private static bool MenuHeaderVisible(Texture2D picture)
        {
            float scale=Mathf.Min(1.5f,Mathf.Min(picture.width/940f,picture.height/652f));
            float left=(picture.width-900*scale)*.5f,top=(picture.height-620*scale)*.5f;
            int y=Mathf.Clamp(picture.height-1-Mathf.RoundToInt(top+2*scale),0,picture.height-1);
            int accent=0,total=0;
            for(int x=Mathf.RoundToInt(left+10*scale);x<left+890*scale;x+=4){
                Color32 pixel=picture.GetPixel(x,y);++total;if(pixel.r>150&&pixel.g<90&&pixel.b<105)++accent;
            }
            return total>0&&accent>total*.7f;
        }
        private IEnumerator CaptureWorld(string name)
        {
            frozen=true;
            var camera=host.GetComponent<Camera>();var scene=host.GetComponent<Idas3SceneRenderer>();var ui=host.GetComponent<Idas3UnityUi>();
            var previous=camera.targetTexture;
            var target=new RenderTexture(1200,720,24,RenderTextureFormat.ARGB32){name="Multiplayer smoke capture",antiAliasing=1};
            Check(target.Create(),"Multiplayer screenshot target failed.");camera.targetTexture=target;scene.ApplyFrame();ui.ApplyFrame();
            yield return new WaitForEndOfFrame();
            var cameras=new List<Camera>();
            foreach(var item in Resources.FindObjectsOfTypeAll<Camera>())
                if(item!=null&&item.enabled&&item.gameObject.activeInHierarchy&&item.targetTexture==target)cameras.Add(item);
            cameras.Sort((a,b)=>{int order=a.depth.CompareTo(b.depth);return order!=0?order:string.CompareOrdinal(a.name,b.name);});
            Check(cameras.Count>0&&(cameras[0].clearFlags==CameraClearFlags.SolidColor||cameras[0].clearFlags==CameraClearFlags.Skybox),"Manual capture stack has no initial clear.");
            foreach(var item in cameras)item.Render();
            var oldActive=RenderTexture.active;RenderTexture.active=target;
            var picture=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);
            picture.ReadPixels(new Rect(0,0,target.width,target.height),0,0);picture.Apply();RenderTexture.active=oldActive;
            SaveCapture(name,"AA1-manual-world",picture,cameras.Count);Destroy(picture);
            camera.targetTexture=previous;scene.ApplyFrame();ui.ApplyFrame();target.Release();Destroy(target);frozen=false;
            yield return null;
        }
        private void SaveCapture(string name,string kind,Texture2D picture,int cameraCount)
        {
            Check(picture!=null&&picture.width>0,"Multiplayer screenshot returned no image.");
            int visible=0;foreach(var pixel in picture.GetPixels32())if(Mathf.Max(pixel.r,Mathf.Max(pixel.g,pixel.b))>24)++visible;
            PeerGeometry(out int peerRanges,out int peerMainRanges);
            var shot=new Shot{name=name,kind=kind,width=picture.width,height=picture.height,visiblePixels=visible,
                screenCaptureVisible=visible>picture.width*picture.height/1000,manualCameras=cameraCount,peerRanges=peerRanges,peerMainRanges=peerMainRanges,
                menuRepaints=menu.DiagnosticRepaints,menuHeaderVisible=menu.IsOpen&&MenuHeaderVisible(picture),
                menuOpen=menu.IsOpen,snapshotsReceived=session.RemoteSnapshotsReceived,snapshotsSent=session.SnapshotsSent,
                local=session.LocalSnapshot,remote=session.RemoteSnapshot};
            File.WriteAllBytes(Path.Combine(root,name+".png"),picture.EncodeToPNG());shots.Add(shot);
            File.WriteAllText(Path.Combine(root,name+".json"),JsonUtility.ToJson(shot,true));
            if(cameraCount>0)Check(shot.screenCaptureVisible,"Multiplayer world screenshot is black.");
        }
        private Report BuildReport(bool passed,string error,bool stopped)
        {
            var report=new Report{passed=passed,error=error,role=role,phase=phase,shutdownComplete=stopped,peerLeft=peerLeft,
                lobbyAutoClosed=lobbyAutoClosed,liveMenuReopened=liveMenuReopened,secondLobbyAutoClosed=secondLobbyAutoClosed,
                menuInputBlocked=menuInputBlocked,checks=checks,course=course,roomCode=roomCode,unityVersion=Application.unityVersion,device=SystemInfo.graphicsDeviceName,
                steamRuntimeCheck=steamCheck,quickMatchRuntimeCheck=quickCheck,releaseObservedUtc=releaseObservedUtc,firstPoseObservedUtc=firstPoseObservedUtc,
                releaseObservedUnixMilliseconds=releaseObservedUnixMilliseconds,releaseObservedRealtime=releaseObservedAt,
                releaseLocalTick=releaseLocalTick,releaseRemoteTick=releaseRemoteTick,firstLocalTick=firstLocalTick,firstRemoteTick=firstRemoteTick,
                firstPoseObservedRealtime=firstPoseObservedAt,observedRaceWallSeconds=raceObserved?lastPoseObservedAt-firstPoseObservedAt:0,
                localSourceSeconds=raceObserved?(lastLocal.raceTicks-firstLocalTick)/60.0:0,
                remoteSourceSeconds=raceObserved?(lastRemote.raceTicks-firstRemoteTick)/60.0:0,
                localSourceMinusWallSeconds=raceObserved?(lastLocal.raceTicks-firstLocalTick)/60.0-(lastPoseObservedAt-firstPoseObservedAt):0,
                minRemoteSnapshotMinusLocalSeconds=raceObserved?minObservedRemoteOffset:0,maxRemoteSnapshotMinusLocalSeconds=raceObserved?maxObservedRemoteOffset:0,
                authorityEnabled=session.ExperimentalAuthority,boostEnabled=session.BoostEnabled,collisionsEnabled=session.CollisionsEnabled,menuPixelsChecked=!WorldOnly,mirrorInputFixture=MirrorCheck,authority=lastAuthority,authorityStallFrames=authorityStallFrames,maxObservedPing=maxObservedPing,
                configuredRttMs=session.DiagnosticRttMs,configuredJitterMs=session.DiagnosticJitterMs,configuredLossPercent=session.DiagnosticLossPercent,droppedPackets=session.DiagnosticDroppedPackets,
                elapsedSeconds=Time.realtimeSinceStartupAsDouble-began,maxPeerRanges=maxPeerRanges,maxPeerMainRanges=maxPeerMainRanges,
                localRaceTicksObserved=raceObserved?lastLocal.raceTicks-firstLocalTick:0,remoteRaceTicksObserved=raceObserved?lastRemote.raceTicks-firstRemoteTick:0,
                snapshotsReceived=greatestReceived,snapshotsSent=greatestSent,localTravel=localTravel,remoteTravel=remoteTravel,
                peakLocalSpeed=peakLocalSpeed,peakRemoteSpeed=peakRemoteSpeed,finalLocal=lastLocal,finalRemote=lastRemote,captures=shots.ToArray(),trajectory=progress.ToArray()};
            if(WorldOnly)report.scope="Two real Unity players using normal TCP/session APIs and " +
                (session.ExperimentalAuthority?"shared native authority with verified peer digests":"interpolated remote poses without shared authority") +
                ", isolated profiles and configured outgoing packet impairment. Game world cameras render to diagnostic targets; hidden-window IMGUI pixel verification is explicitly omitted. Race clocks, car geometry, lobby state, and disconnect/return checks are exercised as requested. This is not a two-account Steam internet test.";
            if(MirrorCheck)report.scope+=" Private input fixture delays the guest throttle for 60 source ticks and holds steering neutral to put the opponent behind the host; mirror visibility requires reviewing the captures.";
            if(ContactMotionCheck)report.scope+=" Thirty-second private braking/merge fixture requires actual native car contact. Motion sampled at end of rendered frame with camera and projected opponent-body coordinates; this does not establish display-panel ghosting.";
            if(steamCheck)report.scope="Official Steam transport initialized inside the real Unity player, created its own host lobby, rendered the actual F1 menu and left its own lobby. No other rooms or contacts were joined. This validates Steam runtime binding and lobby/menu lifecycle, not a second Steam peer or internet relay performance.";
            if(quickCheck)report.scope="Actual Unity Quick Match with real Steam and a unique diagnostic build key: cancel in-flight discovery, restart, empty-search host fallback, periodic waiting-room discovery, actual F1 browser/waiting/cancel screenshots and hosted cancellation. No other player's room can match the diagnostic key. This does not prove an online match between two Steam accounts.";
            return report;
        }
        private void Finish(bool passed,string error)
        {
            if(finished)return;WriteMotion();finished=true;frozen=true;driving=false;
            if(passed)try{Idas3MultiplayerPresentationSmoke.RequirePassed();}catch(Exception failure){passed=false;error=failure.ToString();}
            if(passed)try{Idas3MultiplayerHudSmoke.RequirePassed();}catch(Exception failure){passed=false;error=failure.ToString();}
            bool stopped=false;
            try{session.LeaveRoom();host.StopNative();stopped=!host.Ready&&Idas3Native.ReadStatus().state==0;}
            catch(Exception e){error=(error??"")+"\nShutdown: "+e.Message;passed=false;}
            if(!stopped)passed=false;
            host.DiagnosticFocusOverride=priorDiagnosticFocus;
            File.WriteAllText(Path.Combine(root,"report.json"),JsonUtility.ToJson(BuildReport(passed,error,stopped),true));
            Debug.Log((passed?"PASS":"FAIL")+" multiplayer "+role+": "+error);
#if UNITY_EDITOR
            UnityEditor.EditorApplication.isPlaying=false;
#else
            Application.Quit(passed?0:1);
#endif
        }
    }
}

