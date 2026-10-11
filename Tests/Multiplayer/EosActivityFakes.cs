using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
namespace Epic.OnlineServices {
    public enum Result { Success, NoChange, NotFound, LobbyInvalidSession, ServiceFailure }
    public enum ComparisonOp { Equal, Greaterthanorequal }
    public sealed class ProductUserId { readonly string id; public ProductUserId(string value){id=value;} public bool IsValid()=>!string.IsNullOrEmpty(id); public override string ToString()=>id; }
}
namespace Epic.OnlineServices.Lobby {
    public enum LobbyPermissionLevel { Publicadvertised, Inviteonly }
    public enum LobbyAttributeVisibility { Public }
    public struct AttributeDataValue { public long? AsInt64; public string AsUtf8; }
    public struct AttributeData { public string Key; public AttributeDataValue Value; }
    public struct Attribute { public AttributeData? Data; }
    public struct CreateLobbyOptions { public ProductUserId LocalUserId; public uint MaxLobbyMembers; public LobbyPermissionLevel PermissionLevel; public string BucketId; public bool DisableHostMigration,EnableJoinById,PresenceEnabled,EnableRTCRoom,AllowInvites,RejoinAfterKickRequiresInvite; }
    public struct CreateLobbyCallbackInfo { public Result ResultCode; public string LobbyId; }
    public struct UpdateLobbyModificationOptions { public string LobbyId; public ProductUserId LocalUserId; }
    public struct UpdateLobbyOptions { public LobbyModification LobbyModificationHandle; }
    public struct UpdateLobbyCallbackInfo { public Result ResultCode; }
    public struct LobbyModificationAddAttributeOptions { public AttributeData Attribute; public LobbyAttributeVisibility Visibility; }
    public struct CreateLobbySearchOptions { public uint MaxResults; }
    public struct LobbySearchSetParameterOptions { public AttributeData Parameter; public ComparisonOp ComparisonOp; }
    public struct LobbySearchFindOptions { public ProductUserId LocalUserId; }
    public struct LobbySearchFindCallbackInfo { public Result ResultCode; }
    public struct LobbySearchGetSearchResultCountOptions { }
    public struct LobbySearchCopySearchResultByIndexOptions { public uint LobbyIndex; }
    public struct LobbyDetailsCopyInfoOptions { }
    public struct LobbyDetailsInfo { public string BucketId; public ProductUserId LobbyOwnerUserId; public uint MaxMembers,AvailableSlots; public LobbyPermissionLevel PermissionLevel; }
    public struct LobbyDetailsCopyAttributeByKeyOptions { public string AttrKey; }
    public struct DestroyLobbyOptions { public string LobbyId; public ProductUserId LocalUserId; }
    public struct DestroyLobbyCallbackInfo { public Result ResultCode; }
    public delegate void CreateCallback(ref CreateLobbyCallbackInfo info);
    public delegate void UpdateCallback(ref UpdateLobbyCallbackInfo info);
    public delegate void FindCallback(ref LobbySearchFindCallbackInfo info);
    public delegate void DestroyCallback(ref DestroyLobbyCallbackInfo info);
    public sealed class LobbyModification {
        internal readonly Dictionary<string,AttributeDataValue> Values=new Dictionary<string,AttributeDataValue>();
        internal int Releases;
        public Result AddAttribute(ref LobbyModificationAddAttributeOptions options){Values[options.Attribute.Key]=options.Attribute.Value;return Result.Success;}
        public void Release(){++Releases;}
    }
    public sealed class LobbyDetails {
        internal LobbyDetailsInfo Info;
        internal readonly Dictionary<string,AttributeDataValue> Values=new Dictionary<string,AttributeDataValue>();
        public Result CopyInfo(ref LobbyDetailsCopyInfoOptions options,out LobbyDetailsInfo? info){info=Info;return Result.Success;}
        public Result CopyAttributeByKey(ref LobbyDetailsCopyAttributeByKeyOptions options,out Attribute? value){if(Values.TryGetValue(options.AttrKey,out var data)){value=new Attribute{Data=new AttributeData{Key=options.AttrKey,Value=data}};return Result.Success;}value=null;return Result.NotFound;}
        public void Release(){}
    }
    public sealed class LobbySearch {
        readonly LobbyInterface owner;
        internal int Releases;
        internal LobbySearch(LobbyInterface value){owner=value;}
        public Result SetParameter(ref LobbySearchSetParameterOptions options){owner.Parameters.Add(options);return Result.Success;}
        public void Find(ref LobbySearchFindOptions options,object ignored,FindCallback callback){owner.PendingFind=callback;if(!owner.HoldFind)owner.CompleteFind();}
        public uint GetSearchResultCount(ref LobbySearchGetSearchResultCountOptions options)=>(uint)owner.Results.Count;
        public Result CopySearchResultByIndex(ref LobbySearchCopySearchResultByIndexOptions options,out LobbyDetails details){details=owner.Results[(int)options.LobbyIndex];return Result.Success;}
        public void Release(){++Releases;}
    }
    public sealed class LobbyInterface {
        public const string SEARCH_BUCKET_ID="bucket",SEARCH_MINSLOTSAVAILABLE="slots";
        internal readonly List<CreateLobbyOptions> Creates=new List<CreateLobbyOptions>();
        internal readonly List<string> Destroys=new List<string>();
        internal readonly List<LobbyDetails> Results=new List<LobbyDetails>();
        internal readonly List<LobbySearchSetParameterOptions> Parameters=new List<LobbySearchSetParameterOptions>();
        internal CreateCallback PendingCreate;
        internal UpdateCallback PendingUpdate;
        internal FindCallback PendingFind;
        internal DestroyCallback PendingDestroy;
        internal LobbyModification LastEdit;
        internal LobbySearch LastSearch;
        internal bool HoldUpdate=false,HoldFind=false;
        internal int Updates,Searches;
        internal Result FindResult=Result.Success;
        public void CreateLobby(ref CreateLobbyOptions options,object ignored,CreateCallback callback){Creates.Add(options);PendingCreate=callback;}
        internal void CompleteCreate(){var callback=PendingCreate;PendingCreate=null;var info=new CreateLobbyCallbackInfo{ResultCode=Result.Success,LobbyId="presence"+Creates.Count};callback(ref info);}
        public Result UpdateLobbyModification(ref UpdateLobbyModificationOptions options,out LobbyModification edit){edit=LastEdit=new LobbyModification();return Result.Success;}
        public void UpdateLobby(ref UpdateLobbyOptions options,object ignored,UpdateCallback callback){++Updates;PendingUpdate=callback;if(!HoldUpdate)CompleteUpdate();}
        internal void CompleteUpdate(){var callback=PendingUpdate;PendingUpdate=null;var info=new UpdateLobbyCallbackInfo{ResultCode=Result.Success};callback(ref info);}
        public Result CreateLobbySearch(ref CreateLobbySearchOptions options,out LobbySearch search){++Searches;search=LastSearch=new LobbySearch(this);return Result.Success;}
        internal void CompleteFind(){var callback=PendingFind;PendingFind=null;var info=new LobbySearchFindCallbackInfo{ResultCode=FindResult};callback(ref info);}
        public void DestroyLobby(ref DestroyLobbyOptions options,object ignored,DestroyCallback callback){Destroys.Add(options.LobbyId);PendingDestroy=callback;}
        internal void CompleteDestroy(){var callback=PendingDestroy;PendingDestroy=null;var info=new DestroyLobbyCallbackInfo{ResultCode=Result.Success};callback(ref info);}
    }
}
namespace Idas3.Multiplayer {
    internal sealed class Idas3EosTransport {
        internal bool ServiceConnected=true,IsBusy=false;
        internal static bool PaceAvailable=true;
        internal static bool TryPace(string kind,double interval)=>PaceAvailable;
        internal static Task Pace(string kind,double interval,CancellationToken cancel)=>Task.CompletedTask;
    }
    internal static class Idas3EosPlatform {
        internal static double Now=100;
        internal static bool SignedIn=true,IsStopping=false;
        internal static void Defer(Action action)=>action();
        internal static void Check(Epic.OnlineServices.Result result,string operation,Epic.OnlineServices.Result also=Epic.OnlineServices.Result.Success){if(result!=Epic.OnlineServices.Result.Success&&result!=Epic.OnlineServices.Result.NoChange&&result!=also)throw new Idas3EosServiceException(result,operation);}
    }
    internal sealed class Idas3EosServiceException:Exception {
        internal Epic.OnlineServices.Result ResultCode{get;}
        internal Idas3EosServiceException(Epic.OnlineServices.Result code,string operation):base(operation){ResultCode=code;}
    }
}
