using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using Idas3.Multiplayer;

internal static class ResultJournalDriver
{
    static string root;
    static bool native;
    static int checks;
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    static extern int Idas3MultiplayerRecoverReward(byte[] saves,int selection,uint points,uint[] before,uint[] after,uint count);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    static extern int Idas3UnityCopyError(byte[] bytes,int capacity);
    static void Check(bool condition,string message){if(!condition)throw new Exception(message);++checks;Console.WriteLine("PASS\t"+message);}
    static void Reject(Action action,string message){bool failed=false;try{action();}catch(Exception){failed=true;}Check(failed,message);}
    static Idas3BattleRecord Advance(Idas3BattleRecord before,bool won,Idas3BattleRecord other,uint xp,out uint next){
        ++before.battles;if(won){++before.wins;before.streak=Math.Min(99,before.streak+1);}else before.streak=0;
        next=xp+(won?25u:10u);before.level+=next/100;next%=100;return before;
    }
    static Idas3MultiplayerRecords Store(string dir)=>new Idas3MultiplayerRecords(dir,Advance);
    static string Journal(string dir)=>Path.Combine(dir,"online_result_v1.json");
    static string Profile(string dir)=>Path.Combine(dir,"driver_profiles_v1","car_08.profile");
    static uint[] Before(){var words=new uint[307];words[4]=8;words[18]=321;words[150]=0x87123456;return words;}
    static Idas3ResultJournal.Reward Plan(){var before=Before();var after=(uint[])before.Clone();after[18]+=2000;return new Idas3ResultJournal.Reward{selection=13,points=2000,before=before,after=after};}
    static Idas3ResultJournal.Entry Request(ulong id=123)=>Idas3ResultJournal.Request(8,id,0,true,Idas3BattleRecord.Fresh,Idas3BattleRecord.Fresh,6000,0);
    static void Put(byte[] bytes,int at,uint value){Array.Copy(BitConverter.GetBytes(value),0,bytes,at,4);}
    static void SaveProfile(string dir,uint[] words){
        Directory.CreateDirectory(Path.GetDirectoryName(Profile(dir)));
        var bytes=new byte[24+307*4];Array.Copy(new byte[]{73,68,51,80,82,70,49,0},bytes,8);
        Put(bytes,8,1);Put(bytes,12,8);Put(bytes,16,307);
        for(int i=0;i<words.Length;++i)Put(bytes,24+i*4,words[i]);
        uint checksum=2166136261;for(int i=0;i<bytes.Length;++i)if(i<20||i>=24)checksum=unchecked((checksum^bytes[i])*16777619);
        Put(bytes,20,checksum);File.WriteAllBytes(Profile(dir),bytes);
    }
    static uint[] ReadProfile(string dir){var bytes=File.ReadAllBytes(Profile(dir));var words=new uint[307];for(int i=0;i<words.Length;++i)words[i]=BitConverter.ToUInt32(bytes,24+i*4);return words;}
    static void Apply(string dir,Idas3ResultJournal.Reward reward){
        if(reward.selection<0)return;
        if(native){
            if(Idas3MultiplayerRecoverReward(Encoding.UTF8.GetBytes(Path.GetFullPath(dir)+"\0"),reward.selection,reward.points,reward.before,reward.after,307)!=1){
                var error=new byte[4096];int length=Idas3UnityCopyError(error,error.Length);
                throw new IOException("Native recovery: "+Encoding.UTF8.GetString(error,0,Math.Max(0,Math.Min(length,error.Length))).TrimEnd('\0'));
            }
        }else{
            var current=ReadProfile(dir);if(current.SequenceEqual(reward.after))return;
            if(!current.SequenceEqual(reward.before))throw new IOException("Conflicting fake native profile");
            SaveProfile(dir,reward.after);
        }
    }
    static string Fixture(string name){string dir=Path.Combine(root,name);Directory.CreateDirectory(dir);SaveProfile(dir,Before());return dir;}
    static void Pending(string dir){Reject(()=>new Idas3ResultJournal(Store(dir)).Settle(Request(),Plan,_=>throw new IOException("before tuning")),"Prepared intent retained at "+Path.GetFileName(dir));}
    static void Basic(){
        string dir=Fixture("basic");var store=Store(dir);var journal=new Idas3ResultJournal(store);int apply=0;
        var result=journal.Settle(Request(),Plan,reward=>{++apply;Apply(dir,reward);});
        Check(result.complete&&result.battlePoints==25&&store.Read(8).battles==1&&ReadProfile(dir)[18]==2321,"Result commits tuning and battle history once");
        string primary=File.ReadAllText(Journal(dir));DateTime stamp=File.GetLastWriteTimeUtc(Profile(dir));
        result=new Idas3ResultJournal(Store(dir)).Settle(Request(),()=>throw new Exception("must not prepare twice"),_=>throw new Exception("must not apply twice"));
        Check(result.complete&&apply==1&&File.GetLastWriteTimeUtc(Profile(dir))==stamp,"Completed result neither replans nor rewrites tuning");
        Check(!journal.Recover(_=>throw new Exception("completed recovery must do nothing")),"Restart of completed result is a no-op");
        Check(primary==File.ReadAllText(Journal(dir)+".previous")&&!File.Exists(Journal(dir)+".pending"),"Both current journal copies retain the completed generation");
        var conflict=Request();conflict.won=false;
        Reject(()=>journal.Settle(conflict,Plan,_=>{}),"Same race with different outcome is rejected");
        string nextDir=Fixture("next-race");var nextStore=Store(nextDir);var nextJournal=new Idas3ResultJournal(nextStore);
        nextJournal.Settle(Request(),Plan,reward=>Apply(nextDir,reward));
        var nextPlan=Plan();nextPlan.before=ReadProfile(nextDir);nextPlan.after=(uint[])nextPlan.before.Clone();nextPlan.after[18]+=2000;
        nextJournal.Settle(Request(124),()=>nextPlan,reward=>Apply(nextDir,reward));
        Check(ReadProfile(nextDir)[18]==4321&&nextStore.Read(8).battles==2,"Next completed race gets one independent award");
    }
    static void Errors(){
        string dir=Fixture("pending-conflict");Pending(dir);
        Reject(()=>new Idas3ResultJournal(Store(dir)).Settle(Request(999),Plan,_=>{}),"New race cannot discard unfinished result");
        var newer=Before();newer[18]=9000;SaveProfile(dir,newer);string intent=File.ReadAllText(Journal(dir));
        Reject(()=>new Idas3ResultJournal(Store(dir)).Recover(reward=>Apply(dir,reward)),"Newer tuning progress blocks stale recovery");
        Check(ReadProfile(dir)[18]==9000&&intent==File.ReadAllText(Journal(dir))&&Store(dir).Read(8).battles==0,"Conflicting recovery preserves progress, intent and history");

        dir=Fixture("damaged-pending");Pending(dir);File.WriteAllText(Journal(dir)+".pending","broken");
        Reject(()=>new Idas3ResultJournal(Store(dir)).Recover(reward=>Apply(dir,reward)),"Damaged durable pending intent is not silently ignored");
        Check(ReadProfile(dir)[18]==321&&File.ReadAllText(Journal(dir)+".pending")=="broken","Unreadable pending intent and profile remain untouched");

        dir=Fixture("damaged-primary");Pending(dir);File.WriteAllText(Journal(dir),"broken");
        Check(new Idas3ResultJournal(Store(dir)).Recover(reward=>Apply(dir,reward))&&ReadProfile(dir)[18]==2321,"Valid backup repairs damaged journal primary and finishes result");
        Check(Directory.GetFiles(dir,"online_result_v1.json.unreadable-*").Length==1,"Damaged journal bytes are archived during repair");

        dir=Fixture("complete-with-stale-backup");Pending(dir);string old=File.ReadAllText(Journal(dir));
        var j=new Idas3ResultJournal(Store(dir));j.Recover(reward=>Apply(dir,reward));File.WriteAllText(Journal(dir)+".previous",old);
        Check(!j.Recover(_=>throw new Exception("must not credit stale backup"))&&File.ReadAllText(Journal(dir))==File.ReadAllText(Journal(dir)+".previous"),"Stale backup cannot resurrect an already completed award");

        dir=Fixture("locked-intent");using(var held=new FileStream(Journal(dir)+".lock",FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){
            Reject(()=>new Idas3ResultJournal(Store(dir)).Settle(Request(),Plan,_=>throw new Exception("unreachable")),"Another writer's journal lock blocks settlement");
            Check(ReadProfile(dir)[18]==321&&!File.Exists(Journal(dir)),"Lock failure cannot partially award tuning");
        }
        new Idas3ResultJournal(Store(dir)).Settle(Request(),Plan,reward=>Apply(dir,reward));
        Check(ReadProfile(dir)[18]==2321,"Retry after writer lock releases succeeds");
    }
    static void Reset(){
        string dir=Fixture("admin-reset");Pending(dir);var store=Store(dir);
        store.ApplyAdministrativeResets(new string('a',32),Array.Empty<string>(),new[]{"fixture-reset"});
        Check(new Idas3ResultJournal(store).Recover(reward=>Apply(dir,reward)),"Result recovers after an administrative reset");
        Check(ReadProfile(dir)[18]==2321&&store.Read(8).Equals(Idas3BattleRecord.Fresh),"New reset suppresses old battle progression but preserves earned tuning");
        Check(store.Commit(8,123,true,Idas3BattleRecord.Fresh).Equals(Idas3BattleRecord.Fresh),"Suppressed result still has a receipt preventing later replay");
    }
    static void Signal(string dir){File.WriteAllText(Path.Combine(dir,"ready"),"ready");Thread.Sleep(Timeout.Infinite);}
    static void Child(string dir,string boundary){
        FileStream locked=null;
        try{
            new Idas3ResultJournal(Store(dir)).Settle(Request(),()=>{
                if(boundary=="before-intent")Signal(dir);return Plan();
            },reward=>{
                if(boundary=="after-intent")Signal(dir);
                Apply(dir,reward);
                if(boundary=="after-tuning")Signal(dir);
                if(boundary=="after-record")Directory.CreateDirectory(Journal(dir)+".pending");
                if(boundary=="completion-intent")locked=new FileStream(Journal(dir),FileMode.Open,FileAccess.Read,FileShare.Read);
            });
            if(boundary!="after-complete")throw new Exception("Requested interruption was not reached");
            Signal(dir);
        }catch(IOException){
            if(boundary!="after-record"&&boundary!="completion-intent")throw;
            if(Store(dir).Read(8).battles!=1)throw new Exception("History boundary was not reached");
            Signal(dir);
        }finally{locked?.Dispose();}
    }
    static void Interruptions(){
        foreach(string boundary in new[]{"before-intent","after-intent","after-tuning","after-record","completion-intent","after-complete"}){
            string dir=Fixture("terminated-"+boundary);
            var start=new ProcessStartInfo(System.Reflection.Assembly.GetExecutingAssembly().Location,
                "\""+dir+"\" "+(native?"native":"fake")+" child "+boundary){UseShellExecute=false,CreateNoWindow=true,WindowStyle=ProcessWindowStyle.Hidden};
            using(var child=Process.Start(start)){
                try{
                    var timer=Stopwatch.StartNew();
                    while(!File.Exists(Path.Combine(dir,"ready"))&&!child.HasExited&&timer.ElapsedMilliseconds<20000)Thread.Sleep(10);
                    Check(!child.HasExited&&File.Exists(Path.Combine(dir,"ready")),"Child reached "+boundary+" boundary");
                }finally{
                    if(!child.HasExited)child.Kill();
                    if(!child.WaitForExit(10000))throw new Exception("Owned test child did not stop");
                }
            }
            if(boundary=="after-record")Directory.Delete(Journal(dir)+".pending",false); // Remove only the owned empty obstruction.
            var store=Store(dir);var journal=new Idas3ResultJournal(store);
            journal.Recover(reward=>Apply(dir,reward));
            bool durable=boundary!="before-intent";
            Check(ReadProfile(dir)[18]==(durable?2321u:321u)&&store.Read(8).battles==(durable?1u:0u),"Process termination at "+boundary+" recovers exact tuning/history");
            string profileHash=Convert.ToBase64String(File.ReadAllBytes(Profile(dir)));
            Check(!journal.Recover(_=>throw new Exception("duplicate recovery"))&&profileHash==Convert.ToBase64String(File.ReadAllBytes(Profile(dir))),"Repeated startup after "+boundary+" never repeats the award");
        }
    }
    static int Main(string[] args){
        try{
            root=Path.GetFullPath(args[0]);native=args[1]=="native";
            if(args.Length==4){Child(root,args[3]);return 0;}
            if(Directory.Exists(root))throw new Exception("Fixture directory must be new");
            Directory.CreateDirectory(root);Basic();Errors();Reset();Interruptions();
            Console.WriteLine("PASS\t"+checks+" journal assertions completed; native recovery="+native);return 0;
        }catch(Exception error){Console.Error.WriteLine(error);return 1;}
    }
}
