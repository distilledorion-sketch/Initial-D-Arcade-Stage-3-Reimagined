// Deterministic API boundary for the production Steam adapter. No Steam login,
// public lobby, player files or real network traffic is used by this fixture.
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace UnityEngine {
    public static class Time { public static double realtimeSinceStartupAsDouble; }
    public static class Debug { public static void Log(object value) {} public static void LogWarning(object value) {} }
}
namespace Idas3.Multiplayer {
    internal sealed class Idas3SteamInputConfig { internal void Initialize() {} internal void Dispose() {} }
    public struct Idas3OnlineActivity { }
    internal sealed class Idas3SteamActivity : IDisposable {
        internal bool Requested;
        internal Idas3SteamActivity(ulong local,string scope) {}
        internal Idas3OnlineActivity Get(double now)=>default;
        internal void SetState(string state) {} internal void Poll(double now,bool busy) {}
        internal void CancelSearch() {} public void Dispose() {}
    }
}
namespace Steamworks {
    public enum EResult { k_EResultOK, k_EResultFail, k_EResultNoConnection, k_EResultLimitExceeded, k_EResultIgnored, k_EResultInvalidParam }
    public enum ESteamAPIInitResult { k_ESteamAPIInitResult_OK }
    public enum ELobbyType { k_ELobbyTypePublic }
    public enum ELobbyDistanceFilter { k_ELobbyDistanceFilterWorldwide, k_ELobbyDistanceFilterDefault, k_ELobbyDistanceFilterClose }
    public enum ELobbyComparison { k_ELobbyComparisonEqual }
    public enum EChatRoomEnterResponse { k_EChatRoomEnterResponseSuccess=1 }
    [Flags] public enum EChatMemberStateChange { k_EChatMemberStateChangeLeft=2, k_EChatMemberStateChangeDisconnected=4, k_EChatMemberStateChangeKicked=8, k_EChatMemberStateChangeBanned=16 }
    public struct CSteamID : IEquatable<CSteamID> {
        public ulong m_SteamID; public CSteamID(ulong id){m_SteamID=id;} public bool IsLobby()=>m_SteamID>=100;
        public static bool operator==(CSteamID a,CSteamID b)=>a.m_SteamID==b.m_SteamID;
        public static bool operator!=(CSteamID a,CSteamID b)=>!(a==b);
        public bool Equals(CSteamID other)=>this==other; public override bool Equals(object o)=>o is CSteamID id&&this==id;
        public override int GetHashCode()=>m_SteamID.GetHashCode();
    }
    public struct SteamAPICall_t : IEquatable<SteamAPICall_t> {
        public ulong Value; public static SteamAPICall_t Invalid=>default;
        public static bool operator==(SteamAPICall_t a,SteamAPICall_t b)=>a.Value==b.Value;
        public static bool operator!=(SteamAPICall_t a,SteamAPICall_t b)=>!(a==b);
        public bool Equals(SteamAPICall_t other)=>this==other;public override bool Equals(object o)=>o is SteamAPICall_t call&&this==call;
        public override int GetHashCode()=>Value.GetHashCode();
    }
    public struct AppId_t { public uint m_AppId; }
    public struct LobbyCreated_t { public ulong m_ulSteamIDLobby;public EResult m_eResult; }
    public struct LobbyEnter_t { public ulong m_ulSteamIDLobby;public uint m_EChatRoomEnterResponse; }
    public struct LobbyDataUpdate_t { public ulong m_ulSteamIDLobby,m_ulSteamIDMember;public byte m_bSuccess; }
    public struct LobbyChatUpdate_t { public ulong m_ulSteamIDLobby,m_ulSteamIDUserChanged;public uint m_rgfChatMemberStateChange; }
    public struct LobbyMatchList_t { public uint m_nLobbiesMatching; }
    public struct SteamServersDisconnected_t { public EResult m_eResult; }
    public struct SteamServersConnected_t { }
    public struct SteamNetworkingIdentity { ulong id; public void SetSteamID64(ulong value){id=value;}public ulong GetSteamID64()=>id; }
    public struct SteamNetConnectionInfo_t { public SteamNetworkingIdentity m_identityRemote;public int m_eEndReason;public string m_szEndDebug; }
    public struct SteamNetworkingMessagesSessionFailed_t { public SteamNetConnectionInfo_t m_info; }
    public struct SteamNetworkingMessagesSessionRequest_t { public SteamNetworkingIdentity m_identityRemote; }
    public sealed class Callback<T> : IDisposable {
        static readonly List<Callback<T>> handlers=new List<Callback<T>>();readonly Action<T> action;
        Callback(Action<T> action){this.action=action;handlers.Add(this);}
        public static Callback<T> Create(Action<T> action)=>new Callback<T>(action);
        public static void Raise(T value){foreach(var h in handlers.ToArray())h.action(value);}
        public void Dispose(){handlers.Remove(this);}
    }
    public sealed class CallResult<T> : IDisposable {
        public static CallResult<T> Last;readonly Action<T,bool> action;public bool Disposed;
        CallResult(Action<T,bool> action){this.action=action;Last=this;}
        public static CallResult<T> Create(Action<T,bool> action)=>new CallResult<T>(action);
        public void Set(SteamAPICall_t call){}
        public void Complete(T value,bool failed=false){if(!Disposed)action(value,failed);}
        public void DeliverStale(T value,bool failed=false){action(value,failed);}
        public void Dispose(){Disposed=true;}
    }
    public static class SteamAPI {
        public static ESteamAPIInitResult InitEx(out string detail){detail="";return ESteamAPIInitResult.k_ESteamAPIInitResult_OK;}
        public static void RunCallbacks(){} public static void Shutdown(){}
    }
    public static class SteamUser { public static bool Online=true; public static bool BLoggedOn()=>Online;public static CSteamID GetSteamID()=>new CSteamID(1); }
    public static class SteamUtils { public static AppId_t GetAppID()=>new AppId_t{m_AppId=480}; }
    public static class SteamFriends { public static string GetPersonaName()=>"Host"; public static string GetFriendPersonaName(CSteamID id)=>"Guest"; }
    public static class SteamNetworkingUtils { public static void InitRelayNetworkAccess(){} }
    public static class Constants { public const int k_nSteamNetworkingSend_ReliableNoNagle=9,k_nSteamNetworkingSend_UnreliableNoDelay=5; }
    public static class SteamMatchmaking {
        public static readonly Dictionary<string,string> Data=new Dictionary<string,string>();
        public static readonly Dictionary<ulong,string> Protocols=new Dictionary<ulong,string>();
        public static readonly List<ulong> Members=new List<ulong>();
        public static ulong Owner=1;public static int Left,Searches;static ulong nextCall;
        static SteamAPICall_t Next()=>new SteamAPICall_t{Value=++nextCall};
        public static void Reset(){Data.Clear();Protocols.Clear();Members.Clear();Members.Add(1);Owner=1;Left=Searches=0;SteamUser.Online=true;SteamNetworkingMessages.Reset();}
        public static SteamAPICall_t CreateLobby(ELobbyType type,int limit)=>Next();
        public static bool SetLobbyJoinable(CSteamID lobby,bool enabled)=>true;
        public static bool SetLobbyData(CSteamID lobby,string key,string value){Data[key]=value;return true;}
        public static string GetLobbyData(CSteamID lobby,string key)=>Data.TryGetValue(key,out var value)?value:"";
        public static void SetLobbyMemberData(CSteamID lobby,string key,string value){Protocols[1]=value;}
        public static string GetLobbyMemberData(CSteamID lobby,CSteamID member,string key)=>Protocols.TryGetValue(member.m_SteamID,out var value)?value:"";
        public static int GetNumLobbyMembers(CSteamID lobby)=>Members.Count;
        public static int GetLobbyMemberLimit(CSteamID lobby)=>2;
        public static CSteamID GetLobbyMemberByIndex(CSteamID lobby,int index)=>new CSteamID(Members[index]);
        public static CSteamID GetLobbyOwner(CSteamID lobby)=>new CSteamID(Owner);
        public static void LeaveLobby(CSteamID lobby){++Left;}
        public static bool RequestLobbyData(CSteamID lobby)=>true;
        public static SteamAPICall_t JoinLobby(CSteamID lobby)=>Next();
        public static void AddRequestLobbyListStringFilter(string key,string value,ELobbyComparison comparison){}
        public static void AddRequestLobbyListFilterSlotsAvailable(int slots){}
        public static void AddRequestLobbyListDistanceFilter(ELobbyDistanceFilter distance){}
        public static void AddRequestLobbyListResultCountFilter(int max){}
        public static SteamAPICall_t RequestLobbyList(){++Searches;return Next();}
        public static CSteamID GetLobbyByIndex(int index)=>new CSteamID(100+(ulong)index);
    }
    public sealed class SteamNetworkingMessage_t {
        public SteamNetworkingIdentity m_identityPeer;public int m_cbSize;public IntPtr m_pData;
        internal static readonly Dictionary<IntPtr,SteamNetworkingMessage_t> Messages=new Dictionary<IntPtr,SteamNetworkingMessage_t>();
        public static SteamNetworkingMessage_t FromIntPtr(IntPtr pointer)=>Messages[pointer];
        public static void Release(IntPtr pointer){Marshal.FreeHGlobal(Messages[pointer].m_pData);Messages.Remove(pointer);}
    }
    public static class SteamNetworkingMessages {
        public static readonly Queue<EResult> Results=new Queue<EResult>();
        public static readonly List<byte[]> Sent=new List<byte[]>();public static readonly List<int> Flags=new List<int>();
        static readonly Queue<IntPtr> incoming=new Queue<IntPtr>();static int next=1;
        public static int Closed,Accepted;public static EResult DefaultResult=EResult.k_EResultOK;
        public static void Reset(){Results.Clear();Sent.Clear();Flags.Clear();Closed=Accepted=0;DefaultResult=EResult.k_EResultOK;while(incoming.Count>0)SteamNetworkingMessage_t.Release(incoming.Dequeue());}
        public static void Receive(ulong peer,byte[] bytes){var id=new SteamNetworkingIdentity();id.SetSteamID64(peer);var p=new IntPtr(next++);var data=Marshal.AllocHGlobal(bytes.Length);Marshal.Copy(bytes,0,data,bytes.Length);SteamNetworkingMessage_t.Messages[p]=new SteamNetworkingMessage_t{m_identityPeer=id,m_cbSize=bytes.Length,m_pData=data};incoming.Enqueue(p);}
        public static EResult SendMessageToUser(ref SteamNetworkingIdentity identity,IntPtr data,uint size,int flags,int channel){
            var result=Results.Count>0?Results.Dequeue():DefaultResult;
            if(result==EResult.k_EResultOK){var bytes=new byte[size];Marshal.Copy(data,bytes,0,bytes.Length);Sent.Add(bytes);Flags.Add(flags);}return result;
        }
        public static int ReceiveMessagesOnChannel(int channel,IntPtr[] output,int max){int count=0;while(count<max&&incoming.Count>0)output[count++]=incoming.Dequeue();return count;}
        public static bool AcceptSessionWithUser(ref SteamNetworkingIdentity identity){++Accepted;return true;}
        public static bool CloseSessionWithUser(ref SteamNetworkingIdentity identity){++Closed;return true;}
    }
}
