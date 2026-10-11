using System;
using System.Collections.Generic;
using System.Reflection;
using System.Threading;
using Epic.OnlineServices;
using Epic.OnlineServices.Lobby;
using Idas3.Multiplayer;

internal static class EosTransportDriver {
    sealed class MainContext:SynchronizationContext {
        readonly Queue<Action> callbacks=new Queue<Action>();
        public override void Post(SendOrPostCallback callback,object state){lock(callbacks)callbacks.Enqueue(()=>callback(state));}
        internal void Drain(){for(int i=0;i<1000;++i){Action next;lock(callbacks){if(callbacks.Count==0)return;next=callbacks.Dequeue();}next();}throw new Exception("Unexpected unbounded callback loop");}
    }
    static readonly MainContext context=new MainContext();
    static int checks;
    static readonly List<string> errors=new List<string>();
    const BindingFlags Hidden=BindingFlags.Instance|BindingFlags.NonPublic;
    const string Room="ID3-111111111111111111111111",OtherRoom="ID3-222222222222222222222222";
    static LobbyInterface Lobby=>Idas3EosPlatform.Handle.Lobby;
    static void Check(bool value,string text){if(!value)throw new Exception(text);++checks;Console.WriteLine("PASS\t"+text);}
    static void Advance(){Idas3EosPlatform.Now+=10;}
    static void Pump(){context.Drain();Idas3EosPlatform.Tick(true);context.Drain();}
    static void Until(Func<bool> condition,string reason){var watch=System.Diagnostics.Stopwatch.StartNew();while(!condition()){Pump();if(watch.Elapsed.TotalSeconds>12)throw new Exception("Timed out: "+reason);Thread.Sleep(2);}Pump();}
    static void Set(Idas3EosTransport transport,string field,object value)=>typeof(Idas3EosTransport).GetField(field,Hidden).SetValue(transport,value);
    static Idas3EosP2P Route(Idas3EosTransport transport)=>(Idas3EosP2P)typeof(Idas3EosTransport).GetField("route",Hidden).GetValue(transport);
    static Idas3EosTransport Create(){var transport=new Idas3EosTransport{BuildCompatibility="fixture-build"};transport.Error+=errors.Add;transport.Initialize();Pump();Check(transport.Available,"Fixture executes production service initialization against the fake SDK boundary");return transport;}
    static void CancelCreate(){
        var transport=Create();transport.HostPrivate("Canceled room");Check(Lobby.Creates.Count==1&&transport.IsBusy,"Room creation is pending until the SDK callback arrives");
        var options=Lobby.Creates[0];Check(options.PermissionLevel==LobbyPermissionLevel.Joinviapresence&&!options.PresenceEnabled&&options.EnableJoinById&&!options.EnableRTCRoom&&options.DisableHostMigration,"Private code-only room uses no presence, voice room or host migration");
        transport.Leave();Advance();transport.HostPrivate("Next room");Check(Lobby.Creates.Count==1,"Immediate retry waits for canceled creation and its cleanup");
        Lobby.CompleteCreate(Result.Success,Room);Pump();Check(Lobby.Destroys.Count==1&&Lobby.Destroys[0]==Room,"Late successful creation destroys precisely the room returned by that request");
        Check(!transport.InLobby&&!transport.Connected&&Lobby.Creates.Count==1&&Lobby.Updates==0,"Canceled late room cannot become visible, advertise attributes or start a newer room before cleanup");
        Advance();Lobby.CompleteDestroy(Result.Success);Pump();Until(()=>Lobby.Creates.Count==2,"new create after cleanup");
        Check(Lobby.Creates[1].LobbyId!=options.LobbyId&&errors.Count==0,"New creation starts only after cleanup and uses a fresh random room code");
    }
    static void CancelJoin(){
        var transport=Create();transport.Join(Room);Check(Lobby.Joins.Count==1,"Manual join waits for its SDK callback");
        transport.Leave();Advance();transport.Join(OtherRoom);Check(Lobby.Joins.Count==1,"Immediate rejoin cannot race canceled membership cleanup");
        Lobby.CompleteJoin(Result.Success);Pump();Check(Lobby.Leaves.Count==1&&Lobby.Leaves[0]==Room&&!transport.InLobby,"Late successful join leaves the old room without joining its game route");
        Advance();Lobby.CompleteLeave(Result.Success);Pump();Until(()=>Lobby.Joins.Count==2,"new join after old leave");
        Check(Lobby.Joins[1]==OtherRoom&&errors.Count==0,"The delayed old leave cannot target the newer room");
    }
    static void FailedCleanup(){
        var transport=Create();transport.HostPrivate("Canceled room");transport.Leave();Advance();transport.HostPrivate("Blocked retry");Lobby.CompleteCreate(Result.Success,Room);Pump();
        for(int attempt=0;attempt<3;++attempt){Until(()=>Lobby.DestroyCallbacks.Count!=0,"cleanup retry");Check(Lobby.Destroys[attempt]==Room,"Cleanup retry keeps the exact canceled room identity");Advance();Lobby.CompleteDestroy(Result.ServiceFailure);Pump();}
        Until(()=>errors.Count!=0,"surface failed cleanup to waiting request");
        Check(Lobby.Creates.Count==1&&!transport.InLobby&&!transport.IsBusy,"Failed canceled-room cleanup cannot release a waiting room creation");
        Check(errors[errors.Count-1].Contains("previous online room")&&errors[errors.Count-1].Contains("Restart"),"Cleanup exhaustion preserves the actionable recovery error through the async barrier");
        Advance();transport.HostPrivate("Another retry");Pump();Check(Lobby.Creates.Count==1,"Further operations remain blocked after uncertain remote cleanup");
    }
    static void RequestFailure(){
        var transport=Create();transport.Join(Room);Lobby.CompleteJoin(Result.AccessDenied);Pump();
        Check(errors.Count==1&&errors[0].Contains("AccessDenied")&&!transport.InLobby&&!transport.IsBusy,"An active request preserves SDK rejection detail and exits the busy state");
        Check(Lobby.Leaves.Count==0&&Lobby.Destroys.Count==0,"A rejected join never detaches an unrelated room");
        Advance();transport.Join(OtherRoom);Check(Lobby.Joins.Count==2,"A genuine request error can be retried after its completed barrier");
    }
    static void DisposedCreate(){
        var transport=Create();transport.HostPrivate("Disposed room");transport.Dispose();Advance();Lobby.CompleteCreate(Result.Success,Room);Pump();
        Check(Lobby.Destroys.Count==1&&Lobby.Destroys[0]==Room&&!transport.Available&&!transport.InLobby,"Disposed transport still cleans a late-created SDK room without reviving itself");
        Lobby.CompleteDestroy(Result.Success);Pump();Check(errors.Count==0,"Disposed request completion cannot publish a stale UI error");
    }
    static FakeRoom SeedPeer(Idas3EosTransport transport){
        var room=new FakeRoom{Info=new LobbyDetailsInfo{LobbyId=Room,LobbyOwnerUserId=new ProductUserId("local"),MaxMembers=2,AvailableSlots=0,PermissionLevel=LobbyPermissionLevel.Publicadvertised}};
        room.Members.Add("local");room.Members.Add("remote");room.Attributes["build"]=transport.CompatibilityHash;
        room.MemberAttributes["remote"]=new Dictionary<string,string>{{"build",transport.CompatibilityHash},{"name","Other driver"}};
        Lobby.Rooms.Add(Room,room);Set(transport,"lobbyId",Room);Set(transport,"originalOwner","local");Set(transport,"members",2);transport.Poll();
        return room;
    }
    static FakeRoom Admit(Idas3EosTransport transport){
        var room=SeedPeer(transport);Pump();
        Check(transport.Connected&&Route(transport)!=null,"Production membership and sanctions validation create a route only for the actual compatible clear member");return room;
    }
    static void StaleNotification(){
        var transport=Create();Admit(transport);var route=Route(transport);
        Lobby.Notify(Room,"local",LobbyMemberStatus.Left);Idas3EosPlatform.Tick(false);Check(transport.Connected,"Native-loading callback pump cannot mutate room membership");transport.Poll();
        Check(transport.Connected&&ReferenceEquals(Route(transport),route)&&errors.Count==0,"A delayed local-Left notification cannot close a room whose current cached membership still contains the player");
        Lobby.Notify(OtherRoom,"local",LobbyMemberStatus.Closed);transport.Poll();Check(transport.Connected&&errors.Count==0,"Notifications for a previous room cannot affect the current room");
    }
    static void LoadingRevocation(bool replacePeer){
        var transport=Create();var room=Admit(transport);var route=Route(transport);int delivered=0;transport.Message+=bytes=>++delivered;
        route.PendingMessage=new byte[]{1,2,3};room.Members.Remove("remote");if(replacePeer)room.Members.Add("replacement");
        Lobby.Notify(Room,"remote",LobbyMemberStatus.Left);Advance();Idas3EosTransport.KeepAliveDuringNativeLoad();
        Check(delivered==0&&!route.Disposed,"Loading keepalive preserves deferred game delivery until normal callback processing");
        transport.Poll();Check(route.Disposed&&!transport.InLobby&&delivered==0,"Current membership is checked and stale route revoked before buffered game data can be delivered");
        Check(errors.Count==1&&errors[0].Contains("other driver left"),replacePeer?"A replacement member cannot inherit the previous authenticated route":"A removed member cannot keep delivering buffered game packets");
    }
    static void OwnerChanged(){
        var transport=Create();var room=Admit(transport);var route=Route(transport);room.Info.LobbyOwnerUserId=new ProductUserId("remote");Lobby.Notify(Room,"remote",LobbyMemberStatus.Promoted);transport.Poll();
        Check(route.Disposed&&!transport.InLobby&&errors.Count==1&&errors[0].Contains("owner changed"),"Unexpected SDK host migration closes the old race authority instead of silently assigning another host");
    }
    static void PeerDeparture(){
        var transport=Create();Admit(transport);var route=Route(transport);var events=new List<string>();
        transport.PeerChanged+=()=>events.Add("peer");transport.Error+=detail=>events.Add("error");transport.Admitted(route);
        Check(events.Count==1&&events[0]=="peer","Successful route admission notifies the session once");events.Clear();
        route.CloseFromSdk();transport.RouteFailed(route,"The other driver left the room.");
        Check(!transport.InLobby&&!transport.Connected&&events.Count==2&&events[0]=="peer"&&events[1]=="error","A closed SDK route still reports peer departure before its error after clearing room state");
        transport.RouteFailed(route,"Late duplicate close");transport.Leave();
        Check(events.Count==2,"Duplicate route-failure callbacks and repeated leave do not emit another departure");
    }
    static void PublicSearch(){
        var transport=Create();
        for(int i=0;i<8;++i){
            var room=new FakeRoom{Info=new LobbyDetailsInfo{LobbyId="ID3-"+i.ToString("X24"),LobbyOwnerUserId=new ProductUserId(i==1?"local":"other"),MaxMembers=i==2?3u:2u,AvailableSlots=i==3?0u:1u,PermissionLevel=i==4?LobbyPermissionLevel.Joinviapresence:LobbyPermissionLevel.Publicadvertised}};
            room.Attributes["build"]=i==5?"different-build":transport.CompatibilityHash;room.Attributes["kind"]=i==6?"queue":"room";
            room.Attributes["name"]="Race room";room.Attributes["host"]="Other driver";
            if(i==7)room.Info.LobbyId="invalid-code";Lobby.SearchResults.Add(room);
        }
        transport.Browse();Check(transport.IsBusy&&transport.Rooms.Count==0,"Public browser waits for the SDK search result");Pump();
        Check(transport.Rooms.Count==1&&transport.Rooms[0].Code=="ID3-"+0.ToString("X24")&&!transport.IsBusy,"Public browser excludes own, wrong-capacity, full, private, incompatible, queue and malformed-code results");
        Check(Lobby.SearchParameters.Count==2&&Lobby.SearchParameters[0].Parameter.Value.AsUtf8.Contains(transport.CompatibilityHash.Substring(0,32))&&Lobby.SearchParameters[1].Parameter.Value.AsInt64==1,"Discovery requests the exact compatibility bucket and an available player slot");
        Advance();transport.Browse();transport.Leave();Pump();
        Check(transport.Rooms.Count==0&&!transport.IsBusy&&errors.Count==0,"A canceled search callback cannot repopulate stale room results");
    }
    static Idas3EosTransport Queue(){
        var transport=Create();transport.StartMatchmaking();Pump();
        Until(()=>Lobby.Creates.Count==1,"empty search creates queue room");
        Lobby.CompleteCreate(Result.Success,Room);Pump();
        Until(()=>transport.InLobby,"queue room publishes attributes");
        Lobby.AutomaticSearch=false;
        Check(transport.IsMatching&&transport.IsBusy&&transport.RoomCode==Room,"Quick Match advertises its waiting room");
        return transport;
    }
    static void LongQueue(){
        var transport=Queue();
        for(int i=0;i<4;++i){
            Until(()=>Lobby.SearchCallbacks.Count==1,"next queue search");
            Idas3EosPlatform.Now+=120;transport.Poll();Pump();
            Lobby.CompleteSearch(Result.Success);Pump();transport.Poll();
            Check(transport.IsMatching&&transport.InLobby&&transport.RoomCode==Room&&errors.Count==0,"Successful empty search keeps the same queue alive beyond the original three-minute limit");
        }
        Check(Lobby.Searches==5&&Lobby.Creates.Count==1&&Lobby.Destroys.Count==0,"Eight minutes of healthy searching does not recreate or abandon the queue room");
        transport.CancelMatchmaking();Pump();
        Check(!transport.IsMatching&&!transport.IsBusy&&!transport.InLobby&&Lobby.Destroys.Count==1,"Cancel still immediately exits and retires a long-running queue");
        Lobby.CompleteDestroy(Result.Success);Pump();
        Check(errors.Count==0,"Canceling the long queue does not report a timeout");
    }
    static void StalledQueue(bool creating){
        var transport=Create();Lobby.AutomaticSearch=creating;transport.StartMatchmaking();Pump();
        Check(creating?Lobby.Creates.Count==1:Lobby.SearchCallbacks.Count==1,"Quick Match request is held at the SDK boundary");
        Idas3EosPlatform.Now+=181;transport.Poll();Pump();
        Check(!transport.IsMatching&&!transport.IsBusy&&!transport.InLobby&&errors.Count==1&&errors[0].Contains("timed out"),"A stalled queue request still times out without waiting forever");
        if(creating){
            Lobby.CompleteCreate(Result.Success,Room);Pump();
            Check(Lobby.Destroys.Count==1&&Lobby.Destroys[0]==Room,"A late successful queue creation is retired after the timeout");
            Lobby.CompleteDestroy(Result.Success);Pump();
        }else{
            Lobby.CompleteSearch(Result.Success);Pump();
            Check(Lobby.Creates.Count==0,"A late search response cannot create a room after the timeout");
        }
        Check(!transport.IsMatching&&!transport.InLobby&&errors.Count==1,"Late callbacks cannot revive an expired queue");
    }
    static void QueuePeerDeadline(bool attributes){
        var transport=Queue();Until(()=>Lobby.SearchCallbacks.Count==1,"pending search before incoming peer");
        Idas3EosPlatform.Now+=170;transport.Poll();Pump();
        var room=Lobby.Rooms[Room];room.Members.Add("remote");
        if(attributes)room.MemberAttributes["remote"]=new Dictionary<string,string>{{"build",transport.CompatibilityHash},{"name","Other driver"}};
        Lobby.Notify(Room,"remote",LobbyMemberStatus.Joined);transport.Poll();Pump();
        if(attributes){
            var route=Route(transport);Check(route!=null,"Incoming queue peer passes current membership and access checks");
            route.CloseFromSdk(); // Hold the fake route before handshake admission.
            Idas3EosPlatform.Now+=20;transport.Poll();Pump();
            Check(transport.InLobby&&transport.IsMatching&&errors.Count==0,"A late-arriving peer gets a fresh connection window beyond the original search deadline");
            Lobby.CompleteSearch(Result.Success);Pump();
            Idas3EosPlatform.Now+=26;transport.Poll();Pump();
            Check(!transport.InLobby&&!transport.IsMatching&&errors.Count==1&&errors[0].Contains("Connecting to driver timed out"),"A late empty-search response cannot extend the peer's stalled connection deadline");
        }else{
            Lobby.CompleteSearch(Result.Success);Pump();
            Idas3EosPlatform.Now+=11;transport.Poll();Pump();
            Check(!transport.InLobby&&!transport.IsMatching&&errors.Count==1,"A peer that never publishes its build attributes cannot leave the queue stuck forever");
        }
    }
    static void InitialAccess(bool restricted){
        var api=Idas3EosPlatform.Handle.Sanctions;
        if(restricted)api.Set("local","RESTRICT_GAME_ACCESS");else api.AutomaticResult=Result.AccessDenied;
        var transport=new Idas3EosTransport{BuildCompatibility="fixture-build"};transport.Error+=errors.Add;transport.Initialize();Pump();
        Check(!transport.Available&&!transport.IsBusy&&!transport.InLobby,"Unverified or restricted local access never becomes online-ready");
        Check(transport.LocalId=="local","Own Player ID remains available for an administrator report after access rejection");
        Check(errors.Count==1&&errors[0].Contains(restricted?"restricted":"AccessDenied"),"Initial sanctions rejection preserves the actual restriction or service error");
        transport.HostPrivate("Denied");transport.Join(Room);transport.StartMatchmaking();transport.Browse();Pump();
        Check(Lobby.Creates.Count==0&&Lobby.Joins.Count==0&&Lobby.SearchParameters.Count==0,"Rejected local access cannot create, join, queue or browse");
    }
    static void PeerAccess(bool canceled){
        var transport=Create();var api=Idas3EosPlatform.Handle.Sanctions;api.Automatic=false;SeedPeer(transport);
        Check(Route(transport)==null&&!transport.Connected&&api.Requests.Count==2,"P2P construction waits for a sanctions response for the verified room member");
        if(canceled)transport.Leave();else api.Set("remote","ONLINE_PLAY");
        api.Complete(1,Result.Success);Idas3EosPlatform.Tick(false);
        Check(Route(transport)==null,"Native-loading callback pump cannot construct or admit an unchecked route");Pump();
        Check(Route(transport)==null&&!transport.Connected&&!transport.InLobby,canceled?"A canceled late peer clearance cannot recreate the abandoned room":"A restricted opponent is rejected before any P2P route exists");
        Check(canceled?errors.Count==0:errors.Count==1&&errors[0]==Idas3EosSanctions.PeerRestriction,canceled?"Canceled clearance cannot publish a stale failure":"Peer restriction is reported accurately without banning the local player");
    }
    static void CancelInitialAccess(){
        var api=Idas3EosPlatform.Handle.Sanctions;api.Automatic=false;
        var transport=new Idas3EosTransport{BuildCompatibility="fixture-build"};transport.Error+=errors.Add;transport.Initialize();transport.Leave();api.Complete(0,Result.Success);Pump();
        Check(!transport.Available&&!transport.IsBusy&&errors.Count==0,"Canceling initialization prevents a late access check from enabling online service");
    }
    static void RefreshAccess(bool outage){
        var transport=Create();Admit(transport);var route=Route(transport);transport.Admitted(route);
        var api=Idas3EosPlatform.Handle.Sanctions;int delivered=0;transport.Message+=bytes=>++delivered;
        if(outage)api.AutomaticResult=Result.AccessDenied;else api.Set("remote","ONLINE_PLAY");
        Idas3EosPlatform.Now+=46;transport.Poll();Pump();
        if(outage){
            Check(transport.Connected&&!route.Disposed&&errors.Count==0,"An already-admitted peer survives a short sanctions-service outage using its prior clear result");
            Idas3EosPlatform.Now+=134;route.PendingMessage=new byte[]{7};transport.Poll();Pump();
            Check(route.Disposed&&!transport.Connected&&!transport.Available&&delivered==0,"Expired clear grace disconnects before buffered race messages can be delivered");
            Check(errors.Count==1&&errors[0]==Idas3EosSanctions.Unavailable,"Grace exhaustion is reported as verification failure, never falsely as a ban");
        }else{
            Check(route.Disposed&&!transport.Connected&&!transport.InLobby&&transport.Available,"A newly discovered opponent restriction closes the existing route immediately without disabling local access");
            Check(errors.Count==1&&errors[0]==Idas3EosSanctions.PeerRestriction,"An active opponent ban is not extended by the outage grace period");
        }
    }
    static void NewRoomAccess(){
        var transport=Create();Idas3EosPlatform.Handle.Sanctions.AutomaticResult=Result.AccessDenied;Idas3EosPlatform.Now+=46;
        transport.HostPrivate("Stale local clearance");Pump();
        Check(Lobby.Creates.Count==0&&!transport.InLobby&&errors.Count==1,"A stale clear result cannot use admitted-peer outage grace to create a new room");
    }
    static void DelayedAdmission(){
        var transport=Create();Admit(transport);var route=Route(transport);int changed=0;transport.PeerChanged+=()=>++changed;
        Idas3EosPlatform.Now+=46;transport.Admitted(route);
        Check(route.Disposed&&!transport.InLobby&&!transport.Connected&&changed==0,"Slow P2P negotiation cannot announce a peer whose required fresh clearance expired");
        Check(errors.Count==1&&errors[0]==Idas3EosSanctions.Unavailable,"Late admission fails as verification expiry rather than a false ban");
    }
    static void DepartedBeforeClear(){
        var transport=Create();var api=Idas3EosPlatform.Handle.Sanctions;api.Automatic=false;var room=SeedPeer(transport);
        room.Members.Remove("remote");api.Complete(1,Result.Success);Pump();
        Check(Route(transport)==null&&!transport.Connected&&transport.InLobby,"Successful delayed clearance rechecks current membership and cannot route a departed player");
    }
    static void LoadingRestriction(){
        var transport=Create();Admit(transport);var route=Route(transport);transport.Admitted(route);int delivered=0;transport.Message+=data=>++delivered;
        var api=Idas3EosPlatform.Handle.Sanctions;api.Automatic=false;Idas3EosPlatform.Now+=46;transport.Poll();api.Complete(2,Result.Success);Pump();
        Check(api.Requests.Count==4&&api.Requests[3].Options.TargetUserId.ToString()=="remote","Active refresh queries both local and authenticated peer access");
        api.Set("remote","ONLINE_PLAY");api.Complete(3,Result.Success);Idas3EosPlatform.Tick(false);
        Check(!route.Disposed&&transport.Connected,"A sanctions callback remains deferred while native loading owns the main thread");
        route.PendingMessage=new byte[]{9};transport.Poll();Pump();
        Check(route.Disposed&&!transport.Connected&&delivered==0,"Deferred ban is enforced before buffered game messages are delivered after native loading");
    }
    static Epic.OnlineServices.Sanctions.PlayerSanction ResetCommand(string id,bool full=false)=>
        new Epic.OnlineServices.Sanctions.PlayerSanction{Action=full?Idas3EosSanctions.ResetBattleStats:Idas3EosSanctions.ResetBattleLevel,ReferenceId=id,TimeExpires=0};
    static void InitialReset(bool writeFailure){
        var api=Idas3EosPlatform.Handle.Sanctions;
        api.SetRecords("local",ResetCommand("level-1"),ResetCommand("full-1",true));
        int applied=0;var transport=new Idas3EosTransport{BuildCompatibility="fixture-build"};transport.Error+=errors.Add;
        transport.ApplyStatsResets=(id,levels,full)=>{
            ++applied;Check(!transport.Available&&!transport.InLobby,"Initial reset completes before online access or room creation");
            Check(id=="local"&&levels.Length==1&&levels[0]=="level-1"&&full.Length==1&&full[0]=="full-1","Save owner receives only the authenticated local policy");
            if(writeFailure)throw new System.IO.IOException("Disk denied");return true;
        };
        transport.Initialize();Pump();
        Check(applied==1,"Local reset is dispatched exactly once on initialization");
        if(writeFailure){
            Check(!transport.Available&&errors.Count==1&&errors[0]==Idas3EosTransport.StatsResetFailed,"A failed durable reset blocks online with a clear non-ban error");
            writeFailure=false;transport.Initialize();Pump();
            Check(transport.Available&&applied==2,"Retry attempts the failed save again rather than remembering it as applied");
        }else{
            Check(transport.Available&&errors.Count==0,"A reset allows the player to earn new progress afterward");
            Idas3EosPlatform.Now+=46;transport.Poll();Pump();transport.Poll();
            Check(applied==1,"Periodic unchanged reset policy does not wipe newly earned progress");
        }
    }
    static void LiveReset(bool peer){
        var transport=Create();Admit(transport);var route=Route(transport);transport.Admitted(route);
        int applied=0,delivered=0;transport.Message+=data=>++delivered;
        transport.ApplyStatsResets=(id,level,full)=>{++applied;transport.Leave();return true;};
        var api=Idas3EosPlatform.Handle.Sanctions;api.Automatic=false;
        Idas3EosPlatform.Now+=46;transport.Poll();
        if(!peer)api.SetRecords("local",ResetCommand("new-local",true));
        api.Complete(2,Result.Success);Pump();
        if(peer){api.SetRecords("remote",ResetCommand("new-peer"));api.Complete(3,Result.Success);Pump();}
        route.PendingMessage=new byte[]{9};transport.Poll();Pump();
        Check(route.Disposed&&!transport.Connected&&!transport.InLobby&&delivered==0,"Changed reset policy closes the prior route before buffered game results can be delivered");
        Check(applied==(peer?0:1),"A peer reset never writes the local player's stats");
        if(peer)Check(errors.Count==1&&errors[0]==Idas3EosTransport.PeerStatsReset,"Peer reset explains that the room must be rejoined");
        else Check(transport.Available&&errors.Count==0,"Intentional record-owner cleanup cancels the old request without a stale failure or ban");
    }
    static void ResetWithoutOwner(){
        var api=Idas3EosPlatform.Handle.Sanctions;api.SetRecords("local",ResetCommand("reset-no-owner"));
        var transport=new Idas3EosTransport{BuildCompatibility="fixture-build"};transport.Error+=errors.Add;transport.Initialize();Pump();
        Check(!transport.Available&&errors.Count==1&&errors[0]==Idas3EosTransport.StatsResetFailed,"A client without a save owner cannot claim that a reset was applied");
    }
    static int Main(string[] args){SynchronizationContext.SetSynchronizationContext(context);try{
        switch(args[0]){case "cancel-create":CancelCreate();break;case "cancel-join":CancelJoin();break;case "cleanup-failure":FailedCleanup();break;case "request-error":RequestFailure();break;case "dispose-create":DisposedCreate();break;case "stale-left":StaleNotification();break;case "removed-peer":LoadingRevocation(false);break;case "replaced-peer":LoadingRevocation(true);break;case "owner-changed":OwnerChanged();break;case "peer-departure":PeerDeparture();break;case "public-search":PublicSearch();break;
        case "sanctions-local":InitialAccess(true);break;case "sanctions-unavailable":InitialAccess(false);break;case "sanctions-peer":PeerAccess(false);break;case "sanctions-cancel-peer":PeerAccess(true);break;case "sanctions-cancel-init":CancelInitialAccess();break;case "sanctions-active-ban":RefreshAccess(false);break;case "sanctions-outage":RefreshAccess(true);break;case "sanctions-new-room":NewRoomAccess();break;
        case "sanctions-late-admission":DelayedAdmission();break;case "sanctions-departed-peer":DepartedBeforeClear();break;case "sanctions-native-load":LoadingRestriction();break;
        case "reset-initial":InitialReset(false);break;case "reset-write-failure":InitialReset(true);break;case "reset-live-local":LiveReset(false);break;case "reset-live-peer":LiveReset(true);break;case "reset-no-owner":ResetWithoutOwner();break;
        case "queue-long":LongQueue();break;case "queue-stalled-search":StalledQueue(false);break;case "queue-stalled-create":StalledQueue(true);break;case "queue-peer-deadline":QueuePeerDeadline(true);break;case "queue-peer-missing-attributes":QueuePeerDeadline(false);break;
        default:throw new Exception("Unknown case");}
        Console.WriteLine("TOTAL\t"+checks);return 0;
    }catch(Exception error){Console.Error.WriteLine(error);return 1;}}
}
