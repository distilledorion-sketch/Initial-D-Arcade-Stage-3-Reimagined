using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using Idas3.Multiplayer;
internal static class RaceLoadingDriver
{
    const BindingFlags Private=BindingFlags.Instance|BindingFlags.NonPublic;
    static void Check(bool value,string text){if(!value)throw new InvalidOperationException(text);Console.WriteLine("PASS\t"+text);}
    static object Get(object target,string field)=>target.GetType().GetField(field,Private).GetValue(target);
    static void Set(object target,string field,object value)=>target.GetType().GetField(field,Private).SetValue(target,value);
    sealed class Fixture:IDisposable
    {
        internal readonly Idas3MultiplayerSession Session;
        internal readonly Idas3EosTransport Transport;
        internal Fixture(bool host=false){
            UnityEngine.Time.realtimeSinceStartupAsDouble=10;
            Idas3MultiplayerNative.Starts=Idas3MultiplayerNative.Releases=0;
            Session=new Idas3MultiplayerSession(8,Path.Combine(Path.GetTempPath(),"idas3-loading-fixture-unavailable"));
            Set(Session,"compatibility","fixture-build");Session.InitializeOnlinePresence();
            Transport=(Idas3EosTransport)Get(Session,"transport");Transport.FinishInitialization=true;Session.BeforeFrame();
            Session.QuickMatch();Transport.EstablishRoute(host);
            Transport.Deliver(Packet(1,w=>{
                w.Write("fixture-build");w.Write(!host);w.Write("Fixture peer");w.Write(8);w.Write(0u);
                Choice(w);w.Write(0u);w.Write(0u);w.Write(1u);w.Write(0u);
                var car=new Idas3OnlineCar{Selection=13};car.Words[4]=8;car.Write(w);
            }));
            Check(Session.HandshakeComplete,"Production compatibility handshake admitted the fixture peer");
            Session.BeforeFrame();Session.CompleteChallengerPresentation();
            Set(Session,"localReady",true);Set(Session,"remoteReady",true);Set(Session,"bestRtt",.1);
            if(host)Session.StartRace();else Load(99);
            Check(Session.StateName=="Loading"&&Session.ErrorText==""&&Idas3MultiplayerNative.Starts==1,"Production Load path started the native race");
        }
        static void Choice(BinaryWriter w){w.Write(17);w.Write(false);w.Write(false);w.Write(true);}
        internal byte[] Packet(byte kind,Action<BinaryWriter> payload){
            using(var m=new MemoryStream())using(var w=new BinaryWriter(m)){
                w.Write(0x504D3349u);w.Write((ushort)11);w.Write(kind);w.Write(202UL);
                w.Write(kind==1?0UL:(ulong)Get(Session,"localNonce"));payload(w);return m.ToArray();
            }
        }
        internal void Load(ulong race){Transport.Deliver(Packet(4,w=>{w.Write(race);w.Write((uint)Get(Session,"revision"));w.Write(0);Choice(w);}));}
        internal int Sent(byte kind)=>Transport.Sent.Count(p=>p[6]==kind);
        internal void Present(ulong race){
            var method=typeof(Idas3MultiplayerSession).GetMethod("ConfirmRacePresentation",Private);
            Check(method!=null,"Session exposes a race-bound presentation completion boundary");method.Invoke(Session,new object[]{race});
        }
        internal void Frame(double elapsed){
            UnityEngine.Time.realtimeSinceStartupAsDouble+=elapsed;
            Transport.Deliver(Packet(9,w=>w.Write(UnityEngine.Time.realtimeSinceStartupAsDouble)));
            Session.BeforeFrame();
        }
        internal void Release(double when){Transport.Deliver(Packet(6,w=>{w.Write(Session.CurrentRaceId);w.Write(when);}));}
        public void Dispose(){Set(Session,"nativeRace",false);Session.Dispose();}
    }
    static void Baseline(){
        using(var f=new Fixture()){
            Check(f.Sent(5)==1,"Baseline sends Loaded immediately after native initialization, before scenery presentation");
            UnityEngine.Time.realtimeSinceStartupAsDouble+=3;
            f.Release(11);
            Check(f.Session.DisconnectedFinish&&f.Session.StatusText.Contains("outside the allowed window"),"A start buffered during a three-second scenery load reproduces the synchronization failure");
        }
    }
    static void Guest(){
        using(var f=new Fixture()){
            Check(f.Sent(5)==0,"Guest does not announce Loaded while scenery is pending");
            f.Frame(20);Check(f.Sent(5)==0&&!f.Session.RaceReleased&&!f.Session.DisconnectedFinish&&f.Session.ErrorText=="","Twenty-second scenery delay stays in loading without releasing the race");
            f.Present(98);f.Frame(0);Check(f.Sent(5)==0,"A stale race's presentation cannot release the next race");
            f.Present(99);f.Frame(0);Check(f.Sent(5)==1,"Completed scenery sends exactly one Loaded acknowledgement");
            f.Present(99);f.Frame(.1);Check(f.Sent(5)==1,"Repeated presentation frames do not resend Loaded");
            f.Release(UnityEngine.Time.realtimeSinceStartupAsDouble+1);
            Check(f.Sent(7)==1&&f.Session.ErrorText=="","Start is acknowledged after presentation completes");
            f.Frame(1.01);Check(f.Session.RaceReleased&&Idas3MultiplayerNative.Releases==1,"Guest releases native gameplay once at the agreed start");
            typeof(Idas3MultiplayerSession).GetMethod("RetireNativeRace",Private).Invoke(f.Session,null);
            Set(f.Session,"localReady",true);Set(f.Session,"remoteReady",true);f.Load(100);
            f.Present(99);f.Frame(0);Check(f.Sent(5)==1,"Rematch discards the preceding race's ready marker");
            f.Present(100);f.Frame(0);Check(f.Sent(5)==2,"Rematch requires its own completed presentation");
        }
    }
    static void Host(){
        using(var f=new Fixture(true)){
            f.Transport.Deliver(f.Packet(5,w=>w.Write(f.Session.CurrentRaceId)));
            f.Frame(20);Check(f.Sent(6)==0&&!f.Session.DisconnectedFinish&&f.Session.ErrorText=="","A ready guest cannot release the host's unfinished scenery");
            f.Present(f.Session.CurrentRaceId);f.Frame(0);
            Check(f.Sent(6)==1&&f.Session.StateName=="Countdown","Host publishes a fresh start time only after both presentations are ready");
            f.Transport.Deliver(f.Packet(7,w=>w.Write(f.Session.CurrentRaceId)));f.Frame(1.01);
            Check(f.Session.RaceReleased&&Idas3MultiplayerNative.Releases==1,"Host releases once after the ready guest acknowledges");
        }
    }
    static void Timeout(){
        using(var f=new Fixture()){
            f.Frame(66);Check(f.Session.DisconnectedFinish&&f.Sent(5)==0,"A genuinely unfinished load still ends through the bounded neutral timeout");
        }
    }
    static void PumpUntil(Func<bool> done,Action pump){
        var clock=Stopwatch.StartNew();
        while(!done()&&clock.Elapsed.TotalSeconds<3){pump();UnityEngine.Time.realtimeSinceStartupAsDouble+=.001;Thread.Sleep(1);}
        if(!done())throw new TimeoutException("Local session did not reach its expected loading state");
    }
    static void TcpHostLoad(){
        var reservation=new TcpListener(IPAddress.Loopback,0);reservation.Start();
        int port=((IPEndPoint)reservation.LocalEndpoint).Port;reservation.Stop();
        UnityEngine.Time.realtimeSinceStartupAsDouble=10;
        using(var host=new Idas3MultiplayerSession(8,Path.Combine(Path.GetTempPath(),"idas3-tcp-host-loading-unavailable")))
        using(var guest=new Idas3MultiplayerSession(8,Path.Combine(Path.GetTempPath(),"idas3-tcp-guest-loading-unavailable"))){
            try {
                Set(host,"compatibility","fixture-build");Set(guest,"compatibility","fixture-build");
                host.ConfigureLocalTest(port,"Host");guest.ConfigureLocalTest(port,"Guest");
                host.HostRoom();guest.JoinRoom(host.RoomCode);
                Action pump=()=>{host.BeforeFrame();guest.BeforeFrame();};
                PumpUntil(()=>host.HandshakeComplete&&guest.HandshakeComplete,pump);pump();
                host.CompleteChallengerPresentation();guest.CompleteChallengerPresentation();
                PumpUntil(()=>host.CanReady&&guest.CanReady,pump);
                host.SetReady(true);guest.SetReady(true);PumpUntil(()=>host.CanStart,pump);
                Check(host.CanStart,"Real TCP sessions complete Hello, lobby and Ready before native loading");
                bool checkedSlowLoad=false;
                Idas3MultiplayerNative.StartAction=()=>{
                    Idas3MultiplayerNative.StartAction=null;
                    // The host stays inside its native call: only the guest can poll.
                    PumpUntil(()=>guest.StateName=="Loading",guest.BeforeFrame);
                    Check(!host.IsRacing&&guest.IsRacing,"Guest receives Load while the host is still initializing its native race");
                    UnityEngine.Time.realtimeSinceStartupAsDouble+=20;
                    guest.BeforeFrame();
                    checkedSlowLoad=guest.IsRacing&&!guest.DisconnectedFinish&&guest.ErrorText=="";
                    Check(checkedSlowLoad,"Twenty-second host load does not expire the guest's lobby timeout");
                    guest.ConfirmRacePresentation(guest.CurrentRaceId);guest.BeforeFrame();
                    Check(!(bool)Get(host,"remoteLoaded"),"Guest Loaded reply is not dispatched into the unfinished native host");
                };
                host.StartRace();
                Check(checkedSlowLoad&&host.IsRacing&&host.ErrorText=="","Host finishes native initialization and retains the connected guest");
                host.ConfirmRacePresentation(host.CurrentRaceId);
                PumpUntil(()=>(bool)Get(host,"releaseAck"),pump);
                UnityEngine.Time.realtimeSinceStartupAsDouble+=2;pump();
                Check(host.RaceReleased&&guest.RaceReleased,"Both real TCP sessions release after loading and presentation complete");
            } finally {
                Idas3MultiplayerNative.StartAction=null;
                Set(host,"nativeRace",false);Set(guest,"nativeRace",false);
            }
        }
    }
    static int Main(string[] args){try{if(args.Contains("baseline"))Baseline();else{Guest();Host();Timeout();TcpHostLoad();}return 0;}catch(Exception e){Console.Error.WriteLine(e);return 1;}}
}
