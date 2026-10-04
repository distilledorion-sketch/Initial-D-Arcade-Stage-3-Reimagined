using System;
using System.Collections.Generic;
using System.Linq;
using Idas3.Multiplayer;
using Steamworks;

static class SteamStabilityDriver {
    static int checks;static double now;
    static void Check(bool condition,string why){if(!condition)throw new Exception(why);++checks;Console.WriteLine("PASS\t"+why);}
    static Idas3SteamTransport Host(List<string> errors){
        SteamMatchmaking.Reset();now=0;
        var transport=new Idas3SteamTransport(()=>now){BuildCompatibility="test-build"};
        transport.Error+=errors.Add;transport.Initialize();transport.Host("Test");
        CallResult<LobbyCreated_t>.Last.Complete(new LobbyCreated_t{m_ulSteamIDLobby=100,m_eResult=EResult.k_EResultOK});return transport;
    }
    static void Peer(bool metadata=true){
        if(!SteamMatchmaking.Members.Contains(2))SteamMatchmaking.Members.Add(2);
        if(metadata)SteamMatchmaking.Protocols[2]="1";
        Callback<LobbyChatUpdate_t>.Raise(new LobbyChatUpdate_t{m_ulSteamIDLobby=100,m_ulSteamIDUserChanged=2,m_rgfChatMemberStateChange=1});
    }
    static void ServiceDown(){SteamUser.Online=false;Callback<SteamServersDisconnected_t>.Raise(new SteamServersDisconnected_t{m_eResult=EResult.k_EResultNoConnection});}
    static void ServiceUp(){SteamUser.Online=true;Callback<SteamServersConnected_t>.Raise(default);}
    static void Departure(ulong who,uint change=2){
        SteamMatchmaking.Members.Remove(who);
        Callback<LobbyChatUpdate_t>.Raise(new LobbyChatUpdate_t{m_ulSteamIDLobby=100,m_ulSteamIDUserChanged=who,m_rgfChatMemberStateChange=change});
    }
    static void BackendAndMembership(){
        var errors=new List<string>();using(var t=Host(errors)){
            Peer();int changes=0,received=0;t.PeerChanged+=()=>++changes;t.Message+=_=>++received;
            ServiceDown();now=5;t.Send(new byte[]{7},true);
            var packet=SteamNetworkingMessages.Sent.Last();SteamNetworkingMessages.Receive(2,packet);t.Poll();
            Check(t.Connected&&errors.Count==0&&changes==0&&received==1,"Steam service outage preserves a working peer route and packet delivery");
            ServiceUp();t.Poll();Check(t.Connected&&errors.Count==0,"Steam services reconnect without restarting peer handshake");
            SteamMatchmaking.Protocols.Remove(2);now=7;t.Poll();Check(t.Connected&&changes==0,"Missing cached protocol data does not evict an admitted member");
            SteamMatchmaking.Protocols[2]="wrong";now=9;t.Poll();Check(!t.Connected&&changes==1,"Explicit incompatible protocol revokes peer admission");
        }
        errors.Clear();using(var t=Host(errors)){
            Peer(false);Check(!t.Connected,"New peer without protocol metadata remains unadmitted");
            Peer();Check(t.Connected,"New peer connects after valid metadata arrives");
            int received=0;t.Message+=_=>++received;t.Send(new byte[]{3},true);var packet=SteamNetworkingMessages.Sent.Last();
            SteamNetworkingMessages.Receive(99,packet);var foreign=(byte[])packet.Clone();foreign[8]^=1;SteamNetworkingMessages.Receive(2,foreign);t.Poll();
            Check(received==0,"Unrelated peer and foreign-lobby envelopes remain rejected");
            ServiceDown();Departure(2,8);Check(!t.Connected,"Explicit peer kick is honored even while Steam services are offline");
        }
        errors.Clear();using(var t=Host(errors)){Peer();ServiceDown();Departure(1);Check(!t.InLobby&&errors.Count==1,"Local room removal ends membership even during backend outage");}
        errors.Clear();using(var t=Host(errors)){Peer();now=76;t.Poll();Check(!t.Connected&&errors.Count==1,"A genuinely silent peer still times out");}
        errors.Clear();using(var t=Host(errors)){
            Peer();var identity=new SteamNetworkingIdentity();identity.SetSteamID64(2);
            Callback<SteamNetworkingMessagesSessionFailed_t>.Raise(new SteamNetworkingMessagesSessionFailed_t{m_info=new SteamNetConnectionInfo_t{m_identityRemote=identity,m_eEndReason=5001,m_szEndDebug="route lost"}});
            Check(!t.Connected&&errors.Count==1&&errors[0].Contains("5001"),"A genuinely broken Steam peer session ends with its reason code");
        }
    }
    static void Discovery(){
        for(int order=0;order<2;++order){var errors=new List<string>();using(var t=Host(errors)){
            t.Browse();var old=CallResult<LobbyMatchList_t>.Last;
            if(order==0)Peer();else {SteamMatchmaking.Members.Add(2);SteamMatchmaking.Protocols[2]="1";}
            old.DeliverStale(default,true);now=30;t.Poll();
            Check(t.Connected&&!t.IsBusy&&errors.Count==0,"Late failed discovery cannot close a connected room, callback order "+order);
        }}
        var e=new List<string>();using(var t=Host(e)){
            t.Browse();var old=CallResult<LobbyMatchList_t>.Last;Peer();
            Check(old.Disposed,"Peer arrival releases cancelled discovery callback resources");
            now=26;t.Poll();Check(t.Connected&&e.Count==0,"Cancelled discovery deadline cannot end a race");
        }
        e.Clear();using(var t=Host(e)){
            for(int i=0;i<10;++i){t.Browse();var call=CallResult<LobbyMatchList_t>.Last;now+=26;t.Poll();Check(call.Disposed,"Timed-out discovery releases callback "+i);}
            Check(SteamMatchmaking.Searches==10&&e.All(message=>message.Contains("timed out")),"Timed-out searches cannot exhaust Steam's outstanding-request limit");
        }
    }
    static byte[] Payloads()=>SteamNetworkingMessages.Sent.Where(p=>p[16]==1).Select(p=>p[17]).ToArray();
    static void Congestion(){
        var errors=new List<string>();using(var t=Host(errors)){
            Peer();SteamNetworkingMessages.Results.Enqueue(EResult.k_EResultLimitExceeded);
            t.Send(new byte[]{1},true);t.Send(new byte[]{2},true);
            Check(t.Connected&&errors.Count==0&&Payloads().Length==0,"A full send buffer queues reliable controls without disconnecting");
            now=.06;t.Poll();Check(Payloads().SequenceEqual(new byte[]{1,2}),"Queued reliable controls retry exactly once in original order");
            t.Send(new byte[]{3},true);Check(Payloads().SequenceEqual(new byte[]{1,2,3}),"New controls cannot overtake queued controls");
            now=.12;t.Poll();Check(Payloads().Length==3,"Subsequent polls do not duplicate accepted reliable messages");
            Check(SteamNetworkingMessages.Sent.Zip(SteamNetworkingMessages.Flags,(p,f)=>p[16]!=0||f==Constants.k_nSteamNetworkingSend_UnreliableNoDelay).All(v=>v),"Heartbeat packets do not accumulate in the reliable send queue");
            SteamNetworkingMessages.Results.Enqueue(EResult.k_EResultIgnored);t.Send(new byte[]{4},false);
            SteamNetworkingMessages.Results.Enqueue(EResult.k_EResultLimitExceeded);t.Send(new byte[]{5},false);
            Check(errors.Count==0&&t.Connected,"Congested or not-yet-routed snapshots are safely dropped");
        }
        errors.Clear();using(var t=Host(errors)){
            Peer();SteamNetworkingMessages.DefaultResult=EResult.k_EResultLimitExceeded;t.Send(new byte[]{1},true);
            now=11;t.Poll();Check(!t.Connected&&errors.Count==1,"Persistent congestion ends after a bounded retry period");
        }
        errors.Clear();using(var t=Host(errors)){
            Peer();SteamNetworkingMessages.DefaultResult=EResult.k_EResultLimitExceeded;
            for(int i=0;i<129;++i)t.Send(new byte[]{1},true);
            Check(!t.Connected&&errors.Count==1,"Reliable retry queue has a message-count bound");
        }
        errors.Clear();using(var t=Host(errors)){
            Peer();SteamNetworkingMessages.DefaultResult=EResult.k_EResultLimitExceeded;
            for(int i=0;i<4;++i)t.Send(new byte[65536],true);
            Check(!t.Connected&&errors.Count==1,"Reliable retry queue has a byte-size bound");
        }
        errors.Clear();using(var t=Host(errors)){
            Peer();SteamNetworkingMessages.Results.Enqueue(EResult.k_EResultLimitExceeded);t.Send(new byte[]{9},true);
            t.Leave();now=1;t.Poll();Check(Payloads().Length==0,"Leaving discards queued controls instead of sending into another room");
        }
        foreach(var result in new[]{EResult.k_EResultNoConnection,EResult.k_EResultInvalidParam}){
            errors.Clear();using(var t=Host(errors)){Peer();SteamNetworkingMessages.Results.Enqueue(result);t.Send(new byte[]{1},true);Check(errors.Count==1,"Permanent send failure remains visible: "+result);}
        }
    }
    static Idas3QuickMatch Queue(Idas3SteamTransport t,List<string> errors){
        t.Leave();errors.Clear();var quick=new Idas3QuickMatch(t,()=>now,()=>0,"Test");t.Error+=message=>quick.HandleTransportError(message);quick.Failed+=errors.Add;quick.Start();return quick;
    }
    static void CompleteHost(){CallResult<LobbyCreated_t>.Last.Complete(new LobbyCreated_t{m_ulSteamIDLobby=100,m_eResult=EResult.k_EResultOK});}
    static void Matchmaking(){
        var e=new List<string>();using(var t=Host(e)){
            var q=Queue(t,e);CallResult<LobbyMatchList_t>.Last.Complete(default,true);
            Check(q.IsActive&&!t.InLobby,"Initial search failure keeps Quick Match active");
            now=3;q.Tick(false);CallResult<LobbyMatchList_t>.Last.Complete(default);q.Tick(false);CompleteHost();q.Tick(false);
            Check(q.IsActive&&t.InLobby&&t.IsHost,"Quick Match recovers to a waiting room after a search retry");
            now=7;q.Tick(false);int left=SteamMatchmaking.Left;CallResult<LobbyMatchList_t>.Last.Complete(default,true);
            Check(q.IsActive&&t.InLobby&&SteamMatchmaking.Left==left,"Waiting-room search failure preserves the advertised room");
            now=10;q.Tick(false);Peer();q.Tick(true);Check(!q.IsActive&&t.Connected,"A matched queue stops searching and retains its peer");
        }
        e.Clear();using(var t=Host(e)){
            var q=Queue(t,e);ServiceDown();q.Tick(false);now=45;t.Poll();q.Tick(false);
            Check(q.IsActive,"Steam service outage pauses matchmaking instead of cancelling it");
            ServiceUp();q.Tick(false);CompleteHost();q.Tick(false);
            Check(q.IsActive&&t.InLobby,"Matchmaking resumes automatically when Steam reconnects");
            q.Cancel();Check(!q.IsActive&&!t.InLobby,"User cancellation still stops a recovered search");
        }
        e.Clear();using(var t=Host(e)){
            var q=Queue(t,e);CallResult<LobbyMatchList_t>.Last.Complete(default);q.Tick(false);CompleteHost();q.Tick(false);
            int left=SteamMatchmaking.Left;ServiceDown();q.Tick(false);now=35;t.Poll();q.Tick(false);ServiceUp();q.Tick(false);
            Check(q.IsActive&&t.InLobby&&SteamMatchmaking.Left==left,"Backend reconnection preserves an existing waiting room");
        }
        e.Clear();using(var t=Host(e)){
            var q=Queue(t,e);ServiceDown();q.Tick(false);now=61;q.Tick(false);
            Check(!q.IsActive&&!t.InLobby,"Matchmaking backend recovery is bounded to 60 seconds");
        }
        e.Clear();using(var t=Host(e)){
            var q=Queue(t,e);
            for(int i=0;i<6;++i){CallResult<LobbyMatchList_t>.Last.Complete(default,true);now+=11;q.Tick(false);}
            Check(!q.IsActive,"Repeated failed searches stop after the retry budget");
        }
    }
    static int Main(){try{BackendAndMembership();Discovery();Congestion();Matchmaking();Console.WriteLine("TOTAL\t"+checks);return 0;}catch(Exception e){Console.Error.WriteLine(e);return 1;}}
}
