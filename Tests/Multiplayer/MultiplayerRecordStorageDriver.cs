using System;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;

namespace Idas3.Multiplayer
{
    public sealed partial class Idas3MultiplayerRecords
    {
        private const string Player="0123456789abcdef0123456789abcdef",OtherPlayer="fedcba9876543210fedcba9876543210";
        private static readonly string[] Empty=Array.Empty<string>();
        private static readonly Idas3BattleRecord Opponent=new Idas3BattleRecord{battles=80,wins=60,level=22,streak=9};
        private static int checks;
        private static void Check(bool value,string message)
        {
            if(!value)throw new InvalidOperationException(message);++checks;Console.WriteLine("PASS\t"+message);
        }
        private static void Reject(Action action,string message)
        {
            bool failed=false;
            try{action();}catch(Exception error)when(error is ArgumentException||error is IOException||error is InvalidDataException||error is UnauthorizedAccessException){failed=true;}
            Check(failed,message);
        }
        private static Idas3BattleRecord Earn(Idas3BattleRecord before,bool won,Idas3BattleRecord opponent,uint xp,out uint nextXp)
        {
            nextXp=42;
            return new Idas3BattleRecord{battles=before.battles+1,wins=before.wins+(won?1u:0u),level=12,streak=won?1u:0u};
        }
        private static Idas3BattleRecord MustNotAdvance(Idas3BattleRecord before,bool won,Idas3BattleRecord opponent,uint xp,out uint nextXp)
        {throw new InvalidOperationException("A pre-reset receipt was awarded again.");}
        private static void Seed(Idas3MultiplayerRecords store,int car)
        {
            store.Write(car,new State{car=car,record=new Idas3BattleRecord{battles=100u+(uint)car,wins=50u+(uint)car,level=31,streak=12},experience=77,
                receipts=new[]{new Receipt{raceId="777",won=true,opponent=Opponent}}},false);
        }
        private static string LegacyBytes(int car)
        {
            // Literal field layout from v1, independent of the production Hash.
            string state="{\"version\":1,\"car\":"+car+",\"record\":{\"battles\":100,\"wins\":50,\"level\":31,\"streak\":12},\"experience\":77,\"receipts\":[{\"raceId\":\"777\",\"won\":true,\"opponent\":{\"battles\":80,\"wins\":60,\"level\":22,\"streak\":9}}]}";
            string hash;using(var sha=SHA256.Create())hash=Convert.ToBase64String(sha.ComputeHash(Encoding.UTF8.GetBytes(state)));
            return "{\"state\":"+state+",\"sha256\":\""+hash+"\"}";
        }
        private static Idas3MultiplayerRecords Store(string root,string name)=>new Idas3MultiplayerRecords(Path.Combine(root,name));
        private static void WriteLegacy(Idas3MultiplayerRecords store,int car)
        {Directory.CreateDirectory(store.directory);File.WriteAllText(store.FilePath(car),LegacyBytes(car));}
        internal static int RunStorageChecks(string root)
        {
            if(Directory.Exists(root)||File.Exists(root))throw new IOException("Storage checks require a fresh isolated root.");
            Directory.CreateDirectory(root);
            RunDiagnosticChecks(root);
            Check(true,"Existing atomic record, receipt, checksum and corruption diagnostic passes with genuine JSON serialization");

            var legacy=Store(root,"legacy");WriteLegacy(legacy,0);
            Check(legacy.Read(0).level==31&&legacy.ReadLevelPoints(0)==3077,"Original v1 field-order checksum remains readable");
            legacy.Commit(0,1001,true,Opponent,Earn);
            Check(legacy.ReadFile(legacy.FilePath(0),0).version==2,"A normal commit migrates a v1 record to v2 without losing history");

            var store=Store(root,"all-cars");for(int car=0;car<35;++car)Seed(store,car);
            string userdata=Path.GetDirectoryName(store.directory);
            string[] unrelated={Path.Combine(userdata,"car-profiles.bin"),Path.Combine(userdata,"time-attack.json"),Path.Combine(userdata,"tuning.dat")};
            foreach(string path in unrelated)File.WriteAllText(path,"unrelated save fixture");
            int callbacks=0;
            Check(store.ApplyAdministrativeResets(Player,new[]{"level-one"},Empty,()=>{
                ++callbacks;Check(!File.Exists(store.ModerationPath)&&store.Read(0).level==31,"Session stop callback runs before journal or records change");
            }),"A new level reset reports a change");
            for(int car=0;car<35;++car){
                var state=store.Load(car,out _);
                Check(state.record.battles==100u+car&&state.record.wins==50u+car&&state.record.level==1&&state.record.streak==0&&state.experience==0&&state.receipts.Length==1,
                    "Level reset clears level, XP and aura streak while preserving counters and receipts for car "+car);
            }
            Check(File.ReadAllText(store.ModerationPath)==File.ReadAllText(store.ModerationPath+".previous")&&!File.Exists(store.ModerationPath+".pending"),"Completed journal primary and backup both retain the full reset union");
            store.Commit(0,1001,true,Opponent,Earn);string earned=File.ReadAllText(store.FilePath(0));
            var reloaded=new Idas3MultiplayerRecords(userdata);
            Check(!reloaded.ApplyAdministrativeResets(Player.ToUpperInvariant(),new[]{"level-one","level-one"},Empty,()=>++callbacks)&&callbacks==1,
                "Reconnect, duplicate IDs and player-ID casing do not reset a second time or stop the session");
            Check(File.ReadAllText(store.FilePath(0))==earned&&reloaded.Read(0).level==12,"New progress survives repeat reset requests and reload");
            Check(!reloaded.ApplyAdministrativeResets(Player,Empty,Empty)&&reloaded.Read(0).level==12,"Shrinking active sanctions does not discard reset history or reset earned progress");
            Check(reloaded.ApplyAdministrativeResets(Player,Empty,new[]{"full-one"},()=>++callbacks)&&callbacks==2,"A new full reset invokes the session stop callback once");
            Check(Enumerable.Range(0,35).All(car=>reloaded.Read(car).Equals(Idas3BattleRecord.Fresh)&&reloaded.ReadLevelPoints(car)==0),"Full reset clears battle counters, level, XP and streak for all 35 models");
            Check(reloaded.Commit(0,777,true,Opponent,MustNotAdvance).Equals(Idas3BattleRecord.Fresh)&&reloaded.Commit(0,1001,true,Opponent,MustNotAdvance).Equals(Idas3BattleRecord.Fresh),
                "Full reset preserves original and subsequent race receipts so old results cannot award again");
            reloaded.Commit(0,1002,true,Opponent,Earn);
            Check(reloaded.ApplyAdministrativeResets(Player,new[]{"same-second-a","same-second-b"},Empty),"Distinct reset references in one second are all recorded");
            var sameSecond=reloaded.Load(0,out _);
            Check(sameSecond.appliedResetKeys.Any(key=>key.EndsWith("same-second-a"))&&sameSecond.appliedResetKeys.Any(key=>key.EndsWith("same-second-b")),"Exact applied-key set retains both same-second events");
            Check(sameSecond.record.battles==1&&sameSecond.record.wins==1,"An already applied full reset does not erase counters on a later level-only reset");
            reloaded.Commit(0,1003,true,Opponent,Earn);
            Check(reloaded.ApplyAdministrativeResets(Player,Empty,new[]{"same-second-a"})&&reloaded.Read(0).battles==0,"The same reference under a different action is a distinct reset");
            reloaded.Commit(0,1004,true,Opponent,Earn);
            Check(reloaded.ApplyAdministrativeResets(OtherPlayer,Empty,new[]{"same-second-a"})&&reloaded.Read(0).battles==0,"The same reference for a different PUID is a distinct reset");
            Check(unrelated.All(path=>File.ReadAllText(path)=="unrelated save fixture"),"Offline profiles, tuning and time-attack sentinels remain byte-for-byte unchanged");

            var restoration=Store(root,"restoration");WriteLegacy(restoration,0);restoration.ApplyAdministrativeResets(Player,Empty,new[]{"wipe"});
            File.WriteAllText(restoration.FilePath(0),LegacyBytes(0));
            Check(restoration.Read(0).Equals(Idas3BattleRecord.Fresh),"Restoring a valid v1 primary reapplies the persisted reset floor before Read");
            File.WriteAllText(restoration.FilePath(0),"{damaged");File.WriteAllText(restoration.FilePath(0)+".previous",LegacyBytes(0));
            Check(restoration.Read(0).Equals(Idas3BattleRecord.Fresh)&&Directory.GetFiles(restoration.directory,"car_00.json.unreadable-*").Length==1,
                "A damaged primary with a pre-reset backup cannot resurrect counters and preserves corrupt evidence");
            File.Delete(restoration.FilePath(0));File.WriteAllText(restoration.FilePath(0)+".previous",LegacyBytes(0));
            Check(restoration.ReadLevelPoints(0)==0&&restoration.Read(0).Equals(Idas3BattleRecord.Fresh),"Missing primary recovery applies the floor to the old backup before exposing XP");
            File.WriteAllText(restoration.FilePath(0),LegacyBytes(0));
            Check(restoration.Commit(0,777,true,Opponent,MustNotAdvance).Equals(Idas3BattleRecord.Fresh),"Commit applies the reset floor before duplicate receipt handling");
            File.WriteAllText(restoration.FilePath(0),LegacyBytes(0));restoration.Commit(0,5001,true,Opponent,Earn);
            Check(restoration.Read(0).battles==1&&restoration.Read(0).wins==1,"A new Commit progresses from the reset floor, never restored old counters");

            var journal=Store(root,"journal");Seed(journal,0);journal.ApplyAdministrativeResets(Player,Empty,new[]{"full"});
            File.WriteAllText(journal.ModerationPath,"{damaged");File.WriteAllText(journal.FilePath(0),LegacyBytes(0));
            Check(journal.Read(0).Equals(Idas3BattleRecord.Fresh)&&File.ReadAllText(journal.ModerationPath)==File.ReadAllText(journal.ModerationPath+".previous"),
                "Damaged journal primary recovers the complete current floor from its mirrored backup");
            File.Delete(journal.ModerationPath);File.WriteAllText(journal.FilePath(0),LegacyBytes(0));
            Check(journal.Read(0).Equals(Idas3BattleRecord.Fresh),"Missing journal primary recovers the current reset floor");
            journal.Commit(0,9001,true,Opponent,Earn);
            var pending=new ModerationState{resetKeys=new[]{Player+"|F|full",Player+"|F|pending-new"}};
            journal.WriteModerationFile(journal.ModerationPath+".pending",pending);
            Check(journal.Read(0).Equals(Idas3BattleRecord.Fresh)&&journal.ReadModerationFile(journal.ModerationPath+".previous").resetKeys.Length==2&&!File.Exists(journal.ModerationPath+".pending"),
                "Interrupted journal transaction recovers the new pending union before returning records");
            File.WriteAllText(journal.ModerationPath+".pending","{damaged intent");string recordBefore=File.ReadAllText(journal.FilePath(0));
            Reject(()=>journal.Read(0),"Damaged pending journal blocks instead of reverting to an older completed floor");
            Check(File.ReadAllText(journal.FilePath(0))==recordBefore,"Rejected pending-journal recovery does not mutate car records");
            File.Delete(journal.ModerationPath+".pending");File.WriteAllText(journal.ModerationPath,"{bad primary");File.WriteAllText(journal.ModerationPath+".previous","{bad backup");
            Reject(()=>journal.Read(0),"Two unreadable journal copies block record reads");
            Reject(()=>journal.Commit(0,9002,true,Opponent,Earn),"Two unreadable journal copies block record commits");
            Check(File.ReadAllText(journal.ModerationPath)=="{bad primary"&&File.ReadAllText(journal.ModerationPath+".previous")=="{bad backup","Unrecoverable journal evidence remains unchanged");

            var interrupted=Store(root,"interrupted-journal");Seed(interrupted,0);
            interrupted.ApplyAdministrativeResets(Player,new[]{"first"},Empty);interrupted.Commit(0,8001,true,Opponent,Earn);
            using(var locked=new FileStream(interrupted.ModerationPath+".previous",FileMode.Open,FileAccess.Read,FileShare.None)){
                Reject(()=>interrupted.ApplyAdministrativeResets(Player,Empty,new[]{"second"}),"A journal backup write failure blocks the new reset");
                Check(File.Exists(interrupted.ModerationPath+".pending")&&interrupted.ReadFile(interrupted.FilePath(0),0).record.battles==101,
                    "Interrupted journal write retains durable pending intent before touching cars");
                Reject(()=>interrupted.Read(0),"Reads block while the reset journal cannot finish its pending transaction");
            }
            Check(interrupted.ApplyAdministrativeResets(Player,Empty,Empty)&&interrupted.Read(0).Equals(Idas3BattleRecord.Fresh),
                "Retry after a journal write failure completes the pending reset even when its active ID is absent");
            Check(interrupted.ReadModerationFile(interrupted.ModerationPath).resetKeys.Length==2&&
                File.ReadAllText(interrupted.ModerationPath)==File.ReadAllText(interrupted.ModerationPath+".previous")&&!File.Exists(interrupted.ModerationPath+".pending"),
                "Recovered journal transaction keeps the complete union in both copies");

            var partial=Store(root,"partial");for(int car=0;car<35;++car)Seed(partial,car);
            // An exclusive handle prevents replacement on Windows after earlier
            // models have completed, exercising actual filesystem failure/retry.
            using(var locked=new FileStream(partial.FilePath(5),FileMode.Open,FileAccess.Read,FileShare.None)){
                Reject(()=>partial.ApplyAdministrativeResets(Player,Empty,new[]{"partial-full"},()=>++callbacks),"A blocked car file makes a partial reset fail");
                Check(partial.ReadModerationFile(partial.ModerationPath).resetKeys.Length==1&&partial.ReadFile(partial.FilePath(0),0).record.battles==0,
                    "The durable floor exists before a later car write fails");
            }
            int beforeRetry=callbacks;
            Check(partial.ApplyAdministrativeResets(Player,Empty,new[]{"partial-full"},()=>++callbacks)&&callbacks==beforeRetry+1,
                "Retry invokes the stop callback and completes remaining cars after a partial failure");
            Check(Enumerable.Range(0,35).All(car=>partial.Read(car).Equals(Idas3BattleRecord.Fresh)),"Partial-reset retry leaves every car reset");
            Check(!partial.ApplyAdministrativeResets(Player,Empty,new[]{"partial-full"},()=>++callbacks)&&callbacks==beforeRetry+1,
                "A fully recovered reset is exact-once on later retries");

            var failedCallback=Store(root,"callback");Seed(failedCallback,0);
            Reject(()=>failedCallback.ApplyAdministrativeResets(Player,new[]{"cancelled"},Empty,()=>{throw new IOException("Cannot stop active race");}),"Session-stop failure aborts reset before persistence");
            Check(!File.Exists(failedCallback.ModerationPath)&&failedCallback.Read(0).level==31,"Failed session-stop callback leaves original journal and stats unchanged");

            var invalid=Store(root,"invalid");
            Reject(()=>invalid.ApplyAdministrativeResets("not-a-player",new[]{"x"},Empty),"Malformed PUID rejected");
            Reject(()=>invalid.ApplyAdministrativeResets(new string('g',32),new[]{"x"},Empty),"Nonhex PUID rejected");
            foreach(string id in new[]{"", "  ", " padded", "nul\0id", "delim|id", "unicode-é", new string('x',129)})
                Reject(()=>invalid.ApplyAdministrativeResets(Player,new[]{id},Empty),"Invalid or oversized reference rejected (length "+id.Length+")");
            Reject(()=>invalid.ApplyAdministrativeResets(Player,null,Empty),"Null reset list rejected");
            Reject(()=>invalid.ApplyAdministrativeResets(Player,Enumerable.Repeat("x",1025).ToArray(),Empty),"Oversized active reset list rejected before deduplication");
            Check(!Directory.Exists(invalid.directory),"All invalid requests leave storage untouched");
            Check(!invalid.ApplyAdministrativeResets(Player,Empty,Empty)&&!Directory.Exists(invalid.directory),"No directives and no journal cause no writes");
            Check(!Directory.GetFiles(root,"*.new-*",SearchOption.AllDirectories).Any(),"Atomic record and journal operations leave no temporary files");
            return checks;
        }
    }
}
internal static class MultiplayerRecordStorageDriver
{
    private static int Main(string[] args)
    {
        try{
            if(args.Length!=1)throw new ArgumentException("Pass a fresh isolated fixture directory.");
            int count=Idas3.Multiplayer.Idas3MultiplayerRecords.RunStorageChecks(Path.GetFullPath(args[0]));
            Console.WriteLine("TOTAL\t"+count);return 0;
        }catch(Exception error){Console.Error.WriteLine(error);return 1;}
    }
}
