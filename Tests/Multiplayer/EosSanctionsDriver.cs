using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Epic.OnlineServices;
using Epic.OnlineServices.Sanctions;
using Idas3.Multiplayer;

internal static class EosSanctionsDriver {
    sealed class MainContext:SynchronizationContext {
        readonly Queue<Action> callbacks=new Queue<Action>();
        public override void Post(SendOrPostCallback callback,object state){lock(callbacks)callbacks.Enqueue(()=>callback(state));}
        internal void Drain(){for(int i=0;i<1000;++i){Action next;lock(callbacks){if(callbacks.Count==0)return;next=callbacks.Dequeue();}next();}throw new Exception("Unbounded callback loop");}
    }
    static readonly MainContext context=new MainContext();
    static SanctionsInterface Api=>Idas3EosPlatform.Handle.Sanctions;
    static ProductUserId Peer=>new ProductUserId("remote");
    static int checks;
    static void Check(bool value,string text){if(!value)throw new Exception(text);++checks;Console.WriteLine("PASS\t"+text);}
    static void Pump(){Idas3EosPlatform.Tick(true);context.Drain();}
    static void Complete(int request,Result result=Result.Success){Api.Complete(request,result);Pump();}
    static void Allowed(Task task,string text){Pump();Check(task.IsCompleted&&!task.IsFaulted&&!task.IsCanceled,text);}
    static void Failed<T>(Task task,string text)where T:Exception{
        Pump();Check(task.IsCompleted,text+" completes");try{task.GetAwaiter().GetResult();}catch(T){Check(true,text);return;}throw new Exception(text+" did not fail as expected");
    }
    static void Throws<T>(Action action,string text)where T:Exception{
        try{action();}catch(T){Check(true,text);return;}throw new Exception(text+" did not fail as expected");
    }
    static PlayerSanction Reset(string action,string reference,long placed=100,long expires=0)=>
        new PlayerSanction{Action=action,ReferenceId=reference,TimePlaced=placed,TimeExpires=expires};
    static void ResetPolicies(){
        var reader=new Idas3EosSanctions(new Idas3OnlinePlayerDirectory());
        Throws<InvalidOperationException>(()=>reader.LevelResetIds(Peer),"Unknown level-reset policy is not treated as empty");
        Throws<InvalidOperationException>(()=>reader.FullResetIds(null),"Missing reset-policy identity is rejected");
        Api.SetRecords("remote",Reset("RESET_BATTLE_LEVEL","level-z"),Reset("RESET_BATTLE_STATS","full-b",99),
            Reset("RESET_BATTLE_LEVEL","level-a"),Reset("RESET_BATTLE_LEVEL","level-z"),
            Reset("reset_battle_level",null,0,1),Reset("RESET_BATTLE_STATS_EXTRA",null,0,1));
        var task=reader.RequireAllowed(Peer,false);Api.Complete(0,Result.Success);Idas3EosPlatform.Tick(false);
        Throws<InvalidOperationException>(()=>reader.FullResetIds(Peer),"Native-load callback defers reset-policy publication");Pump();
        Allowed(task,"Administrative reset commands do not themselves ban a player");
        Check(string.Join(",",reader.LevelResetIds(Peer))=="level-a,level-z","Same-second reset IDs are retained, sorted and deduplicated");
        Check(string.Join(",",reader.FullResetIds(Peer))=="full-b","Only the exact full-reset action enters its independent policy");
        var copy=reader.LevelResetIds(Peer);copy[0]="tampered";
        Check(reader.LevelResetIds(Peer)[0]=="level-a","Reset-policy callers cannot mutate the cached ID collection");
        Check(reader.IsClearWithin(Peer,45)&&!reader.IsBlocked(Peer),"Reset policy and online-access policy remain independent");
        Allowed(reader.RequireAllowed(Peer,false,true),"Fresh reset policy uses the existing short forced-query cache");
        Check(Api.Requests.Count==1,"Reading reset IDs and a fresh policy does not issue extra service calls");
        Idas3EosPlatform.Now+=46;
        Api.SetRecords("remote",Reset("RESET_BATTLE_LEVEL","level-z"),Reset("RESET_BATTLE_LEVEL","level-b",1),
            Reset("RESET_BATTLE_STATS","full-b",99),Reset("RESET_BATTLE_LEVEL","level-a"));
        task=reader.RequireAllowed(Peer,false);Complete(1);Allowed(task,"A later query accepts every active reset marker regardless of timestamp order");
        Check(string.Join(",",reader.LevelResetIds(Peer))=="level-a,level-b,level-z","Older or reordered TimePlaced values cannot hide a new reset ID");
        Idas3EosPlatform.Now+=46;Api.Records["remote"].Reverse();task=reader.RequireAllowed(Peer,false);Complete(2);Allowed(task,"Reordered service results refresh successfully");
        Check(string.Join(",",reader.LevelResetIds(Peer))=="level-a,level-b,level-z","Query enumeration order does not change a reset policy");
        reader.Dispose();Throws<InvalidOperationException>(()=>reader.LevelResetIds(Peer),"Disposed reset-policy cache cannot be reused");
    }
    static void ResetFailures(){
        var reader=new Idas3EosSanctions(new Idas3OnlinePlayerDirectory());
        Api.SetRecords("remote",Reset("RESET_BATTLE_LEVEL","level-old"),Reset("RESET_BATTLE_STATS","full-old"),Reset("ONLINE_PLAY",null));
        var task=reader.RequireAllowed(Peer,false);Complete(0);Failed<Idas3EosRestrictedException>(task,"An online ban still applies when reset commands coexist");
        Check(reader.LevelResetIds(Peer)[0]=="level-old"&&reader.FullResetIds(Peer)[0]=="full-old","A complete banned-player response still publishes its reset policy");
        Idas3EosPlatform.Now+=46;Api.SetRecords("remote",Reset("RESET_BATTLE_LEVEL","level-new"),Reset("RESET_BATTLE_STATS","full-new"));Api.NullCopyAt=1;
        task=reader.RequireAllowed(Peer,false);Complete(1);Failed<InvalidOperationException>(task,"A failed copy after parsing a reset rejects the whole response");
        Check(reader.IsBlocked(Peer)&&reader.LevelResetIds(Peer)[0]=="level-old"&&reader.FullResetIds(Peer)[0]=="full-old","Partial copies cannot replace a cached ban or either reset policy");
        Api.NullCopyAt=-1;Idas3EosPlatform.Now+=11;Api.SetRecords("remote",Reset("RESET_BATTLE_LEVEL","temporary",100,200));
        task=reader.RequireAllowed(Peer,false);Complete(2);Failed<InvalidOperationException>(task,"Temporary reset commands fail closed");
        Check(task.Exception.InnerException.Message==Idas3EosSanctions.TemporaryReset,"Temporary reset misconfiguration has a visible administrator instruction");
        Check(reader.IsBlocked(Peer)&&reader.LevelResetIds(Peer)[0]=="level-old"&&reader.FullResetIds(Peer)[0]=="full-old","Temporary reset failure preserves the last complete policy");
        Failed<InvalidOperationException>(reader.RequireAllowed(Peer,false,true),"Failed reset-policy refresh obeys existing retry pacing");
        Check(Api.Requests.Count==3,"Invalid reset policy cannot cause immediate repeated service queries");
        Idas3EosPlatform.Now+=11;Api.Set("remote");task=reader.RequireAllowed(Peer,false);Complete(3);Allowed(task,"Only a successful complete response replaces old policy");
        Check(!reader.IsBlocked(Peer)&&reader.LevelResetIds(Peer).Length==0&&reader.FullResetIds(Peer).Length==0,"A verified empty response clears the cached current policy");
        foreach(string reference in new[]{null,"","with space","has.dot","non-ascii-\u00e9",new string('a',129)}){
            reader=new Idas3EosSanctions(new Idas3OnlinePlayerDirectory());Api.SetRecords("remote",Reset("RESET_BATTLE_STATS",reference));
            task=reader.RequireAllowed(Peer,false);Complete(Api.Requests.Count-1);Failed<InvalidOperationException>(task,"Invalid reset reference fails closed: "+(reference??"null"));
            Throws<InvalidOperationException>(()=>reader.FullResetIds(Peer),"Invalid reset references cannot publish a known policy");reader.Dispose();
        }
        reader=new Idas3EosSanctions(new Idas3OnlinePlayerDirectory());Api.SetRecords("remote",Reset("RESET_BATTLE_STATS",new string('A',126)+"_-"));
        task=reader.RequireAllowed(Peer,false);Complete(Api.Requests.Count-1);Allowed(task,"The bounded ASCII reset-ID format accepts its maximum length");
        Check(reader.FullResetIds(Peer)[0].Length==128,"Maximum reset-ID length is preserved exactly");reader.Dispose();
    }
    static void Cache(){
        var reader=new Idas3EosSanctions(new Idas3OnlinePlayerDirectory());var task=reader.RequireAllowed(Peer,false);
        Check(Api.Requests.Count==1&&Api.Requests[0].Options.TargetUserId.ToString()=="remote"&&Api.Requests[0].Options.LocalUserId.ToString()=="local","SDK query uses the real target and signed-in local product IDs");
        var same=reader.RequireAllowed(Peer,false);Check(Api.Requests.Count==1,"Concurrent requests for the same user share one SDK query");
        Api.Set("remote","MUTE","CUSTOM_WARNING");Api.Complete(0,Result.OperationWillRetry);Pump();Check(!task.IsCompleted,"SDK retry notifications do not complete or reject access");
        Api.Complete(0,Result.Success);Idas3EosPlatform.Tick(false);
        Check(!task.IsCompleted&&Api.Copies==0&&!reader.IsClearWithin(Peer,45),"Native-load pumping neither reads the cache nor completes game access decisions");Pump();
        Allowed(task,"Successful clear query completes");Allowed(same,"All waiters see the same successful query");
        Check(reader.IsClearWithin(Peer,45)&&!reader.IsBlocked(Peer)&&Api.Copies==2,"Non-online sanctions do not block online play");
        Allowed(reader.RequireAllowed(Peer,false),"Fresh clear result may be reused");Allowed(reader.RequireAllowed(Peer,false,true),"Immediate force recheck reuses the documented short successful interval");
        Check(Api.Requests.Count==1,"Cached access does not spam SDK reads");
        Idas3EosPlatform.Now+=46;task=reader.RequireAllowed(Peer,false);Complete(1,Result.AccessDenied);
        Failed<Idas3EosServiceException>(task,"Service error is preserved as an error rather than an empty sanctions list");
        Check(!reader.IsClearWithin(Peer,45)&&reader.IsClearWithin(Peer,180)&&!reader.IsBlocked(Peer),"Failed refresh neither extends fresh clearance nor falsely bans the player");
        Failed<InvalidOperationException>(reader.RequireAllowed(Peer,false,true),"Failed forced retries are paced");
        Check(Api.Requests.Count==2,"Pacing prevents another SDK call during retry interval");
    }
    static void Restriction(){
        var reader=new Idas3EosSanctions(new Idas3OnlinePlayerDirectory());Api.Set("remote","MUTE","ONLINE_PLAY");var task=reader.RequireAllowed(Peer,false);Complete(0);
        Failed<Idas3EosRestrictedException>(task,"Online-play sanction blocks the opponent");
        Check(reader.IsBlocked(Peer)&&!reader.IsClearWithin(Peer,180),"Active SDK ban cannot be expired by an old TimeExpires field");
        Idas3EosPlatform.Now+=46;task=reader.RequireAllowed(Peer,false);Complete(1,Result.ServiceFailure);Failed<Idas3EosServiceException>(task,"A failed refresh of a known ban stays an error");
        Check(reader.IsBlocked(Peer),"Known ban remains blocked through service failure");
        Idas3EosPlatform.Now+=11;Api.Set("remote");task=reader.RequireAllowed(Peer,false);Complete(2);Allowed(task,"A successful query after administrator revocation restores access");
        Check(!reader.IsBlocked(Peer)&&reader.IsClearWithin(Peer,45),"Only a new clear service response removes cached restriction");
        Api.Set("local","RESTRICT_GAME_ACCESS");task=reader.RequireAllowed(Idas3EosPlatform.User,true);Complete(3);
        Failed<Idas3EosRestrictedException>(task,"Game-access action blocks the local player");
        Check(task.Exception.InnerException.Message==Idas3EosSanctions.LocalRestriction,"Local restriction gives the Player ID reporting instruction");
        Check(!Idas3EosSanctions.RestrictsOnline("ONLINE_PLAY_EXTRA")&&!Idas3EosSanctions.RestrictsOnline("online_play"),"Only exact configured online restriction actions match");
    }
    static void Malformed(){
        foreach(string failure in new[]{"target","requester","copy-error","copy-null","empty-action","excess-count"}){
            var reader=new Idas3EosSanctions(new Idas3OnlinePlayerDirectory());Api.CopyResult=Result.Success;Api.NullCopyAt=-1;Api.CountOverride=null;Api.Set("remote","ONLINE_PLAY");
            var task=reader.RequireAllowed(Peer,false);int index=Api.Requests.Count-1;
            if(failure=="copy-error")Api.CopyResult=Result.NotFound;
            if(failure=="copy-null")Api.NullCopyAt=0;
            if(failure=="empty-action")Api.Set("remote","");
            if(failure=="excess-count")Api.CountOverride=1025;
            Api.Complete(index,Result.Success,failure=="target"?"outsider":null,failure=="requester"?"different-local":null);Pump();
            Failed<InvalidOperationException>(task,"Malformed sanctions response fails closed: "+failure);
            Check(!reader.IsClearWithin(Peer,180),"Malformed response cannot cache clearance: "+failure);reader.Dispose();
        }
    }
    static void Timeouts(){
        var reader=new Idas3EosSanctions(new Idas3OnlinePlayerDirectory());var first=reader.RequireAllowed(Peer,false);Idas3EosPlatform.Now+=35;reader.Poll();
        Failed<InvalidOperationException>(first,"A missing SDK callback times out without hanging initialization");
        var second=reader.RequireAllowed(Peer,false);Check(Api.Requests.Count==2,"An expired query can start a fresh retry");
        Api.Set("remote","ONLINE_PLAY");Complete(0);Check(!second.IsCompleted&&!reader.IsBlocked(Peer),"Late expired callback cannot complete or poison the replacement query");
        Api.Set("remote");Complete(1);Allowed(second,"The replacement query accepts only its own response");
        Idas3EosPlatform.Now+=46;var deferred=reader.RequireAllowed(Peer,false);Api.Complete(2,Result.Success);Idas3EosPlatform.Tick(false);Idas3EosPlatform.Now+=35;Pump();
        Failed<InvalidOperationException>(deferred,"A response deferred through a long native load cannot revive expired clearance");
        Check(!reader.IsClearWithin(Peer,45),"An expired deferred response does not update last-clear time");
    }
    static void DisposalAndBounds(){
        var reader=new Idas3EosSanctions(new Idas3OnlinePlayerDirectory());Failed<InvalidOperationException>(reader.RequireAllowed(null,true),"Null identity is rejected");
        Failed<InvalidOperationException>(reader.RequireAllowed(new ProductUserId(""),true),"Invalid identity is rejected");
        Idas3EosPlatform.SignedIn=false;Failed<InvalidOperationException>(reader.RequireAllowed(Peer,false),"Signed-out identity is rejected");Idas3EosPlatform.SignedIn=true;
        Check(Api.Requests.Count==0,"Invalid identity paths never contact the SDK");
        var task=reader.RequireAllowed(Peer,false);reader.Dispose();Failed<OperationCanceledException>(task,"Disposal cancels the pending access decision");
        Complete(0);Check(!reader.IsClearWithin(Peer,180),"Late disposed callback cannot resurrect cached permission");
        reader=new Idas3EosSanctions(new Idas3OnlinePlayerDirectory());var pending=new List<Task>();for(int i=0;i<32;++i)pending.Add(reader.RequireAllowed(new ProductUserId("peer-"+i),false));
        int issued=Api.Requests.Count;Failed<InvalidOperationException>(reader.RequireAllowed(new ProductUserId("overflow"),false),"Full pending cache rejects excess parallel access checks");
        Check(Api.Requests.Count==issued,"Pending entries are never evicted to bypass cache bound");Complete(1);Allowed(pending[0],"One bounded-cache request completes");
        var added=reader.RequireAllowed(new ProductUserId("overflow"),false);Check(Api.Requests.Count==issued+1&&!added.IsCompleted,"A completed oldest entry can be evicted to allow a new request");reader.Dispose();
    }
    static int Main(string[] args){SynchronizationContext.SetSynchronizationContext(context);Api.Automatic=false;try{
        switch(args[0]){case "cache":Cache();break;case "restriction":Restriction();break;case "malformed":Malformed();break;case "timeouts":Timeouts();break;case "dispose-bounds":DisposalAndBounds();break;case "reset-policies":ResetPolicies();break;case "reset-failures":ResetFailures();break;default:throw new Exception("Unknown case");}
        Console.WriteLine("TOTAL\t"+checks);return 0;
    }catch(Exception error){Console.Error.WriteLine(error);return 1;}}
}
