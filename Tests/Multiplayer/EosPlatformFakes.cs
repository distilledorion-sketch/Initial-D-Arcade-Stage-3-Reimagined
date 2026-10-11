using System;
using System.Collections.Generic;

// Only external SDK/Unity boundaries are substituted. The production platform
// lifecycle, callback queue and authentication state machine compile unchanged.
namespace UnityEngine {
    public class Object {public static void DontDestroyOnLoad(Object value){}}
    public class MonoBehaviour:Object {}
    public enum HideFlags {HideInHierarchy}
    public class GameObject:Object {public HideFlags hideFlags;public GameObject(string name){}public T AddComponent<T>()where T:new()=>new T();}
    public static class Application {
        public static bool isEditor=false;
        public static string persistentDataPath="unused-diagnostic-path";
        public static string platform="WindowsPlayer";
        public static event Action quitting;
        internal static void Quit()=>quitting?.Invoke();
    }
}
namespace Epic.OnlineServices {
    public enum Result {Success,AlreadyConfigured,DuplicateNotAllowed,NoChange,InvalidUser,AccessDenied,ServiceFailure}
    public enum LoginStatus {NotLoggedIn,LoggedIn}
    public enum ExternalCredentialType {DeviceidAccessToken}
    public sealed class ProductUserId {
        readonly string value;public ProductUserId(string id){value=id;}
        public bool IsValid()=>!string.IsNullOrEmpty(value);public override string ToString()=>value;
    }
    public sealed class ContinuanceToken {}
}
namespace Epic.OnlineServices.Connect {
    public struct CreateDeviceIdOptions {public string DeviceModel;}
    public struct Credentials {public ExternalCredentialType Type;}
    public struct UserLoginInfo {public string DisplayName;}
    public struct LoginOptions {public Credentials Credentials;public UserLoginInfo UserLoginInfo;}
    public struct CreateUserOptions {public ContinuanceToken ContinuanceToken;}
    public struct AddNotifyAuthExpirationOptions {}
    public struct AddNotifyLoginStatusChangedOptions {}
    public struct CreateDeviceIdCallbackInfo {public Result ResultCode;}
    public struct LoginCallbackInfo {public Result ResultCode;public ProductUserId LocalUserId;public ContinuanceToken ContinuanceToken;}
    public struct CreateUserCallbackInfo {public Result ResultCode;public ProductUserId LocalUserId;}
    public struct AuthExpirationCallbackInfo {}
    public struct LoginStatusChangedCallbackInfo {public LoginStatus CurrentStatus;}
    public delegate void CreateDeviceIdCallback(ref CreateDeviceIdCallbackInfo info);
    public delegate void LoginCallback(ref LoginCallbackInfo info);
    public delegate void CreateUserCallback(ref CreateUserCallbackInfo info);
    public delegate void AuthExpirationCallback(ref AuthExpirationCallbackInfo info);
    public delegate void LoginStatusChangedCallback(ref LoginStatusChangedCallbackInfo info);
    public sealed class ConnectInterface {
        internal int Creates,Logins,Users,RemovedNotify;
        internal CreateDeviceIdOptions LastDevice;
        internal LoginOptions LastLogin;
        internal CreateUserOptions LastUser;
        internal CreateDeviceIdCallback create;
        internal LoginCallback login;
        internal CreateUserCallback user;
        internal AuthExpirationCallback expiry;
        internal LoginStatusChangedCallback status;
        internal string ActiveIdentity;
        public LoginStatus GetLoginStatus(ProductUserId id)=>id!=null&&id.ToString()==ActiveIdentity?LoginStatus.LoggedIn:LoginStatus.NotLoggedIn;
        public void CreateDeviceId(ref CreateDeviceIdOptions options,object ignored,CreateDeviceIdCallback callback){++Creates;LastDevice=options;create=callback;}
        public void Login(ref LoginOptions options,object ignored,LoginCallback callback){++Logins;LastLogin=options;login=callback;}
        public void CreateUser(ref CreateUserOptions options,object ignored,CreateUserCallback callback){++Users;LastUser=options;user=callback;}
        public ulong AddNotifyAuthExpiration(ref AddNotifyAuthExpirationOptions options,object ignored,AuthExpirationCallback callback){expiry=callback;return 10;}
        public ulong AddNotifyLoginStatusChanged(ref AddNotifyLoginStatusChangedOptions options,object ignored,LoginStatusChangedCallback callback){status=callback;return 20;}
        public void RemoveNotifyAuthExpiration(ulong id){if(id!=10)throw new Exception("Invalid expiry notify");++RemovedNotify;}
        public void RemoveNotifyLoginStatusChanged(ulong id){if(id!=20)throw new Exception("Invalid status notify");++RemovedNotify;}
        internal void CompleteDevice(Result result){var callback=create;create=null;var info=new CreateDeviceIdCallbackInfo{ResultCode=result};callback(ref info);}
        internal void CompleteLogin(Result result,string identity=null,ContinuanceToken token=null){var callback=login;login=null;if(result==Result.Success)ActiveIdentity=identity;var info=new LoginCallbackInfo{ResultCode=result,LocalUserId=identity==null?null:new ProductUserId(identity),ContinuanceToken=token};callback(ref info);}
        internal void CompleteUser(Result result,string identity=null){var callback=user;user=null;if(result==Result.Success)ActiveIdentity=identity;var info=new CreateUserCallbackInfo{ResultCode=result,LocalUserId=identity==null?null:new ProductUserId(identity)};callback(ref info);}
        internal void Expire(){var info=new AuthExpirationCallbackInfo();expiry(ref info);}
        internal void LoseLogin(){ActiveIdentity=null;var info=new LoginStatusChangedCallbackInfo{CurrentStatus=LoginStatus.NotLoggedIn};status(ref info);}
    }
}
namespace Epic.OnlineServices.Platform {
    public struct InitializeOptions {public string ProductName,ProductVersion;}
    public struct ClientCredentials {public string ClientId,ClientSecret;}
    [Flags]public enum PlatformFlags {None=0,DisableOverlay=1,DisableSocialOverlay=2,LoadingInEditor=4}
    public struct Options {
        public string ProductId,SandboxId,DeploymentId,CacheDirectory;
        public ClientCredentials ClientCredentials;public PlatformFlags Flags;
        public bool IsServer;public uint TickBudgetInMilliseconds;public double TaskNetworkTimeoutSeconds;
    }
    public sealed class PlatformInterface {
        internal static readonly PlatformInterface Instance=new PlatformInterface();
        internal static Options LastOptions;
        internal static int Initializations,Creations,Shutdowns;
        internal int Ticks,Releases;
        internal readonly Connect.ConnectInterface Connect=new Connect.ConnectInterface();
        internal readonly Queue<Action> NativeEvents=new Queue<Action>();
        public static Result Initialize(ref InitializeOptions options){++Initializations;return Result.Success;}
        public static PlatformInterface Create(ref Options options){LastOptions=options;++Creations;return Instance;}
        public static Result Shutdown(){++Shutdowns;return Result.Success;}
        public Connect.ConnectInterface GetConnectInterface()=>Connect;
        public void Tick(){++Ticks;while(NativeEvents.Count!=0)NativeEvents.Dequeue()();}
        public void Release(){++Releases;}
    }
}
namespace Idas3.Multiplayer {
    internal static class Idas3EosNativeLibrary {internal static void Initialize(){}internal static void Shutdown(){}}
    internal static class Idas3EosTransport {
        internal static int Shutdowns;
        internal static bool HandleWasLive;
        internal static void ShutdownAll(){++Shutdowns;HandleWasLive=Idas3EosPlatform.Handle!=null&&Epic.OnlineServices.Platform.PlatformInterface.Instance.Releases==0;}
    }
    public sealed class Idas3EosOnlineConfig {
        public string ProductId="fixture-product",SandboxId="fixture-sandbox",DeploymentId="fixture-deployment",ClientId="fixture-client",ClientSecret="not-a-real-credential";
        public static string AuthenticationProfile=>"fixture";
        public static string PlayerName()=>"Fixture driver";
    }
}
