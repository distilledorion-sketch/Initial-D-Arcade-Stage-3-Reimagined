using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace UnityEngine { public static class Time { public static double realtimeSinceStartupAsDouble => Idas3.Multiplayer.Idas3EosPlatform.Now; } }

namespace Epic.OnlineServices {
    public enum Result {Success,NoChange,NotFound,AccessDenied,ServiceFailure,TimedOut,TooManyRequests,LobbyTooManyPlayers,LobbyInvalidSession,OperationWillRetry}
    public static class Common {public static bool IsOperationComplete(Result result)=>result!=Result.OperationWillRetry;}
    public enum ComparisonOp {Equal,Greaterthanorequal}
    public sealed class ProductUserId {readonly string id;public ProductUserId(string value){id=value;}public bool IsValid()=>!string.IsNullOrEmpty(id);public static ProductUserId FromString(string value)=>string.IsNullOrEmpty(value)?null:new ProductUserId(value);public override string ToString()=>id;}
}
namespace Epic.OnlineServices.Lobby {
    public enum LobbyMemberStatus {Joined,Promoted,Left,Kicked,Closed}
    public enum LobbyPermissionLevel {Publicadvertised,Joinviapresence,Inviteonly}
    public enum LobbyAttributeVisibility {Public}
    public struct AddNotifyLobbyMemberStatusReceivedOptions {}
    public struct AddNotifyLobbyUpdateReceivedOptions {}
    public struct AddNotifyLobbyMemberUpdateReceivedOptions {}
    public struct LobbyMemberStatusReceivedCallbackInfo {public string LobbyId;public ProductUserId TargetUserId;public LobbyMemberStatus CurrentStatus;}
    public struct LobbyUpdateReceivedCallbackInfo {public string LobbyId;}
    public struct LobbyMemberUpdateReceivedCallbackInfo {public string LobbyId;}
    public struct CreateLobbyCallbackInfo {public Result ResultCode;public string LobbyId;}
    public struct JoinLobbyByIdCallbackInfo {public Result ResultCode;}
    public struct UpdateLobbyCallbackInfo {public Result ResultCode;}
    public struct DestroyLobbyCallbackInfo {public Result ResultCode;}
    public struct LeaveLobbyCallbackInfo {public Result ResultCode;}
    public struct LobbySearchFindCallbackInfo {public Result ResultCode;}
    public delegate void MemberStatusCallback(ref LobbyMemberStatusReceivedCallbackInfo info);
    public delegate void LobbyUpdatedCallback(ref LobbyUpdateReceivedCallbackInfo info);
    public delegate void MemberUpdatedCallback(ref LobbyMemberUpdateReceivedCallbackInfo info);
    public delegate void CreateCallback(ref CreateLobbyCallbackInfo info);
    public delegate void JoinCallback(ref JoinLobbyByIdCallbackInfo info);
    public delegate void UpdateCallback(ref UpdateLobbyCallbackInfo info);
    public delegate void DestroyCallback(ref DestroyLobbyCallbackInfo info);
    public delegate void LeaveCallback(ref LeaveLobbyCallbackInfo info);
    public delegate void SearchCallback(ref LobbySearchFindCallbackInfo info);
    public struct CreateLobbyOptions {public ProductUserId LocalUserId;public uint MaxLobbyMembers;public LobbyPermissionLevel PermissionLevel;public string BucketId,LobbyId;public bool DisableHostMigration,EnableJoinById,PresenceEnabled,EnableRTCRoom,AllowInvites,RejoinAfterKickRequiresInvite;}
    public struct JoinLobbyByIdOptions {public string LobbyId;public ProductUserId LocalUserId;public bool PresenceEnabled;}
    public struct UpdateLobbyModificationOptions {public string LobbyId;public ProductUserId LocalUserId;}
    public struct UpdateLobbyOptions {public LobbyModification LobbyModificationHandle;}
    public struct AttributeDataValue {public string AsUtf8;public long AsInt64;}
    public struct AttributeData {public string Key;public AttributeDataValue Value;}
    public struct Attribute {public AttributeData? Data;}
    public struct LobbyModificationAddMemberAttributeOptions {public AttributeData Attribute;public LobbyAttributeVisibility Visibility;}
    public struct LobbyModificationAddAttributeOptions {public AttributeData Attribute;public LobbyAttributeVisibility Visibility;}
    public struct CreateLobbySearchOptions {public uint MaxResults;}
    public struct LobbySearchSetParameterOptions {public AttributeData Parameter;public ComparisonOp ComparisonOp;}
    public struct LobbySearchFindOptions {public ProductUserId LocalUserId;}
    public struct LobbySearchGetSearchResultCountOptions {}
    public struct LobbySearchCopySearchResultByIndexOptions {public uint LobbyIndex;}
    public struct CopyLobbyDetailsHandleOptions {public ProductUserId LocalUserId;public string LobbyId;}
    public struct LobbyDetailsCopyInfoOptions {}
    public struct LobbyDetailsInfo {public string LobbyId;public ProductUserId LobbyOwnerUserId;public uint MaxMembers,AvailableSlots;public LobbyPermissionLevel PermissionLevel;}
    public struct LobbyDetailsCopyAttributeByKeyOptions {public string AttrKey;}
    public struct LobbyDetailsCopyMemberAttributeByKeyOptions {public string AttrKey;public ProductUserId TargetUserId;}
    public struct LobbyDetailsGetMemberCountOptions {}
    public struct LobbyDetailsGetMemberByIndexOptions {public uint MemberIndex;}
    public struct DestroyLobbyOptions {public string LobbyId;public ProductUserId LocalUserId;}
    public struct LeaveLobbyOptions {public string LobbyId;public ProductUserId LocalUserId;}
    internal sealed class FakeRoom {
        internal LobbyDetailsInfo Info;
        internal readonly List<string> Members=new List<string>();
        internal readonly Dictionary<string,string> Attributes=new Dictionary<string,string>();
        internal readonly Dictionary<string,Dictionary<string,string>> MemberAttributes=new Dictionary<string,Dictionary<string,string>>();
    }
    public sealed class LobbyModification {
        internal readonly string Id;
        internal readonly List<AttributeData> Attributes=new List<AttributeData>(),MemberAttributes=new List<AttributeData>();
        internal LobbyModification(string id){Id=id;}
        public Result AddAttribute(ref LobbyModificationAddAttributeOptions options){Attributes.Add(options.Attribute);return Result.Success;}
        public Result AddMemberAttribute(ref LobbyModificationAddMemberAttributeOptions options){MemberAttributes.Add(options.Attribute);return Result.Success;}
        public void Release(){}
    }
    public sealed class LobbyDetails {
        readonly FakeRoom room;internal LobbyDetails(FakeRoom value){room=value;}
        public Result CopyInfo(ref LobbyDetailsCopyInfoOptions options,out LobbyDetailsInfo? info){info=room.Info;return Result.Success;}
        public uint GetMemberCount(ref LobbyDetailsGetMemberCountOptions options)=>(uint)room.Members.Count;
        public ProductUserId GetMemberByIndex(ref LobbyDetailsGetMemberByIndexOptions options)=>new ProductUserId(room.Members[(int)options.MemberIndex]);
        public Result CopyAttributeByKey(ref LobbyDetailsCopyAttributeByKeyOptions options,out Attribute? value)=>Read(room.Attributes,options.AttrKey,out value);
        public Result CopyMemberAttributeByKey(ref LobbyDetailsCopyMemberAttributeByKeyOptions options,out Attribute? value){room.MemberAttributes.TryGetValue(options.TargetUserId.ToString(),out var values);return Read(values,options.AttrKey,out value);}
        static Result Read(Dictionary<string,string> values,string key,out Attribute? value){if(values!=null&&values.TryGetValue(key,out string text)){value=new Attribute{Data=new AttributeData{Key=key,Value=new AttributeDataValue{AsUtf8=text}}};return Result.Success;}value=null;return Result.NotFound;}
        public void Release(){}
    }
    public sealed class LobbySearch {
        readonly List<FakeRoom> results;readonly List<LobbySearchSetParameterOptions> parameters;
        internal LobbySearch(List<FakeRoom> rooms,List<LobbySearchSetParameterOptions> filters){results=rooms;parameters=filters;}
        public Result SetParameter(ref LobbySearchSetParameterOptions options){parameters.Add(options);return Result.Success;}
        public void Find(ref LobbySearchFindOptions options,object ignored,SearchCallback callback){var info=new LobbySearchFindCallbackInfo{ResultCode=Result.Success};callback(ref info);}
        public uint GetSearchResultCount(ref LobbySearchGetSearchResultCountOptions options)=>(uint)results.Count;
        public Result CopySearchResultByIndex(ref LobbySearchCopySearchResultByIndexOptions options,out LobbyDetails details){details=new LobbyDetails(results[(int)options.LobbyIndex]);return Result.Success;}
        public void Release(){}
    }
    public sealed class LobbyInterface {
        public const string SEARCH_BUCKET_ID="bucket",SEARCH_MINSLOTSAVAILABLE="slots";
        internal readonly Dictionary<string,FakeRoom> Rooms=new Dictionary<string,FakeRoom>();
        internal readonly List<FakeRoom> SearchResults=new List<FakeRoom>();
        internal readonly List<LobbySearchSetParameterOptions> SearchParameters=new List<LobbySearchSetParameterOptions>();
        internal readonly List<CreateLobbyOptions> Creates=new List<CreateLobbyOptions>();
        internal readonly List<string> Joins=new List<string>(),Destroys=new List<string>(),Leaves=new List<string>();
        internal readonly Queue<CreateCallback> CreateCallbacks=new Queue<CreateCallback>();
        internal readonly Queue<JoinCallback> JoinCallbacks=new Queue<JoinCallback>();
        internal readonly Queue<DestroyCallback> DestroyCallbacks=new Queue<DestroyCallback>();
        internal readonly Queue<LeaveCallback> LeaveCallbacks=new Queue<LeaveCallback>();
        internal MemberStatusCallback MemberStatus;
        internal LobbyUpdatedCallback Updated;
        internal MemberUpdatedCallback MemberUpdated;
        internal int Updates;
        public ulong AddNotifyLobbyMemberStatusReceived(ref AddNotifyLobbyMemberStatusReceivedOptions options,object ignored,MemberStatusCallback callback){MemberStatus=callback;return 1;}
        public ulong AddNotifyLobbyUpdateReceived(ref AddNotifyLobbyUpdateReceivedOptions options,object ignored,LobbyUpdatedCallback callback){Updated=callback;return 2;}
        public ulong AddNotifyLobbyMemberUpdateReceived(ref AddNotifyLobbyMemberUpdateReceivedOptions options,object ignored,MemberUpdatedCallback callback){MemberUpdated=callback;return 3;}
        public void RemoveNotifyLobbyMemberStatusReceived(ulong id){}
        public void RemoveNotifyLobbyUpdateReceived(ulong id){}
        public void RemoveNotifyLobbyMemberUpdateReceived(ulong id){}
        public void CreateLobby(ref CreateLobbyOptions options,object ignored,CreateCallback callback){Creates.Add(options);CreateCallbacks.Enqueue(callback);}
        public void JoinLobbyById(ref JoinLobbyByIdOptions options,object ignored,JoinCallback callback){Joins.Add(options.LobbyId);JoinCallbacks.Enqueue(callback);}
        public void DestroyLobby(ref DestroyLobbyOptions options,object ignored,DestroyCallback callback){Destroys.Add(options.LobbyId);DestroyCallbacks.Enqueue(callback);}
        public void LeaveLobby(ref LeaveLobbyOptions options,object ignored,LeaveCallback callback){Leaves.Add(options.LobbyId);LeaveCallbacks.Enqueue(callback);}
        public Result UpdateLobbyModification(ref UpdateLobbyModificationOptions options,out LobbyModification edit){edit=new LobbyModification(options.LobbyId);return Result.Success;}
        public void UpdateLobby(ref UpdateLobbyOptions options,object ignored,UpdateCallback callback){
            ++Updates;var edit=options.LobbyModificationHandle;var room=Rooms[edit.Id];
            foreach(var item in edit.Attributes)room.Attributes[item.Key]=item.Value.AsUtf8;
            if(!room.MemberAttributes.TryGetValue("local",out var member)){member=new Dictionary<string,string>();room.MemberAttributes.Add("local",member);}
            foreach(var item in edit.MemberAttributes)member[item.Key]=item.Value.AsUtf8;
            var info=new UpdateLobbyCallbackInfo{ResultCode=Result.Success};callback(ref info);
        }
        public Result CreateLobbySearch(ref CreateLobbySearchOptions options,out LobbySearch search){search=new LobbySearch(SearchResults,SearchParameters);return Result.Success;}
        public Result CopyLobbyDetailsHandle(ref CopyLobbyDetailsHandleOptions options,out LobbyDetails details){if(Rooms.TryGetValue(options.LobbyId,out var room)){details=new LobbyDetails(room);return Result.Success;}details=null;return Result.NotFound;}
        internal void CompleteCreate(Result result,string id){
            if(result==Result.Success){var spec=Creates[Creates.Count-1];var room=new FakeRoom{Info=new LobbyDetailsInfo{LobbyId=id,LobbyOwnerUserId=new ProductUserId("local"),MaxMembers=2,AvailableSlots=1,PermissionLevel=spec.PermissionLevel}};room.Members.Add("local");Rooms[id]=room;}
            var info=new CreateLobbyCallbackInfo{ResultCode=result,LobbyId=id};CreateCallbacks.Dequeue()(ref info);
        }
        internal void CompleteJoin(Result result){var info=new JoinLobbyByIdCallbackInfo{ResultCode=result};JoinCallbacks.Dequeue()(ref info);}
        internal void CompleteDestroy(Result result){var info=new DestroyLobbyCallbackInfo{ResultCode=result};DestroyCallbacks.Dequeue()(ref info);}
        internal void CompleteLeave(Result result){var info=new LeaveLobbyCallbackInfo{ResultCode=result};LeaveCallbacks.Dequeue()(ref info);}
        internal void Notify(string room,string user,LobbyMemberStatus state){var info=new LobbyMemberStatusReceivedCallbackInfo{LobbyId=room,TargetUserId=new ProductUserId(user),CurrentStatus=state};MemberStatus(ref info);}
    }
}
namespace Idas3.Multiplayer {
    // Activity has its own SDK integration checks; keep this transport fixture
    // focused on cancellation, sanctions, and authenticated race membership.
    internal sealed class Idas3EosActivity : IDisposable {
        internal Idas3EosActivity(Idas3EosTransport owner, Epic.OnlineServices.Lobby.LobbyInterface lobby, Epic.OnlineServices.ProductUserId user, Func<double> clock) {}
        internal Idas3OnlineActivity Snapshot => default;
        internal void Poll(bool publish, bool requested, string state) {}
        public void Dispose() {}
    }
    internal sealed class FakeEosHandle {
        internal readonly Epic.OnlineServices.Lobby.LobbyInterface Lobby=new Epic.OnlineServices.Lobby.LobbyInterface();
        internal readonly Epic.OnlineServices.Sanctions.SanctionsInterface Sanctions=new Epic.OnlineServices.Sanctions.SanctionsInterface();
        internal Epic.OnlineServices.Lobby.LobbyInterface GetLobbyInterface()=>Lobby;
        internal Epic.OnlineServices.Sanctions.SanctionsInterface GetSanctionsInterface()=>Sanctions;
    }
    internal static class Idas3EosPlatform {
        internal static readonly FakeEosHandle Handle=new FakeEosHandle();
        internal static Epic.OnlineServices.ProductUserId User=new Epic.OnlineServices.ProductUserId("local");
        internal static bool SignedIn=true,IsStopping=false;
        internal static string Failure=null;
        internal static double Now=100;
        static readonly Queue<Action> callbacks=new Queue<Action>();
        internal static Task InitializeAsync(Idas3EosOnlineConfig config)=>Task.CompletedTask;
        internal static void Defer(Action callback){callbacks.Enqueue(callback);}
        internal static void Tick(bool dispatch){if(dispatch)while(callbacks.Count>0)callbacks.Dequeue()();}
        internal static void Check(Epic.OnlineServices.Result result,string operation){if(result!=Epic.OnlineServices.Result.Success&&result!=Epic.OnlineServices.Result.NoChange)throw new Idas3EosServiceException(result,operation);}
    }
    internal sealed class Idas3EosServiceException:InvalidOperationException {
        internal Epic.OnlineServices.Result ResultCode{get;}
        internal Idas3EosServiceException(Epic.OnlineServices.Result result,string operation):base(operation+" failed ("+result+")."){ResultCode=result;}
    }
    internal sealed class Idas3EosOnlineConfig {internal static bool DiagnosticRun=>false;internal static Idas3EosOnlineConfig Load()=>new Idas3EosOnlineConfig();internal static string PlayerName()=>"Fixture driver";}
    internal sealed class Idas3EosP2P:IDisposable {
        readonly Idas3EosTransport owner;
        internal bool Connected{get;private set;}=true;
        internal string RemoteId{get;}internal string RemoteName{get;}internal string ConnectionType=>"direct";
        internal string LocalResetStamp{get;}internal string RemoteResetStamp{get;}
        internal long BufferedEvents=>0;
        internal bool Disposed;internal int Polls;
        internal byte[] PendingMessage;
        internal Idas3EosP2P(Idas3EosTransport transport,Epic.OnlineServices.ProductUserId peer,string name,string room,string build,bool host){owner=transport;RemoteId=peer.ToString();RemoteName=name;LocalResetStamp=owner.ResetStamp(Idas3EosPlatform.User);RemoteResetStamp=owner.ResetStamp(peer);}
        internal bool KeepAliveOnly()=>true;
        internal void Poll(){++Polls;if(PendingMessage!=null){var data=PendingMessage;PendingMessage=null;owner.Deliver(this,data);}}
        internal void SendGame(byte[] data,bool reliable){}
        internal void CloseFromSdk(){Connected=false;}
        public void Dispose(){Disposed=true;Connected=false;}
    }
}
