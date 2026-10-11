using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using UnityEngine;

namespace Idas3.Multiplayer
{
    public sealed partial class Idas3MultiplayerRecords
    {
        private const int MaximumResetKeys=4096,MaximumActiveResetIds=1024;
        [Serializable] private sealed class ModerationState
        {
            public int version=1;
            public string[] resetKeys=Array.Empty<string>();
        }
        [Serializable] private sealed class ModerationEnvelope
        {
            public ModerationState state;
            public string sha256;
        }

        // Only the EOS sanctions adapter supplies these authenticated reference
        // IDs. This is deliberately not a network packet or public menu action.
        // The key includes identity AND action; neither timestamps nor an active
        // sanctions list alone can remember distinct or subsequently expired resets.
        internal bool ApplyAdministrativeResets(string playerId,string[] levelResetIds,string[] fullResetIds,Action beforeReset=null)
        {
            if(!ValidPlayerId(playerId))throw new ArgumentException("A valid online player identity is required.",nameof(playerId));
            playerId=playerId.ToLowerInvariant();
            var requested=new HashSet<string>(StringComparer.Ordinal);
            AddResetKeys(requested,playerId,"L",levelResetIds);
            AddResetKeys(requested,playerId,"F",fullResetIds);
            var floor=LoadModerationJournal(out bool repair);
            var union=new HashSet<string>(floor.resetKeys,StringComparer.Ordinal);
            bool changed=false;
            foreach(string key in requested)if(union.Add(key))changed=true;
            if(union.Count>MaximumResetKeys)throw new InvalidDataException("Online reset history is full; existing history was preserved.");
            string[] keys=SortedResetKeys(union);
            if(!changed&&keys.Length!=0){
                for(int car=0;car<35;++car)
                    if(HasUnappliedReset(LoadStoredState(car,out _),keys)){changed=true;break;}
            }
            // Stop a running/lobby session before any new reset is persisted.
            // A callback failure leaves the record and journal untouched.
            if(changed)beforeReset?.Invoke();
            floor.resetKeys=keys;
            if(changed||repair)WriteModerationJournal(floor);
            if(changed){
                // The durable union comes first. Any failure stops online play;
                // later reads and retries finish remaining cars from that floor.
                for(int car=0;car<35;++car){
                    var state=LoadStoredState(car,out bool primaryValid);
                    if(ApplyResetFloor(state,keys))Write(car,state,primaryValid);
                }
            }
            return changed;
        }

        private static bool ValidPlayerId(string value)
        {
            if(value==null||value.Length!=32)return false;
            foreach(char c in value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;
            return true;
        }
        private static bool ValidResetId(string value)
        {
            if(string.IsNullOrWhiteSpace(value)||value.Length>128||value!=value.Trim())return false;
            foreach(char c in value)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;
            return true;
        }
        private static void AddResetKeys(HashSet<string> keys,string playerId,string action,string[] ids)
        {
            if(ids==null)throw new ArgumentNullException(nameof(ids));
            if(ids.Length>MaximumActiveResetIds)throw new ArgumentException("Too many active online reset references.",nameof(ids));
            foreach(string id in ids){
                if(!ValidResetId(id))throw new ArgumentException("Invalid online reset reference.",nameof(ids));
                keys.Add(playerId+"|"+action+"|"+id);
            }
        }
        private static void ValidateResetKeys(string[] keys)
        {
            if(keys==null||keys.Length>MaximumResetKeys)throw new InvalidDataException("Invalid online reset history.");
            var seen=new HashSet<string>(StringComparer.Ordinal);
            foreach(string key in keys){
                if(key==null||key.Length<36||key.Length>163||key[32]!='|'||key[34]!='|'||
                    (key[33]!='L'&&key[33]!='F')||!ValidPlayerId(key.Substring(0,32))||
                    key.Substring(0,32)!=key.Substring(0,32).ToLowerInvariant()||!ValidResetId(key.Substring(35))||!seen.Add(key))
                    throw new InvalidDataException("Invalid online reset key.");
            }
        }
        private static string[] SortedResetKeys(HashSet<string> keys)
        {
            var result=new string[keys.Count];keys.CopyTo(result);Array.Sort(result,StringComparer.Ordinal);return result;
        }
        private static bool SameResetKeys(string[] first,string[] second)
        {
            if(first.Length!=second.Length)return false;
            var keys=new HashSet<string>(first,StringComparer.Ordinal);return keys.SetEquals(second);
        }
        private static bool HasUnappliedReset(State state,string[] floor)
        {
            var applied=new HashSet<string>(state.appliedResetKeys,StringComparer.Ordinal);
            foreach(string key in floor)if(!applied.Contains(key))return true;
            return false;
        }
        private static bool ApplyResetFloor(State state,string[] floor)
        {
            var applied=new HashSet<string>(state.appliedResetKeys,StringComparer.Ordinal);
            bool reset=false,full=false;
            foreach(string key in floor)if(applied.Add(key)){reset=true;if(key[33]=='F')full=true;}
            if(!reset)return false;
            if(full)state.record=Idas3BattleRecord.Fresh;
            else{state.record.level=1;state.record.streak=0;}
            state.experience=0;state.version=2;state.appliedResetKeys=SortedResetKeys(applied);
            // Race receipts survive both actions: an old result cannot award
            // its points or win again after an administrative reset.
            return true;
        }

        private string ModerationPath=>Path.Combine(directory,"moderation.json");
        private ModerationState ReadModerationFile(string path)
        {
            // Bound parsing/allocation even when a local file is damaged.
            if(new FileInfo(path).Length>4*1024*1024)throw new InvalidDataException("Online reset journal is too large.");
            var envelope=JsonUtility.FromJson<ModerationEnvelope>(File.ReadAllText(path));
            var state=envelope?.state;
            if(state==null||state.version!=1||envelope.sha256!=HashJson(state))throw new InvalidDataException("Invalid online reset journal.");
            ValidateResetKeys(state.resetKeys);return state;
        }
        private ModerationState LoadModerationJournal(out bool repair)
        {
            string file=ModerationPath;
            string[] paths={file,file+".previous",file+".pending"};
            var states=new ModerationState[paths.Length];
            var union=new HashSet<string>(StringComparer.Ordinal);
            repair=false;bool exists=false;Exception failure=null;
            for(int i=0;i<paths.Length;++i){
                if(!File.Exists(paths[i]))continue;
                exists=true;
                try{
                    states[i]=ReadModerationFile(paths[i]);
                    foreach(string key in states[i].resetKeys)union.Add(key);
                }catch(Exception error)when(error is IOException||error is InvalidDataException||error is ArgumentException){
                    // A pending journal is the durable intent for an interrupted
                    // transaction. Ignoring damaged intent could lose a reset.
                    if(i==2)throw new InvalidDataException("Pending online reset history could not be read; existing files were preserved.",error);
                    failure=error;
                }
            }
            if(exists&&states[0]==null&&states[1]==null&&states[2]==null)
                throw new InvalidDataException("Online reset history could not be read; existing files were preserved.",failure);
            if(union.Count>MaximumResetKeys)throw new InvalidDataException("Online reset history is full; existing files were preserved.");
            var floor=new ModerationState{resetKeys=SortedResetKeys(union)};
            if(exists)repair=states[0]==null||states[1]==null||states[2]!=null||
                !SameResetKeys(states[0].resetKeys,floor.resetKeys)||!SameResetKeys(states[1].resetKeys,floor.resetKeys);
            return floor;
        }
        private void WriteModerationJournal(ModerationState state)
        {
            ValidateResetKeys(state.resetKeys);
            Directory.CreateDirectory(directory);
            string file=ModerationPath;
            // Durable intent precedes both redundant current copies. Unlike a
            // record's historical backup, the journal backup must retain the
            // entire union. Recovery unions valid copies, including this intent.
            WriteModerationFile(file+".pending",state);
            WriteModerationFile(file,state);
            WriteModerationFile(file+".previous",state);
            File.Delete(file+".pending");
        }
        private void WriteModerationFile(string file,ModerationState state)
        {
            string temporary=file+".new-"+Guid.NewGuid().ToString("N");
            try{
                byte[] bytes=new UTF8Encoding(false).GetBytes(JsonUtility.ToJson(new ModerationEnvelope{state=state,sha256=HashJson(state)},true));
                using(var output=new FileStream(temporary,FileMode.CreateNew,FileAccess.Write,FileShare.None)){
                    output.Write(bytes,0,bytes.Length);output.Flush(true);
                }
                ReadModerationFile(temporary);
                if(File.Exists(file)){
                    try{ReadModerationFile(file);}
                    catch(Exception error)when(error is IOException||error is InvalidDataException||error is ArgumentException){
                        File.Copy(file,file+".unreadable-"+Guid.NewGuid().ToString("N"));
                    }
                    File.Replace(temporary,file,null);
                }else File.Move(temporary,file);
            }finally{if(File.Exists(temporary))File.Delete(temporary);}
        }
    }
}
