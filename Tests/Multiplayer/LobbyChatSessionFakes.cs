// Runtime boundaries for production session packet tests. These deliberately
// throw on game rendering/progression work; the fixture never starts a race,
// connects to online services, reads a player profile or invokes native game methods.
using System;
using System.Collections.Generic;
using System.IO;

namespace UnityEngine {
    // In-memory preferences only; never touches Unity or a player's registry.
    public static class PlayerPrefs {
        static readonly Dictionary<string,string> values=new Dictionary<string,string>{{"idas3.online.name","Fixture driver"}};
        public static string GetString(string key,string fallback="")=>values.TryGetValue(key,out var value)?value:fallback;
        public static bool HasKey(string key)=>values.ContainsKey(key);
        public static void SetString(string key,string value){values[key]=value;}
        public static void DeleteKey(string key){values.Remove(key);}
        public static void Save(){}
    }
    public static class Time {public static double realtimeSinceStartupAsDouble;}
    public static class Application {
        public static string dataPath=>Path.Combine(Path.GetTempPath(),"idas3-chat-fixture-unavailable");
        public static string streamingAssetsPath=>dataPath;
        public static string persistentDataPath=>dataPath;
        public static string version=>"fixture";
    }
    public static class Debug {public static void Log(object value){}public static void LogWarning(object value){}public static void LogError(object value){}}
    public static class Random {public static float value=>.5f;}
#if !IDAS3_RECORD_STORAGE_INTEGRATION
    public static class JsonUtility {
        public static string ToJson(object value,bool pretty=false)=>throw new InvalidOperationException("Chat must not save game records");
        public static T FromJson<T>(string value)=>throw new InvalidOperationException("Chat must not read game records");
    }
#endif
    public static class Mathf {
        public const float Rad2Deg=57.29578f,Deg2Rad=.01745329f;
        public static int Clamp(int value,int min,int max)=>Math.Max(min,Math.Min(max,value));
        public static float Abs(float value)=>Math.Abs(value);
        public static float Clamp01(float value)=>throw new InvalidOperationException("Unexpected race interpolation");
        public static float Lerp(float a,float b,float t)=>throw new InvalidOperationException("Unexpected race interpolation");
        public static float LerpAngle(float a,float b,float t)=>throw new InvalidOperationException("Unexpected race interpolation");
    }
    public struct Vector3 {
        public float x,y,z;
        public Vector3(float x,float y,float z){this.x=x;this.y=y;this.z=z;}
        public static Vector3 Lerp(Vector3 a,Vector3 b,float t)=>throw new InvalidOperationException("Unexpected race interpolation");
    }
    public struct Vector4 {
        public float this[int at] {get=>throw new InvalidOperationException("Unexpected race pose");set=>throw new InvalidOperationException("Unexpected race pose");}
        public static Vector4 Lerp(Vector4 a,Vector4 b,float t)=>throw new InvalidOperationException("Unexpected race interpolation");
    }
}
public static class Idas3Native {
    public static string Error()=>"Fixture must not invoke native game state";
    public static int Idas3SceneSetPreRaceNames(string local,string remote)=>throw new InvalidOperationException(Error());
}
public static class Idas3CourseCatalog {
    public const int Count=18;
    public static bool RequiresNight(int course)=>false;
    public static bool Available(int course)=>false;
    public static readonly string[] Packs=new string[9],Slugs=new string[9];
}
namespace Idas3.Multiplayer {
    internal static class Idas3EosOnlineConfig {
        internal static bool DiagnosticRun=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-p2p-check")>=0;
    }
    public class ChatFixtureTransport:IIdas3Transport {
        public string Kind {get;set;}="Fixture";
        public bool Available {get;set;}=true;
        public bool Connected {get;set;}=true;
        public bool IsHost {get;set;}=true;
        public string LocalId=>"fixture-local";
        public string LocalName=>"Local driver";
        public string RemoteId=>"fixture-peer";
        public string RemoteName=>"Transport rival";
        public string RoomCode {get;set;}="private-chat-fixture";
        public string Status=>"";
        public IReadOnlyList<Idas3Room> Rooms=>Array.Empty<Idas3Room>();
        public event Action<byte[]> Message;
        public event Action PeerChanged {add{} remove{}}
        public event Action<string> Error {add{} remove{}}
        public readonly List<byte[]> Sent=new List<byte[]>();
        public readonly List<bool> Reliable=new List<bool>();
        public bool Initialize()=>true;
        public void Host(string name)=>throw new InvalidOperationException("Unexpected room creation");
        public void Join(string code)=>throw new InvalidOperationException("Unexpected room join");
        public void Browse()=>throw new InvalidOperationException("Unexpected room search");
        public void Send(byte[] data,bool reliable){Sent.Add(data);Reliable.Add(reliable);}
        public void Poll(){}
        public void Deliver(byte[] data)=>Message?.Invoke(data);
        public void Leave(){Connected=false;RoomCode="";}
        public void Dispose(){Leave();}
    }
#if !IDAS3_ONLINE_SESSION_TEST
    // The fixture never initializes cloud services or opens a Relay route.
    public sealed class Idas3EosTransport:ChatFixtureTransport,IIdas3OnlineActivityTransport {
        internal void RefreshPlayerName(){}
        internal Func<string,string[],string[],bool> ApplyStatsResets {get;set;}
        public bool IsBusy=>false;
        public bool PublishActivity {get;set;}
        public bool ActivityRequested {get;set;}
        public string ActivityState {get;set;}
        public Idas3OnlineActivity Activity=>default;
    }
#endif
}
