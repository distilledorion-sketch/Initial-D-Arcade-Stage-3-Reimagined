using System;
using System.Collections.Generic;
using System.IO;
using Idas3.Multiplayer;

// This boundary never calls Unity or reads/writes the user's preferences.
namespace UnityEngine {
    public static class PlayerPrefs {
        static Dictionary<string,string> memory=new Dictionary<string,string>(),disk=new Dictionary<string,string>();
        public static bool FailSave,FailWrite,FailDelete;
        public static int Saves;
        public static bool HasKey(string key)=>memory.ContainsKey(key);
        public static string GetString(string key,string fallback)=>memory.TryGetValue(key,out string value)?value:fallback;
        public static void SetString(string key,string value){if(FailWrite)throw new IOException("simulated write failure");memory[key]=value;}
        public static void DeleteKey(string key){if(FailDelete)throw new IOException("simulated delete failure");memory.Remove(key);}
        public static void Save(){if(FailSave)throw new IOException("simulated persistence failure");disk=new Dictionary<string,string>(memory);++Saves;}
        public static void Reload(){memory=new Dictionary<string,string>(disk);}
    }
}
static class OnlineUsernameDriver {
    static void Check(bool value,string reason){if(!value)throw new Exception(reason);Console.WriteLine("PASS\t"+reason);}
    static void Main(string[] args){
        if(args.Length!=1)throw new ArgumentException("An isolated report directory is required");
        Console.WriteLine("VALIDATION\t"+Idas3OnlineUsernameChecks.Run(args[0]));
        const string key=Idas3OnlineUsername.PreferenceKey;
        Check(Idas3OnlineUsername.Saved=="","Fresh user requires name");
        Check(Idas3OnlineUsername.TrySave(" 日本の運転手 ",out string error)&&error=="","Unicode username saves");
        Check(UnityEngine.PlayerPrefs.Saves==1&&Idas3OnlineUsername.Saved=="日本の運転手","Normalized name commits once");
        UnityEngine.PlayerPrefs.Reload();Check(Idas3OnlineUsername.Saved=="日本の運転手","Name survives preference reload");
        Check(!Idas3OnlineUsername.TrySave("<b>admin</b>",out _)&&Idas3OnlineUsername.Saved=="日本の運転手"&&UnityEngine.PlayerPrefs.Saves==1,"Invalid edit cannot replace persisted name");
        Check(Idas3OnlineUsername.TrySave("中文玩家",out _)&&Idas3OnlineUsername.Saved=="中文玩家","Rename uses same preference");
        UnityEngine.PlayerPrefs.FailSave=true;
        Check(!Idas3OnlineUsername.TrySave("New driver",out error)&&error.Length>0&&Idas3OnlineUsername.Saved=="中文玩家","Failed save rolls memory back to last name");
        UnityEngine.PlayerPrefs.Reload();Check(Idas3OnlineUsername.Saved=="中文玩家","Failed save leaves persisted name intact");
        UnityEngine.PlayerPrefs.DeleteKey(key);
        Check(!Idas3OnlineUsername.TrySave("New driver",out _)&&!UnityEngine.PlayerPrefs.HasKey(key)&&Idas3OnlineUsername.Saved=="","Failed first save still requires a name");
        UnityEngine.PlayerPrefs.FailSave=false;
        UnityEngine.PlayerPrefs.SetString(key,"\u202Ebad");Check(Idas3OnlineUsername.Saved=="","Corrupt stored name requires new entry");
        UnityEngine.PlayerPrefs.SetString(key,"Prior name");UnityEngine.PlayerPrefs.FailWrite=true;
        Check(!Idas3OnlineUsername.TrySave("New driver",out error)&&error.Length>0&&Idas3OnlineUsername.Saved=="Prior name","Failed write and failed rollback cannot crash or replace the prior name");
        UnityEngine.PlayerPrefs.DeleteKey(key);UnityEngine.PlayerPrefs.FailDelete=true;
        Check(!Idas3OnlineUsername.TrySave("New driver",out error)&&error.Length>0&&Idas3OnlineUsername.Saved=="","Failed first write and failed rollback cannot bypass setup or crash");
    }
}
