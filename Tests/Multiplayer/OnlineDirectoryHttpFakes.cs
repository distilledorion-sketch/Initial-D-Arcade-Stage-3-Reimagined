using System;
using System.Collections.Generic;
using System.Text;
using System.Web.Script.Serialization;

// External Unity/SDK boundaries only. The request lifecycle and HTTP adapter
// under test are the production Idas3OnlinePlayerDirectoryHttp implementation.
namespace UnityEngine {
    public static class Application {
        public static string version="test-build";
        public static event Action quitting;
        internal static int Subscribers=>quitting?.GetInvocationList().Length??0;
        internal static void Quit()=>quitting?.Invoke();
    }
    public static class JsonUtility {
        public static string ToJson(object value)=>new JavaScriptSerializer().Serialize(value);
        public static T FromJson<T>(string json)=>new JavaScriptSerializer().Deserialize<T>(json);
    }
    public static class Debug {
        internal static readonly List<string> Logs=new List<string>();
        public static void Log(object value)=>Logs.Add(Convert.ToString(value));
        public static void LogWarning(object value)=>Logs.Add(Convert.ToString(value));
        public static void LogError(object value)=>Logs.Add(Convert.ToString(value));
        public static void LogException(Exception error)=>Logs.Add(error.ToString());
    }
    public class AsyncOperation {
        public bool isDone;
        public event Action<AsyncOperation> completed;
        internal void Complete(){isDone=true;completed?.Invoke(this);}
    }
}
namespace UnityEditor {
    public static class AssemblyReloadEvents {
        public static event Action beforeAssemblyReload;
        internal static int Subscribers=>beforeAssemblyReload?.GetInvocationList().Length??0;
        internal static void Reload()=>beforeAssemblyReload?.Invoke();
    }
}
namespace UnityEngine.Networking {
    public sealed class UploadHandlerRaw:IDisposable {
        public readonly byte[] data;internal bool Disposed;
        public UploadHandlerRaw(byte[] data){this.data=data;}
        public void Dispose(){Disposed=true;}
    }
    public sealed class DownloadHandlerBuffer:IDisposable {
        public byte[] data;
        public string text=>data==null?null:Encoding.UTF8.GetString(data);
        internal bool Disposed;
        public void Dispose(){Disposed=true;}
    }
    public sealed class UnityWebRequest:IDisposable {
        public enum Result {Success,ConnectionError,ProtocolError,DataProcessingError}
        internal static readonly List<UnityWebRequest> Created=new List<UnityWebRequest>();
        internal static bool CompleteSynchronously=false,ThrowOnSend=false,CompleteOnAbort=false;
        internal readonly UnityEngine.AsyncOperation Operation=new UnityEngine.AsyncOperation();
        internal readonly Dictionary<string,string> Headers=new Dictionary<string,string>();
        internal readonly string Url,Method;
        internal int Aborts,Disposals;
        public UploadHandlerRaw uploadHandler;
        public DownloadHandlerBuffer downloadHandler;
        public int timeout,redirectLimit;
        public Result result=Result.Success;
        public long responseCode=200;
        public UnityWebRequest(string url,string method){Url=url;Method=method;Created.Add(this);}
        public void SetRequestHeader(string name,string value){Headers[name]=value;}
        public UnityEngine.AsyncOperation SendWebRequest(){
            if(ThrowOnSend)throw new InvalidOperationException("Send failed");
            if(CompleteSynchronously){downloadHandler.data=Encoding.UTF8.GetBytes("{\"version\":1,\"status\":\"allowed\"}");Operation.isDone=true;}
            return Operation;
        }
        internal void Complete(string body="{\"version\":1,\"status\":\"allowed\"}",long code=200,Result status=Result.Success){result=status;responseCode=code;downloadHandler.data=body==null?null:Encoding.UTF8.GetBytes(body);Operation.Complete();}
        public void Abort(){Aborts++;if(CompleteOnAbort)Operation.Complete();}
        public void Dispose(){Disposals++;uploadHandler?.Dispose();downloadHandler?.Dispose();}
    }
}
namespace Epic.OnlineServices {
    public enum Result {Success,NotFound,AccessDenied}
    public sealed class ProductUserId {
        readonly string value;public ProductUserId(string value){this.value=value;}
        public override string ToString()=>value;
    }
}
namespace Epic.OnlineServices.Connect {
    public struct CopyIdTokenOptions {public Epic.OnlineServices.ProductUserId LocalUserId;}
    public struct IdToken {public Epic.OnlineServices.ProductUserId ProductUserId;public string JsonWebToken;}
    public sealed class ConnectInterface {
        internal Epic.OnlineServices.Result Result=Epic.OnlineServices.Result.Success;
        internal IdToken? Token=new IdToken{ProductUserId=new Epic.OnlineServices.ProductUserId(Idas3.Multiplayer.Idas3EosPlatform.Self),JsonWebToken="fixture-secret-token"};
        internal string RequestedUser;
        public Epic.OnlineServices.Result CopyIdToken(ref CopyIdTokenOptions options,out IdToken? token){RequestedUser=options.LocalUserId.ToString();token=Token;return Result;}
    }
}
namespace Idas3.Multiplayer {
    internal static class Idas3CommunityTimes {internal const string ServiceUrl="https://fixture.invalid";}
    internal static class Idas3EosSanctions {internal const string Unavailable="Online access could not be verified. Please retry shortly.";}
    internal sealed class Idas3OnlinePlayerDirectory {
        internal sealed class Reply {public int version=0;public string playerId=null,status=null,targetPlayerId=null,targetStatus=null;public long checkedAt=0;}
    }
    internal sealed class HttpFakeHandle {
        internal readonly Epic.OnlineServices.Connect.ConnectInterface Connect=new Epic.OnlineServices.Connect.ConnectInterface();
        internal Epic.OnlineServices.Connect.ConnectInterface GetConnectInterface()=>Connect;
    }
    internal static class Idas3EosPlatform {
        internal const string Self="11111111111111111111111111111111";
        internal static bool SignedIn=true,IsStopping=false;
        internal static Epic.OnlineServices.ProductUserId User=new Epic.OnlineServices.ProductUserId(Self);
        internal static readonly HttpFakeHandle Handle=new HttpFakeHandle();
        internal static readonly Queue<Action> Deferred=new Queue<Action>();
        internal static void Defer(Action action){if(!IsStopping)Deferred.Enqueue(action);}
        internal static void Pump(){while(!IsStopping&&Deferred.Count>0)Deferred.Dequeue()();}
        internal static void Stop(){IsStopping=true;Deferred.Clear();}
    }
}
