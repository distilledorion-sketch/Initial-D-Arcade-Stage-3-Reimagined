using System;
using System.IO;
using System.Linq;
using UnityEngine;

namespace Idas3.Multiplayer
{
    public sealed partial class Idas3MultiplayerRecords
    {
        [Serializable] private sealed class CheckReport
        {
            public string schema="idas3-online-record-store-v1",applicationVersion=Application.version,error;
            public string scope="Private file-store checks use an injected deterministic evaluator to test EXP persistence and atomic/idempotent storage independently of native progression. Original level arithmetic is tested separately by the native evaluator tests.";
            public int checks;
            public bool passed;
        }
        // Called only after SeedDiagnostic's explicit CLI/root/marker guard.
        // This root is a sibling of userdata so fixture results cannot alter
        // even the seeded race history exercised by the real two-client test.
        private static void RunDiagnosticChecks(string parent)
        {
            string root=Path.Combine(parent,"online-record-store-checks");
            string reportPath=Path.Combine(parent,"online-record-store-report.json");
            if(Directory.Exists(root)||File.Exists(reportPath))throw new IOException("Online record store checks require fresh private output.");
            var report=new CheckReport();
            Action<bool,string> check=(value,message)=>{++report.checks;if(!value)throw new InvalidOperationException(message);};
            Action<Action,string> rejects=(action,message)=>{
                bool rejected=false;try{action();}catch(ArgumentException){rejected=true;}catch(InvalidDataException){rejected=true;}
                check(rejected,message);
            };
            int advances=0;
            Advance advance=(Idas3BattleRecord before,bool won,Idas3BattleRecord opponent,uint xp,out uint nextXp)=>{
                ++advances;nextXp=(xp+37u)%100u;return new Idas3BattleRecord{
                    battles=checked(before.battles+1),wins=checked(before.wins+(won?1u:0u)),level=before.level,
                    streak=won?Math.Min(before.streak+1u,99u):0u};
            };
            try{
                var store=new Idas3MultiplayerRecords(root);var opponent=new Idas3BattleRecord{battles=12,wins=8,level=7,streak=2};
                for(int car=0;car<35;++car)check(store.Read(car).Equals(Idas3BattleRecord.Fresh),"Fresh car history differs from zero/level one.");
                check(!Directory.Exists(root),"Reading fresh history created files.");
                rejects(()=>store.Read(-1),"Negative car accepted.");rejects(()=>store.Read(35),"Out-of-range car accepted.");
                rejects(()=>store.Commit(0,0,true,opponent,advance),"Race ID zero accepted.");
                rejects(()=>store.Commit(0,1,true,new Idas3BattleRecord{level=0},advance),"Invalid opponent accepted.");
                check(!Directory.Exists(root)&&advances==0,"Rejected inputs mutated history.");

                var first=store.Commit(0,1001,true,opponent,advance);string file=store.FilePath(0);
                check(first.battles==1&&first.wins==1&&first.streak==1&&first.level==1,"First win was not persisted.");
                check(store.Load(0,out bool valid).experience==37&&valid,"First EXP was not persisted.");
                string firstBytes=File.ReadAllText(file);
                var reloaded=new Idas3MultiplayerRecords(root);
                check(reloaded.Commit(0,1001,true,opponent,advance).Equals(first)&&advances==1&&File.ReadAllText(file)==firstBytes,"Reloaded duplicate advanced or rewrote history.");
                rejects(()=>reloaded.Commit(0,1001,false,opponent,advance),"Conflicting outcome accepted.");
                var different=opponent;different.level=8;
                rejects(()=>reloaded.Commit(0,1001,true,different,advance),"Conflicting opponent accepted.");
                check(File.ReadAllText(file)==firstBytes&&advances==1,"Conflicting receipt changed history.");

                var second=reloaded.Commit(0,1002,false,opponent,advance);
                check(second.battles==2&&second.wins==1&&second.streak==0,"Loss was not persisted independently from wins.");
                check(reloaded.Load(0,out valid).experience==74&&valid,"Reload lost EXP between commits.");
                check(File.ReadAllText(file+".previous")==firstBytes,"Atomic replacement did not preserve previous commit.");
                string secondBytes=File.ReadAllText(file);
                check(reloaded.Commit(0,1001,true,opponent,advance).Equals(second)&&advances==2&&File.ReadAllText(file)==secondBytes,"Older retry rolled back or advanced current history.");
                check(store.Read(8).Equals(Idas3BattleRecord.Fresh),"A different car inherited history.");
                var other=store.Commit(8,1001,true,opponent,advance);
                check(other.battles==1&&store.Read(0).Equals(second),"Per-car race receipts interfered.");

                // Simulate a partially written/corrupt primary, retaining the
                // complete previous commit as the only recoverable state.
                File.WriteAllText(file,"{damaged primary");
                check(store.Read(0).Equals(first)&&!string.IsNullOrEmpty(store.LastRecovery),"Damaged primary did not report valid-backup recovery.");
                check(File.ReadAllText(file)=="{damaged primary","Read recovery overwrote forensic evidence.");
                var recovered=store.Commit(0,1003,false,opponent,advance);
                check(recovered.battles==2&&recovered.wins==1&&store.Load(0,out valid).experience==74&&valid,"Recovery commit did not use recovered EXP/history.");
                check(File.ReadAllText(file+".previous")==firstBytes,"Corrupt primary replaced valid backup.");
                var damaged=Directory.GetFiles(store.directory,"car_00.json.unreadable-*");
                check(damaged.Length==1&&File.ReadAllText(damaged[0])=="{damaged primary","Recovery did not preserve corrupt primary separately.");
                string recoveredBytes=File.ReadAllText(file);
                // A well-formed but changed record must also fail its checksum.
                var altered=JsonUtility.FromJson<FileEnvelope>(recoveredBytes);altered.state.experience=75;
                File.WriteAllText(file,JsonUtility.ToJson(altered,true));
                check(File.ReadAllText(file)!=recoveredBytes,"Checksum fixture did not change its field.");
                check(store.Read(0).Equals(first),"Checksum mismatch was accepted.");
                File.WriteAllText(file+".previous","{damaged backup");
                string badPrimary=File.ReadAllText(file),badBackup=File.ReadAllText(file+".previous");int beforeRejected=advances;
                rejects(()=>store.Read(0),"Two damaged files reset history.");
                rejects(()=>store.Commit(0,1004,true,opponent,advance),"Commit accepted unreadable history.");
                check(File.ReadAllText(file)==badPrimary&&File.ReadAllText(file+".previous")==badBackup&&advances==beforeRejected,"Unrecoverable history was overwritten.");
                check(!Directory.GetFiles(store.directory,"*.new-*").Any(),"Temporary commit files remain.");

                var missingPrimary=new Idas3MultiplayerRecords(Path.Combine(root,"missing-primary"));
                missingPrimary.Commit(1,1,true,opponent,advance);missingPrimary.Commit(1,2,true,opponent,advance);
                string missing=missingPrimary.FilePath(1);File.Delete(missing);
                check(missingPrimary.Read(1).battles==1&&!string.IsNullOrEmpty(missingPrimary.LastRecovery),"Missing primary ignored valid backup.");
                check(missingPrimary.Commit(1,3,true,opponent,advance).battles==2,"Missing-primary recovery could not commit.");
                check(store.Read(8).Equals(other),"Independent save root changed another car.");
                report.passed=true;
            }catch(Exception error){report.error=error.ToString();throw;}
            finally{File.WriteAllText(reportPath,JsonUtility.ToJson(report,true));}
        }
    }
}
