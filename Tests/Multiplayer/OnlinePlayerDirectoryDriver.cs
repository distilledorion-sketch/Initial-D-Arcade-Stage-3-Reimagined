using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Epic.OnlineServices;
using Idas3.Multiplayer;

// The production directory receives an injected request delegate below. Its
// default HTTP dependency exists only to fail loudly if a test uses it.
namespace Idas3.Multiplayer {
    internal static class Idas3OnlineUsername { internal static string Saved=>"Fixture driver"; }
    internal sealed class Idas3OnlinePlayerDirectoryHttp:IDisposable {
        internal Task<Idas3OnlinePlayerDirectory.Reply> Query(string self,string target,string name){throw new Exception("HTTP is forbidden in this test");}
        public void Dispose(){}
    }
}

internal static class OnlinePlayerDirectoryDriver {
    const string Self="11111111111111111111111111111111",Peer="22222222222222222222222222222222",Other="33333333333333333333333333333333";
    sealed class MainContext:SynchronizationContext {
        readonly Queue<Action> work=new Queue<Action>();
        public override void Post(SendOrPostCallback callback,object state){lock(work)work.Enqueue(()=>callback(state));}
        internal void Drain(){for(int i=0;i<1000;i++){Action next;lock(work){if(work.Count==0)return;next=work.Dequeue();}next();}throw new Exception("Unbounded continuation loop");}
    }
    sealed class Request {
        internal string Self,Target,Name;
        internal readonly TaskCompletionSource<Idas3OnlinePlayerDirectory.Reply> Completion=new TaskCompletionSource<Idas3OnlinePlayerDirectory.Reply>();
    }
    sealed class Fixture {
        internal string Identity=Self,Name="Deon";
        internal readonly List<Request> Requests=new List<Request>();
        internal readonly Idas3OnlinePlayerDirectory Directory;
        internal Fixture(){Idas3EosPlatform.User=new ProductUserId(Self);Idas3EosPlatform.Now=100;Directory=new Idas3OnlinePlayerDirectory(Request,()=>Idas3EosPlatform.Now,()=>Identity,()=>Name);}
        Task<Idas3OnlinePlayerDirectory.Reply> Request(string self,string target,string name){var item=new Request{Self=self,Target=target,Name=name};Requests.Add(item);return item.Completion.Task;}
        internal void Complete(int index,string own="allowed",string target="allowed"){
            var item=Requests[index];item.Completion.SetResult(new Idas3OnlinePlayerDirectory.Reply{version=1,playerId=item.Self,status=own,targetPlayerId=item.Target,targetStatus=item.Target==""?"":target,checkedAt=1234});Pump();
        }
        internal void Fail(int index){Requests[index].Completion.SetException(new InvalidOperationException("Service outage"));Pump();}
        internal Task Require(string target=Peer,bool local=false,bool force=false)=>Directory.RequireAllowed(target,local,force);
    }
    static readonly MainContext context=new MainContext();
    static int checks;
    static void Pump(){Idas3EosPlatform.Tick(true);context.Drain();}
    static void Check(bool value,string text){if(!value)throw new Exception(text);checks++;Console.WriteLine("PASS\t"+text);}
    static void Allowed(Task task,string text){Pump();Check(task.IsCompleted&&!task.IsFaulted&&!task.IsCanceled,text);}
    static void Failed<T>(Task task,string text)where T:Exception{Pump();Check(task.IsCompleted,text+" completes");try{task.GetAwaiter().GetResult();}catch(T){Check(true,text);return;}throw new Exception(text+" did not fail as expected");}
    static void CacheAndRename(){
        var f=new Fixture();var task=f.Require(Self,true);var shared=f.Require(Self,true);
        Check(f.Requests.Count==1&&f.Requests[0].Self==Self&&f.Requests[0].Target==""&&f.Requests[0].Name=="Deon","Local registration uses authenticated identity and selected online username; duplicate reads share a request");
        f.Complete(0);Allowed(task,"Explicit allowed local response grants access");Allowed(shared,"Concurrent callers share the successful decision");
        Check(f.Directory.IsClearWithin(Self,45)&&!f.Directory.IsBlocked(Self),"Only a completed valid response caches local clearance");
        Allowed(f.Require(Self,true),"Fresh access reuses cache");Allowed(f.Require(Self,true,true),"Immediate forced access uses short successful cache");Check(f.Requests.Count==1,"Fresh repeated checks do not spam directory");
        f.Name="New name";Check(!f.Directory.IsClearWithin(Self,45),"Renaming immediately invalidates clearance under the old name");task=f.Require(Self,true);
        Check(f.Requests.Count==2&&f.Requests[1].Name=="New name","Renaming bypasses old-name retry cache and registers the new username");f.Complete(1);Allowed(task,"New-name confirmation restores fresh clearance");
        Idas3EosPlatform.Now+=46;task=f.Require(Self,true);f.Fail(2);Failed<InvalidOperationException>(task,"Directory outage rejects new access");
        Check(!f.Directory.IsClearWithin(Self,45)&&f.Directory.IsClearWithin(Self,180),"Outage cannot extend fresh clearance, but preserves bounded established-session grace");
        Failed<InvalidOperationException>(f.Require(Self,true,true),"Repeated failed checks obey retry pacing");Check(f.Requests.Count==3,"Retry throttling does not send another request");
        Idas3EosPlatform.Now+=135;Check(!f.Directory.IsClearWithin(Self,180),"Grace expires based on last successful request start");
        f.Directory.Dispose();
        f=new Fixture();f.Name="";task=f.Require(Self,true);f.Name="Chosen during first online entry";f.Complete(0);
        Check(!task.IsCompleted&&!f.Directory.IsClearWithin(Self,180),"A response for the initial blank name cannot grant access after username selection");
        Check(f.Requests.Count==2&&f.Requests[1].Name==f.Name,"Pending first-use registration automatically re-queries the chosen name");f.Complete(1);Allowed(task,"First online entry completes after registering the chosen name");
        f.Directory.Dispose();f=new Fixture();task=f.Require();f.Name="Changed while pending";shared=f.Require();Check(f.Requests.Count==1,"Renamed concurrent caller waits for pending older-name query");f.Complete(0);
        Check(f.Requests.Count==2&&f.Requests[1].Name==f.Name&&!task.IsCompleted&&!shared.IsCompleted,"Both pending callers share exactly one replacement query for new name");f.Complete(1);Allowed(task,"Original caller waits for current-name confirmation");Allowed(shared,"Renamed caller completes after current-name confirmation");
    }
    static void BanAndOutage(){
        var f=new Fixture();var task=f.Require();f.Complete(0,"allowed","blocked");Failed<Idas3EosRestrictedException>(task,"Explicit peer ban denies peer admission");
        Check(task.Exception.InnerException.Message==Idas3EosSanctions.PeerRestriction&&f.Directory.IsBlocked(Peer)&&!f.Directory.IsClearWithin(Peer,180),"Peer ban uses peer-facing message and cannot use grace");
        Idas3EosPlatform.Now+=46;task=f.Require();f.Fail(1);Failed<InvalidOperationException>(task,"Ban refresh outage is an error");Check(f.Directory.IsBlocked(Peer),"An outage cannot erase a known ban");
        Idas3EosPlatform.Now+=11;task=f.Require();f.Complete(2);Allowed(task,"Explicit allowed response after unban restores access");Check(!f.Directory.IsBlocked(Peer),"Only an authoritative allowed response clears a cached ban");
        f.Directory.Dispose();f=new Fixture();task=f.Require();f.Complete(0,"blocked","allowed");Failed<Idas3EosRestrictedException>(task,"Peer check also enforces a newly returned local ban");
        Check(f.Directory.IsBlocked(Self)&&task.Exception.InnerException.Message==Idas3EosSanctions.LocalRestriction,"Local ban is retained even without a previous self lookup");
    }
    static void InvalidResponses(){
        foreach(string invalid in new[]{"null","version","self","missing-status","unknown-status","time","target","missing-target-status","unknown-target-status"}){
            var f=new Fixture();var task=f.Require();var reply=new Idas3OnlinePlayerDirectory.Reply{version=1,playerId=Self,status="allowed",targetPlayerId=Peer,targetStatus="allowed",checkedAt=1234};
            switch(invalid){case "null":reply=null;break;case "version":reply.version=0;break;case "self":reply.playerId=Other;break;case "missing-status":reply.status=null;break;case "unknown-status":reply.status="unknown";break;case "time":reply.checkedAt=0;break;case "target":reply.targetPlayerId=Other;break;case "missing-target-status":reply.targetStatus=null;break;case "unknown-target-status":reply.targetStatus="ALLOW";break;}
            f.Requests[0].Completion.SetResult(reply);Failed<InvalidOperationException>(task,"Invalid directory reply fails closed: "+invalid);Check(!f.Directory.IsClearWithin(Peer,180),"Invalid reply cannot cache clearance: "+invalid);f.Directory.Dispose();
        }
        var own=new Fixture();var ownTask=own.Require(Self,true);own.Requests[0].Completion.SetResult(new Idas3OnlinePlayerDirectory.Reply{version=1,playerId=Self,status="allowed",targetPlayerId=Peer,targetStatus="allowed",checkedAt=1234});Failed<InvalidOperationException>(ownTask,"Unexpected peer data in a self-only reply fails closed");
        foreach(string invalid in new[]{null,"",new string('A',32),new string('f',31),new string('g',32)}){
            var f=new Fixture();Failed<InvalidOperationException>(f.Require(invalid),"Invalid product ID rejected: "+(invalid??"null"));Check(f.Requests.Count==0,"Invalid IDs never reach service");
        }
        var changed=new Fixture();var changing=changed.Require();changed.Identity=Other;changed.Complete(0);Failed<InvalidOperationException>(changing,"Identity changes during request cannot publish clearance");Check(!changed.Directory.IsClearWithin(Peer,180),"A different signed-in identity cannot reuse clearance");
    }
    static void TimeoutsAndDisposal(){
        var f=new Fixture();var first=f.Require();Idas3EosPlatform.Now+=35;f.Directory.Poll();Failed<InvalidOperationException>(first,"Missing directory response times out");
        var replacement=f.Require();Check(f.Requests.Count==2,"Expired query permits a replacement request");f.Complete(0,"allowed","blocked");Check(!replacement.IsCompleted&&!f.Directory.IsBlocked(Peer),"Expired response cannot poison or finish replacement query");
        f.Complete(1);Allowed(replacement,"Only replacement response completes current query");Idas3EosPlatform.Now+=46;var late=f.Require();Idas3EosPlatform.Now+=35;f.Complete(2);Failed<InvalidOperationException>(late,"Late success cannot revive expired clearance without Poll");Check(!f.Directory.IsClearWithin(Peer,45),"Late response does not refresh checked time");
        var pending=f.Require();f.Directory.Dispose();Failed<OperationCanceledException>(pending,"Dispose cancels pending access decisions");f.Complete(3,"allowed","blocked");Check(!f.Directory.IsBlocked(Peer)&&!f.Directory.IsClearWithin(Peer,180),"Late disposed response cannot resurrect policy");Failed<InvalidOperationException>(f.Require(),"Disposed directory cannot issue new requests");
        f=new Fixture();var all=new List<Task>();for(int i=0;i<31;i++)all.Add(f.Require((i+100).ToString("x32")));Failed<InvalidOperationException>(f.Require(Other),"Bounded pending directory reserves own identity and rejects excess parallel requests");Check(f.Requests.Count==31,"Cache bound does not evict pending requests or reserved self entry");f.Directory.Dispose();
        f=new Fixture();var banned=f.Require(Self,true);f.Complete(0,"blocked");Failed<Idas3EosRestrictedException>(banned,"Self ban establishes cache-eviction scenario");
        for(int i=0;i<33;i++){Idas3EosPlatform.Now+=11;var failed=f.Require((i+100).ToString("x32"));f.Fail(i+1);Failed<InvalidOperationException>(failed,"Peer lookup failure during bounded-cache turnover "+i);}
        Check(f.Directory.IsBlocked(Self),"Bounded peer-cache turnover cannot evict the remembered local ban");f.Directory.Dispose();
    }
    static void ReplyOrder(){
        var f=new Fixture();var initial=f.Require(Self,true);f.Complete(0);Allowed(initial,"Initial self query permits ordering scenario");
        Idas3EosPlatform.Now+=46;var older=f.Require(Peer);var newer=f.Require(Other);f.Complete(2,"blocked","allowed");Failed<Idas3EosRestrictedException>(newer,"Newest peer lookup records a local ban");
        f.Complete(1,"allowed","allowed");Failed<Idas3EosRestrictedException>(older,"Older clear reply cannot clear a newer local ban");Check(f.Directory.IsBlocked(Self)&&!f.Directory.IsClearWithin(Self,180),"Out-of-order older clearance preserves local restriction");
        f.Directory.Dispose();f=new Fixture();var self=f.Require(Self,true);var peer=f.Require();f.Complete(1,"blocked","allowed");Failed<Idas3EosRestrictedException>(peer,"Peer lookup records ban while self lookup remains pending");f.Complete(0);Failed<Idas3EosRestrictedException>(self,"Older self lookup cannot overwrite newer peer-reported local ban");Check(f.Directory.IsBlocked(Self),"Cross-target response order preserves latest self policy");
    }
    static void CombinedPolicies(){
        var api=Idas3EosPlatform.Handle.Sanctions;api.Automatic=false;
        foreach(bool epicBan in new[]{false,true})foreach(bool directoryBan in new[]{false,true}){
            var f=new Fixture();var policy=new Idas3EosSanctions(f.Directory);api.Set(Peer,epicBan?new[]{"ONLINE_PLAY"}:new string[0]);var task=policy.RequireAllowed(new ProductUserId(Peer),false);int epicRequest=api.Requests.Count-1;
            api.Complete(epicRequest,Result.Success);Pump();Check(!task.IsCompleted&&!policy.IsClearWithin(new ProductUserId(Peer),45),"EOS alone cannot grant access while directory query is pending");
            f.Complete(0,"allowed",directoryBan?"blocked":"allowed");
            if(epicBan||directoryBan)Failed<Idas3EosRestrictedException>(task,"Either policy ban denies access: EOS="+epicBan+", directory="+directoryBan);else Allowed(task,"Both verified allowed policies permit access");
            Check(policy.IsBlocked(new ProductUserId(Peer))==(epicBan||directoryBan),"Combined ban state is EOS OR directory");Check(policy.IsClearWithin(new ProductUserId(Peer),45)==(!epicBan&&!directoryBan),"Combined clearance requires both policies clear");policy.Dispose();
        }
        var outage=new Fixture();var combined=new Idas3EosSanctions(outage.Directory);api.Set(Peer);var decision=combined.RequireAllowed(new ProductUserId(Peer),false);api.Complete(api.Requests.Count-1,Result.Success);outage.Fail(0);Failed<InvalidOperationException>(decision,"EOS clearance cannot override unavailable directory");Check(!combined.IsClearWithin(new ProductUserId(Peer),180),"No directory success means no established-session grace");combined.Dispose();
        outage=new Fixture();combined=new Idas3EosSanctions(outage.Directory);decision=combined.RequireAllowed(new ProductUserId(Peer),false);outage.Complete(0);Check(!decision.IsCompleted&&!combined.IsClearWithin(new ProductUserId(Peer),45),"Directory alone cannot grant access while EOS remains pending");api.Complete(api.Requests.Count-1,Result.ServiceFailure);Failed<Idas3EosServiceException>(decision,"Directory clearance cannot override unavailable EOS");Check(!combined.IsClearWithin(new ProductUserId(Peer),180),"No EOS success means no established-session grace");
    }
    static int Main(string[] args){SynchronizationContext.SetSynchronizationContext(context);try{
        switch(args[0]){case "cache-rename":CacheAndRename();break;case "ban-outage":BanAndOutage();break;case "invalid":InvalidResponses();break;case "timeout-dispose":TimeoutsAndDisposal();break;case "reply-order":ReplyOrder();break;case "combined":CombinedPolicies();break;default:throw new Exception("Unknown scenario");}
        Console.WriteLine("TOTAL\t"+checks);return 0;
    }catch(Exception error){Console.Error.WriteLine(error);return 1;}}
}
