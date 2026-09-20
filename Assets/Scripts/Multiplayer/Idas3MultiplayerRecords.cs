using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;

namespace Idas3.Multiplayer
{
    [Serializable,StructLayout(LayoutKind.Sequential,Pack=8)]
    public struct Idas3BattleRecord : IEquatable<Idas3BattleRecord>
    {
        public uint battles,wins,level,streak;
        public static Idas3BattleRecord Fresh => new Idas3BattleRecord{level=1};
        public bool Valid => wins<=battles&&streak<=wins&&streak<=99&&level>=1&&level<=99;
        public bool Equals(Idas3BattleRecord other)=>battles==other.battles&&wins==other.wins&&level==other.level&&streak==other.streak;
        public override bool Equals(object other)=>other is Idas3BattleRecord record&&Equals(record);
        public override int GetHashCode()=>unchecked((int)(battles*397u^wins*31u^level*7u^streak));
    }

    // Online history is deliberately independent of the original card/profile
    // files. The caller passes the same (possibly private diagnostic) save root
    // used by the game, and freezes the race's two records before loading.
    public sealed partial class Idas3MultiplayerRecords
    {
        [Serializable] private sealed class Receipt
        {
            public string raceId;
            public bool won;
            public Idas3BattleRecord opponent;
        }
        [Serializable] private sealed class State
        {
            public int version=1,car;
            public Idas3BattleRecord record=Idas3BattleRecord.Fresh;
            public uint experience;
            public Receipt[] receipts=Array.Empty<Receipt>();
        }
        [Serializable] private sealed class FileEnvelope
        {
            public State state;
            public string sha256;
        }
        private readonly string directory;
        public string LastRecovery {get;private set;}
        public Idas3MultiplayerRecords(string saveRoot)
        {
            if(string.IsNullOrWhiteSpace(saveRoot))throw new ArgumentException("An actual save root is required.",nameof(saveRoot));
            directory=Path.Combine(Path.GetFullPath(saveRoot),"online_records_v1");
        }
        private string FilePath(int car)
        {
            if(car<0||car>=35)throw new ArgumentOutOfRangeException(nameof(car));
            return Path.Combine(directory,"car_"+car.ToString("D2",CultureInfo.InvariantCulture)+".json");
        }
        public Idas3BattleRecord Read(int car)=>Load(car,out _).record;

        // The native evaluator owns the original progression arithmetic. This
        // store neither infers levels from win ratio nor awards tuning points.
        [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
        private static extern int Idas3MultiplayerNextBattleRecord(ref Idas3BattleRecord before,int won,ref Idas3BattleRecord opponent,uint experience,out Idas3BattleRecord after,out uint nextExperience);
        public static Idas3BattleRecord Next(Idas3BattleRecord before,bool won,Idas3BattleRecord opponent,uint experience,out uint nextExperience)
        {
            Validate(before);Validate(opponent);
            if(experience>99)throw new ArgumentOutOfRangeException(nameof(experience));
            if(Idas3MultiplayerNextBattleRecord(ref before,won?1:0,ref opponent,experience,out var after,out nextExperience)!=1)
                throw new InvalidOperationException("Original online record update failed: "+Idas3Native.Error());
            Validate(after);if(nextExperience>99)throw new InvalidDataException("Invalid original battle experience.");return after;
        }
        private delegate Idas3BattleRecord Advance(Idas3BattleRecord before,bool won,Idas3BattleRecord opponent,uint experience,out uint nextExperience);
        public Idas3BattleRecord Commit(int car,ulong raceId,bool won,Idas3BattleRecord opponent)
        {
            return Commit(car,raceId,won,opponent,Next);
        }
        private Idas3BattleRecord Commit(int car,ulong raceId,bool won,Idas3BattleRecord opponent,
            Advance advance)
        {
            if(raceId==0)throw new ArgumentOutOfRangeException(nameof(raceId));Validate(opponent);
            var state=Load(car,out bool primaryValid);string id=raceId.ToString(CultureInfo.InvariantCulture);
            foreach(var receipt in state.receipts)if(receipt.raceId==id){
                if(receipt.won!=won||!receipt.opponent.Equals(opponent))throw new InvalidDataException("Conflicting result for a recorded online race.");
                return state.record;
            }
            var after=advance(state.record,won,opponent,state.experience,out uint experience);Validate(after);
            if(experience>99)throw new InvalidDataException("Invalid next online battle experience.");
            var receipts=new Receipt[state.receipts.Length+1];Array.Copy(state.receipts,receipts,state.receipts.Length);
            receipts[receipts.Length-1]=new Receipt{raceId=id,won=won,opponent=opponent};
            var next=new State{car=car,record=after,experience=experience,receipts=receipts};Write(car,next,primaryValid);return after;
        }
        private static void Validate(Idas3BattleRecord record)
        {if(!record.Valid)throw new InvalidDataException("Invalid online battle record.");}
        private static string Hash(State state)
        {
            using(var sha=SHA256.Create())return Convert.ToBase64String(sha.ComputeHash(new UTF8Encoding(false).GetBytes(JsonUtility.ToJson(state))));
        }
        private State ReadFile(string path,int car)
        {
            var envelope=JsonUtility.FromJson<FileEnvelope>(File.ReadAllText(path));var state=envelope?.state;
            if(state==null||state.version!=1||state.car!=car||state.experience>99||state.receipts==null||envelope.sha256!=Hash(state))
                throw new InvalidDataException("Invalid online record file: "+path);
            Validate(state.record);var ids=new HashSet<string>(StringComparer.Ordinal);
            foreach(var receipt in state.receipts){
                if(receipt==null||!ulong.TryParse(receipt.raceId,NumberStyles.None,CultureInfo.InvariantCulture,out ulong id)||id==0||
                    id.ToString(CultureInfo.InvariantCulture)!=receipt.raceId||!ids.Add(receipt.raceId))
                    throw new InvalidDataException("Invalid online race receipt.");
                Validate(receipt.opponent);
            }
            return state;
        }
        private State Load(int car,out bool primaryValid)
        {
            string file=FilePath(car);primaryValid=false;LastRecovery=null;Exception failure=null;
            if(File.Exists(file))try{var state=ReadFile(file,car);primaryValid=true;return state;}catch(Exception error)when(error is IOException||error is InvalidDataException||error is ArgumentException){failure=error;}
            string backup=file+".previous";
            if(File.Exists(backup))try{var state=ReadFile(backup,car);LastRecovery="Recovered the previous online record: "+file;return state;}catch(Exception error)when(error is IOException||error is InvalidDataException||error is ArgumentException){failure=error;}
            if(failure!=null)throw new InvalidDataException("Online history could not be read; existing files were preserved.",failure);
            return new State{car=car};
        }
        private void Write(int car,State state,bool primaryValid)
        {
            string file=FilePath(car);Directory.CreateDirectory(directory);
            string temporary=file+".new-"+Guid.NewGuid().ToString("N");
            try{
                byte[] bytes=new UTF8Encoding(false).GetBytes(JsonUtility.ToJson(new FileEnvelope{state=state,sha256=Hash(state)},true));
                using(var output=new FileStream(temporary,FileMode.CreateNew,FileAccess.Write,FileShare.None)){
                    output.Write(bytes,0,bytes.Length);output.Flush(true);
                }
                // Validate the complete prospective file before replacing any
                // committed state. A damaged primary never replaces good backup.
                ReadFile(temporary,car);
                if(File.Exists(file)){
                    if(!primaryValid)File.Copy(file,file+".unreadable-"+Guid.NewGuid().ToString("N"));
                    File.Replace(temporary,file,primaryValid?file+".previous":null);
                }else File.Move(temporary,file);
            }finally{if(File.Exists(temporary))File.Delete(temporary);}
        }

        internal static void SeedDiagnostic(string saveRoot,int car,Idas3BattleRecord record,uint experience=0)
        {
            var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-multiplayer-smoke");
            string absolute=Path.GetFullPath(saveRoot),parent=Path.GetDirectoryName(absolute);
            if(at<0||at+1>=args.Length||Array.IndexOf(args,"-idas3-multiplayer-showcase-check")<0||
                !string.Equals(absolute,Path.Combine(Path.GetFullPath(args[at+1]),"userdata"),StringComparison.OrdinalIgnoreCase)||
                !File.Exists(Path.Combine(parent,"ISOLATED_MULTIPLAYER_TEST.txt")))
                throw new InvalidOperationException("Online record seeding requires the explicit isolated multiplayer showcase diagnostic.");
            Validate(record);if(experience>99)throw new ArgumentOutOfRangeException(nameof(experience));var store=new Idas3MultiplayerRecords(absolute);string file=store.FilePath(car);
            if(File.Exists(file)||File.Exists(file+".previous"))throw new IOException("Diagnostic online history already exists.");
            RunDiagnosticChecks(parent);
            store.Write(car,new State{car=car,record=record,experience=experience},false);
        }
    }
}
