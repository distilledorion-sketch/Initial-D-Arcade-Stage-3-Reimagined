using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using UnityEngine;

namespace Idas3.Multiplayer
{
    // Explicit blank-scene service probe. No SceneGame, game input, native
    // simulation or game save is created. Each process needs its own cache profile and a distinct EOS product user.
    public sealed class Idas3OnlineTransportProbe : MonoBehaviour
    {
        const uint Magic=0x42503349;
        const int HeaderBytes=16,ReliableCount=24,UnreliableCount=60,UnreliableBytes=1064;
        static readonly int[] ReliableSizes={128,512,1064,1400,4096,60000};
        enum Packet:byte { Hello=1,Reliable,Unreliable,Done,LeaveRequest,LeaveAck,LeaveCommit }
        readonly Stopwatch clock=Stopwatch.StartNew();
        readonly HashSet<int> unreliableSeen=new HashSet<int>();
        Idas3EosTransport transport;
        string root,role,mode,scopeHash,roomName,phase="starting",failure;
        string localIdentityHash,remoteIdentityHash,connectionType="unknown";
        bool coordinator,started,outputOwned,roomRequested,browsing,roomPublished,wasConnected,actualHost;
        bool helloSent,helloReceived,peerHost,doneSent,doneReceived;
        bool leaveRequestSent,leaveRequestReceived,leaveAckSent,leaveAckReceived,leaveCommitSent,leaveCommitReceived;
        bool peerLeft,cleanupStarted,localLobbyCleared,finished,cancelRetryRequested,loadStallRequested,loadStallComplete,forceRelayRequested;
        int cancelRetryCount;
        int reliableReceivedBeforeStall;
        long loadStallKeepAlivePumps;
        double loadStallSeconds;
        int reliableSent,reliableReceived,unreliableSent,unreliableDuplicates,unreliableOutOfOrder,lastUnreliable=-1;
        long reliableBytesSent,reliableBytesReceived,unreliableBytesReceived;
        double deadline,nextBrowse,nextReliable,nextUnreliable,cleanupAt;

        [Serializable] sealed class RoomNotice { public string scopeHash,mode,code,hostIdentityHash; }
        [Serializable] sealed class Report {
            public bool passed,authenticated,distinctIdentities,oppositeHostRoles,actualHost,peerHost;
            public bool peerLeft,localLobbyCleared,cleanupRequested,cancelRetryRequested,loadStallRequested,loadStallComplete,forceRelayRequested;
            public int cancelRetryCount;
            public int reliableReceivedBeforeStall;
            public long loadStallKeepAlivePumps;
            public double loadStallSeconds;
            public string role,mode,phase,error,scopeHash,localIdentityHash,remoteIdentityHash,connectionType;
            public int reliableSent,reliableReceived,unreliableSent,unreliableReceived,unreliableDuplicates,unreliableOutOfOrder;
            public long reliableBytesSent,reliableBytesReceived,unreliableBytesReceived;
            public double elapsedSeconds;
            public string verificationScope="Two isolated Unity clients use the production EOS P2P transport. Authenticated identity, opposite host roles, 24 ordered reliable messages with 128 to 60000 payload bytes, 60 unreliable 1064-byte frames, received byte patterns and coordinated peer departure are checked. Unreliable losses are reported. This is a route/lifecycle check, not a wider internet latency or game-race test. Cleanup is requested and local state verified; cloud cleanup requests are given five seconds to settle.";
        }

        void Start()
        {
            try {
                var args=Environment.GetCommandLineArgs();
                cancelRetryRequested=Array.IndexOf(args,"-idas3-probe-cancel-retry")>=0;
                loadStallRequested=Array.IndexOf(args,"-idas3-probe-load-stall")>=0;
                forceRelayRequested=Array.IndexOf(args,"-idas3-probe-force-relay")>=0;
                role=Argument(args,"-idas3-online-probe");
                Require(role=="host"||role=="join","Probe role must be host or join.");
                coordinator=role=="host";
                mode=Argument(args,"-idas3-probe-mode");
                Require(mode=="private"||mode=="public"||mode=="queue","Probe mode must be private, public or queue.");
                string profile=Argument(args,"-idas3-online-profile");
                Require(profile!="default"&&profile.Length<=30,"Use a separate cache profile for the probe; EOS identities must also be distinct.");
                foreach(char c in profile)Require(char.IsLetterOrDigit(c),"Probe profiles must be alphanumeric.");
                string scope=Argument(args,"-idas3-probe-scope");
                Require(scope.Length>=6&&scope.Length<=64,"Use a unique shared probe scope of 6 to 64 characters.");
                foreach(char c in scope)Require(char.IsLetterOrDigit(c)||c=='-'||c=='_',"Probe scope contains invalid characters.");
                scopeHash=Hash(scope);roomName="Probe "+scopeHash.Substring(0,24);
                root=Path.GetFullPath(Argument(args,"-idas3-probe-root"));
                string saves=Path.GetFullPath(Application.persistentDataPath).TrimEnd(Path.DirectorySeparatorChar,Path.AltDirectorySeparatorChar);
                Require(!root.Equals(saves,StringComparison.OrdinalIgnoreCase)&&!root.StartsWith(saves+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase),"Probe output must be outside the player save folder.");
                Directory.CreateDirectory(root);
                using(var marker=new FileStream(Path.Combine(root,role+".started"),FileMode.CreateNew,FileAccess.Write,FileShare.Read)){
                    byte[] bytes=Encoding.UTF8.GetBytes(scopeHash+"\n");marker.Write(bytes,0,bytes.Length);
                }
                outputOwned=true;
                Application.runInBackground=true;Application.targetFrameRate=60;QualitySettings.vSyncCount=0;
                deadline=mode=="queue"?180:120;
                transport=new Idas3EosTransport{BuildCompatibility="idas3-cloud-probe-v1-"+scopeHash,PublishActivity=false,ActivityRequested=false};
                transport.Message+=Receive;transport.PeerChanged+=PeerChanged;transport.Error+=TransportError;
                SetPhase("signing-in");started=true;transport.Initialize();
            }catch(Exception error){Fail(error.Message);}
        }

        void Update()
        {
            if(finished)return;
            if(!started){Complete();return;}
            try {
                transport.Poll();
                double now=clock.Elapsed.TotalSeconds;
                if(cleanupStarted){if(now-cleanupAt>=5||now>=deadline)Complete();return;}
                Require(now<deadline-5,"Probe timed out during "+phase+".");
                if(!transport.Available)return;
                if(localIdentityHash==null){Require(!string.IsNullOrEmpty(transport.LocalId),"Sign-in returned no player identity.");localIdentityHash=Hash(transport.LocalId);}
                if(!wasConnected&&!CheckPeerIdentity())return;
                if(!roomRequested){
                    if(mode=="queue") {
                        roomRequested=true;SetPhase("matchmaking");
                        if(cancelRetryRequested){transport.StartMatchmaking();transport.CancelMatchmaking();CheckCancelled();}
                        transport.StartMatchmaking();
                    }
                    else if(coordinator){
                        roomRequested=true;SetPhase("creating-"+mode+"-room");
                        if(cancelRetryRequested){transport.HostPrivate(roomName+" canceled");transport.Leave();CheckCancelled();}
                        if(mode=="private")transport.HostPrivate(roomName);else transport.Host(roomName);
                    }
                    else FindRoom(now);
                }
                if(coordinator&&transport.InLobby&&!roomPublished){
                    if(mode!="queue")Require(transport.PrivateRoom==(mode=="private"),"The room visibility differs from the requested probe mode.");
                    Require(!string.IsNullOrEmpty(transport.RoomCode),"Created session has no room code.");
                    WriteNew("room.json",JsonUtility.ToJson(new RoomNotice{scopeHash=scopeHash,mode=mode,code=transport.RoomCode,hostIdentityHash=localIdentityHash},true));
                    roomPublished=true;
                }
                if(!transport.Connected){
                    if(wasConnected){Require(peerLeft&&coordinator&&leaveCommitSent,"Peer disconnected before verification completed.");BeginCleanup();}
                    return;
                }
                if(!wasConnected)Connected();
                connectionType=transport.ConnectionType;
                if(!helloSent){helloSent=true;Send(Packet.Hello,-1,Encoding.UTF8.GetBytes(transport.LocalId),true);}
                if(!helloReceived)return;
                if(coordinator&&loadStallRequested&&!loadStallComplete&&reliableReceived>0)VerifyLoadingStall();
                if(!leaveCommitSent)SetPhase("exchanging-packets");
                if(reliableSent<ReliableCount&&now>=nextReliable){
                    int size=ReliableSizes[reliableSent%ReliableSizes.Length];
                    SendPattern(Packet.Reliable,reliableSent,size,true);++reliableSent;reliableBytesSent+=size;nextReliable=now+.1;
                }
                if(unreliableSent<UnreliableCount&&now>=nextUnreliable){
                    SendPattern(Packet.Unreliable,unreliableSent,UnreliableBytes-HeaderBytes,false);++unreliableSent;nextUnreliable=now+1.0/60;
                }
                if(reliableSent==ReliableCount&&unreliableSent==UnreliableCount&&!doneSent){
                    doneSent=true;Send(Packet.Done,-1,Counts(),true);
                }
                bool complete=doneSent&&doneReceived&&reliableReceived==ReliableCount&&unreliableSeen.Count>0;
                if(complete){
                    Require(connectionType=="direct"||connectionType=="relayed","EOS did not report the established connection type.");
                    Require(!forceRelayRequested||connectionType=="relayed","The forced-relay probe did not establish a relayed connection.");
                }
                if(coordinator&&complete&&!leaveRequestSent){leaveRequestSent=true;Send(Packet.LeaveRequest,-1,Array.Empty<byte>(),true);}
                if(!coordinator&&complete&&leaveRequestReceived&&!leaveAckSent){leaveAckSent=true;Send(Packet.LeaveAck,-1,Array.Empty<byte>(),true);}
                if(coordinator&&complete&&leaveAckReceived&&!leaveCommitSent){leaveCommitSent=true;Send(Packet.LeaveCommit,-1,Array.Empty<byte>(),true);}
                if(!coordinator&&leaveCommitReceived){Require(complete&&leaveAckSent,"Departure started before both transfers completed.");BeginCleanup();}
                else if(leaveCommitSent)SetPhase("waiting-for-peer-departure");
            }catch(Exception error){Fail(error.Message);}
        }

        void FindRoom(double now)
        {
            if(mode=="private"){
                string file=Path.Combine(root,"room.json");if(!File.Exists(file))return;
                RoomNotice notice;
                try{notice=JsonUtility.FromJson<RoomNotice>(File.ReadAllText(file));}catch(IOException){return;}
                Require(notice!=null&&notice.scopeHash==scopeHash&&notice.mode==mode&&!string.IsNullOrWhiteSpace(notice.code),"Private room notice belongs to another probe run.");
                roomRequested=true;SetPhase("joining-private-room");
                if(cancelRetryRequested){transport.Join(notice.code);transport.Leave();CheckCancelled();}
                transport.Join(notice.code);return;
            }
            // Public discovery uses only the service result. It never reads the
            // host's room-code file, so this path proves scoped room discovery.
            if(browsing&&!transport.IsBusy){
                browsing=false;
                foreach(var room in transport.Rooms)if(room.Name==roomName&&!room.Private&&room.Members==1&&room.Capacity==2){
                    roomRequested=true;SetPhase("joining-discovered-room");transport.Join(room.Code);return;
                }
            }
            if(!browsing&&!transport.IsBusy&&now>=nextBrowse){SetPhase("discovering-public-room");nextBrowse=now+2;browsing=true;transport.Browse();}
        }

        bool CheckPeerIdentity()
        {
            // An EOS Device ID can represent the same product user even when
            // processes use different cache profiles. Never delete or replace
            // that identity to manufacture a two-player test on one OS user.
            string identityPath=Path.Combine(root,role+".identity");
            if(!File.Exists(identityPath))WriteNew(role+".identity",scopeHash+"\n"+localIdentityHash);
            string peerPath=Path.Combine(root,(coordinator?"join":"host")+".identity");
            if(!File.Exists(peerPath))return false;
            string[] identity;
            try{identity=File.ReadAllLines(peerPath);}catch(IOException){return false;}
            Require(identity.Length==2&&identity[0]==scopeHash,"The peer identity notice belongs to a different probe run.");
            Require(identity[1]!=localIdentityHash,"Both test processes authenticated as the same EOS product user. Separate cache profiles do not guarantee separate Device IDs; use two OS users or two PCs. No normal identity was changed.");
            return true;
        }

        void CheckCancelled()
        {
            Require(!transport.Connected&&!transport.InLobby&&!transport.IsMatching&&!transport.IsBusy,"Canceled operation retained local room state.");
            ++cancelRetryCount;
        }

        void VerifyLoadingStall()
        {
            // Wait for real peer traffic before holding this main thread so its
            // remaining paced data arrives during the same pause as course load.
            Require(reliableReceived<ReliableCount,"The peer completed its transfer before the loading-stall fixture could begin.");
            reliableReceivedBeforeStall=reliableReceived;SetPhase("loading-stall");
            double began=clock.Elapsed.TotalSeconds;
            long pumps=Idas3OnlineTransportPump.NativeLoadKeepAlivePumps;
            while(clock.Elapsed.TotalSeconds-began<12){
                Idas3OnlineTransportPump.KeepAliveDuringNativeLoad();
                System.Threading.Thread.Sleep(25);
            }
            loadStallSeconds=clock.Elapsed.TotalSeconds-began;
            loadStallKeepAlivePumps=Idas3OnlineTransportPump.NativeLoadKeepAlivePumps-pumps;
            Require(loadStallKeepAlivePumps>0,"The simulated loading pause did not service the online connection.");
            // The keepalive may buffer received bytes but must never deliver a
            // gameplay callback while native scene code owns its lock.
            Require(reliableReceived==reliableReceivedBeforeStall,"The loading keepalive dispatched a gameplay callback during the pause.");
            loadStallComplete=true;
        }

        void PeerChanged()
        {
            if(cleanupStarted||finished)return;
            if(transport.Connected)return;
            if(wasConnected&&coordinator&&leaveCommitSent){peerLeft=true;SetPhase("peer-departed");}
            else if(wasConnected)Fail("The peer left before the departure handshake.");
        }
        void Connected()
        {
            Require(!string.IsNullOrEmpty(transport.RemoteId)&&transport.RemoteId!=transport.LocalId,"EOS P2P did not authenticate a distinct remote player.");
            if(mode!="queue")Require(transport.IsHost==coordinator,"Manual room assigned an unexpected host role.");
            actualHost=transport.IsHost;remoteIdentityHash=Hash(transport.RemoteId);wasConnected=true;SetPhase("authenticated");
            connectionType=transport.ConnectionType;
        }

        void Receive(byte[] bytes)
        {
            if(cleanupStarted||finished)return;
            try {
                if(!wasConnected)Connected();
                Require(bytes!=null&&bytes.Length>=HeaderBytes,"Received a truncated probe packet.");
                using(var input=new BinaryReader(new MemoryStream(bytes,false))){
                    Require(input.ReadUInt32()==Magic&&input.ReadByte()==1,"Received an unrelated probe packet.");
                    var kind=(Packet)input.ReadByte();int sender=input.ReadByte();byte hostFlag=input.ReadByte();
                    Require(hostFlag<=1,"Peer packet has an invalid network-host flag.");bool senderHost=hostFlag!=0;
                    int sequence=input.ReadInt32(),length=input.ReadInt32();
                    Require(sender==(coordinator?2:1)&&senderHost!=actualHost,"Peer packet has the wrong probe or network-host role.");
                    Require(length>=0&&length==bytes.Length-HeaderBytes,"Probe packet length is invalid.");
                    if(kind==Packet.Hello){
                        Require(!helloReceived&&sequence==-1&&length>0&&length<=128,"Unexpected identity greeting.");
                        string identity=new UTF8Encoding(false,true).GetString(input.ReadBytes(length));
                        Require(identity==transport.RemoteId&&identity!=transport.LocalId,"Greeting identity differs from the authenticated EOS peer.");
                        peerHost=senderHost;helloReceived=true;return;
                    }
                    // Unreliable frames can overtake the reliable probe greeting.
                    // The route already authenticated RemoteId; still verify the
                    // complete role/sequence/pattern before counting these frames.
                    Require(helloReceived||kind==Packet.Unreliable,"Received reliable game data before the peer identity greeting.");
                    if(kind==Packet.Reliable||kind==Packet.Unreliable){
                        if(kind==Packet.Reliable)Require(sequence==reliableReceived&&sequence<ReliableCount&&length==ReliableSizes[sequence%ReliableSizes.Length],"Reliable payloads arrived out of order or at the wrong size.");
                        else Require(sequence>=0&&sequence<UnreliableCount&&bytes.Length==UnreliableBytes,"Unreliable frame has an invalid sequence or size.");
                        for(int i=0;i<length;++i)Require(input.ReadByte()==Pattern(sender,sequence,i),"Received payload bytes differ from the sent pattern.");
                        if(kind==Packet.Reliable){++reliableReceived;reliableBytesReceived+=length;}
                        else {
                            if(!unreliableSeen.Add(sequence))++unreliableDuplicates;
                            if(sequence<lastUnreliable)++unreliableOutOfOrder;
                            lastUnreliable=Math.Max(lastUnreliable,sequence);unreliableBytesReceived+=bytes.Length;
                        }
                        return;
                    }
                    Require(sequence==-1,"Control packet sequence is invalid.");
                    if(kind==Packet.Done){
                        Require(!doneReceived&&length==8&&input.ReadInt32()==ReliableCount&&input.ReadInt32()==UnreliableCount,"Peer reported an incomplete send run.");
                        Require(reliableReceived==ReliableCount,"Peer completion overtook reliable payloads.");doneReceived=true;return;
                    }
                    Require(length==0,"Departure control has unexpected payload.");
                    switch(kind){
                    case Packet.LeaveRequest:Require(!coordinator&&!leaveRequestReceived,"Unexpected departure request.");leaveRequestReceived=true;break;
                    case Packet.LeaveAck:Require(coordinator&&leaveRequestSent&&!leaveAckReceived,"Unexpected departure acknowledgment.");leaveAckReceived=true;break;
                    case Packet.LeaveCommit:Require(!coordinator&&leaveAckSent&&!leaveCommitReceived,"Unexpected departure commit.");leaveCommitReceived=true;break;
                    default:throw new InvalidDataException("Unknown probe packet type.");
                    }
                }
            }catch(Exception error){Fail(error.Message);}
        }

        static byte Pattern(int sender,int sequence,int offset)=>unchecked((byte)(sender*71+sequence*29+offset*13));
        void SendPattern(Packet kind,int sequence,int count,bool reliable){
            var data=new byte[count];for(int i=0;i<count;++i)data[i]=Pattern(coordinator?1:2,sequence,i);Send(kind,sequence,data,reliable);
        }
        static byte[] Counts(){using(var memory=new MemoryStream())using(var writer=new BinaryWriter(memory)){writer.Write(ReliableCount);writer.Write(UnreliableCount);return memory.ToArray();}}
        void Send(Packet kind,int sequence,byte[] data,bool reliable){
            using(var memory=new MemoryStream(HeaderBytes+data.Length))using(var writer=new BinaryWriter(memory)){
                writer.Write(Magic);writer.Write((byte)1);writer.Write((byte)kind);writer.Write((byte)(coordinator?1:2));writer.Write((byte)(actualHost?1:0));
                writer.Write(sequence);writer.Write(data.Length);writer.Write(data);transport.Send(memory.ToArray(),reliable);
            }
        }
        void TransportError(string detail){if(cleanupStarted||finished||coordinator&&leaveCommitSent&&peerLeft)return;Fail(detail);}
        void Fail(string detail){
            if(finished||failure!=null)return;failure=Redact(detail);UnityEngine.Debug.LogError("Online probe failed: "+failure);
            if(started)BeginCleanup();else Complete();
        }
        void BeginCleanup(){
            if(cleanupStarted)return;cleanupStarted=true;cleanupAt=clock.Elapsed.TotalSeconds;SetPhase("leaving");
            transport?.Leave();localLobbyCleared=transport==null||!transport.Connected&&!transport.InLobby&&!transport.IsMatching;
            if(!localLobbyCleared&&failure==null)failure="Leave retained a local room or peer.";
        }
        void Complete(){
            if(finished)return;finished=true;
            bool passed=failure==null&&helloReceived&&doneSent&&doneReceived&&reliableSent==ReliableCount&&reliableReceived==ReliableCount&&unreliableSent==UnreliableCount&&unreliableSeen.Count>0&&localLobbyCleared&&(coordinator?peerLeft:leaveCommitReceived);
            if(coordinator&&loadStallRequested)passed=passed&&loadStallComplete&&loadStallSeconds>=12&&loadStallKeepAlivePumps>0&&reliableReceivedBeforeStall<ReliableCount;
            if(!passed&&failure==null)failure="Probe finished without all required checks.";
            transport?.Dispose();
            var report=new Report{passed=passed,authenticated=helloReceived,distinctIdentities=helloReceived&&localIdentityHash!=remoteIdentityHash,oppositeHostRoles=helloReceived&&actualHost!=peerHost,
                actualHost=actualHost,peerHost=peerHost,peerLeft=peerLeft,localLobbyCleared=localLobbyCleared,cleanupRequested=cleanupStarted,
                cancelRetryRequested=cancelRetryRequested,cancelRetryCount=cancelRetryCount,forceRelayRequested=forceRelayRequested,connectionType=connectionType,
                loadStallRequested=loadStallRequested,loadStallComplete=loadStallComplete,loadStallSeconds=loadStallSeconds,loadStallKeepAlivePumps=loadStallKeepAlivePumps,reliableReceivedBeforeStall=reliableReceivedBeforeStall,
                role=role,mode=mode,phase=phase,error=failure,scopeHash=scopeHash,localIdentityHash=localIdentityHash,remoteIdentityHash=remoteIdentityHash,
                reliableSent=reliableSent,reliableReceived=reliableReceived,unreliableSent=unreliableSent,unreliableReceived=unreliableSeen.Count,
                unreliableDuplicates=unreliableDuplicates,unreliableOutOfOrder=unreliableOutOfOrder,reliableBytesSent=reliableBytesSent,reliableBytesReceived=reliableBytesReceived,
                unreliableBytesReceived=unreliableBytesReceived,elapsedSeconds=clock.Elapsed.TotalSeconds};
            if(outputOwned){
                File.WriteAllText(Path.Combine(root,role+"-report.json"),JsonUtility.ToJson(report,true));
                File.WriteAllText(Path.Combine(root,role+(passed?"-PASS.txt":"-FAIL.txt")),passed?"PASS: authenticated EOS P2P transfer ("+connectionType+") and coordinated cleanup.\n":failure+"\n");
            }
            UnityEngine.Debug.Log((passed?"PASS":"FAIL")+" online probe "+role+" "+mode+" reliable="+reliableReceived+" unreliable="+unreliableSeen.Count);
#if UNITY_EDITOR
            UnityEditor.EditorApplication.isPlaying=false;
#else
            Application.Quit(passed?0:1);
#endif
        }
        void SetPhase(string value){if(phase==value)return;phase=value;if(outputOwned)File.WriteAllText(Path.Combine(root,role+"-phase.txt"),value+"\n");}
        void WriteNew(string name,string value){string path=Path.Combine(root,name),temporary=path+".new";File.WriteAllText(temporary,value);File.Move(temporary,path);}
        static string Argument(string[] args,string key){int at=Array.IndexOf(args,key);if(at<0||at+1>=args.Length||string.IsNullOrWhiteSpace(args[at+1]))throw new ArgumentException("Missing "+key+".");return args[at+1];}
        static string Hash(string value){using(var sha=SHA256.Create()){var bytes=sha.ComputeHash(Encoding.UTF8.GetBytes(value));var result=new StringBuilder(64);foreach(byte b in bytes)result.Append(b.ToString("x2"));return result.ToString();}}
        static void Require(bool condition,string message){if(!condition)throw new InvalidDataException(message);}
        static string Redact(string value){
            string result=Regex.Replace(value??"Unknown probe failure.",@"(?i)(bearer\s+)[^\s,;]+","$1[redacted]");
            result=Regex.Replace(result,@"(?i)((?:access|session|refresh|id)[_-]?token\s*[:=]\s*)[^\s,;]+","$1[redacted]");
            return result.Length>2000?result.Substring(0,2000):result;
        }
        void OnDestroy(){if(transport!=null){transport.Message-=Receive;transport.PeerChanged-=PeerChanged;transport.Error-=TransportError;transport.Dispose();transport=null;}}
    }
}
