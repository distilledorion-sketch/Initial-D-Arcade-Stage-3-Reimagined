using System;
using System.IO;
using System.Reflection;
using Idas3.Multiplayer;

internal static class ResultSettlementDriver
{
    const BindingFlags Private=BindingFlags.Instance|BindingFlags.NonPublic;
    static string root;
    static bool nativeProgression;
    static int checks,advances;
    static void Check(bool value,string text){if(!value)throw new Exception(text);++checks;Console.WriteLine("PASS\t"+text);}
    static void Set(Idas3MultiplayerSession session,string field,object value)=>typeof(Idas3MultiplayerSession).GetField(field,Private).SetValue(session,value);
    static bool Committed(Idas3MultiplayerSession session)=>(bool)typeof(Idas3MultiplayerSession).GetField("recordCommitted",Private).GetValue(session);
    static object Get(Idas3MultiplayerSession session,string field)=>typeof(Idas3MultiplayerSession).GetField(field,Private).GetValue(session);
    static void Call(Idas3MultiplayerSession session,string name,params object[] args){
        try{typeof(Idas3MultiplayerSession).GetMethod(name,Private).Invoke(session,args);}
        catch(TargetInvocationException error){throw error.InnerException;}
    }
    static Idas3BattleRecord Advance(Idas3BattleRecord before,bool won,Idas3BattleRecord opponent,uint experience,out uint nextExperience){
        ++advances;
        if(nativeProgression)return Idas3MultiplayerRecords.Next(before,won,opponent,experience,out nextExperience);
        var after=before;++after.battles;if(won){++after.wins;after.streak=Math.Min(99,before.streak+1);}else after.streak=0;
        uint points=experience+(won?25u:10u);after.level+=points/100;nextExperience=points%100;return after;
    }
    static Idas3MultiplayerRecords Store(string path)=>new Idas3MultiplayerRecords(path,Advance);
    sealed class Fixture:IDisposable
    {
        internal readonly string Path;
        internal readonly Idas3MultiplayerRecords Records;
        internal readonly Idas3MultiplayerSession Session;
        internal readonly ChatFixtureTransport Transport;
        internal Fixture(string name,bool host=true,int winner=0){
            Path=System.IO.Path.Combine(root,name);Directory.CreateDirectory(Path);Records=Store(Path);
            Session=new Idas3MultiplayerSession(Records,8);Transport=new ChatFixtureTransport{IsHost=host};
            Set(Session,"transport",Transport);Set(Session,"nativeRace",true);Set(Session,"state","Racing");Set(Session,"raceId",123UL);
            Set(Session,"localNonce",101UL);Set(Session,"remoteNonce",202UL);
            Set(Session,"<RaceLocalRecord>k__BackingField",Idas3BattleRecord.Fresh);
            Set(Session,"<RaceRemoteRecord>k__BackingField",Idas3BattleRecord.Fresh);
            Set(Session,"<HandshakeComplete>k__BackingField",true);Set(Session,"<RaceReleased>k__BackingField",true);
            UnityEngine.Time.realtimeSinceStartupAsDouble=10;Set(Session,"lastHeard",10d);Set(Session,"lastPing",10d);
            Idas3MultiplayerNative.ResultCalls=Idas3MultiplayerNative.PointsCalls=Idas3MultiplayerNative.RecordCalls=Idas3MultiplayerNative.NativeAwards=0;
            Idas3MultiplayerNative.Disconnects=Idas3MultiplayerNative.NativeLeaves=0;
            Idas3MultiplayerNative.OnDisconnect=null;Idas3MultiplayerNative.OnNativeLeave=null;
            Idas3MultiplayerNative.FailNativeResult=Idas3MultiplayerNative.FailPresentation=false;
            Idas3MultiplayerNative.PointsToAward=winner<0?0:winner==(host?0:1)?2000:1000;
        }
        internal void Finish(int winner=0)=>Call(Session,"ShowResult",winner,6000UL,0UL);
        internal void Frame(double time){UnityEngine.Time.realtimeSinceStartupAsDouble=time;Session.BeforeFrame();}
        public void Dispose(){Set(Session,"nativeRace",false);Session.Dispose();}
    }
    static void Success(bool host,int winner){
        using(var f=new Fixture("success-"+host+"-"+winner,host,winner)){
            f.Finish(winner);var record=f.Records.Read(8);bool won=winner==(host?0:1);
            Check(Committed(f.Session)&&!f.Session.ResultSavePending&&record.battles==1&&record.wins==(won?1u:0u),"Successful "+host+"/"+winner+" result persists the correct win/loss");
            Check(f.Session.TuningPointsEarned==(won?2000:1000)&&f.Session.BattlePointsEarned==f.Records.ReadLevelPoints(8),"Recorded points are presented for "+host+"/"+winner);
            string file=System.IO.Path.Combine(f.Path,"online_records_v1/car_08.json");byte[] saved=File.ReadAllBytes(file);DateTime stamp=File.GetLastWriteTimeUtc(file);
            f.Finish(winner);f.Frame(12);
            Check(Idas3MultiplayerNative.ResultCalls==1&&Idas3MultiplayerNative.NativeAwards==1&&Idas3MultiplayerNative.RecordCalls==1,"Duplicate result and frame cannot repeat native awards or presentation");
            Check(Convert.ToBase64String(File.ReadAllBytes(file))==Convert.ToBase64String(saved)&&File.GetLastWriteTimeUtc(file)==stamp&&Store(f.Path).Read(8).battles==1,"Duplicate result preserves saved bytes, timestamp and reloaded receipt");
            bool conflict=false;try{f.Finish(1-winner);}catch(InvalidDataException){conflict=true;}
            Check(conflict&&f.Records.Read(8).wins==record.wins,"Conflicting late winner cannot change a settled result");
        }
    }
    static void FailedWrite(bool peerLeaves){
        using(var f=new Fixture("failed-write-"+peerLeaves)){
            string blocker=System.IO.Path.Combine(f.Path,"online_records_v1");File.WriteAllText(blocker,"Owned fixture blocks record directory creation");
            f.Finish();int attempts=advances;
            Check(f.Session.ResultSavePending&&!Committed(f.Session)&&f.Session.ErrorText=="Battle record could not be saved.","Failed write stays pending rather than becoming committed");
            f.Session.ReturnToLobby();f.Session.ReturnToMode();f.Session.ClearError();
            Check(!f.Session.LocalContinueRequested&&f.Session.IsRacing&&!f.Session.CanReturnToLobby&&f.Session.ErrorText.Length>0,"Continue, No and error dismissal cannot discard the pending record");
            f.Frame(10.25);f.Frame(10.75);
            Check(advances==attempts&&Idas3MultiplayerNative.NativeAwards==1,"Pending frames throttle disk retries and do not repeat native tuning awards");
            if(peerLeaves){
                Call(f.Session,"DisconnectRace","Fixture peer disappeared");
                Check(f.Session.ResultPeerLeft&&!f.Session.DisconnectedFinish&&f.Session.ResultText=="YOU WIN"&&Idas3MultiplayerNative.Disconnects==0,"Peer departure during a failed save preserves the earned win");
            }
            File.Delete(blocker);int sent=f.Transport.Sent.Count;f.Frame(12);
            Check(Committed(f.Session)&&!f.Session.ResultSavePending&&f.Session.ErrorText==""&&Store(f.Path).Read(8).wins==1,"Frame retry saves the win after filesystem repair without another result packet");
            Check(Idas3MultiplayerNative.NativeAwards==1&&Idas3MultiplayerNative.RecordCalls==1&&f.Session.BattlePointsEarned==f.Records.ReadLevelPoints(8),"Retry updates displayed battle points without awarding tuning twice");
            if(peerLeaves)Check(f.Transport.Sent.Count==sent,"Successful save after peer departure sends no stale record packet");
        }
    }
    static void LockedRecord(){
        using(var f=new Fixture("locked-record")){
            f.Records.Commit(8,99,true,Idas3BattleRecord.Fresh);int before=f.Records.ReadLevelPoints(8);
            string file=System.IO.Path.Combine(f.Path,"online_records_v1/car_08.json");string original=File.ReadAllText(file);
            using(var locked=new FileStream(file,FileMode.Open,FileAccess.Read,FileShare.Read)){
                f.Finish();Check(f.Session.ResultSavePending&&File.ReadAllText(file)==original,"Locked record replacement preserves the original history");
            }
            f.Frame(12);
            Check(Committed(f.Session)&&Store(f.Path).Read(8).battles==2&&f.Session.BattlePointsEarned==f.Records.ReadLevelPoints(8)-before,"Unlocked replacement retries once with the correct points delta");
        }
    }
    static void Neutral(int winner){
        using(var f=new Fixture("neutral-"+winner,true,winner)){
            string blocker=System.IO.Path.Combine(f.Path,"online_records_v1");File.WriteAllText(blocker,"Do not write history for neutral outcomes");
            int before=advances;f.Finish(winner);f.Frame(12);
            Check(Committed(f.Session)&&!f.Session.ResultSavePending&&advances==before&&File.Exists(blocker)&&f.Session.BattlePointsEarned==0,"Neutral outcome "+winner+" needs no record write or progression call");
        }
    }
    static byte[] RecordPacket(Idas3BattleRecord record){
        using(var stream=new MemoryStream())using(var writer=new BinaryWriter(stream)){
            writer.Write(0x504D3349u);writer.Write((ushort)11);writer.Write((byte)17);
            writer.Write(202UL);writer.Write(101UL);writer.Write(123UL);
            writer.Write(record.battles);writer.Write(record.wins);writer.Write(record.level);writer.Write(record.streak);
            return stream.ToArray();
        }
    }
    static void RemoteRecordWhilePending(){
        using(var f=new Fixture("peer-record")){
            string blocker=System.IO.Path.Combine(f.Path,"online_records_v1");File.WriteAllText(blocker,"Blocked record write");
            f.Finish();var remote=new Idas3BattleRecord{battles=11,wins=4,level=2,streak=0};
            Call(f.Session,"Receive",RecordPacket(remote));
            Check(f.Session.ResultSavePending&&!f.Session.ResultPeerLeft&&((Idas3BattleRecord)Get(f.Session,"remoteRecord")).Equals(remote),
                "Peer's committed record is accepted while our save is pending without closing the room");
            File.Delete(blocker);f.Frame(12);
            Check(Committed(f.Session)&&((Idas3BattleRecord)Get(f.Session,"remoteRecord")).Equals(remote),"Retry retains the peer's updated history");
        }
    }
    static void BufferedRemoteRecord(bool host){
        using(var f=new Fixture("buffered-record-"+host,host)){
            var remote=new Idas3BattleRecord{battles=11,wins=4,level=2,streak=0};
            Call(f.Session,"Receive",RecordPacket(remote));
            Check(!f.Session.ResultPeerLeft&&Get(f.Session,"pendingAuthorityRecord")!=null&&f.Records.Read(8).battles==0,
                "Early peer record waits for local confirmed result on "+(host?"host":"guest"));
            f.Finish();
            Check(((Idas3BattleRecord)Get(f.Session,"remoteRecord")).Equals(remote)&&Get(f.Session,"pendingAuthorityRecord")==null,
                "Confirmed result consumes buffered peer history on "+(host?"host":"guest"));
        }
    }
    static void ResetWhilePending(bool full){
        using(var f=new Fixture("reset-pending-"+full)){
            string blocker=System.IO.Path.Combine(f.Path,"online_records_v1");File.WriteAllText(blocker,"Blocked record write");
            f.Finish();File.Delete(blocker);
            string[] ids={"fixture-reset"},empty=Array.Empty<string>();
            Call(f.Session,"ApplyAdministrativeStatsResets","0123456789abcdef0123456789abcdef",full?empty:ids,full?ids:empty);
            int awards=Idas3MultiplayerNative.NativeAwards;f.Frame(12);f.Finish();
            var expected=Idas3BattleRecord.Fresh;if(!full){expected.battles=expected.wins=1;}
            Check(!f.Session.IsRacing&&!f.Session.ResultSavePending&&f.Records.Read(8).Equals(expected),
                "Administrative "+(full?"full":"level")+" reset follows earned history and prevents stale aura credit");
            Check(Idas3MultiplayerNative.NativeAwards==awards&&f.Session.StatsResetNotice.Length>0&&f.Session.ErrorText.Length==0,"Reset preserves already-earned native tuning and clears the superseded save error");
        }
    }
    static void NativeFailure(){
        using(var f=new Fixture("native-save-failure")){
            Idas3MultiplayerNative.FailNativeResult=true;f.Finish();
            Check(f.Session.ResultSavePending&&Idas3MultiplayerNative.NativeAwards==0&&f.Records.Read(8).battles==0&&File.Exists(System.IO.Path.Combine(f.Path,"online_result_v1.json")),
                "Failed native award retains durable intent without premature history credit");
            f.Session.ReturnToMode();Check(f.Session.IsRacing,"Native save failure cannot enter offline play over the pending profile");
            Idas3MultiplayerNative.FailNativeResult=false;f.Session.RetryResultSave();
            Check(Committed(f.Session)&&Idas3MultiplayerNative.NativeAwards==1&&f.Records.Read(8).battles==1,"Explicit retry completes a failed native save once");
        }
    }
    static void PresentationFailure(){
        using(var f=new Fixture("presentation-failure")){
            Idas3MultiplayerNative.FailPresentation=true;f.Finish();
            Check(!Committed(f.Session)&&f.Session.ResultSavePending&&f.Records.Read(8).battles==1&&Idas3MultiplayerNative.NativeAwards==1,
                "Presentation failure does not mark the result screen complete or discard saved rewards");
            f.Frame(10.5);Check(Idas3MultiplayerNative.RecordCalls==1,"Presentation retry is throttled");
            Idas3MultiplayerNative.FailPresentation=false;f.Session.RetryResultSave();
            Check(Committed(f.Session)&&f.Session.ErrorText.Length==0&&Idas3MultiplayerNative.NativeAwards==1&&f.Records.Read(8).battles==1,
                "Result screen retry never repeats either saved reward");
        }
    }
    public static int Main(string[] args){try{
        root=System.IO.Path.GetFullPath(args[0]);if(Directory.Exists(root))throw new Exception("Result fixtures must be new");Directory.CreateDirectory(root);
        nativeProgression=args.Length>1&&args[1]=="native";
        foreach(bool host in new[]{true,false})foreach(int winner in new[]{0,1})Success(host,winner);
        FailedWrite(false);FailedWrite(true);LockedRecord();Neutral(-1);Neutral(2);
        RemoteRecordWhilePending();ResetWhilePending(false);ResetWhilePending(true);
        BufferedRemoteRecord(true);BufferedRemoteRecord(false);
        NativeFailure();PresentationFailure();
        Console.WriteLine("CHECKS "+checks);return 0;
    }catch(Exception error){Console.Error.WriteLine(error);return 1;}}
}
