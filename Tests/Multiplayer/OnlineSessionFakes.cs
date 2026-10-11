using System;
using System.Collections.Generic;

namespace Idas3.Multiplayer
{
    // Service boundary only. Production session, packet parser, car validation,
    // records and LAN transport are compiled alongside this deterministic fake.
    public sealed class Idas3EosTransport : IIdas3MatchmakingTransport,
        IIdas3QueuedMatchmakingTransport, IIdas3OnlineActivityTransport
    {
        public string Kind=>"Online";
        internal Func<string,string[],string[],bool> ApplyStatsResets {get;set;}
        public bool Available {get;private set;}
        public bool Connected {get;private set;}
        public bool IsHost {get;private set;}
        public bool IsBusy {get;private set;}
        public bool IsMatching {get;private set;}
        public bool InLobby=>RoomCode.Length!=0;
        public ulong RoomOrder=>InLobby?1UL:0UL;
        public int RoomMembers=>Connected?2:InLobby?1:0;
        public string LocalId=>"fixture-local";
        public string LocalName=>"Local driver";
        internal string PeerIdentity="fixture-peer";
        public string RemoteId=>Connected?PeerIdentity:"";
        public string RemoteName=>Connected?"Other driver":"";
        public string RoomCode {get;private set;}="";
        public string Status {get;private set;}="Not initialized";
        public string BuildCompatibility {get;set;}
        public IReadOnlyList<Idas3Room> Rooms=>Array.Empty<Idas3Room>();
        public bool ActivityRequested {get;set;}
        public bool PublishActivity {get;set;}
        public string ActivityState {get;set;}
        public Idas3OnlineActivity Activity=>default;
        public event Action<byte[]> Message;
        public event Action PeerChanged;
        public event Action<string> Error;
        internal int Initializations,Polls,Starts,Cancellations,Leaves,Disposals,NameRefreshes;
        internal void RefreshPlayerName(){++NameRefreshes;}
        internal bool FinishInitialization,ReentrantCancellation;
        internal Action PollAction;
        internal readonly List<byte[]> Sent=new List<byte[]>();
        public bool Initialize(){
            if(Available||IsBusy)return Available;
            ++Initializations;IsBusy=true;Status="Signing in";return false;
        }
        public void Poll(){
            ++Polls;
            if(FinishInitialization){FinishInitialization=false;Available=true;IsBusy=false;Status="Online ready";}
            var action=PollAction;PollAction=null;action?.Invoke();
        }
        public void StartMatchmaking(){++Starts;IsMatching=IsBusy=true;Status="Finding a compatible driver";}
        public void CancelMatchmaking(){
            ++Cancellations;IsMatching=IsBusy=false;
            if(ReentrantCancellation){Error?.Invoke("Late cancelled operation");PeerChanged?.Invoke();}
        }
        internal void Fail(string reason)=>Error?.Invoke(reason);
        internal void AttachRoom(){IsHost=true;RoomCode="ABC123";if(!IsMatching)IsBusy=false;}
        internal void EstablishRoute(){
            IsMatching=IsBusy=false;Connected=IsHost=true;RoomCode="ABC123";
            Status="Peer route established";PeerChanged?.Invoke();
        }
        internal void Deliver(byte[] packet)=>Message?.Invoke(packet);
        public void Leave(){++Leaves;IsMatching=IsBusy=Connected=IsHost=false;RoomCode="";PeerChanged?.Invoke();}
        public void Dispose(){++Disposals;Available=false;}
        public void Send(byte[] data,bool reliable)=>Sent.Add((byte[])data.Clone());
        public void Host(string name){IsBusy=true;Status="Creating room";}
        public void HostQuickMatch(string name)=>throw new InvalidOperationException("Queue must not create a fallback room");
        public void Join(string code)=>throw new InvalidOperationException("Queue must not join a discovered room");
        public void Browse()=>throw new InvalidOperationException("Queue must not use legacy discovery");
    }

    // No native library is loaded. Only reading one synthetic stock car is
    // allowed; entering the real simulation makes this fixture fail loudly.
    internal static class Idas3MultiplayerNative
    {
        static Exception Unexpected()=>new InvalidOperationException("Lifecycle fixture must not enter the native race or save progression");
        internal static int Idas3MultiplayerReadCar(int selection,uint[] words,uint count){
            if(selection!=13)return 0;words[4]=8;return 2;
        }
        internal static int Idas3MultiplayerCurrentCar()=>13;
        internal static int Idas3MultiplayerEnableAuthorityRules(ulong race,int automatic,int boost,int collisions)=>throw Unexpected();
        internal static int Idas3MultiplayerAuthorityPacket(byte[] bytes,uint count)=>throw Unexpected();
        internal static int Idas3MultiplayerAuthorityReceive(byte[] bytes,uint count)=>throw Unexpected();
        internal static int Idas3MultiplayerSetRemoteHeadlights(ulong sequence,uint enabled)=>throw Unexpected();
        internal static int Idas3MultiplayerAuthorityStatus(ref Idas3AuthorityStatus status)=>throw Unexpected();
        internal static int Idas3MultiplayerGetRemoteSnapshot(ref Idas3CarSnapshot snapshot)=>throw Unexpected();
        internal static int Idas3MultiplayerStartSaved(ref Idas3MultiplayerConfig config,int selection,uint[] local,uint[] remote,uint count)=>throw Unexpected();
        internal static int Idas3MultiplayerSetBattleRecords(ref Idas3BattleRecord local,ref Idas3BattleRecord remote)=>throw Unexpected();
        internal static int Idas3MultiplayerSetGo(int released)=>throw Unexpected();
        internal static int Idas3MultiplayerSetResult(int winner)=>throw Unexpected();
        internal static int Idas3MultiplayerPointsEarned()=>throw Unexpected();
        internal static int Idas3MultiplayerResultRecord(int points,uint level)=>throw Unexpected();
#if IDAS3_RECORD_STORAGE_INTEGRATION
        internal static int Disconnects,NativeLeaves;
        internal static Action OnDisconnect,OnNativeLeave;
        internal static int Idas3MultiplayerDisconnect(){++Disconnects;OnDisconnect?.Invoke();return 1;}
        internal static int Idas3MultiplayerGetLocalSnapshot(ref Idas3CarSnapshot snapshot)=>1;
#else
        internal static int Idas3MultiplayerDisconnect()=>throw Unexpected();
        internal static int Idas3MultiplayerGetLocalSnapshot(ref Idas3CarSnapshot snapshot)=>throw Unexpected();
#endif
        internal static int Idas3MultiplayerApplyRemoteSnapshot(ref Idas3CarSnapshot snapshot)=>throw Unexpected();
#if IDAS3_RECORD_STORAGE_INTEGRATION
        internal static int Idas3MultiplayerLeave(){++NativeLeaves;OnNativeLeave?.Invoke();return 1;}
#else
        internal static int Idas3MultiplayerLeave()=>throw Unexpected();
#endif
    }
}
