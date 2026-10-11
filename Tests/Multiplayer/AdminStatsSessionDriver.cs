using System;
using System.IO;
using System.Reflection;
using Idas3.Multiplayer;

namespace Idas3.Multiplayer {
    public sealed partial class Idas3MultiplayerRecords {
        internal void SeedAdminSessionFixture(bool allCars) {
            for(int car=0;car<35;++car)if(allCars||car==8)
                Write(car,new State{car=car,record=new Idas3BattleRecord{battles=90,wins=70,level=31,streak=20},experience=77},false);
        }
    }
}
static class AdminStatsSessionDriver {
    const string Player="0123456789abcdef0123456789abcdef";
    const BindingFlags Hidden=BindingFlags.Instance|BindingFlags.NonPublic;
    static int checks;
    static void Check(bool value,string name){if(!value)throw new Exception(name);++checks;Console.WriteLine("PASS\t"+name);}
    static object Get(Idas3MultiplayerSession session,string name)=>typeof(Idas3MultiplayerSession).GetField(name,Hidden).GetValue(session);
    static void Set(Idas3MultiplayerSession session,string name,object value)=>typeof(Idas3MultiplayerSession).GetField(name,Hidden).SetValue(session,value);
    sealed class Fixture:IDisposable {
        internal readonly string Root;
        internal readonly Idas3MultiplayerSession Session;
        internal readonly Idas3EosTransport Transport;
        internal readonly Idas3MultiplayerRecords Records;
        internal Fixture(string parent,string name){
            Root=Path.Combine(parent,name);
            Session=new Idas3MultiplayerSession(8,Root);Set(Session,"compatibility","fixture-build");
            Session.InitializeOnlinePresence();Transport=(Idas3EosTransport)Get(Session,"transport");
            Transport.ReentrantCancellation=false;Transport.FinishInitialization=true;Session.BeforeFrame();Records=(Idas3MultiplayerRecords)Get(Session,"records");
            Records.SeedAdminSessionFixture(name=="idle");
            Idas3MultiplayerNative.Disconnects=Idas3MultiplayerNative.NativeLeaves=0;
            Idas3MultiplayerNative.OnDisconnect=Idas3MultiplayerNative.OnNativeLeave=null;
        }
        internal string Journal=>Path.Combine(Root,"online_records_v1","moderation.json");
        internal bool Reset(string id,bool full=true)=>Transport.ApplyStatsResets(Player,full?Array.Empty<string>():new[]{id},full?new[]{id}:Array.Empty<string>());
        internal void Race(bool settled=false){
            Transport.AttachRoom();Set(Session,"nativeRace",true);Set(Session,"recordCommitted",settled);Set(Session,"state",settled?"Results":"Racing");
            Set(Session,"raceId",777UL);Set(Session,"remoteNonce",202UL);Set(Session,"localNonce",101UL);
            Set(Session,"<HandshakeComplete>k__BackingField",true);
            Set(Session,"<RaceReleased>k__BackingField",true);
            Set(Session,"<RaceLocalRecord>k__BackingField",Records.Read(8));
            Set(Session,"<RaceRemoteRecord>k__BackingField",Records.Read(9));
        }
        public void Dispose(){Idas3MultiplayerNative.OnDisconnect=Idas3MultiplayerNative.OnNativeLeave=null;Set(Session,"nativeRace",false);Session.Dispose();}
    }
    static byte[] ResultPacket(){
        using(var stream=new MemoryStream())using(var writer=new BinaryWriter(stream)){
            writer.Write(0x504D3349u);writer.Write((ushort)11);writer.Write((byte)12);writer.Write(202UL);writer.Write(101UL);
            writer.Write(777UL);writer.Write(0);writer.Write(200UL);writer.Write(300UL);return stream.ToArray();
        }
    }
    static void LateResult(Fixture f){
        f.Transport.Deliver(ResultPacket());
        typeof(Idas3MultiplayerSession).GetMethod("ShowResult",Hidden).Invoke(f.Session,new object[]{0,200UL,300UL});
    }
    static int Main(string[] args){
        try{
            string root=args[0];if(Directory.Exists(root))throw new IOException("Session integration requires a fresh isolated root.");Directory.CreateDirectory(root);
            using(var f=new Fixture(root,"idle")){
                Check(f.Transport.ApplyStatsResets!=null,"session wires authenticated transport reset callback");
                Check(f.Reset("idle-reset"),"idle authenticated reset is applied");
                bool fresh=true;for(int car=0;car<35;++car)fresh&=f.Records.Read(car).Equals(Idas3BattleRecord.Fresh)&&f.Records.ReadLevelPoints(car)==0;
                Check(fresh,"idle reset clears all 35 car records using real storage");
                Check(f.Session.StatsResetNotice.Length>0&&Idas3MultiplayerNative.Disconnects==0,"idle reset displays notice without native race work");
                Check(!f.Reset("idle-reset"),"same authenticated reset is idempotent through session callback");
            }
            using(var f=new Fixture(root,"active")){
                f.Race();
                Idas3MultiplayerNative.OnDisconnect=()=>Check(f.Session.DisconnectedFinish&&!f.Session.RaceReleased&&!File.Exists(f.Journal)&&f.Records.Read(8).level==31,
                    "neutral finish and release guard are set before native disconnect and before any reset write");
                Check(f.Reset("active-reset"),"active race reset completes after neutral finish");
                Check(f.Session.DisconnectedFinish&&!f.Session.HandshakeComplete&&!f.Session.InLobby&&Idas3MultiplayerNative.Disconnects==1,"active reset leaves room and preserves neutral terminal state");
                LateResult(f);
                Check(f.Records.Read(8).Equals(Idas3BattleRecord.Fresh)&&!(bool)Get(f.Session,"recordCommitted")&&f.Session.BattlePointsEarned==0,
                    "buffered Results and direct result path cannot credit frozen race after reset");
                Check((int)Get(f.Session,"pendingAuthorityWinner")==-3,"buffered Results cannot become pending authority result after reset");
            }
            using(var f=new Fixture(root,"settled")){
                f.Race(true);Set(f.Session,"<TuningPointsEarned>k__BackingField",321);
                Check(f.Reset("settled-reset",false),"settled result accepts level-only reset");
                Check(!f.Session.IsRacing&&!f.Session.DisconnectedFinish&&Idas3MultiplayerNative.Disconnects==0&&Idas3MultiplayerNative.NativeLeaves==1,
                    "settled result exits native race instead of rewriting it as a disconnect");
                Check(f.Session.TuningPointsEarned==321&&f.Records.Read(8).battles==90&&f.Records.Read(8).level==1,
                    "settled tuning remains earned while level-only action preserves battle history");
            }
            using(var f=new Fixture(root,"disk-failure")){
                f.Race();string carPath=Path.Combine(f.Root,"online_records_v1","car_08.json"),original=File.ReadAllText(carPath);
                Directory.CreateDirectory(f.Journal);bool failed=false;
                try{f.Reset("disk-failure-reset");}catch(IOException){failed=true;}
                Check(failed&&f.Session.DisconnectedFinish&&Idas3MultiplayerNative.Disconnects==1,"reset persistence failure occurs only after active race is guarded");
                LateResult(f);
                bool readBlocked=false;try{f.Records.Read(8);}catch(IOException){readBlocked=true;}
                Check(!(bool)Get(f.Session,"recordCommitted")&&readBlocked&&File.ReadAllText(carPath)==original,"failed reset blocks stale record reads and cannot commit frozen race or replace original record");
            }
            using(var f=new Fixture(root,"native-failure")){
                f.Race();Idas3MultiplayerNative.OnDisconnect=()=>{throw new IOException("fixture disconnect unavailable");};
                Idas3MultiplayerNative.OnNativeLeave=()=>{throw new IOException("fixture leave unavailable");};
                Check(f.Reset("native-failure-reset")&&f.Session.DisconnectedFinish,"native shutdown failures retain neutral managed guard during reset");
                LateResult(f);bool stopped=false;try{f.Session.BeforeFrame();}catch(InvalidOperationException){stopped=true;}
                Check(stopped&&!(bool)Get(f.Session,"recordCommitted")&&f.Records.Read(8).Equals(Idas3BattleRecord.Fresh),"native shutdown failure blocks further frame execution and result credit");
            }
            using(var f=new Fixture(root,"peer-packet")){
                f.Transport.Deliver(ResultPacket());
                Check(!File.Exists(f.Journal)&&f.Records.Read(8).level==31&&f.Session.StatsResetNotice.Length==0,
                    "unadmitted peer packet cannot invoke administrative record reset");
            }
            Console.WriteLine("Completed "+checks+" actual session/storage integration checks.");return 0;
        }catch(Exception error){Console.Error.WriteLine(error);return 1;}
    }
}
