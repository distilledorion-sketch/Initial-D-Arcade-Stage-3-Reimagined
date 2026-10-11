using System;
using System.IO;
using System.Linq;
using System.Reflection;
using Idas3.Multiplayer;

internal static class OnlineSessionDriver
{
    const BindingFlags Private=BindingFlags.Instance|BindingFlags.NonPublic;
    static int checks;
    static void Check(bool value,string message){if(!value)throw new InvalidOperationException(message);++checks;Console.WriteLine("PASS\t"+message);}
    static void Throws<T>(Action action,string message)where T:Exception{
        try{action();}catch(T){Check(true,message);return;}throw new InvalidOperationException(message);
    }
    static object Get(Idas3MultiplayerSession session,string field)=>typeof(Idas3MultiplayerSession).GetField(field,Private).GetValue(session);
    static void Set(Idas3MultiplayerSession session,string field,object value)=>typeof(Idas3MultiplayerSession).GetField(field,Private).SetValue(session,value);
    sealed class Fixture:IDisposable
    {
        internal readonly Idas3MultiplayerSession Session;
        internal readonly Idas3EosTransport Transport;
        internal Fixture(bool ready=true){
            UnityEngine.Time.realtimeSinceStartupAsDouble=10;
            Session=new Idas3MultiplayerSession(8,Path.Combine(Path.GetTempPath(),"idas3-online-session-fixture-unavailable"));
            Set(Session,"compatibility","fixture-build");
            Session.InitializeOnlinePresence();Transport=(Idas3EosTransport)Get(Session,"transport");
            if(ready){Transport.FinishInitialization=true;Session.BeforeFrame();}
        }
        public void Dispose(){Set(Session,"nativeRace",false);Session.Dispose();}
    }
    static byte[] Hello(string build){
        using(var stream=new MemoryStream())using(var writer=new BinaryWriter(stream)){
            writer.Write(0x504D3349u);writer.Write((ushort)11);writer.Write((byte)1);writer.Write(202UL);writer.Write(0UL);
            writer.Write(build);writer.Write(false);writer.Write("Compatible opponent");writer.Write(8);writer.Write(0u);
            writer.Write(3);writer.Write(false);writer.Write(false);writer.Write(true);
            writer.Write(0u);writer.Write(0u);writer.Write(1u);writer.Write(0u);
            var car=new Idas3OnlineCar{Selection=13};car.Words[4]=8;car.Write(writer);return stream.ToArray();
        }
    }
    static void Initialization(){
        using(var f=new Fixture(false)){
            Check(f.Session.TransportIndex==0&&f.Session.TransportName=="Online","Default provider is online with the EOS provider and without Steam");
            Check(!f.Session.Available&&f.Session.Busy&&f.Session.StatusText=="Signing in","Async initialization exposes busy status before service availability");
            f.Session.InitializeOnlinePresence();Check(f.Transport.Initializations==1,"Repeated menu initialization retains one in-flight service initialization");
            f.Session.ActivityRequested=true;f.Session.PublishActivity=true;
            f.Transport.FinishInitialization=true;f.Session.BeforeFrame();
            Check(f.Transport.Polls==1&&f.Session.Available&&!f.Session.Busy,"Frame pump completes async initialization while provider was unavailable");
            Check(f.Transport.ActivityRequested&&f.Transport.PublishActivity&&f.Transport.ActivityState=="online","Activity state uses provider-neutral optional interface");
        }
        using(var f=new Fixture(false)){
            f.Transport.PollAction=()=>f.Transport.Fail("Sign-in failed");f.Session.BeforeFrame();
            Check(f.Session.ErrorText=="Sign-in failed"&&!f.Session.Busy,"Async initialization failure leaves a retryable session");
            f.Session.InitializeOnlinePresence();f.Transport.FinishInitialization=true;f.Session.BeforeFrame();
            Check(f.Session.Available&&f.Session.ErrorText==""&&f.Transport.Initializations==2,"Retry after initialization failure clears the old error");
        }
    }
    static void QueueCancellationAndErrors(){
        using(var f=new Fixture()){
            f.Session.QuickMatch();
            Check(f.Transport.Starts==1&&f.Session.IsQuickMatching&&f.Session.Busy,"Quick Match uses the managed queue without fallback room discovery");
            Check(f.Session.StatusText==f.Transport.Status&&f.Session.StateName=="Matching","Queued search status comes from the service provider");
            f.Session.SelectTransport(1);Check(f.Session.TransportIndex==0,"Transport cannot change during an active queued search");
            f.Transport.ReentrantCancellation=true;f.Session.CancelQuickMatch();
            Check(f.Transport.Cancellations==1&&f.Transport.Leaves==1&&!f.Session.IsQuickMatching&&!f.Session.Busy,"Cancel dispatches one queue cancellation and one room leave");
            Check(f.Session.ErrorText==""&&f.Session.StateName=="Offline"&&f.Session.StatusText=="Quick Match cancelled.","Reentrant cancellation callbacks cannot replace the completed cancel state");
            f.Session.CancelQuickMatch();Check(f.Transport.Cancellations==1&&f.Transport.Leaves==1,"Repeating cancellation is harmless");
            f.Session.QuickMatch();f.Transport.Fail("Queue unavailable");
            Check(!f.Session.IsQuickMatching&&f.Session.ErrorText=="Queue unavailable"&&f.Session.StateName=="Offline","Queued error exits cleanly without a legacy matcher null dereference");
            f.Session.QuickMatch();Check(f.Transport.Starts==3&&f.Session.IsQuickMatching&&f.Session.ErrorText=="","A failed queue can be retried through the same session");
        }
        using(var f=new Fixture()){
            f.Session.QuickMatch();f.Transport.AttachRoom();f.Session.BeforeFrame();
            Check(f.Session.InLobby&&f.Session.IsQuickMatching&&f.Session.Busy&&!f.Session.HandshakeComplete,"An assigned queue room remains a cancellable search before route admission");
            f.Session.CancelQuickMatch();Check(!f.Session.InLobby&&!f.Session.IsQuickMatching,"Cancellation also cleans up a queue room waiting for its peer route");
        }
        using(var f=new Fixture()){
            f.Session.HostRoom();Check(f.Session.StateName=="Connecting"&&f.Session.Busy,"Manual hosting waits for asynchronous room creation");
            UnityEngine.Time.realtimeSinceStartupAsDouble+=36;f.Session.BeforeFrame();
            Check(f.Session.StateName=="Connecting"&&f.Session.ErrorText=="","An in-flight service operation owns its timeout before the peer handshake starts");
            f.Transport.AttachRoom();f.Session.BeforeFrame();
            Check(f.Session.StateName=="Lobby"&&f.Session.IsHost&&!f.Session.Busy&&!f.Session.HandshakeComplete,"An empty created host room becomes waiting lobby without a peer callback");
            UnityEngine.Time.realtimeSinceStartupAsDouble+=60;f.Session.BeforeFrame();
            Check(f.Session.InLobby&&f.Session.ErrorText=="","A host waiting alone does not hit the peer-handshake deadline");
        }
    }
    static void HandshakeAndLan(){
        using(var f=new Fixture()){
            int announcements=0;f.Session.ChallengerFound+=()=>++announcements;
            f.Session.QuickMatch();f.Transport.EstablishRoute();f.Session.BeforeFrame();
            Check(!f.Session.IsQuickMatching&&f.Session.StateName=="Connecting"&&f.Session.Busy,"Queue assignment transitions to the ordinary connection handshake");
            Check(!f.Session.HandshakeComplete&&announcements==0&&!f.Session.CanReady,"P2P route alone cannot announce or ready an opponent");
            Check(f.Transport.Sent.Any(packet=>packet[6]==1),"Established provider route sends the existing compatibility Hello packet");
            f.Transport.Deliver(Hello("fixture-build"));f.Session.BeforeFrame();
            Check(f.Session.HandshakeComplete&&f.Session.InLobby&&f.Session.StateName=="Lobby"&&announcements==1,"Compatible queued peer reaches the same lobby and challenger event");
            f.Session.BeforeFrame();Check(announcements==1,"The compatible opponent is announced only once");
            Check(f.Session.ChallengerPending&&!f.Session.CanReady,"Challenge presentation still gates Ready after queued matching");
            f.Session.CompleteChallengerPresentation();f.Session.SetReady(true);
            Check(f.Session.LocalReady&&!f.Session.IsRacing,"Ready remains a player action and does not start the native simulation");
            f.Session.SelectTransport(1);Check(f.Session.TransportIndex==0,"An admitted lobby prevents switching transport");
            f.Transport.PollAction=f.Session.LeaveRoom;f.Session.LeaveRoom();
            Check(!f.Session.InLobby&&!f.Session.HandshakeComplete&&f.Transport.Leaves==1,"Reentrant leave during final send does not duplicate cleanup");
            Check(f.Transport.Sent.Any(packet=>packet[6]==13),"Leaving an admitted room sends the unchanged Leave packet");
            f.Session.SelectTransport(1);
            Check(f.Session.TransportIndex==1&&f.Session.Available&&Get(f.Session,"transport") is Idas3TcpTransport&&f.Transport.Disposals==1,"After leave the existing LAN provider is selected and online provider disposed");
            f.Session.SelectTransport(0);
            Check(f.Session.TransportIndex==0&&Get(f.Session,"transport") is Idas3EosTransport&&!f.Session.Available&&f.Session.Busy,"Switching back starts a fresh async online provider");
        }
        using(var f=new Fixture()){
            f.Session.QuickMatch();f.Transport.EstablishRoute();f.Transport.Deliver(Hello("wrong-build"));
            Check(!f.Session.HandshakeComplete&&!f.Session.InLobby&&!f.Session.IsRacing&&f.Session.ErrorText.Contains("same game build"),"The original build handshake rejects an incompatible queued opponent");
        }
        using(var f=new Fixture()){
            f.Session.QuickMatch();f.Transport.EstablishRoute();UnityEngine.Time.realtimeSinceStartupAsDouble+=36;f.Session.BeforeFrame();
            Check(!f.Session.HandshakeComplete&&!f.Session.InLobby&&f.Session.ErrorText.Contains("did not complete the connection"),"An admitted route still times out if its game handshake never arrives");
        }
        using(var f=new Fixture()){
            Set(f.Session,"nativeRace",true);f.Session.QuickMatch();f.Session.SelectTransport(1);
            Check(f.Transport.Starts==0&&f.Session.TransportIndex==0,"Active simulation prevents matchmaking or transport replacement");
        }
    }
    static void RelayScope(bool enabled){
        using(var f=new Fixture()){
            if(!enabled){Throws<InvalidOperationException>(()=>f.Session.ConfigureP2pSmokeScope("shared-scope"),"Shared P2P diagnostic scope requires its explicit command-line flag");return;}
            foreach(string scope in new[]{null,"short",new string('a',65),"unsafe space","escape/route","非ASCII123"})
                Throws<ArgumentException>(()=>f.Session.ConfigureP2pSmokeScope(scope),"Invalid P2P diagnostic scope is rejected: "+(scope??"null"));
            Check(f.Transport.Leaves==0,"Invalid diagnostic scopes cannot disturb an idle provider");
            f.Session.PublishActivity=true;f.Session.ConfigureP2pSmokeScope("Shared_run-123");
            Check(f.Transport.Leaves==1&&!f.Transport.PublishActivity&&f.Transport.Initializations==1,"P2P scope clears idle state and activity without reinitializing before garage setup");
            f.Session.InitializeOnlinePresence();
            string expected=Idas3BuildCompatibility.ForMatchmaking("fixture-build")+"-p2p-smoke-Shared_run-123";
            Check(f.Transport.BuildCompatibility==expected,"The next initialization installs the deterministic shared P2P build identity");
            f.Session.PublishActivity=true;Check(!f.Transport.PublishActivity,"Scoped diagnostics cannot reenable community activity publication");
            f.Session.QuickMatch();Throws<InvalidOperationException>(()=>f.Session.ConfigureP2pSmokeScope("changed-scope"),"An active queue cannot change its diagnostic scope");
        }
        if(enabled)using(var f=new Fixture()){
            f.Session.HostRoom();f.Transport.AttachRoom();f.Session.BeforeFrame();
            Throws<InvalidOperationException>(()=>f.Session.ConfigureP2pSmokeScope("changed-scope"),"An existing room cannot change its diagnostic scope");
        }
        if(enabled)using(var f=new Fixture()){
            Set(f.Session,"nativeRace",true);
            Throws<InvalidOperationException>(()=>f.Session.ConfigureP2pSmokeScope("changed-scope"),"An active native race cannot change its diagnostic scope");
        }
    }
    static void PlayerIdentityDetails(){
        using(var f=new Fixture()){
            Check(f.Session.HasOnlinePlayerDetails&&f.Session.OnlinePlayerId==f.Transport.LocalId,"Own online Player ID comes from the authenticated provider");
            Check(f.Session.RecentOpponentPlayerId==""&&!f.Session.HasCurrentOpponentPlayerId,"Room discovery exposes no opponent identity");
            f.Session.QuickMatch();f.Transport.EstablishRoute();
            Check(f.Session.RecentOpponentPlayerId==""&&!f.Session.HasCurrentOpponentPlayerId,"A peer route without a valid game handshake is not a reportable opponent");
            f.Transport.Deliver(Hello("fixture-build"));
            Check(f.Session.RecentOpponentPlayerId==f.Transport.RemoteId&&f.Session.RecentOpponentPlayerName=="Compatible opponent"&&f.Session.HasCurrentOpponentPlayerId,"A compatible admitted opponent displays the provider ID alongside the game name");
            f.Session.LeaveRoom();
            Check(f.Session.RecentOpponentPlayerId=="fixture-peer"&&!f.Session.HasCurrentOpponentPlayerId,"Leaving retains only the latest opponent for a moderation report");
            f.Transport.PeerIdentity="second-peer";f.Session.QuickMatch();f.Transport.EstablishRoute();f.Transport.Deliver(Hello("wrong-build"));
            Check(f.Session.RecentOpponentPlayerId=="fixture-peer"&&!f.Session.HasCurrentOpponentPlayerId,"Rejected compatibility handshakes cannot overwrite the last verified opponent");
            f.Session.QuickMatch();f.Transport.EstablishRoute();f.Transport.Deliver(Hello("fixture-build"));
            Check(f.Session.RecentOpponentPlayerId=="second-peer"&&f.Session.HasCurrentOpponentPlayerId,"A new admitted opponent replaces the one recent identity");
            f.Session.LeaveRoom();f.Session.SelectTransport(1);
            Check(!f.Session.HasOnlinePlayerDetails&&f.Session.OnlinePlayerId==""&&!f.Session.HasCurrentOpponentPlayerId,"LAN addresses are never presented as online sanction Player IDs");
        }
        using(var f=new Fixture())Check(f.Session.RecentOpponentPlayerId=="","A new game session has no stored opponent history");
    }
    static int Main(){
        try{
            bool relay=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-p2p-check")>=0;
            if(!relay){Initialization();QueueCancellationAndErrors();HandshakeAndLan();PlayerIdentityDetails();}
            RelayScope(relay);Console.WriteLine("TOTAL\t"+checks);return 0;
        }
        catch(Exception error){Console.Error.WriteLine(error);return 1;}
    }
}
