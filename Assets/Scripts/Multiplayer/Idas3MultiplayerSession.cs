using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;

namespace Idas3.Multiplayer
{
    public readonly struct Idas3RaceChoice : IEquatable<Idas3RaceChoice>
    {
        public readonly int Course;
        public readonly bool Reverse,Wet,Night;
        public Idas3RaceChoice(int course,bool reverse,bool wet,bool night) {
            if(course<0||course>11)throw new ArgumentOutOfRangeException(nameof(course));
            Course=course;Reverse=reverse;Wet=wet||course==8;Night=night||course==4||course==8||course==11;
        }
        public bool Equals(Idas3RaceChoice other)=>Course==other.Course&&Reverse==other.Reverse&&Wet==other.Wet&&Night==other.Night;
        public override bool Equals(object other)=>other is Idas3RaceChoice choice&&Equals(choice);
        public override int GetHashCode()=>Course|(Reverse?16:0)|(Wet?32:0)|(Night?64:0);
    }

    public sealed class Idas3PlayerInfo
    {
        public string Name, CarName;
        public bool Connected, Ready, IsLocal, IsHost;
        public int PingMilliseconds;
    }

    // All transport callbacks, protocol transitions and native calls run on the
    // Unity main thread. Both cars share the same verified input simulation.
    public sealed class Idas3MultiplayerSession : IDisposable
    {
        const uint Magic = 0x504D3349;
        const ushort Protocol = 9;
        enum Packet : byte { Hello=1, Lobby, Player, Load, Loaded, Release, ReleaseAck, Pose, Ping, Pong, Finish, Results, Leave, ReturnRequest, ReturnLobby, ReturnAck, RecordUpdate, AuthorityInputs }
        // Retain this diagnostic property name; shared simulation is now the
        // normal online path, including rooms with car collisions switched off.
        public bool ExperimentalAuthority { get; } = true;
        public Idas3AuthorityStatus AuthorityStatus { get; private set; }
        readonly byte[] authorityPacket=new byte[1064];
        ulong authorityPacketFrame;
        int pendingAuthorityWinner=-3;
        Idas3BattleRecord? pendingAuthorityRecord;
        ulong pendingAuthorityHostTicks,pendingAuthorityClientTicks;
        IIdas3Transport transport;
        readonly Idas3NetworkImpairment impairment;
        public double DiagnosticRttMs=>impairment.RttMs;
        public double DiagnosticJitterMs=>impairment.JitterMs;
        public double DiagnosticLossPercent=>impairment.LossPercent;
        public long DiagnosticDroppedPackets=>impairment.Dropped;
        Idas3QuickMatch quickMatch;
        string matchmakingTestScope = "";
        readonly Idas3PlayerInfo[] players = { new Idas3PlayerInfo(), new Idas3PlayerInfo() };
        readonly List<Idas3Room> emptyRooms = new List<Idas3Room>();
        ulong localNonce, remoteNonce, raceId;
        ulong returnRaceId;
        uint returnRevision;
        readonly HashSet<ulong> retiredRaceIds = new HashSet<ulong>();
        uint revision,localPlayerSerial,remotePlayerSerial,localSelectionSerial,acknowledgedLocalPlayerSerial;
        Idas3RaceChoice selectedChoice;
        int remoteCar = 8;
        readonly List<Idas3OnlineCar> garage=new List<Idas3OnlineCar>();
        public IReadOnlyList<Idas3OnlineCar> Garage=>garage;
        public Idas3OnlineCar LocalSavedCar {get;private set;}
        public Idas3OnlineCar RemoteSavedCar {get;private set;}
        public void RefreshGarage(){
            if(nativeRace||HasCourseDraw||Busy)return;
            garage.Clear();
            Idas3OnlineCar ReadCar(int id){
                var item=new Idas3OnlineCar{Selection=id};
                int found=Idas3MultiplayerNative.Idas3MultiplayerReadCar(id,item.Words,307);
                if(found<0)throw new InvalidOperationException(Idas3Native.Error());
                if(found==0)return null;
                item.Saved=found==1;item.Automatic=item.Words[17]==0;
                if(LocalSavedCar?.Selection==id)item.Automatic=LocalSavedCar.Automatic;
                return item;
            }
            for(int slot=0;slot<5;++slot){
                var primary=ReadCar(slot);if(primary==null)continue;
                garage.Add(primary);
                // A slot may contain other driven cars as well as the car on
                // its file-screen card. Never substitute the legacy directory.
                for(int car=0;car<35;++car){
                    if(car==primary.Car)continue;
                    var item=ReadCar(40+slot*35+car);if(item!=null)garage.Add(item);
                }
            }
            for(int car=0;car<35;++car){
                var item=ReadCar(5+car);if(item==null)continue;
                // Distinct saved builds stay selectable, even for the same
                // model. Only redundant stock fallback entries are hidden.
                if(!item.Saved&&garage.Exists(c=>c.Car==item.Car))continue;
                garage.Add(item);
            }
            int preferred=LocalSavedCar?.Selection??Idas3MultiplayerNative.Idas3MultiplayerCurrentCar();
            var chosen=garage.Find(c=>c.Selection==preferred)??garage.Find(c=>c.Saved&&c.Car==LocalCar)??garage.Find(c=>c.Saved)??garage.Find(c=>c.Car==LocalCar);
            if(LocalSavedCar==null&&chosen!=null&&chosen.SaveSlot<0)
                chosen=garage.Find(c=>c.SaveSlot>=0&&c.Car==LocalCar)??garage.Find(c=>c.SaveSlot>=0)??chosen;
            if(chosen!=null)SelectSavedCar(chosen);
        }
        public void SetAutomatic(bool automatic){
            if(nativeRace||HasCourseDraw||Busy||LocalSavedCar==null||LocalSavedCar.Automatic==automatic)return;
            LocalSavedCar.Automatic=automatic;localReady=remoteReady=false;++localPlayerSerial;localSelectionSerial=localPlayerSerial;
            if(!HandshakeComplete)return;
            if(IsHost){++revision;SendLobby();}else SendPlayer();
        }
        public void CycleCar(int direction){
            if(garage.Count==0)RefreshGarage();
            if(garage.Count==0)return;
            int index=garage.FindIndex(c=>c.Selection==LocalSavedCar?.Selection);
            SelectSavedCar(garage[(Math.Max(0,index)+direction+garage.Count)%garage.Count]);
        }
        public void SelectSavedCar(Idas3OnlineCar car){
            if(nativeRace||HasCourseDraw||Busy||car==null||!garage.Contains(car))return;
            if(LocalSavedCar!=null&&LocalSavedCar.Same(car,true))return;
            LocalSavedCar=car;LocalCar=car.Car;localReady=remoteReady=false;++localPlayerSerial;localSelectionSerial=localPlayerSerial;
            if(!HandshakeComplete)return;
            if(IsHost){++revision;SendLobby();}else SendPlayer();
        }
        readonly Idas3MultiplayerRecords records;
        Idas3BattleRecord remoteRecord=Idas3BattleRecord.Fresh;
        bool recordCommitted;
        public Idas3BattleRecord LocalRecord => records.Read(LocalCar);
        public Idas3BattleRecord RemoteRecord => remoteRecord;
        public Idas3BattleRecord RaceLocalRecord { get; private set; }
        public Idas3BattleRecord RaceRemoteRecord { get; private set; }
        public ulong CurrentRaceId => raceId;
        public int RemoteCar => remoteCar;
        string remoteName = "Other driver", compatibility;
        bool localReady, remoteReady, nativeRace, remoteLoaded, releaseAck, disposing, leaving, helloSent, finishSent, resultSent, polling;
        bool remoteFinished, remoteTimeUp;
        ulong remoteFinishTicks;
        double lastHeard, lastPing, lastPose, releaseAt, operationAt, peerOffset, bestRtt = double.MaxValue;
        double lastPingStamp, previousPoseAt, newestPoseAt;
        Idas3CarSnapshot previousPose;
        readonly Idas3CarSnapshot[] poseBuffer = new Idas3CarSnapshot[8];
        readonly double[] poseTimes = new double[8];
        int poseCount;
        readonly Dictionary<double, byte> pendingPings = new Dictionary<double, byte>();
        string status = "Press Host to open a two-driver room.";
        string state = "Offline";
        Exception terminalFailure;
        public bool Available => transport != null && transport.Available;
        public bool InLobby => transport != null && !string.IsNullOrEmpty(transport.RoomCode);
        public bool IsHost => transport != null && transport.IsHost;
        public bool IsRacing => nativeRace;
        public bool IsQuickMatching => quickMatch != null && quickMatch.IsActive;
        bool matchAnnounced;
        public bool ChallengerPending { get; private set; }
        public event Action ChallengerFound;
        public void CompleteChallengerPresentation(){ChallengerPending=false;}
        public bool Busy => IsQuickMatching || state == "Connecting" || state == "Loading" || state == "Returning" || (transport is Idas3SteamTransport steam && steam.IsBusy);
        public bool LocalReady => localReady;
        public bool CanReady => HandshakeComplete&&matchAnnounced&&!ChallengerPending&&!nativeRace&&!HasCourseDraw&&!Busy&&
            (IsHost||acknowledgedLocalPlayerSerial>=localSelectionSerial);
        public bool CanStart => IsHost && HandshakeComplete && localReady && remoteReady && !nativeRace && !HasCourseDraw && state=="Lobby" && bestRtt < 2;
        public bool HandshakeComplete { get; private set; }
        public bool RaceReleased { get; private set; }
        public string StateName => state;
        public string StatusText => IsQuickMatching ? quickMatch.Status : state=="Returning" || !string.IsNullOrEmpty(ErrorText) ? status : Busy && transport != null ? transport.Status : status;
        public string ErrorText { get; private set; } = "";
        public string RoomCode => transport?.RoomCode ?? "";
        public string TransportName => transport?.Kind ?? "Steam";
        public bool ActivityRequested {get;set;}
        public Idas3OnlineActivity Activity=>transport is Idas3SteamTransport steam?steam.Activity:default;
        public string LobbyName=>Idas3LobbyNames.ForHost(IsHost?transport?.LocalName:remoteName);
        public string ResultText { get; private set; } = "";
        public bool DisconnectedFinish { get; private set; }
        public event Action RaceDisconnected;
        public bool CanReturnToLobby => nativeRace && !DisconnectedFinish && state=="Results" && HandshakeComplete && transport!=null && transport.Connected;
        public event Action ReturnedToLobby;
        public string CountdownText => nativeRace && !RaceReleased && !DisconnectedFinish && state!="Returning" ? "SYNCHRONIZING START" : "";
        public int PingMilliseconds { get; private set; } = -1;
        readonly Queue<double> recentRtt=new Queue<double>();
        public double RecentRttMilliseconds { get; private set; }
        public double JitterMilliseconds { get; private set; }
        public string ConnectionQuality => recentRtt.Count<3?"Measuring connection":RecentRttMilliseconds>180||JitterMilliseconds>40?"High latency — contact corrections may be noticeable":RecentRttMilliseconds>100?"Moderate latency":"Low latency";

        public Idas3RaceChoice LocalChoice { get; private set; } = new Idas3RaceChoice(3,false,false,true);
        public Idas3RaceChoice RemoteChoice { get; private set; } = new Idas3RaceChoice(3,false,false,true);
        public int CourseWinnerSlot { get; private set; } = -1;
        public bool HasCourseDraw => CourseWinnerSlot>=0;
        public Idas3RaceChoice SelectedChoice => HasCourseDraw?selectedChoice:LocalChoice;
        public string CourseDrawText => HasCourseDraw?"RANDOM SELECTION: "+(CourseWinnerSlot==(IsHost?0:1)?Clean(transport?.LocalName):remoteName)+"'S PICK":"Each driver's course pick has a 50% chance.";
        public int Course => SelectedChoice.Course;
        public int LocalCar { get; private set; }
        public int TransportIndex { get; private set; }
        public bool Reverse => SelectedChoice.Reverse;
        public bool Wet => SelectedChoice.Wet;
        public bool Night => SelectedChoice.Night;
        public bool BoostEnabled { get; private set; } = true;
        public bool CollisionsEnabled { get; private set; } = true;
        public void SetCollisions(bool enabled) {
            if(!IsHost||nativeRace||HasCourseDraw||Busy||CollisionsEnabled==enabled)return;
            CollisionsEnabled=enabled;localReady=remoteReady=false;++revision;
            if(HandshakeComplete)SendLobby();
        }
        public void SetBoost(bool enabled) {
            if(!IsHost||nativeRace||HasCourseDraw||Busy||BoostEnabled==enabled)return;
            BoostEnabled=enabled;localReady=remoteReady=false;++revision;
            if(HandshakeComplete)SendLobby();
        }
        public long RemoteSnapshotsReceived { get; private set; }
        public long SnapshotsSent { get; private set; }
        public Idas3CarSnapshot LocalSnapshot { get; private set; }
        public Idas3CarSnapshot RemoteSnapshot { get; private set; }
        public IReadOnlyList<Idas3Room> Rooms => transport?.Rooms ?? emptyRooms;
        public bool EnnaAvailable { get; } = File.Exists(Path.Combine(Application.streamingAssetsPath,"ENNA/menu.idastex"));
        public int AvailableCourseCount=>EnnaAvailable?12:11;
        public IReadOnlyList<Idas3PlayerInfo> Players { get { UpdatePlayers(); return players; } }
        public static readonly string[] CarNames = {
            "AE86 TRUENO","AE86 LEVIN","AE85 LEVIN","SW20 MR2","ZZW30 MR-S","SXE10 ALTEZZA","ST205 CELICA",
            "BNR32 SKYLINE","BNR34 SKYLINE","S13 SILVIA","S14 SILVIA Q's","S14 SILVIA K's","S15 SILVIA",
            "RPS13 180SX","RPS13 SILEIGHTY","EK9 CIVIC","EG6 CIVIC","DC2 INTEGRA","AP1 S2000",
            "CE9A LANCER","CN9A LANCER","CT9A LANCER","FD3S RX-7 Type R","FD3S RX-7 Spirit R","FC3S RX-7",
            "NA6CE ROADSTER","NB8C ROADSTER","GC8 IMPREZA","GDB IMPREZA","GC8 IMPREZA Type R",
            "EA11R CAPPUCCINO","ER34 SKYLINE","CP9A LANCER V","CP9A LANCER VI","SE3P RX-8"
        };
        static double Now => Time.realtimeSinceStartupAsDouble;
        static ulong Nonce() { var b = new byte[8]; using (var r = RandomNumberGenerator.Create()) r.GetBytes(b); return BitConverter.ToUInt64(b,0) | 1UL; }
        // One unbiased bit per accepted start. Do not use Nonce(), whose low
        // bit is deliberately forced to 1, or redraw independently on peers.
        static int ChooseCourseSlot() { var b=new byte[1];using(var r=RandomNumberGenerator.Create())r.GetBytes(b);return b[0]&1; }
        public Idas3MultiplayerSession(int selectedCar=0,string saveRoot=null) {
            impairment=new Idas3NetworkImpairment(()=>Now);
            LocalCar=Mathf.Clamp(selectedCar,0,34);
            records=new Idas3MultiplayerRecords(saveRoot??Path.Combine(Application.persistentDataPath,"userdata-unity-scene"));
        }
        public void OpenMenu()
        {
            if(DisconnectedFinish)return;
            RefreshGarage();InitializeOnlinePresence();
        }
        public void InitializeOnlinePresence()
        {
            if(DisconnectedFinish)return;
            if (transport == null) SetTransport(TransportIndex == 0 ? (IIdas3Transport)new Idas3SteamTransport() : new Idas3TcpTransport());
            if (transport is IIdas3MatchmakingTransport matchmaking) matchmaking.BuildCompatibility=Compatibility()+matchmakingTestScope;
            if (!Available) { ClearError(); transport.Initialize(); }
        }
        void SetTransport(IIdas3Transport next)
        {
            if (transport != null) { LeaveRoom(); transport.Message -= Receive; transport.PeerChanged -= PeerChanged; transport.Error -= Fail; transport.Dispose(); }
            impairment.Clear();transport = next; transport.Message += Receive; transport.PeerChanged += PeerChanged; transport.Error += Fail;
            state = "Offline"; localNonce = Nonce();
        }
        public void SelectTransport(int index)
        {
            if (nativeRace || DisconnectedFinish || InLobby || Busy || index < 0 || index > 1) return;
            if (TransportIndex == index && Available) return;
            TransportIndex = index;
            SetTransport(index == 0 ? (IIdas3Transport)new Idas3SteamTransport() : new Idas3TcpTransport()); OpenMenu();
        }
        public void ConfigureLocalTest(int port, string name)
        {
            TransportIndex = 1;
            SetTransport(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-challenger-check")>=0?
                (IIdas3Transport)new Idas3ChallengerTestTransport(port,name):new Idas3TcpTransport(port,true,name)); OpenMenu();
        }
        public void HostRoom()
        {
            OpenMenu(); if (nativeRace || DisconnectedFinish || !Available || Busy || InLobby) return;
            ResetPeer(); localNonce = Nonce(); ClearError(); state = "Connecting"; operationAt=Now;
            transport.Host(Idas3LobbyNames.ForHost(transport.LocalName));
            if (IsHost) { state="Lobby"; status="Share the room code. Both drivers must select Ready before the host starts."; }
        }
        public void JoinRoom(string code)
        {
            OpenMenu(); if (nativeRace || DisconnectedFinish || !Available || Busy || InLobby) return;
            ResetPeer(); localNonce=Nonce(); ClearError(); state="Connecting"; operationAt=Now; transport.Join(code);
        }
        public void RefreshRooms() { OpenMenu(); if (!nativeRace && !DisconnectedFinish && Available && !InLobby && !Busy) { ClearError(); transport.Browse(); } }
        public void QuickMatch()
        {
            OpenMenu();if(nativeRace||DisconnectedFinish||!Available||Busy||InLobby||!(transport is IIdas3MatchmakingTransport matchmaking))return;
            ResetPeer();localNonce=Nonce();ClearError();state="Matching";ResultText="";
            quickMatch=new Idas3QuickMatch(matchmaking,()=>Now,()=>UnityEngine.Random.value,Idas3LobbyNames.ForHost(transport.LocalName));
            quickMatch.Failed+=Fail;
            quickMatch.Matched+=()=>{state="Lobby";status="Opponent found. Choose your car and course, then Ready. One driver's course is selected randomly.";};
            quickMatch.Start();
        }
        public void CancelQuickMatch()
        {
            if(!IsQuickMatching)return;LeaveRoom();status="Quick Match cancelled.";
        }
        internal void ConfigureQuickMatchSmokeScope()
        {
            if(InLobby||Busy||Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-quick-check")<0)
                throw new InvalidOperationException("Quick Match test scope requires an idle diagnostic session.");
            // An isolated build key prevents automated tests from joining or
            // advertising to any real player's compatible room search.
            matchmakingTestScope="-quick-smoke-"+Guid.NewGuid().ToString("N");
        }
        public void ClearError() { ErrorText=""; }
        public void SetReady(bool value)
        {
            if (!CanReady || localReady==value) return;
            localReady=value;++localPlayerSerial;
            if (IsHost) SendLobby(); else SendPlayer();
        }
        public void SetCar(int car)
        {
            if(nativeRace||HasCourseDraw||Busy||car<0||car>=CarNames.Length)return;
            if(garage.Count==0)RefreshGarage();
            var chosen=garage.Find(c=>c.Car==car&&c.Saved)??garage.Find(c=>c.Car==car);
            if(chosen!=null)SelectSavedCar(chosen);
        }
        public void SetRaceOptions(int course, bool reverse, bool wet, bool night)
        {
            if (nativeRace || HasCourseDraw || Busy || course < 0 || course > 11) return;
            if(course==11&&!EnnaAvailable)return;
            var choice=new Idas3RaceChoice(course,reverse,wet,night);
            if(LocalChoice.Equals(choice))return;
            LocalChoice=choice;localReady=remoteReady=false;++localPlayerSerial;localSelectionSerial=localPlayerSerial;
            if(IsHost){++revision;if(HandshakeComplete)SendLobby();}
            else if(HandshakeComplete)SendPlayer();
        }
        void SendPlayer() { Send(Packet.Player,w=>{w.Write(revision);w.Write(localPlayerSerial);w.Write(LocalCar);WriteChoice(w,LocalChoice);w.Write(localReady);WriteRecord(w,LocalRecord);LocalSavedCar.Write(w);}); }
        void SendLobby()
        {
            Send(Packet.Lobby,w=>{w.Write(revision);w.Write(remotePlayerSerial);WriteChoice(w,LocalChoice);WriteChoice(w,RemoteChoice);w.Write(LocalCar);w.Write(remoteCar);w.Write(localReady);w.Write(remoteReady);WriteRecord(w,LocalRecord);LocalSavedCar.Write(w);w.Write(BoostEnabled);w.Write(CollisionsEnabled);});
        }
        static void WriteRecord(BinaryWriter writer,Idas3BattleRecord record) {
            writer.Write(record.battles);writer.Write(record.wins);writer.Write(record.level);writer.Write(record.streak);
        }
        static Idas3BattleRecord ReadRecord(BinaryReader reader) {
            var record=new Idas3BattleRecord{battles=reader.ReadUInt32(),wins=reader.ReadUInt32(),level=reader.ReadUInt32(),streak=reader.ReadUInt32()};
            Require(record.Valid,"Invalid driver battle record.");return record;
        }
        static void WriteChoice(BinaryWriter w,Idas3RaceChoice choice) {w.Write(choice.Course);w.Write(choice.Reverse);w.Write(choice.Wet);w.Write(choice.Night);}
        static Idas3RaceChoice ReadChoice(BinaryReader r)
        {
            int course=r.ReadInt32(); bool reverse=r.ReadBoolean(),wet=r.ReadBoolean(),night=r.ReadBoolean();
            Require(course>=0 && course<=11 && ((course!=4&&course!=11) || night) && (course!=8 || wet&&night),"Invalid course options.");
            return new Idas3RaceChoice(course,reverse,wet,night);
        }
        void PeerChanged()
        {
            if (leaving || disposing || DisconnectedFinish) return;
            if (transport.Connected) {
                if (!helloSent) { ResetPeer(); helloSent=true; lastHeard=Now; operationAt=Now; state="Connecting";
                    Send(Packet.Hello,w=>{w.Write(Compatibility());w.Write(IsHost);w.Write(Clean(transport.LocalName));w.Write(LocalCar);w.Write(localPlayerSerial);WriteChoice(w,LocalChoice);WriteRecord(w,LocalRecord);LocalSavedCar.Write(w);}); }
            } else if (HandshakeComplete || nativeRace || helloSent) {
                if(nativeRace){DisconnectRace("Connection to the other driver was lost.");return;}
                StopRace(); ResetPeer(); state=InLobby ? "Lobby" : "Offline";
                status="The other driver left. Race ended; single-player progress was preserved.";
            } else if (IsHost) { state="Lobby";status="Room open. Share the room code with the second driver."; }
        }
        string Compatibility()
        {
            if (compatibility!=null) return compatibility;
            string path=Path.Combine(Application.dataPath,"Plugins/x86_64/Idas3Unity.dll");
            using(var hash=SHA256.Create()) {
                using(var file=File.OpenRead(path)) compatibility="idas3-mp9-"+Convert.ToBase64String(hash.ComputeHash(file));
                using(var file=File.OpenRead(typeof(Idas3MultiplayerSession).Assembly.Location)) compatibility+="-"+Convert.ToBase64String(hash.ComputeHash(file));
            }
            compatibility+="-enna-"+EnnaFingerprint(Path.Combine(Application.streamingAssetsPath,"ENNA"));
            compatibility+=ExperimentalAuthority?"-authority1":"-pose1";
            return compatibility;
        }
        internal static string EnnaFingerprint(string folder)
        {
            if(!File.Exists(Path.Combine(folder,"menu.idastex")))return "absent";
            // Hash only simulation inputs, once per handshake. Two peers must
            // not run different paths/collision meshes under identical code.
            using(var hash=SHA256.Create())using(var combined=new MemoryStream()){
                foreach(string name in new[]{"course.id","enna_path.bin","enna_path_l.bin","enna_path_r.bin","race-markers.bin","collision-0.rcl","collision-1.rcl"}){
                    using(var file=File.OpenRead(Path.Combine(folder,name))){var part=hash.ComputeHash(file);combined.Write(part,0,part.Length);}
                }
                return Convert.ToBase64String(hash.ComputeHash(combined.ToArray()));
            }
        }
        static string Clean(string name)
        {
            var s=new StringBuilder();foreach(char c in name??"Driver") if(!char.IsControl(c)&&c!='<'&&c!='>') { s.Append(c);if(s.Length>=32)break; }
            return s.Length==0?"Driver":s.ToString();
        }
        void Send(Packet type, Action<BinaryWriter> payload=null, bool reliable=true)
        {
            if (DisconnectedFinish || transport==null || !transport.Connected) return;
            using(var m=new MemoryStream(256)) using(var w=new BinaryWriter(m,Encoding.UTF8,true)) {
                w.Write(Magic);w.Write(Protocol);w.Write((byte)type);w.Write(localNonce);w.Write(type==Packet.Hello?0UL:remoteNonce);
                payload?.Invoke(w);impairment.Send(transport,m.ToArray(),reliable);
            }
        }
        static void Require(bool condition,string message) { if(!condition)throw new InvalidDataException(message); }
        void Receive(byte[] bytes)
        {
            if(leaving||disposing||DisconnectedFinish)return;
            try {
                Require(bytes!=null && bytes.Length>=23 && bytes.Length<=4096,"Invalid packet size.");
                using(var m=new MemoryStream(bytes,false)) using(var r=new BinaryReader(m,Encoding.UTF8)) {
                    Require(r.ReadUInt32()==Magic && r.ReadUInt16()==Protocol,"The other game uses a different multiplayer protocol.");
                    Packet type=(Packet)r.ReadByte();ulong sender=r.ReadUInt64(),recipient=r.ReadUInt64();
                    if(type!=Packet.Hello && (sender!=remoteNonce || recipient!=localNonce || !HandshakeComplete)) return;
                    lastHeard=Now;
                    if(IsRacePacket(type)){
                        Require(m.Length-m.Position>=8,"Missing race identity.");
                        ulong packetRace=r.ReadUInt64();m.Position-=8;
                        if(retiredRaceIds.Contains(packetRace)||state=="Returning")return;
                    }
                    switch(type) {
                    case Packet.Hello:
                        Require(sender!=0 && recipient==0,"Invalid handshake.");
                        string build=r.ReadString();bool host=r.ReadBoolean();string name=r.ReadString();int car=r.ReadInt32();
                        uint helloSerial=r.ReadUInt32();var helloChoice=ReadChoice(r);var helloRecord=ReadRecord(r);var helloCar=Idas3OnlineCar.Read(r,car);
                        Require(build==Compatibility(),"Both drivers need the same game build. Copy the complete Current folder to the other PC.");
                        Require(host!=IsHost && name.Length<=128 && car>=0 && car<35,"Invalid driver handshake.");
                        if (HandshakeComplete) { Require(sender==remoteNonce,"Driver session changed unexpectedly."); break; }
                        remoteNonce=sender;remoteName=Clean(name);RemoteSavedCar=helloCar;remoteCar=car;remoteRecord=helloRecord;remotePlayerSerial=helloSerial;RemoteChoice=helloChoice;HandshakeComplete=true;state="Lobby";
                        status="Choose your car and course, then Ready. The game randomly selects one driver's complete course pick when the host starts.";
                        if(IsHost)SendLobby(); break;
                    case Packet.Lobby:
                        Require(!IsHost,"Unexpected room configuration.");
                        uint rev=r.ReadUInt32(),ackSerial=r.ReadUInt32();var hostChoice=ReadChoice(r);var guestChoice=ReadChoice(r);
                        int hostCar=r.ReadInt32(),guestCar=r.ReadInt32();
                        Require(hostCar>=0&&hostCar<35&&guestCar>=0&&guestCar<35,"Invalid car selection.");
                        bool hostReady=r.ReadBoolean(),guestReady=r.ReadBoolean();var hostRecord=ReadRecord(r);var hostSavedCar=Idas3OnlineCar.Read(r,hostCar);bool hostBoost=r.ReadBoolean(),hostCollisions=r.ReadBoolean();
                        Require(ackSerial<=localPlayerSerial,"Invalid player acknowledgement.");
                        if(nativeRace||HasCourseDraw){
                            Require(rev<=revision,"Race settings changed after the selection was locked.");break;
                        }
                        if(rev>=revision) {
                            acknowledgedLocalPlayerSerial=Math.Max(acknowledgedLocalPlayerSerial,ackSerial);
                            bool changed=rev!=revision;revision=rev;RemoteSavedCar=hostSavedCar;remoteCar=hostCar;remoteRecord=hostRecord;RemoteChoice=hostChoice;BoostEnabled=hostBoost;CollisionsEnabled=hostCollisions;
                            if(ackSerial==localPlayerSerial){
                                Require(guestCar==LocalCar&&guestChoice.Equals(LocalChoice),"Acknowledged pick differs from your selection.");
                                localReady=guestReady;remoteReady=hostReady;
                            }else{
                                // A queued lobby update must not erase a newer
                                // local pick or Ready request. Resend against
                                // the latest host revision until acknowledged.
                                if(changed)localReady=false;
                                remoteReady=false;SendPlayer();
                            }
                        }break;
                    case Packet.Player:
                        Require(IsHost,"Unexpected driver configuration.");
                        uint playerRev=r.ReadUInt32(),playerSerial=r.ReadUInt32();int selected=r.ReadInt32();var playerChoice=ReadChoice(r);
                        bool ready=r.ReadBoolean();var selectedRecord=ReadRecord(r);var selectedSavedCar=Idas3OnlineCar.Read(r,selected);Require(selected>=0&&selected<35,"Invalid car selection.");
                        if(nativeRace||HasCourseDraw){
                            Require(playerSerial<=remotePlayerSerial,"Driver selection changed after race start.");break;
                        }
                        if(playerSerial>remotePlayerSerial){
                            remotePlayerSerial=playerSerial;remoteRecord=selectedRecord;
                            if(remoteCar!=selected||!RemoteChoice.Equals(playerChoice)||!selectedSavedCar.Same(RemoteSavedCar)){
                                RemoteSavedCar=selectedSavedCar;remoteCar=selected;RemoteChoice=playerChoice;localReady=remoteReady=false;++revision;
                            }else remoteReady=ready&&playerRev==revision;
                        }
                        SendLobby();break;
                    case Packet.Load:
                        Require(!IsHost&&!nativeRace&&localReady&&remoteReady,"Unexpected race start.");
                        ulong incomingRace=r.ReadUInt64();Require(incomingRace!=0,"Invalid race identity.");
                        uint loadRev=r.ReadUInt32();Require(loadRev==revision,"Race settings changed before start.");
                        int winnerSlot=r.ReadInt32();var winningChoice=ReadChoice(r);
                        Require(!HasCourseDraw&&winnerSlot>=0&&winnerSlot<=1,"Invalid course selection result.");
                        Require(winningChoice.Equals(winnerSlot==0?RemoteChoice:LocalChoice),"Selected race does not match that driver's agreed course pick.");
                        Require(m.Position==m.Length,"Unexpected extra race selection data.");
                        raceId=incomingRace;CourseWinnerSlot=winnerSlot;selectedChoice=winningChoice;
                        LoadRace();Send(Packet.Loaded,w=>w.Write(raceId));break;
                    case Packet.Loaded:
                        if(r.ReadUInt64()!=raceId)break;Require(IsHost&&nativeRace&&!RaceReleased,"Unexpected loading acknowledgement.");
                        remoteLoaded=true;lastHeard=Now;break;
                    case Packet.Release:
                        ulong startId=r.ReadUInt64();double hostAt=r.ReadDouble();
                        if(startId!=raceId)break;
                        Require(!IsHost&&nativeRace&&!RaceReleased&&Finite(hostAt)&&bestRtt<2,"Invalid start synchronization.");
                        releaseAt=hostAt-peerOffset;Require(releaseAt>Now-.1&&releaseAt<Now+10,"Start synchronization arrived outside the allowed window. Please retry.");
                        state="Countdown";Send(Packet.ReleaseAck,w=>w.Write(raceId));break;
                    case Packet.ReleaseAck:
                        if(r.ReadUInt64()==raceId) {Require(IsHost&&nativeRace,"Unexpected start acknowledgement.");releaseAck=true;}break;
                    case Packet.Pose:
                        Require(!ExperimentalAuthority,"Pose authority is disabled in this room.");
                        ulong poseRace=r.ReadUInt64();var pose=Idas3CarSnapshot.Read(r);
                        if(!nativeRace||poseRace!=raceId)break;
                        Require(pose.Valid((uint)remoteCar),"Rejected an invalid car snapshot.");
                        if(pose.sequence>RemoteSnapshot.sequence) {
                            previousPose=RemoteSnapshot.sequence==0?pose:RemoteSnapshot;previousPoseAt=newestPoseAt;newestPoseAt=Now;
                            RemoteSnapshot=pose;++RemoteSnapshotsReceived;
                            if(poseCount==poseBuffer.Length) {Array.Copy(poseBuffer,1,poseBuffer,0,--poseCount);Array.Copy(poseTimes,1,poseTimes,0,poseCount);}
                            poseBuffer[poseCount]=pose;poseTimes[poseCount++]=Now;
                        }break;
                    case Packet.AuthorityInputs:
                        ulong authorityRace=r.ReadUInt64();int byteCount=r.ReadInt32();
                        Require(byteCount>=40&&byteCount<=1064,"Invalid authority packet length.");
                        var authorityBytes=r.ReadBytes(byteCount);Require(authorityBytes.Length==byteCount,"Truncated authority packet.");
                        if(!nativeRace||authorityRace!=raceId)break;
                        Require(ExperimentalAuthority,"Unexpected authority packet.");
                        Require(Idas3MultiplayerNative.Idas3MultiplayerAuthorityReceive(authorityBytes,(uint)authorityBytes.Length)==1,Idas3Native.Error());
                        ++RemoteSnapshotsReceived;
                        break;
                    case Packet.Ping:
                        double stamp=r.ReadDouble();Require(Finite(stamp),"Invalid clock packet.");Send(Packet.Pong,w=>{w.Write(stamp);w.Write(Now);});break;
                    case Packet.Pong:
                        double echoed=r.ReadDouble(),peerAt=r.ReadDouble();double rtt=Now-echoed;
                        Require(Finite(echoed)&&Finite(peerAt),"Invalid clock reply.");
                        if(pendingPings.Remove(echoed)&&rtt>=0&&rtt<5) {PingMilliseconds=(int)(rtt*1000);recentRtt.Enqueue(rtt*1000);if(recentRtt.Count>12)recentRtt.Dequeue();
                            double sum=0,spread=0,prior=-1;foreach(double sample in recentRtt){sum+=sample;if(prior>=0)spread+=Math.Abs(sample-prior);prior=sample;}
                            RecentRttMilliseconds=sum/recentRtt.Count;JitterMilliseconds=recentRtt.Count>1?spread/(recentRtt.Count-1):0;
                            if(rtt<bestRtt){bestRtt=rtt;peerOffset=peerAt-(echoed+Now)*.5;}}break;
                    case Packet.Finish:
                        Require(!ExperimentalAuthority,"Client finish claims are disabled in this room.");
                        ulong finishId=r.ReadUInt64(),ticks=r.ReadUInt64();bool timeUp=r.ReadBoolean();
                        if(nativeRace&&finishId==raceId&&!remoteFinished){remoteFinished=true;remoteFinishTicks=ticks;remoteTimeUp=timeUp;}
                        break;
                    case Packet.Results:
                        ulong resultId=r.ReadUInt64();int winner=r.ReadInt32();ulong hostTicks=r.ReadUInt64(),guestTicks=r.ReadUInt64();
                        Require(!IsHost&&winner>=-1&&winner<=2,"Invalid race result.");
                        if(nativeRace&&resultId==raceId) {
                            if(ExperimentalAuthority){pendingAuthorityWinner=winner;pendingAuthorityHostTicks=hostTicks;pendingAuthorityClientTicks=guestTicks;}
                            else ShowResult(winner,hostTicks,guestTicks);
                        }break;
                    case Packet.RecordUpdate:
                        ulong recordRace=r.ReadUInt64();var completedRecord=ReadRecord(r);
                        if(nativeRace&&recordRace==raceId){if(ExperimentalAuthority&&!recordCommitted)pendingAuthorityRecord=completedRecord;else {Require(recordCommitted,"Driver record arrived before the result.");remoteRecord=completedRecord;}}
                        break;
                    case Packet.Leave:
                        if(nativeRace)DisconnectRace("The other driver left the race.");
                        else {status="The other driver left the room.";LeaveRoom();}break;
                    case Packet.ReturnRequest:
                        ulong requestedReturn=r.ReadUInt64();Require(m.Position==m.Length,"Unexpected lobby return data.");
                        Require(IsHost,"Only the host commits a lobby return.");
                        if(requestedReturn==returnRaceId&&retiredRaceIds.Contains(requestedReturn)){
                            SendReturnCommit();break;
                        }
                        if(requestedReturn!=raceId)break;
                        Require(nativeRace&&(state=="Results"||(resultSent&&finishSent&&remoteFinished)),"The race has not finished.");
                        BeginLobbyReturn();break;
                    case Packet.ReturnLobby:
                        ulong committedReturn=r.ReadUInt64();uint committedRevision=r.ReadUInt32();
                        Require(!IsHost&&m.Position==m.Length,"Invalid lobby return commit.");
                        if(retiredRaceIds.Contains(committedReturn)){
                            if(committedReturn==returnRaceId&&committedRevision==returnRevision)SendReturnAck();
                            break;
                        }
                        if(committedReturn!=raceId)break;
                        Require(nativeRace&&(state=="Results"||state=="Returning")&&committedRevision==unchecked(revision+1),"Unexpected lobby return commit.");
                        returnRaceId=committedReturn;returnRevision=revision=committedRevision;
                        RetireNativeRace();state="Lobby";status="Both drivers returned to the lobby. Choose Ready for another race.";
                        SendReturnAck();
                        if(HandshakeComplete&&InLobby&&!DisconnectedFinish)ReturnedToLobby?.Invoke();
                        break;
                    case Packet.ReturnAck:
                        ulong acknowledgedReturn=r.ReadUInt64();uint acknowledgedRevision=r.ReadUInt32();
                        var returningRecord=ReadRecord(r);
                        Require(IsHost&&m.Position==m.Length,"Invalid lobby return acknowledgement.");
                        if(acknowledgedReturn!=returnRaceId||acknowledgedRevision!=returnRevision||state!="Returning")break;
                        remoteRecord=returningRecord;
                        state="Lobby";status="Both drivers returned to the lobby. Choose Ready for another race.";
                        SendLobby();
                        if(HandshakeComplete&&InLobby&&!DisconnectedFinish)ReturnedToLobby?.Invoke();
                        break;
                    default: throw new InvalidDataException("Unknown multiplayer packet.");
                    }
                    Require(m.Position==m.Length,"Unexpected extra packet data.");
                }
            } catch(Exception e) { Fail("Connection stopped: "+e.Message); }
        }
        static bool Finite(double x)=>!double.IsNaN(x)&&!double.IsInfinity(x);
        static bool IsRacePacket(Packet type) => type==Packet.Load||type==Packet.Loaded||type==Packet.Release||type==Packet.ReleaseAck||type==Packet.Pose||type==Packet.AuthorityInputs||type==Packet.Finish||type==Packet.Results||type==Packet.RecordUpdate;
        public void ReturnToLobby()
        {
            if(DisconnectedFinish){LeaveRoom();ReturnedToLobby?.Invoke();return;}
            if(!CanReturnToLobby)return;
            try{
                if(IsHost)BeginLobbyReturn();
                else{
                    returnRaceId=raceId;state="Returning";operationAt=Now;
                    localReady=remoteReady=false;ResultText="";status="Returning both drivers to the lobby...";
                    Send(Packet.ReturnRequest,w=>w.Write(returnRaceId));
                }
            }catch(Exception e){Fail("Could not return to the lobby: "+e.Message);}
        }
        void SendReturnCommit() => Send(Packet.ReturnLobby,w=>{w.Write(returnRaceId);w.Write(returnRevision);});
        // The host may retire its race before the peer's RecordUpdate arrives.
        // This acknowledgement also publishes the peer's committed history.
        void SendReturnAck() => Send(Packet.ReturnAck,w=>{w.Write(returnRaceId);w.Write(returnRevision);WriteRecord(w,LocalRecord);});
        void BeginLobbyReturn()
        {
            returnRaceId=raceId;returnRevision=unchecked(revision+1);
            RetireNativeRace();revision=returnRevision;state="Returning";operationAt=Now;
            status="Returning both drivers to the lobby...";SendReturnCommit();
        }
        void RetireNativeRace()
        {
            Require(nativeRace&&Idas3MultiplayerNative.Idas3MultiplayerLeave()==1,Idas3Native.Error());
            retiredRaceIds.Add(raceId);raceId=0;nativeRace=false;RaceReleased=false;
            localReady=remoteReady=remoteLoaded=releaseAck=finishSent=resultSent=remoteFinished=remoteTimeUp=false;
            releaseAt=lastPose=previousPoseAt=newestPoseAt=0;remoteFinishTicks=0;poseCount=0;
            LocalSnapshot=RemoteSnapshot=previousPose=default;RemoteSnapshotsReceived=SnapshotsSent=0;
            Array.Clear(poseBuffer,0,poseBuffer.Length);Array.Clear(poseTimes,0,poseTimes.Length);
            CourseWinnerSlot=-1;selectedChoice=default;ResultText="";ErrorText="";
        }
        public void StartRace()
        {
            try {
                PollTransport();if(!CanStart)return;
                raceId=Nonce();CourseWinnerSlot=ChooseCourseSlot();selectedChoice=CourseWinnerSlot==0?LocalChoice:RemoteChoice;
                state="Loading";operationAt=Now;
                Send(Packet.Load,w=>{w.Write(raceId);w.Write(revision);w.Write(CourseWinnerSlot);WriteChoice(w,selectedChoice);});
                if(!HandshakeComplete||!HasCourseDraw)return;
                // Native loading initializes remoteLoaded. Only poll after it
                // finishes, so a fast peer's Loaded reply cannot be rejected
                // or erased before our own waiting race exists.
                LoadRace();PollTransport();
            }catch(Exception e){Fail("Could not load multiplayer race: "+e.Message);}
        }
        void LoadRace()
        {
            Require(HasCourseDraw,"Race requires a shared course selection.");
            RaceLocalRecord=LocalRecord;RaceRemoteRecord=remoteRecord;recordCommitted=false;
            state="Loading";operationAt=Now;releaseAt=0;RaceReleased=remoteLoaded=releaseAck=finishSent=resultSent=remoteFinished=false;
            LocalSnapshot=RemoteSnapshot=previousPose=default;RemoteSnapshotsReceived=SnapshotsSent=0;ResultText="";
            poseCount=0;
            var config=new Idas3MultiplayerConfig{size=40,version=ExperimentalAuthority?2u:1u,course=(uint)Course,reverse=Reverse?1u:0u,wet=Wet?1u:0u,night=Night?1u:0u,
                localCar=(uint)LocalCar,remoteCar=(uint)remoteCar,localSlot=IsHost?0u:1u,automatic=LocalSavedCar.Automatic?1u:0u};
            Require(LocalSavedCar!=null&&RemoteSavedCar!=null,"Both drivers must choose a car before starting.");
            if(Idas3MultiplayerNative.Idas3MultiplayerStartSaved(ref config,LocalSavedCar.Selection,LocalSavedCar.Words,RemoteSavedCar.Words,307)!=1) {
                string error=Idas3Native.Error();Idas3MultiplayerNative.Idas3MultiplayerLeave();throw new InvalidOperationException(error);
            }
            nativeRace=true;
            pendingAuthorityWinner=-3;pendingAuthorityRecord=null;AuthorityStatus=default;
            if(ExperimentalAuthority)Require(Idas3MultiplayerNative.Idas3MultiplayerEnableAuthorityRules(raceId,RemoteSavedCar.Automatic?1:0,BoostEnabled?1:0,CollisionsEnabled?1:0)==1,Idas3Native.Error());
            Require(Idas3Native.Idas3SceneSetPreRaceNames(Clean(transport?.LocalName),remoteName)==1,Idas3Native.Error());
            var localStats=RaceLocalRecord;var remoteStats=RaceRemoteRecord;
            Require(Idas3MultiplayerNative.Idas3MultiplayerSetBattleRecords(ref localStats,ref remoteStats)==1,Idas3Native.Error());
            lastHeard=Now;operationAt=Now;status=CourseDrawText+". Loading both cars, then the showcase and countdown.";
        }
        public void BeforeFrame()
        {
            if(terminalFailure!=null)throw terminalFailure;
            try { BeforeFrameCore(); } catch(Exception e) { Fail("Multiplayer stopped: "+e.Message); }
            if(terminalFailure!=null)throw terminalFailure;
        }
        void PollTransport()
        {
            if(polling||transport==null)return;polling=true;
            try {
                if(transport is Idas3SteamTransport steam){steam.ActivityRequested=ActivityRequested;steam.ActivityState=Idas3OnlineActivity.StateFor(nativeRace,state,IsQuickMatching,InLobby,HandshakeComplete,DisconnectedFinish);}
                impairment.Flush(transport);transport.Poll();} finally {polling=false;}
        }
        void BeforeFrameCore()
        {
            // The neutral finish blocks race work, but Steam presence still
            // reports this connected client as online after leaving its peer.
            if(DisconnectedFinish){if(transport is Idas3SteamTransport&&Available)PollTransport();return;}
            if(transport==null||!Available){if(nativeRace)DisconnectRace("The network connection is unavailable.");return;}
            PollTransport();
            if(DisconnectedFinish)return;
            quickMatch?.Tick(HandshakeComplete);
            // Announce only an admitted, compatible peer, once per connection.
            // Transport retries and lobby membership alone are not a match.
            if(HandshakeComplete&&!matchAnnounced&&!nativeRace){
                matchAnnounced=true;ChallengerPending=true;ChallengerFound?.Invoke();
            }
            double now=Now;
            if(state=="Connecting" && now-operationAt>35) {Fail("The other driver did not complete the connection.");return;}
            if(!HandshakeComplete)return;
            if(state=="Returning"&&now-operationAt>30){Fail("The other driver did not return to the lobby.");return;}
            if(now-lastHeard>(state=="Loading"?60:15)) {Fail("Connection to the other driver timed out.");return;}
            if(now-lastPing>.35) {
                lastPing=lastPingStamp=now;
                if(pendingPings.Count>=20)pendingPings.Clear();
                pendingPings[lastPingStamp]=0;Send(Packet.Ping,w=>w.Write(lastPingStamp));
            }
            if(DisconnectedFinish)return;
            if(state=="Returning")return;
            if(nativeRace&&!RaceReleased) {
                if(now-operationAt>65) {Fail("The second driver did not finish loading the race.");return;}
                if(IsHost&&remoteLoaded&&releaseAt==0&&bestRtt<2) {
                    releaseAt=now+Math.Max(1.0,bestRtt*4);state="Countdown";
                    Send(Packet.Release,w=>{w.Write(raceId);w.Write(releaseAt);});
                }
                if(DisconnectedFinish)return;
                if(releaseAt>0&&now>=releaseAt) {
                    if(IsHost&&!releaseAck) {Fail("Start acknowledgement was not received. Please reconnect and retry.");return;}
                    Require(Idas3MultiplayerNative.Idas3MultiplayerSetGo(1)==1,Idas3Native.Error());
                    RaceReleased=true;state="Racing";status="Race live. Car collisions "+(CollisionsEnabled?"ON":"OFF")+". Boost "+(BoostEnabled?"ON":"OFF")+". F1 opens the room.";
                }
            }
            if(nativeRace&&!ExperimentalAuthority&&poseCount>0) {
                double target=now-.075;
                while(poseCount>2&&poseTimes[1]<=target) {Array.Copy(poseBuffer,1,poseBuffer,0,--poseCount);Array.Copy(poseTimes,1,poseTimes,0,poseCount);}
                previousPose=poseBuffer[0];var pose=poseBuffer[poseCount>1?1:0];
                float t=poseCount>1&&poseTimes[1]>poseTimes[0]?(float)((target-poseTimes[0])/(poseTimes[1]-poseTimes[0])):1;
                t=Mathf.Clamp01(t);pose.body=Vector3.Lerp(previousPose.body,pose.body,t);pose.actor=Vector3.Lerp(previousPose.actor,pose.actor,t);
                pose.yaw=Angle(previousPose.yaw,pose.yaw,t);pose.pitch=Angle(previousPose.pitch,pose.pitch,t);pose.roll=Angle(previousPose.roll,pose.roll,t);
                pose.steering=Mathf.Lerp(previousPose.steering,pose.steering,t);pose.suspension=Vector4.Lerp(previousPose.suspension,pose.suspension,t);
                for(int i=0;i<4;i++)pose.rotation[i]=Angle(previousPose.rotation[i],pose.rotation[i],t);
                Require(Idas3MultiplayerNative.Idas3MultiplayerApplyRemoteSnapshot(ref pose)==1,Idas3Native.Error());
            }
        }
        static float Angle(float a,float b,float t)=>Mathf.LerpAngle(a*Mathf.Rad2Deg,b*Mathf.Rad2Deg,t)*Mathf.Deg2Rad;
        public void AfterFrame()
        {
            if(terminalFailure!=null)throw terminalFailure;
            try { AfterFrameCore(); } catch(Exception e) { Fail("Multiplayer stopped: "+e.Message); }
            if(terminalFailure!=null)throw terminalFailure;
        }
        void AfterAuthorityFrame()
        {
            var status=new Idas3AuthorityStatus{size=96,version=1};
            Require(Idas3MultiplayerNative.Idas3MultiplayerAuthorityStatus(ref status)==1,Idas3Native.Error());AuthorityStatus=status;
            // Publish each newly simulated input frame. A wall-clock 30Hz
            // threshold made a 60Hz peer predict two (sometimes three) ticks
            // between updates. Retransmit while stalled so recovery still works.
            if(status.frame!=authorityPacketFrame||Now-lastPose>=1.0/30) {
                authorityPacketFrame=status.frame;
                lastPose=Now;int count=Idas3MultiplayerNative.Idas3MultiplayerAuthorityPacket(authorityPacket,(uint)authorityPacket.Length);
                Require(count>=40&&count<=authorityPacket.Length,Idas3Native.Error());
                Send(Packet.AuthorityInputs,w=>{w.Write(raceId);w.Write(count);w.Write(authorityPacket,0,count);},false);++SnapshotsSent;
            }
            var remote=new Idas3CarSnapshot{size=128,version=1};
            Require(Idas3MultiplayerNative.Idas3MultiplayerGetRemoteSnapshot(ref remote)==1,Idas3Native.Error());RemoteSnapshot=remote;
            if(status.winner==-3)return;
            if(IsHost&&!resultSent){
                resultSent=true;Send(Packet.Results,w=>{w.Write(raceId);w.Write(status.winner);w.Write(status.hostFinishTicks);w.Write(status.clientFinishTicks);});
                if(!DisconnectedFinish)ShowResult(status.winner,status.hostFinishTicks,status.clientFinishTicks);
            } else if(!IsHost&&pendingAuthorityWinner!=-3){
                Require(pendingAuthorityWinner==status.winner&&pendingAuthorityHostTicks==status.hostFinishTicks&&pendingAuthorityClientTicks==status.clientFinishTicks,"Host result does not match the confirmed race.");
                pendingAuthorityWinner=-3;ShowResult(status.winner,status.hostFinishTicks,status.clientFinishTicks);
                if(pendingAuthorityRecord.HasValue){remoteRecord=pendingAuthorityRecord.Value;pendingAuthorityRecord=null;}
            }
        }
        void AfterFrameCore()
        {
            if(DisconnectedFinish||state=="Returning"||!nativeRace||!HandshakeComplete)return;
            var pose=new Idas3CarSnapshot{size=128,version=1};
            Require(Idas3MultiplayerNative.Idas3MultiplayerGetLocalSnapshot(ref pose)==1,Idas3Native.Error());LocalSnapshot=pose;
            if(ExperimentalAuthority){AfterAuthorityFrame();return;}
            if(Now-lastPose>=1.0/30) {lastPose=Now;Send(Packet.Pose,w=>{w.Write(raceId);pose.Write(w);},false);++SnapshotsSent;}
            if(DisconnectedFinish)return;
            if((pose.Finished||pose.TimeUp)&&!finishSent) {
                finishSent=true;Send(Packet.Finish,w=>{w.Write(raceId);w.Write(pose.raceTicks);w.Write(pose.TimeUp);});
                if(DisconnectedFinish)return;
                status="Finished. Waiting for the other driver to finish; F1 shows the result.";
            }
            if(IsHost&&finishSent&&remoteFinished&&!resultSent) {
                resultSent=true;int winner=pose.TimeUp?(remoteTimeUp?-1:1):remoteTimeUp?0:pose.raceTicks==remoteFinishTicks?2:pose.raceTicks<remoteFinishTicks?0:1;
                Send(Packet.Results,w=>{w.Write(raceId);w.Write(winner);w.Write(pose.raceTicks);w.Write(remoteFinishTicks);});
                ShowResult(winner,pose.raceTicks,remoteFinishTicks);
            }
        }
        void ShowResult(int winner,ulong hostTicks,ulong guestTicks)
        {
            if(DisconnectedFinish||state=="Returning"||!nativeRace)return;
            Require(Idas3MultiplayerNative.Idas3MultiplayerSetResult(winner)==1,Idas3Native.Error());
            if(!recordCommitted){
                // Only a settled win/loss changes history. A draw, double
                // time-up or disconnected race cannot award a battle result.
                if(winner==0||winner==1)records.Commit(LocalCar,raceId,winner==(IsHost?0:1),RaceRemoteRecord);
                recordCommitted=true;
                if(winner==0||winner==1)Send(Packet.RecordUpdate,w=>{w.Write(raceId);WriteRecord(w,LocalRecord);});
            }
            state="Results";ResultText=winner==-1?"BOTH DRIVERS — TIME UP":winner==2?"DRAW":winner==(IsHost?0:1)?"YOU WIN":"OPPONENT WINS";
            ResultText+="   /   Host "+(hostTicks/60.0).ToString("F3")+" s   /   Guest "+(guestTicks/60.0).ToString("F3")+" s. Return to the lobby for another race.";
        }
        void UpdatePlayers()
        {
            for(int i=0;i<2;i++) {
                bool local=i==(IsHost?0:1);var p=players[i];p.IsLocal=local;p.IsHost=i==0;p.Connected=local?InLobby:HandshakeComplete;
                p.Name=local?Clean(transport?.LocalName):remoteName;p.CarName=CarNames[local?LocalCar:remoteCar];p.Ready=local?localReady:remoteReady;p.PingMilliseconds=local?0:PingMilliseconds;
            }
        }
        void ResetPeer()
        {
            matchAnnounced=false;ChallengerPending=false;
            HandshakeComplete=false;helloSent=false;remoteNonce=0;localReady=remoteReady=false;bestRtt=double.MaxValue;PingMilliseconds=-1;recentRtt.Clear();RecentRttMilliseconds=JitterMilliseconds=0;
            remoteLoaded=releaseAck=false;releaseAt=0;lastPing=0;
            pendingPings.Clear();
            revision=localPlayerSerial=remotePlayerSerial=localSelectionSerial=acknowledgedLocalPlayerSerial=0;CourseWinnerSlot=-1;selectedChoice=default;
            RemoteChoice=new Idas3RaceChoice(3,false,false,true);
            remoteRecord=Idas3BattleRecord.Fresh;
            raceId=returnRaceId=0;returnRevision=0;retiredRaceIds.Clear();
        }
        void StopRace()
        {
            if(nativeRace) {Idas3MultiplayerNative.Idas3MultiplayerLeave();nativeRace=false;}
            RaceReleased=false;releaseAt=0;
        }
        void DisconnectRace(string reason)
        {
            if(!nativeRace||DisconnectedFinish||leaving||disposing)return;
            // Commit the neutral terminal state before transport cleanup can
            // deliver another callback. Keep nativeRace and its save barrier.
            DisconnectedFinish=true;RaceReleased=false;state="Disconnected";
            finishSent=resultSent=true;remoteFinished=false;releaseAt=0;poseCount=0;
            HandshakeComplete=false;helloSent=false;pendingPings.Clear();
            localReady=remoteReady=false;ErrorText="";
            ResultText="CONNECTION LOST — NO RESULT RECORDED — 0 POINTS AWARDED";
            status=reason;
            try {
                Require(Idas3MultiplayerNative.Idas3MultiplayerDisconnect()==1,Idas3Native.Error());
                var stopped=new Idas3CarSnapshot{size=128,version=1};
                if(Idas3MultiplayerNative.Idas3MultiplayerGetLocalSnapshot(ref stopped)==1)LocalSnapshot=stopped;
            }catch(Exception e){
                Debug.LogError("IDAS3 neutral finish failed; ending native race safely: "+e.Message);
                try {Require(Idas3MultiplayerNative.Idas3MultiplayerLeave()==1,Idas3Native.Error());nativeRace=false;}
                catch(Exception stop){terminalFailure=new InvalidOperationException("Could not stop the disconnected native race.",stop);}
            }
            leaving=true;
            try {quickMatch?.Cancel();transport?.Leave();}
            catch(Exception e){Debug.LogWarning("IDAS3 disconnect cleanup: "+e.Message);}
            finally {impairment.Clear();leaving=false;}
            RaceDisconnected?.Invoke();
        }
        void Fail(string message)
        {
            if(disposing||leaving||DisconnectedFinish)return;
            if(nativeRace){DisconnectRace(message);return;}
            if(IsQuickMatching&&quickMatch.HandleTransportError(message))return;
            LeaveRoom();ErrorText=message;status=message;Debug.LogWarning("IDAS3 multiplayer: "+message);
        }
        public void LeaveRoom()
        {
            if(leaving)return;leaving=true;
            try { quickMatch?.Cancel();if(HandshakeComplete){Send(Packet.Leave);PollTransport();}StopRace();transport?.Leave();ResetPeer();DisconnectedFinish=false;terminalFailure=null;ResultText="";state="Offline";status="Room closed. Your single-player progress is unchanged."; }
            finally {leaving=false;}
        }
        public void Dispose()
        {
            if(disposing)return;LeaveRoom();disposing=true;transport?.Dispose();transport=null;
        }
    }
}

