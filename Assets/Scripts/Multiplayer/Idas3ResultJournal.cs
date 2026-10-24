using System;
using System.Globalization;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;

namespace Idas3.Multiplayer
{
    // One outstanding result per save root. Intent precedes both reward stores;
    // a complete generation reaches both current copies before play can resume.
    // Native profiles use absolute compare-and-save; history uses race receipts.
    internal sealed class Idas3ResultJournal
    {
        [Serializable] internal sealed class Reward
        {
            public int selection=-1;
            public uint points;
            public uint[] before=Array.Empty<uint>(),after=Array.Empty<uint>();
        }
        [Serializable] internal sealed class Entry
        {
            public string raceId,hostTicks,guestTicks;
            public int car,winner;
            public bool won;
            public Idas3BattleRecord opponent,previous;
            public int beforePoints;
            public string[] resetKeys=Array.Empty<string>();
            public Reward reward;
            public bool complete;
            public Idas3BattleRecord record=Idas3BattleRecord.Fresh;
            public int battlePoints;
        }
        [Serializable] sealed class State
        {
            public int version=1;
            public long generation;
            public Entry entry;
        }
        [Serializable] sealed class Envelope
        {
            public State state;
            public string sha256;
        }
        readonly Idas3MultiplayerRecords records;
        readonly string path;
        internal Idas3ResultJournal(Idas3MultiplayerRecords records)
        {
            this.records=records??throw new ArgumentNullException(nameof(records));
            path=Path.Combine(records.SaveRoot,"online_result_v1.json");
        }
        internal static Entry Request(int car,ulong raceId,int winner,bool won,
            Idas3BattleRecord opponent,Idas3BattleRecord previous,ulong hostTicks,ulong guestTicks)
        {
            var entry=new Entry{car=car,raceId=Text(raceId),winner=winner,won=won,
                opponent=opponent,previous=previous,hostTicks=Text(hostTicks),guestTicks=Text(guestTicks)};
            ValidateRequest(entry);return entry;
        }
        internal Entry Settle(Entry request,Func<Reward> prepare,Action<Reward> apply)
        {
            ValidateRequest(request);
            if(prepare==null||apply==null)throw new ArgumentNullException();
            using(Acquire()){
                var state=Load(out bool repair);
                if(repair)Write(state);
                if(state!=null&&state.entry.raceId==request.raceId){
                    if(!Matches(state.entry,request))throw new InvalidDataException("Conflicting saved online result.");
                }else{
                    if(state!=null&&!state.entry.complete)throw new IOException("A previous online result still needs recovery.");
                    // Never mutate the caller's request or recapture the native
                    // plan after the durable intent already exists.
                    var entry=Request(request.car,Number(request.raceId),request.winner,request.won,
                        request.opponent,request.previous,Number(request.hostTicks),Number(request.guestTicks));
                    records.CaptureResultBaseline(entry.car,out entry.beforePoints,out entry.resetKeys);
                    entry.reward=prepare();ValidateEntry(entry);
                    state=new State{generation=checked((state?.generation??0)+1),entry=entry};
                    Write(state);
                }
                return Finish(state,apply);
            }
        }
        // Must run before native scene initialization/offline profile writes.
        internal bool Recover(Action<Reward> apply)
        {
            if(apply==null)throw new ArgumentNullException(nameof(apply));
            using(Acquire()){
                var state=Load(out bool repair);
                if(state==null)return false;
                if(repair)Write(state);
                if(state.entry.complete)return false;
                Finish(state,apply);return true;
            }
        }
        Entry Finish(State state,Action<Reward> apply)
        {
            var entry=state.entry;
            if(entry.complete)return entry;
            apply(entry.reward);
            if(entry.winner==0||entry.winner==1){
                entry.record=records.CommitRecovered(entry.car,Number(entry.raceId),entry.won,entry.opponent,entry.resetKeys);
                entry.battlePoints=records.ResultWasReset(entry.car,entry.resetKeys)?0:
                    records.ReadLevelPoints(entry.car)-entry.beforePoints;
            }else{entry.record=records.Read(entry.car);entry.battlePoints=0;}
            entry.complete=true;
            Write(new State{generation=checked(state.generation+1),entry=entry});
            return entry;
        }
        FileStream Acquire()
        {
            Directory.CreateDirectory(records.SaveRoot);
            // Keep the lock file in place; the OS releases the handle on crash.
            return new FileStream(path+".lock",FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None);
        }
        static string Text(ulong value)=>value.ToString(CultureInfo.InvariantCulture);
        static ulong Number(string value)
        {
            if(!ulong.TryParse(value,NumberStyles.None,CultureInfo.InvariantCulture,out ulong number)||Text(number)!=value)
                throw new InvalidDataException("Invalid result identifier or time.");
            return number;
        }
        static void ValidateRequest(Entry entry)
        {
            if(entry==null||Number(entry.raceId)==0||entry.car<0||entry.car>=35||entry.winner< -1||entry.winner>2||
                (entry.winner<0||entry.winner==2)&&entry.won||!entry.opponent.Valid||!entry.previous.Valid)
                throw new InvalidDataException("Invalid online result intent.");
            Number(entry.hostTicks);Number(entry.guestTicks);
        }
        static void ValidateEntry(Entry entry)
        {
            ValidateRequest(entry);Idas3MultiplayerRecords.ValidateResetKeys(entry.resetKeys);
            if(entry.beforePoints<0||entry.beforePoints>9899||!entry.record.Valid||Math.Abs((long)entry.battlePoints)>9899)
                throw new InvalidDataException("Invalid result progression snapshot.");
            var reward=entry.reward;
            if(reward==null||reward.before==null||reward.after==null||reward.points>2000)
                throw new InvalidDataException("Invalid result tuning snapshot.");
            if(reward.selection==-1){
                if(reward.points!=0||reward.before.Length!=0||reward.after.Length!=0)
                    throw new InvalidDataException("Invalid empty result tuning snapshot.");
            }else{
                int car=reward.selection<40?reward.selection-5:(reward.selection-40)%35;
                if(reward.selection<5||reward.selection>=215||car!=entry.car||entry.winner<0||
                    reward.before.Length!=307||reward.after.Length!=307||reward.before[4]!=(uint)car||reward.after[4]!=(uint)car||
                    reward.after[18]>999999999||reward.after[18]<reward.points)
                    throw new InvalidDataException("Invalid selected result tuning snapshot.");
            }
        }
        static bool Matches(Entry a,Entry b)=>a.raceId==b.raceId&&a.car==b.car&&a.winner==b.winner&&a.won==b.won&&
            a.hostTicks==b.hostTicks&&a.guestTicks==b.guestTicks&&a.opponent.Equals(b.opponent)&&a.previous.Equals(b.previous);
        static string Hash(State state)
        {
            using(var sha=SHA256.Create())return Convert.ToBase64String(sha.ComputeHash(new UTF8Encoding(false).GetBytes(JsonUtility.ToJson(state))));
        }
        State Read(string file)
        {
            if(new FileInfo(file).Length>2*1024*1024)throw new InvalidDataException("Online result journal is too large.");
            var envelope=JsonUtility.FromJson<Envelope>(File.ReadAllText(file));var state=envelope?.state;
            if(state==null||state.version!=1||state.generation<1||envelope.sha256!=Hash(state))
                throw new InvalidDataException("Invalid online result journal.");
            ValidateEntry(state.entry);return state;
        }
        State Load(out bool repair)
        {
            string[] paths={path,path+".previous",path+".pending"};var states=new State[3];
            State newest=null;bool exists=false;Exception failure=null;repair=false;
            for(int i=0;i<paths.Length;++i){
                if(!File.Exists(paths[i]))continue;
                exists=true;
                try{
                    var state=states[i]=Read(paths[i]);
                    if(newest==null||state.generation>newest.generation)newest=state;
                    else if(state.generation==newest.generation&&Hash(state)!=Hash(newest))
                        throw new InvalidOperationException("Conflicting online result journal generations.");
                }catch(Exception error)when(error is IOException||error is InvalidDataException||error is ArgumentException){
                    if(i==2)throw new InvalidDataException("Pending online result is unreadable; existing files were preserved.",error);
                    failure=error;
                }
            }
            if(exists&&newest==null)throw new InvalidDataException("Online result could not be recovered; existing files were preserved.",failure);
            if(newest!=null){
                // Compare every same-generation copy, including a newer pending
                // record encountered after the primary and backup.
                foreach(var state in states)if(state!=null&&state.generation==newest.generation&&Hash(state)!=Hash(newest))
                    throw new InvalidDataException("Conflicting online result journal generations.");
                repair=states[0]==null||states[1]==null||states[2]!=null||
                    states[0].generation!=newest.generation||states[1].generation!=newest.generation;
            }
            return newest;
        }
        void Write(State state)
        {
            ValidateEntry(state.entry);
            WriteFile(path+".pending",state);WriteFile(path,state);WriteFile(path+".previous",state);
            File.Delete(path+".pending");
        }
        void WriteFile(string file,State state)
        {
            string temporary=file+".new-"+Guid.NewGuid().ToString("N");
            try{
                byte[] bytes=new UTF8Encoding(false).GetBytes(JsonUtility.ToJson(new Envelope{state=state,sha256=Hash(state)},true));
                using(var output=new FileStream(temporary,FileMode.CreateNew,FileAccess.Write,FileShare.None)){
                    output.Write(bytes,0,bytes.Length);output.Flush(true);
                }
                Read(temporary);
                if(File.Exists(file)){
                    try{Read(file);}catch(Exception error)when(error is IOException||error is InvalidDataException||error is ArgumentException){
                        File.Copy(file,file+".unreadable-"+Guid.NewGuid().ToString("N"));
                    }
                    File.Replace(temporary,file,null);
                }else File.Move(temporary,file);
            }finally{if(File.Exists(temporary))File.Delete(temporary);}
        }
    }
}
