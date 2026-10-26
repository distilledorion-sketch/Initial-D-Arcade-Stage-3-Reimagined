using System;
using System.IO;
using System.Linq;
using System.Reflection;
using Idas3.Multiplayer;

// Production session, packet parser and clock deadlines; simulated native
// counters and transport. This fixture does not claim numerical/EOS coverage.
internal static class RaceProgressDriver
{
    const BindingFlags Private=BindingFlags.Instance|BindingFlags.NonPublic;
    static int checks;
    static void Check(bool value,string message){if(!value)throw new Exception(message);++checks;Console.WriteLine("PASS\t"+message);}
    static object Get(object target,string name)=>target.GetType().GetField(name,Private).GetValue(target);
    static void Set(object target,string name,object value)=>target.GetType().GetField(name,Private).SetValue(target,value);
    sealed class Fixture:IDisposable
    {
        internal readonly Idas3MultiplayerSession Session;
        internal readonly Idas3EosTransport Transport;
        internal int Disconnected;
        internal Fixture(){
            UnityEngine.Time.realtimeSinceStartupAsDouble=10;
            Idas3MultiplayerNative.Sample=new Idas3AuthorityStatus{size=96,version=1,frame=64,stalled=1,winner=-3};
            Idas3MultiplayerNative.Disconnects=Idas3MultiplayerNative.InputReceives=0;
            Session=new Idas3MultiplayerSession(8,Path.Combine(Path.GetTempPath(),"idas3-progress-no-saves"));
            Set(Session,"compatibility","fixture-build");Session.InitializeOnlinePresence();
            Transport=(Idas3EosTransport)Get(Session,"transport");Transport.FinishInitialization=true;Session.BeforeFrame();
            Session.QuickMatch();Transport.EstablishRoute();
            Transport.Deliver(Packet(1,w=>{
                w.Write("fixture-build");w.Write(false);w.Write("Fixture peer");w.Write(8);w.Write(0u);
                w.Write(3);w.Write(false);w.Write(false);w.Write(true);
                w.Write(0u);w.Write(0u);w.Write(1u);w.Write(0u);
                var car=new Idas3OnlineCar{Selection=13};car.Words[4]=8;car.Write(w);
            }));
            Check(Session.HandshakeComplete,"Fixture entered through the production compatibility handshake");
            Set(Session,"nativeRace",true);Set(Session,"raceId",99UL);Set(Session,"state","Racing");
            Set(Session,"<RaceReleased>k__BackingField",true);
            Session.RaceDisconnected+=()=>++Disconnected;
            Frame(0);
        }
        byte[] Packet(byte kind,Action<BinaryWriter> payload){
            using(var m=new MemoryStream())using(var w=new BinaryWriter(m)){
                w.Write(0x504D3349u);w.Write((ushort)11);w.Write(kind);w.Write(202UL);
                w.Write(kind==1?0UL:(ulong)Get(Session,"localNonce"));payload(w);return m.ToArray();
            }
        }
        internal void Frame(double elapsed,bool duplicateInput=false){
            UnityEngine.Time.realtimeSinceStartupAsDouble+=elapsed;
            Transport.Deliver(Packet(9,w=>w.Write(UnityEngine.Time.realtimeSinceStartupAsDouble)));
            if(duplicateInput)Transport.Deliver(Packet(18,w=>{w.Write(99UL);w.Write(40);w.Write(new byte[40]);w.Write(1UL);w.Write(1u);}));
            Session.BeforeFrame();Session.AfterFrame();
        }
        internal int RecoverySends=>Transport.Sent.Where((p,i)=>p[6]==18&&Transport.Reliable[i]).Count();
        public void Dispose(){Set(Session,"nativeRace",false);Session.Dispose();}
    }
    static void Recovery(){
        using(var f=new Fixture()){
            f.Frame(.9);Check(f.RecoverySends==0,"A brief delay keeps the ordinary unreliable race stream");
            f.Frame(.11);Check(f.RecoverySends==1,"Stalled confirmation retries the real authority payload reliably");
            var data=f.Transport.Sent.Last(p=>p[6]==18);
            Check(data.Length==1111&&BitConverter.ToUInt64(data,23)==99&&BitConverter.ToInt32(data,31)==1064,
                "Maximum history plus session framing fits one EOS payload and retains race identity");
            for(int i=0;i<9;i++)f.Frame(.05,true);
            Check(f.RecoverySends==1,"Recovery retry is bounded instead of flooding the reliable queue");
            f.Frame(.06,true);Check(f.RecoverySends==2,"Duplicate inputs and pings cannot postpone recovery");
            Idas3MultiplayerNative.Sample.confirmed=32;Idas3MultiplayerNative.Sample.frame=96;
            f.Frame(.1);int recovered=f.RecoverySends;
            for(int i=0;i<400;i++){
                ++Idas3MultiplayerNative.Sample.confirmed;++Idas3MultiplayerNative.Sample.frame;f.Frame(.05);
            }
            Check(f.RecoverySends==recovered&&!f.Session.DisconnectedFinish&&f.Session.AuthorityRecoveries==1,
                "Confirmed progress stops recovery and keeps a resumed race alive beyond the former deadline");
        }
    }
    static void PermanentStall(bool baseline){
        using(var f=new Fixture()){
            for(int i=0;i<310&&!f.Session.DisconnectedFinish;i++){
                // Speculative movement and valid packets are not a confirmation.
                ++Idas3MultiplayerNative.Sample.frame;f.Frame(.05,true);
            }
            if(baseline){
                Check(!f.Session.DisconnectedFinish&&f.Session.IsRacing&&f.RecoverySends==0,
                    "Published session remains stuck indefinitely with healthy pings but no confirmed race progress");return;
            }
            Check(f.Session.DisconnectedFinish&&f.Session.StateName=="Disconnected", "An unrecoverable race ends despite continuing valid pings and input packets");
            Check(f.Disconnected==1&&Idas3MultiplayerNative.Disconnects==1&&f.Transport.Leaves==1,
                "Progress timeout performs one neutral native disconnect and one transport cleanup");
            Check(f.Session.ResultText.Contains("NO RESULT RECORDED")&&f.Session.ResultText.Contains("0 POINTS"),
                "A stalled race cannot manufacture a winner or progression award");
            Check(f.RecoverySends>0&&f.RecoverySends<=29&&Idas3MultiplayerNative.InputReceives>200,
                "Recovery remains bounded while duplicate history continues arriving");
            for(int i=0;i<20;i++)f.Frame(.1);
            Check(f.Disconnected==1&&Idas3MultiplayerNative.Disconnects==1,"Repeated updates cannot repeat terminal race effects");
        }
    }
    static void InactiveAndBoundary(){
        using(var f=new Fixture()){
            Idas3MultiplayerNative.Sample.frame=0;
            Set(f.Session,"authorityProgressStarted",false);
            for(int i=0;i<400;i++)f.Frame(.05);
            Check(f.RecoverySends==0&&!f.Session.DisconnectedFinish,"Released car showcase does not trigger recovery before the first simulation frame");
            Idas3MultiplayerNative.Sample.frame=1;f.Frame(0);f.Frame(1.1);
            Check(f.RecoverySends==1,"The watchdog arms when the first native simulation frame runs");
        }
        using(var f=new Fixture()){
            Set(f.Session,"<RaceReleased>k__BackingField",false);Set(f.Session,"state","Loading");
            for(int i=0;i<400;i++)f.Frame(.05);
            Check(f.RecoverySends==0&&!f.Session.DisconnectedFinish,"Loading is governed by its existing timeout, not the running-race watchdog");
            Set(f.Session,"<RaceReleased>k__BackingField",true);Set(f.Session,"state","Racing");f.Frame(0);
            f.Frame(14.9,true);Check(!f.Session.DisconnectedFinish,"A running race gets the full recovery window after loading");
            Idas3MultiplayerNative.Sample.confirmed=1;f.Frame(.2);
            Check(!f.Session.DisconnectedFinish,"Confirmed progress on the timeout boundary wins before expiry");
        }
    }
    static int Main(string[] args){try{
        if(args.Contains("baseline")){PermanentStall(true);return 0;}
        Recovery();PermanentStall(false);InactiveAndBoundary();
        Console.WriteLine("TOTAL "+checks);return 0;
    }catch(Exception e){Console.Error.WriteLine(e);return 1;}}
}
