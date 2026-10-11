using System;
using System.Collections.Generic;

// External SDK boundary only. Tests execute the production sanctions adapter,
// including deferred callbacks, cache policy and transport admission decisions.
namespace Epic.OnlineServices.Sanctions {
    public struct QueryActivePlayerSanctionsOptions {public ProductUserId LocalUserId,TargetUserId;}
    public struct QueryActivePlayerSanctionsCallbackInfo {public ProductUserId LocalUserId,TargetUserId;public Result ResultCode;}
    public struct GetPlayerSanctionCountOptions {public ProductUserId TargetUserId;}
    public struct CopyPlayerSanctionByIndexOptions {public ProductUserId TargetUserId;public uint SanctionIndex;}
    public struct PlayerSanction {public string Action,ReferenceId;public long TimePlaced,TimeExpires;}
    public delegate void QueryCallback(ref QueryActivePlayerSanctionsCallbackInfo info);
    public sealed class SanctionsInterface {
        internal sealed class Request {internal QueryActivePlayerSanctionsOptions Options;internal QueryCallback Callback;internal bool Completed;}
        internal bool Automatic=true;
        internal Result AutomaticResult=Result.Success,CopyResult=Result.Success;
        internal int Copies,NullCopyAt=-1;
        internal uint? CountOverride=null;
        internal readonly List<Request> Requests=new List<Request>();
        internal readonly Dictionary<string,List<PlayerSanction>> Records=new Dictionary<string,List<PlayerSanction>>();
        public void QueryActivePlayerSanctions(ref QueryActivePlayerSanctionsOptions options,object ignored,QueryCallback callback){
            Requests.Add(new Request{Options=options,Callback=callback});if(Automatic)Complete(Requests.Count-1,AutomaticResult);
        }
        internal void Complete(int index,Result result,string target=null,string requester=null){
            var request=Requests[index];request.Completed=true;
            var info=new QueryActivePlayerSanctionsCallbackInfo{ResultCode=result,TargetUserId=target==null?request.Options.TargetUserId:new ProductUserId(target),LocalUserId=requester==null?request.Options.LocalUserId:new ProductUserId(requester)};
            request.Callback(ref info);
        }
        public uint GetPlayerSanctionCount(ref GetPlayerSanctionCountOptions options)=>CountOverride??(Records.TryGetValue(options.TargetUserId.ToString(),out var records)?(uint)records.Count:0);
        public Result CopyPlayerSanctionByIndex(ref CopyPlayerSanctionByIndexOptions options,out PlayerSanction? sanction){
            ++Copies;sanction=null;if(CopyResult!=Result.Success)return CopyResult;
            if((int)options.SanctionIndex==NullCopyAt)return Result.Success;
            if(!Records.TryGetValue(options.TargetUserId.ToString(),out var records)||options.SanctionIndex>=records.Count)return Result.NotFound;
            sanction=records[(int)options.SanctionIndex];return Result.Success;
        }
        internal void Set(string user,params string[] actions){var records=new List<PlayerSanction>();foreach(string action in actions)records.Add(new PlayerSanction{Action=action,ReferenceId="fixture",TimeExpires=1});Records[user]=records;}
        internal void SetRecords(string user,params PlayerSanction[] sanctions){Records[user]=new List<PlayerSanction>(sanctions);}
    }
}
