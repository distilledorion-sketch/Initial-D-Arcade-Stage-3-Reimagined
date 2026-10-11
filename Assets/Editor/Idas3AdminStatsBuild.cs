using System;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using Idas3.Multiplayer;
using UnityEngine;

// Run with Unity's actual JsonUtility before staging the administrator-reset
// player. All records are freshly authored fixtures outside personal userdata.
public static class Idas3AdminStatsBuild
{
    [Serializable] sealed class Report { public bool passed; public int checks; public string error, scope="Real Unity JsonUtility, legacy checksum migration and persistent reset recovery; isolated fixtures only."; }
    public static void ValidateAndBuild() { Validate(); Idas3OnlineBuild.BuildCurrent(); }
    public static void Validate()
    {
        string proof=Path.GetFullPath("Verification/admin-reset-20261011");
        Directory.CreateDirectory(proof);
        string root=Path.Combine(proof,"unity-records-"+Guid.NewGuid().ToString("N"));
        string folder=Path.Combine(root,"online_records_v1");Directory.CreateDirectory(folder);
        var report=new Report();
        Action<bool,string> check=(ok,message)=>{++report.checks;if(!ok)throw new InvalidOperationException(message);};
        try {
            const string player="0123456789abcdef0123456789abcdef";
            const string legacy="{\"version\":1,\"car\":8,\"record\":{\"battles\":70,\"wins\":60,\"level\":31,\"streak\":10},\"experience\":77,\"receipts\":[{\"raceId\":\"42\",\"won\":true,\"opponent\":{\"battles\":0,\"wins\":0,\"level\":1,\"streak\":0}}]}";
            string hash;using(var sha=SHA256.Create())hash=Convert.ToBase64String(sha.ComputeHash(Encoding.UTF8.GetBytes(legacy)));
            string file=Path.Combine(folder,"car_08.json");
            string envelope="{\"state\":"+legacy+",\"sha256\":\""+hash+"\"}";
            File.WriteAllText(file,envelope);
            string sentinel=Path.Combine(root,"offline-profile-and-tuning.bin");File.WriteAllText(sentinel,"Preserved offline profile, car upgrades and tuning points");
            var records=new Idas3MultiplayerRecords(root);
            var original=records.Read(8);
            check(original.level==31&&original.battles==70&&original.wins==60&&records.ReadLevelPoints(8)==3077,"Unity cannot read the independent legacy record checksum");
            var reset=typeof(Idas3MultiplayerRecords).GetMethod("ApplyAdministrativeResets",BindingFlags.Instance|BindingFlags.NonPublic);
            int callbacks=0;
            Func<string[],string[],bool> apply=(level,full)=>(bool)reset.Invoke(records,new object[]{player,level,full,(Action)(()=>{++callbacks;})});
            check(apply(new[]{"level-reset"},Array.Empty<string>()),"First level reset was not applied");
            var lowered=records.Read(8);
            check(lowered.level==1&&lowered.streak==0&&lowered.battles==70&&lowered.wins==60&&records.ReadLevelPoints(8)==0,"Level reset damaged battle history or retained aura progression");
            check(!apply(new[]{"level-reset"},Array.Empty<string>())&&callbacks==1,"Reconnecting applied the same reset again");
            // Restore an authentic pre-reset file; the retained journal must
            // correct it even before another online service request occurs.
            File.WriteAllText(file,envelope);records=new Idas3MultiplayerRecords(root);
            check(records.Read(8).level==1&&records.Read(8).wins==60,"Restored legacy save bypassed the reset journal");
            check(apply(new[]{"level-reset"},new[]{"full-reset"}),"Full reset was not applied");
            for(int car=0;car<35;++car)check(records.Read(car).Equals(Idas3BattleRecord.Fresh)&&records.ReadLevelPoints(car)==0,"Full reset omitted a car model");
            // This calls the public production path. Its retained receipt
            // returns before the native progression evaluator can be invoked.
            check(records.Commit(8,42,true,Idas3BattleRecord.Fresh).Equals(Idas3BattleRecord.Fresh),"Old race receipt credited progress after reset");
            File.WriteAllText(file,"damaged primary");
            check(records.Read(8).Equals(Idas3BattleRecord.Fresh),"Previous-record recovery restored pre-reset stats");
            check(!apply(new[]{"level-reset"},new[]{"full-reset"}),"Recovery forgot reset identity");
            check(File.ReadAllText(sentinel)=="Preserved offline profile, car upgrades and tuning points","Reset modified an unrelated save");
            typeof(Idas3MultiplayerRecords).GetMethod("RunDiagnosticChecks",BindingFlags.Static|BindingFlags.NonPublic).Invoke(null,new object[]{root});
            check(File.Exists(Path.Combine(root,"online-record-store-report.json")),"Existing store regression did not run");
            report.passed=true;
        }catch(Exception error){report.error=(error is TargetInvocationException&&error.InnerException!=null?error.InnerException:error).ToString();throw;}
        finally {File.WriteAllText(Path.Combine(proof,"unity-records-report.json"),JsonUtility.ToJson(report,true));}
        Debug.Log("PASS administrative reset Unity serialization: "+report.checks+" checks");
    }
}
